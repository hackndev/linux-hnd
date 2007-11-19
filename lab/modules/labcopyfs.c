/*
 *  lab/modules/labcopyfs.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  fs module for LAB "copy" command.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/lab/lab.h>
#include <linux/lab/copy.h>

static int getcheck(char* filename)
{
	struct stat sstat;
	
	if (sys_newstat(filename, &sstat) < 0)
	{
		lab_printf("fs: getcheck: Sorry, %s seems to be missing - or at least, not stat'able.\n", filename);
		return 0;
	}
	return 1;
}

static int putcheck(char* filename)
{
	return 1;
}
extern void dumpmemstats(char* detail);
static unsigned char* get(int* count, char* filename)
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

static int put(int count, unsigned char* data, char* filename)
{
	int fd;
	int total = 0;
	int sz;
	
	set_fs(KERNEL_DS);
	
	if ((fd=sys_open(filename,O_WRONLY|O_CREAT,S_IRWXU)) < 0)
			return 0;
	
	while (count)
	{
		sz = (count > 1048576) ? 1048576 : count;
		total += sz;
		
		if (sys_write(fd,data,sz) < sz)
		{
			sys_close(fd);
			return 0;
		}
		data += sz;
		count -= sz;
	}
	
	sys_close(fd);

	return 1;
}

static int unlink(char* filename)
{
	if (!getcheck(filename))
		return 0;
	sys_unlink(filename);
	return 1;
}

int labcopyfs_init(void)
{
	lab_copy_addsrc("fs",getcheck,get);
	lab_copy_adddest("fs",putcheck,put);
	lab_copy_addunlink("fs",getcheck,unlink);
	
	return 0;
}

void labcopyfs_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy fs module");
MODULE_LICENSE("GPL");
module_init(labcopyfs_init);
module_exit(labcopyfs_cleanup);
