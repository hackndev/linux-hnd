/* labdummy.c
 * A dummy module for LAB - Linux As Bootldr.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_dummy(int argc,const char** argv);
void lab_cmd_dummy2(int argc,const char** argv);

int labdummy_init(void)
{
	lab_addcommand("dummy", lab_cmd_dummy, "Stupid dummy LAB tester");
	lab_addcommand("dummymf", lab_cmd_dummy, "Multifunction dummy");
	lab_addsubcommand("dummymf", "foo", lab_cmd_dummy2, "A subcommand dummy");
	lab_addsubcommand("dummyso", "bar", lab_cmd_dummy, "Subcommand-only dummy");
	lab_addsubcommand("dummyso", "baz", lab_cmd_dummy2, "Second command in subcommand-only dummy");
	
	return 0;
}

void labdummy_cleanup(void)
{
	lab_delcommand("dummy");
	lab_delcommand("dummymf");
	lab_delcommand("dummyso");
}

void lab_cmd_dummy(int argc,const char** argv)
{
	lab_puts("LAB dummy test!\r\n"
	         "This was from an optional command!\r\n");
}

void lab_cmd_dummy2(int argc,const char** argv)
{
	lab_puts("A second test!\r\n");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB Dummy Test Module");
MODULE_LICENSE("GPL");
module_init(labdummy_init);
module_exit(labdummy_cleanup);
