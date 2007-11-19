/*
 * labls.c
 * An ls module for LAB
 *
 * Copyright (c) 2006 Paul Sokolovsky
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/vmalloc.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

struct _linux_dirent {
	unsigned long	d_ino;
	unsigned long	d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};


void lab_cmd_ls(int argc, const char** argv);

int labls_init(void)
{
	lab_addcommand("ls", lab_cmd_ls, "List directory contents");
	return 0;
}

void labls_cleanup(void)
{
	lab_delcommand("ls");
}

void lab_cmd_ls(int argc, const char** argv)
{
	int fd, sz;
	char *buf, *p;

	if (argc != 2) {
		lab_puts("ls: syntax: ls <dir>\r\n");
		return;
	}

	if ((fd = sys_open(argv[1], O_RDONLY, 0)) < 0) {
		lab_printf("lab: couldn't open the dir, errno=%d\r\n", fd);
		return;
	}

	buf = vmalloc(32768);

	sz = sys_getdents(fd, (struct linux_dirent*)buf, 32768);
	if (sz < 0) {
		lab_printf("lab: couldn't read the dir, errno=%d\r\n", sz);
		sys_close(fd);
		vfree(buf);
		return;
	}
	
	for (p = buf; p < buf + sz;) {
		lab_printf("%s\r\n", &((struct _linux_dirent*)p)->d_name);
		p += ((struct _linux_dirent*)p)->d_reclen;
	}

	sys_close(fd);
	vfree(buf);
}

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("LAB ls Module");
MODULE_LICENSE("GPL");
module_init(labls_init);
module_exit(labls_cleanup);
