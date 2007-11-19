/*
 * Linux-boots-Linux wrapper for ARMboot
 *
 * Copyright (c) 2004 Nexus Electronics Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <asm/uaccess.h>

extern int armboot(unsigned char* kerneladdr, int size, unsigned char* cmdline);

int sys_lbl (unsigned long r0, unsigned long r1, unsigned long r2)
{
	unsigned long image_size;
	void *block;
	char cmdline[1024];

	image_size = r1;
	block = (void *)__get_free_pages (GFP_KERNEL, 8);

	if (copy_from_user (block, (void *)r0, image_size))
		return -EFAULT;

	if (strncpy_from_user (cmdline, (void *)r2, sizeof (cmdline) - 1) == -EFAULT)
		return -EFAULT;
	cmdline[sizeof(cmdline) - 1] = 0;

	return armboot (block, image_size, cmdline);
}
