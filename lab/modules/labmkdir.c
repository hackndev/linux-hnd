/*
 * labmkdir.c
 * A mkdir module for LAB - Linux As Bootldr.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_mkdir(int argc,const char** argv);

int labmkdir_init(void)
{
	lab_addcommand("mkdir", lab_cmd_mkdir, "Create a directory");
	
	return 0;
}

void labmkdir_cleanup(void)
{
	lab_delcommand("mkdir");
}

void lab_cmd_mkdir(int argc,const char** argv)
{
	int err;
	
	if (argc != 2) {
		lab_puts("mkdir: syntax: mkdir dirname\r\n");
		return;
	}
	
	err = sys_mkdir(argv[1], 0000);
	if (err)
		lab_printf("mkdir: failed to create %s (errno %d)\n", argv[1], err);
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB mkdir Module");
MODULE_LICENSE("GPL");
module_init(labmkdir_init);
module_exit(labmkdir_cleanup);
