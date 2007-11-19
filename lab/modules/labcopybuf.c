/*
 *  lab/modules/labcopybuf.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  vmalloc buffer module for LAB "copy" command.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/lab/lab.h>
#include <linux/lab/copy.h>

static char* buffers[10];
static int bufsizes[10];

static int check(char* filename)
{
	if (*filename < '0' || *filename > '9' || filename[1] != '\0')
		return 0;
	return 1;
}

static unsigned char* get(int* count, char* filename)
{
	int bufid;
	unsigned char* buf;
	
	if (*filename < '0' || *filename > '9' || filename[1] != '\0')
		return 0;
		
	bufid = *filename - '0';
	
	if (buffers[bufid])
	{
		*count = bufsizes[bufid];
		buf = buffers[bufid];
		buffers[bufid] = 0;
		return buf;
	}
	return NULL;
}

static int put(int count, unsigned char* data, char* filename)
{
	int bufid;
	
	if (*filename < '0' || *filename > '9' || filename[1] != '\0')
		return 0;
		
	bufid = *filename - '0';
	
	if (buffers[bufid])
		vfree(buffers[bufid]);
		
	if ((buffers[bufid] = vmalloc(count)) == NULL)
		return 0;

	bufsizes[bufid] = count;
	memcpy(buffers[bufid], data, count);
	
	return 1;
}

int labcopybuf_init(void)
{
	int i;
	
	for (i=0;i<9;i++)
		buffers[i]=0;
		
	lab_copy_addsrc("buf",check,get);
	lab_copy_adddest("buf",check,put);
	
	return 0;
}

void labcopybuf_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy buf module");
MODULE_LICENSE("GPL");
module_init(labcopybuf_init);
module_exit(labcopybuf_cleanup);
