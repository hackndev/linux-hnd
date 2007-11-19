/*
 *  lab/modules/labcopydummy.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  dummy module for LAB "copy" command.
 */

#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/lab/lab.h>
#include <linux/lab/copy.h>

static int check(char* filename)
{
	return 1;
}

static unsigned char* get(int* count, char* filename)
{
	unsigned char* ptr;
	ptr = vmalloc(18*1024*1024);	/* 18 mb file */
	if (ptr)
		*count = 18*1024*1024;
	return ptr;
}

static int put(int count, unsigned char* data, char* filename)
{
	return 1;
}

int labcopydummy_init(void)
{
	lab_copy_adddest("dummy",check,put);
	lab_copy_addsrc("dummy",check,get);
	
	return 0;
}

void labcopydummy_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy dummy module");
MODULE_LICENSE("GPL");
module_init(labcopydummy_init);
module_exit(labcopydummy_cleanup);
