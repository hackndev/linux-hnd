/*
 *  ARMBoot interface based on the code from LAB
 *
 *  by Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 *  Original Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, October 07, 2003
 *
 */

#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#ifndef CONFIG_ARM
#	error "ARMBoot must be compiled on an ARM system. Duh."
#endif

#ifdef CONFIG_ARCH_PXA
#include <asm/hardware.h>
#	define HAVE_APPROPRIATE_ARCH
#	define TAGBASE 0xA0000100
#	define MEMSTART 0xA0000000
#endif

#ifdef CONFIG_ARCH_SA1100
#	define HAVE_APPROPRIATE_ARCH
#	define TAGBASE 0xC0000100
#	define MEMSTART 0xC0000000
#endif

#ifdef CONFIG_ARCH_OMAP
#	define HAVE_APPROPRIATE_ARCH
#	define TAGBASE 0x10000100
#	define MEMSTART 0x10000000
#endif

#ifndef HAVE_APPROPRIATE_ARCH
#	error "Your ARM architecture is not supported."
#endif

#ifdef CONFIG_DISCONTIGMEM
#	error "Memory is discontiguous - can't help you. Sorry."
#endif

#define DEBUG_ARMBOOT

#ifdef DEBUG_ARMBOOT
#define DBGK(x, ...) printk(KERN_ERR "ARMBoot: " x "\n", ##__VA_ARGS__)
#else
#define DBGK
#endif

struct physlist {		// 128bit == 16byte
	int	physaddr;	// old address
	int	newphysaddr;	// address to relocate to
	int	size;		// size of block to relocate
	int	last;		// is this the last block?
};

extern void armboot_asm(int listaddr, int myaddr, int rambase, int machtype);
void(*armboot_ptr)(int listaddr, int myaddr, int rambase, int machtype);

extern char* relocdone;

