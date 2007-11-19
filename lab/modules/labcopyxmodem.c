/*
 *  lab/modules/labcopyxmodem.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  xmodem module for LAB "copy" command.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/lab/copy.h>

extern void lab_xmodem_send(unsigned char* buf, int size);

static int putcheck(char* filename)
{
	if (*filename != '\0')
		return 0;
	return 1;
}

static int put(int count, unsigned char* data, char* filename)
{
	if (*filename != '\0')
		return 0;
	lab_xmodem_send(data, count);
	return 1;
}

int labcopyxmodem_init(void)
{
	lab_copy_adddest("xmodem",putcheck,put);
	
	return 0;
}

void labcopyxmodem_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy xmodem module");
MODULE_LICENSE("GPL");
module_init(labcopyxmodem_init);
module_exit(labcopyxmodem_cleanup);
