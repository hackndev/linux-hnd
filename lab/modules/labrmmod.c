/*
 *  lab/modules/labrmmod.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, September 06, 2003
 *
 *  "rmmod" command for LAB CLI.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <asm/dma.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>
#include <linux/lab/copy.h>

void lab_cmd_rmmod(int argc,const char** argv);

int labrmmod_init(void)
{
	lab_addcommand("rmmod", lab_cmd_rmmod, "Removes a kernel module from LAB.");
	return 0;
}

void labrmmod_cleanup(void)
{
	lab_delcommand("rmmod");
}

void lab_cmd_rmmod(int argc,const char** argv)
{
	int errno;
	
	if (argc != 2) {
		lab_puts("Usage: boot> rmmod modname\r\n");
		return;
	}
	
	lab_printf("Removing module %s ...\r\n", argv[1]);
	if ((errno = sys_delete_module(argv[1], 0)))
		lab_printf("Error %d while removing module %s.\r\n", errno, argv[1]);
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB rmmod command module");
MODULE_LICENSE("GPL");
module_init(labrmmod_init);
module_exit(labrmmod_cleanup);
