/* 
 * labmount.c
 * A mount module for LAB
 *
 * Copyright (c) 2003 Joshua Wise
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_mount(int argc,const char** argv);
void lab_cmd_umount(int argc,const char** argv);

int labmount_init(void)
{
	lab_addcommand("mount", lab_cmd_mount, "Allows you to mount a filesystem");
	lab_addcommand("umount", lab_cmd_umount, "Allows you to unmount a filesystem");
	return 0;
}

void labmount_cleanup(void)
{
	lab_delcommand("mount");
	lab_delcommand("umount");
}


void lab_cmd_mount(int argc,const char** argv)
{
	const char* devname;
	const char* path;
	const char* fstype;
	int retval;
	
	if (argc < 3 || argc > 4) {
		lab_puts("mount: syntax: mount devname path [fstype]\r\n"
			 "mount: where: devname is a device name including the /dev\r\n"
			 "mount:        path is a path to mount at including the leading /\r\n"
			 "mount:        and fstype is an optional filesystem type (default is jffs2)\r\n");
		return;
	}
	
	devname = argv[1];
	path = argv[2];
	if (argc == 4)
		fstype = argv[3];
	else
		fstype = "jffs2";
	
	if ((retval = sys_mount((char*)devname, (char*)path, (char*)fstype, 0, "")) < 0) {
		lab_printf("lab: mount -t %s %s %s failed with errno %d\r\n",
			fstype, devname, path, retval);
		globfail++;
	}
}

void lab_cmd_umount(int argc,const char** argv)
{
	int retval;
	
	if (argc != 2) {
		lab_puts("umount: syntax: umount path\r\n");
		return;
	}
	
	if ((retval = sys_oldumount((char*)argv[1])) < 0) {
		lab_printf("lab: umount %s failed with errno %d\r\n", argv[1], retval);
		globfail++;
	}
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB mount Module");
MODULE_LICENSE("GPL");
module_init(labmount_init);
module_exit(labmount_cleanup);
