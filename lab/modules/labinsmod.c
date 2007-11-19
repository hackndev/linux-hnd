/*
 *  lab/modules/labinsmod.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, September 06, 2003
 *
 *  "insmod" command for LAB CLI.
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

extern struct lab_srclist* srclist;

void lab_cmd_insmod(int argc,const char** argv);

int labinsmod_init(void)
{
	lab_addcommand("insmod", lab_cmd_insmod, "Inserts a kernel module into LAB.");
	return 0;
}

void labinsmod_cleanup(void)
{
	lab_delcommand("insmod");
}

void lab_insmod(char* source, char* sourcefile)
{
	struct lab_srclist *mysrclist;
	int count;
	long retval;
	unsigned char* data;
	
	mysrclist = srclist;
	
	while (mysrclist) {
		if (!strcmp(source,mysrclist->src->name))
			break;
		mysrclist = mysrclist->next;
	}
	
	if (!mysrclist) {
		lab_printf("Source [%s] does not exist.\r\n", source);
		return;
	}
	
	if (!mysrclist->src->check(sourcefile)) {
		lab_printf("Source [%s] rejected filename \"%s\".\r\n",
			   source, sourcefile);
		return;
	}
	
	data = mysrclist->src->get(&count,sourcefile);
	if (!data) {
		lab_puts("Error occured while getting file.\r\n");
		return;
	}
	
	retval = sys_init_module(data, count, "");
	if (retval != 0)
		lab_printf("Error inserting module [%s]%s --> %d\r\n",
			   source, sourcefile, (int)retval);
	
	vfree(data);
}
EXPORT_SYMBOL(lab_insmod);

void lab_cmd_insmod(int argc,const char** argv)
{
	char *temp;
	char *source;
	char *sourcefile;

	if (argc != 2) {
		lab_puts("Usage: boot> insmod filespec\r\n"
			 "filespec is device:[file]\r\n"
			 "example: boot> insmod ymodem:\r\n"
			 "would receive data over ymodem, then load it as a module.\r\n");
		return;
	}
	
	temp = source = (char*)argv[1];
	while (*temp != ':' && *temp != '\0')
		temp++;
	if (*temp == '\0') {
		lab_puts("Illegal filespec\r\n");
		return;
	}
	*temp = '\0';
	temp++;
	sourcefile = temp;
	
	lab_printf("Inserting module [%s]%s...\r\n", source, sourcefile);
	lab_insmod(source, sourcefile);
	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB insmod command module");
MODULE_LICENSE("GPL");
module_init(labinsmod_init);
module_exit(labinsmod_cleanup);
