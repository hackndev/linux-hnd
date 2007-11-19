/* labdmesg.c
 * A dmesg module for LAB
 *
 * Derived from dmesg.c from util-linux-2.12pre, which is
 * Copyright (C) 1993 Theodore Ts'o (tytso@athena.mit.edu)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_dmesg(int argc,const char** argv);

int labdmesg_init(void)
{
	lab_addcommand("dmesg", lab_cmd_dmesg, "Prints the kernel/LAB debug messages");
	return 0;
}

void labdmesg_cleanup(void)
{
	lab_delcommand("dmesg");
}

void lab_cmd_dmesg(int argc,const char** argv)
{
	char *buf;
	int lastc;
	int i, n, j;
	char buffer[1024];
	
	buf = (char*)kmalloc(32767, GFP_KERNEL);
	
	n = sys_syslog(3, buf, 32767);
	if (!n)
	{
		lab_puts("Error while retrieving syslog\r\n");
		kfree(buf);
		return;
	}
	
	lastc = '\n';
	j = 0;
	for (i = 0; i < n; i++) {
		if ((i == 0 || buf[i - 1] == '\n') && buf[i] == '<') {
			i++;
			while (buf[i] >= '0' && buf[i] <= '9')
				i++;
			if (buf[i] == '>')
				i++;
		}
		lastc = buf[i];
		if (lastc == '\n')
		{
			buffer[j] = '\r';
			j++;
			buffer[j] = '\n';
			j++;
			buffer[j] = 0;
			lab_puts(buffer);
			j = -1;
		} else
			buffer[j] = lastc;
		j++;
	}
	if (lastc != '\n')
	{
		buffer[j] = '\r';
		j++;
		buffer[j] = '\n';
		j++;
		buffer[j] = 0;
		lab_puts(buffer);
		j = 0;
	}
	
	kfree(buf);
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB dmesg Module");
MODULE_LICENSE("GPL");
module_init(labdmesg_init);
module_exit(labdmesg_cleanup);
