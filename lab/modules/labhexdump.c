/*
 * labhexdump.c
 * A hexdump module for LAB
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

void lab_cmd_hexdump(int argc,const char** argv);

int labhexdump_init(void)
{
	lab_addcommand("xd", lab_cmd_hexdump, "Dumps file in hex form");
	return 0;
}

void labhexdump_cleanup(void)
{
	lab_delcommand("xd");
}

void lab_cmd_hexdump(int argc, const char** argv)
{
	char *buf;
	int fd, sz, i, offset = 0;

	if (argc != 2) {
		lab_puts("xd: syntax: xd <filename>\r\n");
		return;
	}

	if ((fd = sys_open(argv[1], O_RDONLY, 0)) < 0) {
		lab_printf("lab: couldn't open the file, errno=%d\r\n", fd);
		return;
	}

	buf = (char*)vmalloc(4096);
	
	while ((sz = sys_read(fd, buf, 4096))) {
	        if (sz < 0) break;
		for (i = 0; i < sz; i++, offset++) {
			if ((i & 0xf) == 0) {
				lab_printf("\r\n%08x:", offset);
			}
			lab_printf(" %02x", buf[i]);
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
MODULE_DESCRIPTION("LAB hexdump Module");
MODULE_LICENSE("GPL");
module_init(labhexdump_init);
module_exit(labhexdump_cleanup);
