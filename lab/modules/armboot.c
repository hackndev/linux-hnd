/*
 *  lab/modules/armboot.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, October 07, 2003
 *
 *  ARMBoot kernel loader
 *  main routines
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

#ifndef HAVE_APPROPRIATE_ARCH
#	error "Your ARM architecture is not supported."
#endif

#ifdef CONFIG_DISCONTIGMEM
#	error "Memory is discontiguous - can't help you. Sorry."
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
int armboot(unsigned char* kerneladdr, int size, unsigned char* cmdline)
{
	struct physlist* addrmap;
	struct tag* t;
	unsigned int* asmaddr;
	int mappos;
	unsigned int kernaddr;
	
	/* At this point, we will allocate 64k for a list of things that need to be mapped later. */
	addrmap = (struct physlist*)kmalloc(64 * 1024, GFP_KERNEL);
	if (!addrmap)
		return -ENOMEM;
	
	/* We start at the first element of the map. */
	mappos = 0;
	
	/* Now we need to allocate 64kbytes for our tagged list. */
	t = (struct tag*)kmalloc(64*1024, GFP_KERNEL);
	if (!t)
		return -ENOMEM;
	
	/* Set up our tagged list reloc block. */
	addrmap[mappos].physaddr = virt_to_phys(t);
	addrmap[mappos].newphysaddr = TAGBASE;
	addrmap[mappos].last = 0;
	
	/* Now set up the tagged list. */
	t->hdr.tag = ATAG_CORE;
	t->hdr.size = tag_size(tag_core);
	t->u.core.flags = 0;
	t->u.core.pagesize = 0x1000;
	t->u.core.rootdev = 0x00FF;
	t = tag_next(t);

	if (cmdline) {
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
	
	t->hdr.tag = ATAG_NONE;
	t->hdr.size = 0;
	t = tag_next(t);
	
	/* Now just fill in the size, and we can begin relocating the kernel. */
	addrmap[mappos].size = virt_to_phys(t) - addrmap[mappos].physaddr;
	mappos++;
	
	kernaddr = MEMSTART+0x8000;
	
	printk(KERN_EMERG "armboot: Placing new kernel into temporary RAM...\n");
	
	/* Begin relocating. We're going to have to trust that the kernel won't give us any addresses in the first few megs of RAM. */
	while(size)
	{
		int relocsize;
		unsigned char* block;
		
		/* Make sure it fits within our block... */
		relocsize = (size > 64*1024) ? 64*1024 : size;
		size -= relocsize;
		
		/* Allocate said block... (always 64k to please your kmallocness) */
		block = (unsigned char*)kmalloc(64*1024, GFP_KERNEL);
		if (!block)
			return -ENOMEM;
		
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

EXPORT_SYMBOL(armboot);

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("ARMBoot kernel loader");
MODULE_LICENSE("GPL");
