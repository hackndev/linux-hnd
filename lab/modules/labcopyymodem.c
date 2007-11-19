/*
 *  lab/modules/labcopyymodem.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  ymodem module for LAB "copy" command.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/lab/lab.h>
#include <linux/lab/copy.h>

extern char* lab_ymodem_receive(unsigned int* length, int ymodemg);


static int getcheck(char* filename)
{
	if (*filename != '\0')
		return 0;
	return 1;
}

static unsigned char* get(int* count, char* filename)
{
	int r;
	if (*filename != '\0')
		return 0;
	r= lab_ymodem_receive(count,0);
	lab_printf("YMODEM: Received %d bytes\n", *count);
	return r;
}

static unsigned char* getg(int* count, char* filename)
{
	if (*filename != '\0')
		return 0;
	return lab_ymodem_receive(count,1);
}


int labcopyymodem_init(void)
{
	lab_copy_addsrc("ymodem",getcheck,get);
	lab_copy_addsrc("ymodem-g",getcheck,getg);
	
	return 0;
}

void labcopyymodem_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy ymodem module");
MODULE_LICENSE("GPL");
module_init(labcopyymodem_init);
module_exit(labcopyymodem_cleanup);
