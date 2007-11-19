/*
 *  lab/modules/labarmboot.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, September 06, 2003
 *
 *  "armboot" command for LAB CLI.
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
#include <linux/list.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>
#include <linux/lab/copy.h>

extern struct lab_srclist* srclist;

extern int armboot(unsigned char* kerneladdr, int size, unsigned char* cmdline);
void lab_cmd_armboot(int argc,const char** argv);

int labarmboot_init(void)
{
	lab_addcommand("armboot", lab_cmd_armboot, "Jumps into a new kernel.");
	return 0;
}

void labarmboot_cleanup(void)
{
	lab_delcommand("armboot");
}

void lab_armboot(char* source, char* sourcefile, char *cmdline)
{
	struct lab_srclist *mysrclist;
	int count;
	int retval;
	unsigned char* data;
	
	mysrclist = srclist;
	
	while(mysrclist) {
		if (!strcmp(source,mysrclist->src->name))
			break;
		mysrclist = mysrclist->next;
	}
	
	if (!mysrclist) {
		lab_printf("Source [%s] does not exist.\r\n", source);
		globfail++;
		return;
	}
	
	if (!strcmp(source,"fs") && !strncmp(sourcefile,"/fs",3)) {
		lab_puts("It looks like you're booting something from /fs. I'll assume you meant to chroot into /fs first so that symlinks don't break.\r\n");
		sys_chroot("/fs");
		sourcefile += 3;
	}
	
	if (!mysrclist->src->check(sourcefile)) {
		lab_printf("Source [%s] rejected filename \"%s\".\r\n",
			   source, sourcefile);
		globfail++;
		return;
	}
	
	data = mysrclist->src->get(&count,sourcefile);
	if (!data) {
		lab_puts("Error occured while getting file.\r\n");
		globfail++;
		return;
	}
	
	
	printk("ARMBooting NOW: Cmdline %s\n", cmdline ? cmdline : "[none]");
	retval = armboot(data, count, cmdline);
	printk(KERN_EMERG "ARMBoot failed to boot - system may be in an unstable state (return %d)\n", retval);
}
EXPORT_SYMBOL(lab_armboot);

void lab_cmd_armboot(int argc,const char** argv)
{
	char *temp;
	char *source;
	char *sourcefile;

	if (argc < 2)
	{
		lab_puts("Usage: boot> armboot filespec [cmdline]\r\n"
		         "filespec is device:[file]\r\n"
		         "example: boot> armboot ymodem: \"console=ttyS1\"\r\n"
		         "would receive data over ymodem, then boot it.\r\n");
		return;
	}
	
	temp = source = (char*)argv[1];
	while (*temp != ':' && *temp != '\0')
		temp++;
	if (*temp == '\0')
	{
		lab_puts("Illegal filespec\r\n");
		globfail++;
		return;
	}
	*temp = '\0';
	temp++;
	sourcefile = temp;

	lab_armboot(source, sourcefile, argc == 3 ? argv[2] : NULL);
	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB armboot command module");
MODULE_LICENSE("GPL");
module_init(labarmboot_init);
module_exit(labarmboot_cleanup);
