/*
 * linux/drivers/video/imageon/w1030.c
 *
 * Frame Buffer Device for ATI Imageon 1030
 *
 * Copyright (C) 2004, Damian Bentley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on:
 *  ATI's w100fb.h Wallaby code
 *  Ian Molton's vsfb.c (based on acornfb by Russell King)
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */


#include <linux/types.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>

#include <asm/io.h>

#include "default.h"
#include "w1030.h"

void *w1030_cfg;
void *w1030_regs;
void *w1030_fb;

/******************************************************************************/

static struct w1030fb_par {
} par;

static struct fb_var_screeninfo w1030fb_var = {
};

static struct fb_fix_screeninfo w1030fb_fix = {
	.id		= "imageon w1030",
};

static struct fb_info info;
static u32 colreg[17]; // Copied from Ian's driver - but is 17 correct?

static struct fb_ops w1030fb_ops = {
	.owner		= THIS_MODULE,
//	.fb_fillrect	= cfb_fillrect,
//	.fb_copyarea	= cfb_copyarea,
//	.fb_imageblit	= cfb_imageblit,
//	.fb_cursor	= soft_cursor,
};

/******************************************************************************/

int __init w1030fb_init(void)
{
	memset(&info, 0, sizeof(info));

	info.flags = FBINFO_FLAG_DEFAULT;
	info.fbops = &w1030fb_ops;
	info.var = w1030fb_var;
	info.fix = w1030fb_fix;
	info.pseudo_palette = colreg;
	fb_alloc_cmap(&info.cmap, 16, 0);

	if (register_framebuffer(&info) < 0) {
		printk("register_framebuffer(...) failed\n");
		return -EINVAL;
	}

        printk(KERN_INFO "fb%d: %s\n", info.node, info.fix.id);

	return 0;
}

static void __exit w1030fb_cleanup(void)
{
	unregister_framebuffer(&info);

	iounmap(w1030_fb);
	iounmap(w1030_regs);
	iounmap(w1030_cfg);

	//release_mem_region(IMAGEON_BASE, IMAGEON_SIZE);
}
