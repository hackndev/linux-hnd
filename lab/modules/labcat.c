/*
 * labcat.c
 * A cat module for LAB
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

void lab_cmd_cat(int argc,const char** argv);

int labcat_init(void)
{
	lab_addcommand("cat", lab_cmd_cat, "Dumps file in text form");
	return 0;
}

void labcat_cleanup(void)
{
	lab_delcommand("cat");
}

void lab_cmd_cat(int argc, const char** argv)
{
	char *buf, *p;
	int fd, sz;

	if (argc != 2) {
		lab_puts("cat: syntax: cat <filename>\r\n");
		return;
	}

	if ((fd = sys_open(argv[1], O_RDONLY, 0)) < 0) {
		lab_printf("lab: couldn't open the file, errno=%d\r\n", fd);
		return;
	}

	buf = (char*)vmalloc(4096);

	while ((sz = sys_read(fd, buf, 4096))) {
	        if (sz < 0) break;
		for (p = buf; p < buf + sz; p++) {
			if (*p == '\n')
				lab_puts("\r\n");
			else
				lab_putc(*p);
		}
		if (lab_char_ready()) break;
	}

	lab_puts("\r\n");

	if (sz < 0) {
		lab_printf("lab: couldn't read the file, errno=%d\r\n", sz);
	} else if (sz > 0) {
		lab_printf("lab: user break\r\n");
	}
	
	sys_close(fd);
	vfree(buf);
}

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("LAB cat Module");
MODULE_LICENSE("GPL");
module_init(labcat_init);
module_exit(labcat_cleanup);