/* This shouldn't return. */
int armboot(unsigned char* kerneladdr, int size, unsigned char* cmdline,
		unsigned char* initrd, int size_initrd)
{
	struct physlist* addrmap;
	struct tag* t;
	unsigned int* asmaddr;
	int mappos;
	unsigned int kernaddr, initrdaddr;
	
	/* At this point, we will allocate 64k for a list of things that need to be mapped later. */
	addrmap = (struct physlist*)kmalloc(64 * 1024, GFP_KERNEL);
	if (!addrmap) {
		DBGK("kmalloc for addrmap failed");
		return -ENOMEM;
	}
	
	/* We start at the first element of the map. */
	mappos = 0;
	
	/* Now we need to allocate 64kbytes for our tagged list. */
	t = (struct tag*)kmalloc(64*1024, GFP_KERNEL);
	if (!t) {
		DBGK("kmalloc for t failed");
		return -ENOMEM;
	}
	
	/* Set up our tagged list reloc block. */
	addrmap[mappos].physaddr = virt_to_phys(t);
	addrmap[mappos].newphysaddr = TAGBASE;
	addrmap[mappos].last = 0;
	
	/* Now set up the tagged list. */
	t->hdr.tag = ATAG_CORE;
	t->hdr.size = tag_size(tag_core);
	t->u.core.flags = 1;
	t->u.core.pagesize = 0x1000;
	t->u.core.rootdev = 0x100;
	t = tag_next(t);
	
	
	if (cmdline[0] != '\0') {
	    t->hdr.tag = ATAG_CMDLINE;
	    t->hdr.size = (sizeof(struct tag_header)+strlen(cmdline)+4) >> 2;
	    strcpy(t->u.cmdline.cmdline, cmdline);
	    t = tag_next(t);
	}
	
	t->hdr.tag = ATAG_MEM;			/* We assume that memory is contiguous,       */
	t->hdr.size = tag_size(tag_mem32);	/* and we won't let anyone tell us otherwise. */
	t->u.mem.size = (num_physpages >> (20-PAGE_SHIFT)) * 1024*1024;
	t->u.mem.start = MEMSTART;
	t = tag_next(t);

	if(size_initrd) {
		DBGK("using intrd");
		t->hdr.tag = ATAG_INITRD2;
		t->hdr.size = tag_size(tag_initrd);
		t->u.initrd.start = MEMSTART + 0x0400000;
		t->u.initrd.size = size_initrd;
		t = tag_next(t);
	}
	
	t->hdr.tag = ATAG_NONE;
	t->hdr.size = 0;
	t = tag_next(t);
	
	/* Now just fill in the size, and we can begin relocating the kernel. */
	addrmap[mappos].size = virt_to_phys(t) - addrmap[mappos].physaddr;
	mappos++;
	
	kernaddr = MEMSTART+0x8000;
	initrdaddr = MEMSTART+0x0400000;
	DBGK("kernel PA: %lx\ninitrd PA:%lx", virt_to_phys(kerneladdr), virt_to_phys(initrd));
	
	printk(KERN_EMERG "armboot: Placing new kernel %sinto temporary RAM...\n",
			(size_initrd?"and initrd ":""));
	
	/* Begin relocating. We're going to have to trust that the kernel won't give us any addresses in the first few megs of RAM. */
	while(size) {
		int relocsize;
		unsigned char* block;
		
		/* Make sure it fits within our block... */
		relocsize = (size > 64*1024) ? 64*1024 : size;
		size -= relocsize;
		
		/* Allocate said block... (always 64k to please your kmallocness) */
		block = (unsigned char*)kmalloc(64*1024, GFP_KERNEL);
		if (!block) {
			printk(KERN_ERR "Failed to grab mem chunk for kernel.\n"
					"Still %d bytes waiting\n", size);
			return -ENOMEM;
		}
	
		/* Copy kernel into said block... */
		memcpy(block, kerneladdr, relocsize);
		
		/* Set up our allocation tables. */
		addrmap[mappos].physaddr = virt_to_phys(block);
		addrmap[mappos].newphysaddr = kernaddr;
		addrmap[mappos].size = relocsize;
		addrmap[mappos].last = 0;
		
		mappos++;
		kernaddr += relocsize;
		kerneladdr += relocsize;
	}
	DBGK("done moving kernel");

	while(size_initrd) {
		int relocsize;
		unsigned char* block;
		
		/* Make sure it fits within our block... */
		relocsize = (size_initrd > 64*1024) ? 64*1024 : size_initrd;
		size_initrd -= relocsize;
		
		/* Allocate said block... (always 64k to please your kmallocness) */
		block = (unsigned char*)kmalloc(64*1024, GFP_KERNEL);
		if (!block) {
			printk(KERN_ERR "Failed to grab mem chunk for initrd.\n"
					"Still %d bytes waiting\n", size_initrd);
			return -ENOMEM;
		}
		
		/* Copy kernel into said block... */
		memcpy(block, initrd, relocsize);
		
		/* Set up our allocation tables. */
		addrmap[mappos].physaddr = virt_to_phys(block);
		addrmap[mappos].newphysaddr = initrdaddr;
		addrmap[mappos].size = relocsize;
		addrmap[mappos].last = 0;
		
		mappos++;
		initrd += relocsize;
		initrdaddr += relocsize;
	}
	DBGK("done moving initrd");
	
	addrmap[mappos-1].last = 1;
	
	/* Nowadays we get an address from the kernel. */
	asmaddr = (unsigned int*)kmalloc(64*1024, GFP_KERNEL);
	memcpy(asmaddr, &armboot_asm, 16384); // 16kb
	armboot_ptr = (void(*)(int,int,int,int))asmaddr;
	
	cpu_proc_fin ();

	printk(KERN_EMERG "armboot: Booting new kernel...\n");
	armboot_ptr(virt_to_phys(addrmap), ((int)&relocdone) - ((int)&armboot_asm) + virt_to_phys(asmaddr), MEMSTART+0x8000, machine_arch_type);
	
	return 0;
}

// FIXME: clean this hell
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/string.h>

static struct proc_dir_entry *proc_intf;
#define PROCFS_NAME		"armboot"
#define MAX_BUF_SIZE		202
#define ARG_MAX_SIZE		200

static char kernel_path	[ARG_MAX_SIZE];
static char cmdline	[ARG_MAX_SIZE];
static char initrd_path	[ARG_MAX_SIZE];

static unsigned long procfs_buffer_size = 0;

static unsigned char* get_my_file(int* count, char* filename)
{
	unsigned char* ptr, *ptr2;
	struct stat sstat;
	int fd;
	int remaining;
	int sz, total=0;
	set_fs(KERNEL_DS);
	
	if (sys_newstat(filename, &sstat) < 0)
		return 0;
	
	*count = remaining = sstat.st_size;
	
	ptr = ptr2 = vmalloc(sstat.st_size+32);
	if (!ptr)
	{
		printk("lab: AIIIIIIIIIIIIIIIIIGHHH!! out of memory!!\n");
		printk("lab: I guess we just can't allocate 0x%08x bytes???\n", (unsigned int)sstat.st_size+32);
		return 0;
 	}
	
	if ((fd=sys_open(filename,O_RDONLY,0)) < 0)
	{
		printk("lab: uh. couldn't open the file.\n");
		vfree(ptr);
		return 0;
	}
	
	while (remaining)
	{
		sz = (1048576 < remaining) ? 1048576 : remaining;
		total += sz;
		if (sys_read(fd,ptr2,sz) < sz)
		{
			printk("lab: uh. couldn't read from the file.\n");
			sys_close(fd);
			vfree(ptr);
			return 0;
		}
		ptr2 += sz;
		remaining -= sz;
	}
	
	sys_close(fd);
	return ptr;
}

