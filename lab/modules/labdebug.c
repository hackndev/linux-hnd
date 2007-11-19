/* labdebug.c
 * A debug module for LAB - Linux As Bootldr.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_debug(int argc,const char** argv);

int labdebug_init(void)
{
	lab_addcommand("debug", lab_cmd_debug, "Puts an entry in the kernel debug log (dmesg)");
	
	return 0;
}

void labdebug_cleanup(void)
{
	lab_delcommand("debug");
}

void lab_cmd_debug(int argc,const char** argv)
{
	int i;
	printk(">>> Debug message from user: ");
	for (i=1; i < argc; i++)
		printk("%s ", argv[i]);
	printk("\n");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB debug Module");
MODULE_LICENSE("GPL");
module_init(labdebug_init);
module_exit(labdebug_cleanup);
