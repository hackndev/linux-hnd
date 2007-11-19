/*
 *  lab/modules/labrun.c
 *
 *  Copyright (C) 2005 Matt Reimer, Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 *  Runs the commands in a file
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
#include <linux/lab/commands.h>


extern struct lab_srclist* srclist;

void lab_cmd_run(int argc,const char** argv);

int labrun_init(void)
{
	lab_addcommand("run", lab_cmd_run, "Runs the LAB commands in a file.");
	return 0;
}

void labrun_cleanup(void)
{	
	lab_delcommand("run");
}

void lab_runfile(char* source, char* sourcefile)
{
	struct lab_srclist *mysrclist;
	int count;
	long retval;
	unsigned char *data, *data2;
	
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
	
	data2 = vmalloc(count+1);
	memcpy(data2, data, count);
	data2[count] = '\0';
	vfree(data);
	
	lab_exec_string(data2);
	
	vfree(data2);
}
EXPORT_SYMBOL(lab_runfile);

void lab_cmd_run(int argc,const char** argv)
{
	char *temp;
	char *source;
	char *sourcefile;

	if (argc != 2) {
		lab_puts("Usage: boot> run filespec\r\n"
			 "filespec is device:[file]\r\n"
			 "example: boot> run ymodem:\r\n"
			 "would receive data over ymodem, then run it as a list of commands.\r\n");
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
	
	lab_printf("Running commands from [%s]%s...\r\n", source, sourcefile);
	lab_runfile(source, sourcefile);
	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB script exec module");
MODULE_LICENSE("GPL");
module_init(labrun_init);
module_exit(labrun_cleanup);