static void handle_proc_rq(void)
{
	int count;
	int count_initrd = 0;
	int retval = 667;
	unsigned char* data;
	unsigned char* initrd = 0;

	data = get_my_file(&count,kernel_path);
	if(initrd_path[0] != '\0')
		initrd = get_my_file(&count_initrd, initrd_path);
	if(data!=0)
		retval = armboot(data, count, cmdline, initrd, count_initrd);
	printk(KERN_EMERG "ARMBoot failed to boot - system may be in an unstable state (return %d)\n", retval);
}

int procfile_read(char *buffer, char **buffer_location,
		off_t offset, int buffer_length, int *eof, void *data)
{
	int ret;
	
	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		char out[ARG_MAX_SIZE*3+200];
		sprintf(out, "Kernel: %s\nCmdline: %s\nInitrd: %s\n", kernel_path,
				cmdline, initrd_path);
		/* fill the buffer, return the buffer size */
		memcpy(buffer, out, strlen(out)+1);
		ret = strlen(out)+1;
	}

	return ret;
}

#define CHECK_LEN(t) if(strlen(procfs_buffer)<3) { \
	printk(KERN_ERR "ARMBoot: argument for %s is too short\n", t); \
	return procfs_buffer_size; \
}
int procfile_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char procfs_buffer[MAX_BUF_SIZE];
	char *shift;
	/* get buffer size */
	procfs_buffer_size = count;
	if (procfs_buffer_size > MAX_BUF_SIZE-1 ) {
		procfs_buffer_size = MAX_BUF_SIZE-1;
	}
	
	/* write data to the buffer */
	if ( copy_from_user(procfs_buffer, buffer, procfs_buffer_size) ) {
		return -EFAULT;
	}
	procfs_buffer[procfs_buffer_size+1] = '\0';

	if(procfs_buffer[0] == '\0') {
		printk(KERN_ERR "ARMBoot: emty call to /proc/%s\n", PROCFS_NAME);
		return procfs_buffer_size;
	}
	
	shift = strchr(procfs_buffer, '\n');
	if(shift) *shift = '\0';

	shift = procfs_buffer+2;

	switch(procfs_buffer[0]) {
		case 'k':
			CHECK_LEN("kernel_path");
			strncpy(kernel_path, shift, ARG_MAX_SIZE);
			break;
		case 'i':
			CHECK_LEN("initrd_path");
			strncpy(initrd_path, shift, ARG_MAX_SIZE);
			break;
		case 'c':
			CHECK_LEN("cmdline");
			strncpy(cmdline, shift, ARG_MAX_SIZE);
			break;
		case 'b':
			printk(KERN_ERR "\nARMBoot running...\n");
			handle_proc_rq();
			break;
		default:
			printk(KERN_ERR "ARMBoot: bad request\n");
			break;
	}
	
	return procfs_buffer_size;
}


int init_module()
{
	kernel_path[0] = '\0';
	cmdline[0] = '\0';
	initrd_path[0] = '\0';
	/* create the /proc file */
	proc_intf = create_proc_entry(PROCFS_NAME, 0644, NULL);
	
	if (proc_intf == NULL) {
		remove_proc_entry(PROCFS_NAME, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
			PROCFS_NAME);
		return -ENOMEM;
	}

	proc_intf->write_proc = procfile_write;
	proc_intf->read_proc  = procfile_read;
	proc_intf->owner 	  = THIS_MODULE;
	proc_intf->mode 	  = S_IFREG | S_IWUSR | S_IRUSR;
	proc_intf->uid 	  = 0;
	proc_intf->gid 	  = 0;
	proc_intf->size 	  = 37;

	printk(KERN_INFO "ARMBoot interface v0.2 loaded\n");	
	return 0;	/* everything is ok */
}

// INFO: no need for cleanup, because armboot-asm.S make impossible modularization
module_init(init_module);

MODULE_AUTHOR("Vladimir \"Farcaller\" Pouzanov");
MODULE_DESCRIPTION("ARMBoot kernel loader");
MODULE_LICENSE("GPL");
