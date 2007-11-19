/* 
 * labmknod.c
 * A mknod module for LAB
 *
 * Copyright (c) 2006 Paul Sokolovsky
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/kdev_t.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_mknod(int argc,const char** argv);

int labmknod_init(void)
{
	lab_addcommand("mknod", lab_cmd_mknod, "Make a device file");
	return 0;
}

void labmknod_cleanup(void)
{
	lab_delcommand("mknod");
}


static void lab_mknod_usage(void)
{
		lab_puts("mknod: syntax: mknod path type major minor\r\n"
			 "mknod: where: path is a device full file name (usually under /dev)\r\n"
			 "mknod:        type is 'c' or 'b' for character or block device\r\n"
			 "mknod:        major and minor are device numbers\r\n");
}

void lab_cmd_mknod(int argc,const char** argv)
{
	int mode, err, major, minor;
	
	if (argc != 5) {
	        lab_mknod_usage();
		return;
	}
	if (*argv[2] != 'b' && *argv[2] != 'c') {
	        lab_mknod_usage();
		return;
	}

	if (*argv[2] == 'b')
		mode = S_IFBLK | 0600;
	else
		mode = S_IFCHR | 0600;
	
	major = simple_strtoul(argv[3], NULL, 10);
	minor = simple_strtoul(argv[4], NULL, 10);

	err = sys_mknod(argv[1], mode,
			     new_encode_dev(MKDEV(major, minor)));
	if (err < 0) {
		lab_printf("lab: mknod %s %o %d %d failed with errno %d\r\n",
			argv[1], mode, major, minor);
		globfail++;
	}
}

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("LAB mknod Module");
MODULE_LICENSE("GPL");
module_init(labmknod_init);
module_exit(labmknod_cleanup);
