/* labpm.c
 * A power management module for LAB - Linux As Bootldr.
 */

#include <linux/module.h>
#include <linux/pm.h>
#include <linux/init.h> 
#include <linux/kernel.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_suspend(int argc,const char** argv);

int labsuspend_init(void)
{
	lab_addcommand("suspend", lab_cmd_suspend, "Puts the system down for a nap");
	
	return 0;
}

void labsuspend_cleanup(void)
{
	lab_delcommand("suspend");
}

void lab_cmd_suspend(int argc,const char** argv)
{
	lab_puts("Ok byebye!!\r\n");
	printk("LAB: We are about to \"go down for the count\", as it were. I'll let you know when we wake up.\n");
	pm_suspend(PM_SUSPEND_MEM);
	printk("LAB: Hi. We're awake. I'm going to return you to the main menu now. As a reminder, I own you.\n");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB Dummy Test Module");
MODULE_LICENSE("GPL");
module_init(labsuspend_init);
module_exit(labsuspend_cleanup);
