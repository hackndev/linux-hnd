/*
 *  linux/drivers/video/vsfb.c
 *
 *  Copyright (C) 2003 Ian Molton
 *
 * Based on acornfb by Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Frame buffer code for Simple platforms
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/vsfb.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

static struct fb_info fb_info;
static u32 colreg[17]; // Copied from other driver - but is 17 correct?


static int
vsfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		  u_int trans, struct fb_info *info)
{
        if (regno > 16)
                return 1;
        
        ((u32 *)(info->pseudo_palette))[regno] = (red & 0xf800) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);

	return 0;
}

static struct fb_ops vsfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= vsfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int vsfb_probe(struct platform_device *pdev) {
	struct vsfb_deviceinfo *vsfb;
	
	vsfb = pdev->dev.platform_data;
	if(!vsfb)
		return -ENODEV;

        memcpy (&fb_info.fix, vsfb->fix, sizeof(struct fb_fix_screeninfo));
        memcpy (&fb_info.var, vsfb->var, sizeof(struct fb_var_screeninfo));

        fb_info.fbops           = &vsfb_ops;
        fb_info.flags           = FBINFO_FLAG_DEFAULT;
        fb_info.pseudo_palette  = colreg;
        fb_alloc_cmap(&fb_info.cmap, 16, 0);  //FIXME - needs to work for all depths
                                                                                 
	/* Try to grab our phys memory space... */
	if (!(request_mem_region(fb_info.fix.smem_start, fb_info.fix.smem_len, "vsfb"))){
                return -ENOMEM;
	}

	/* Try to map this so we can access it */
	fb_info.screen_base    = ioremap(fb_info.fix.smem_start, fb_info.fix.smem_len);
	if (!fb_info.screen_base) {
		release_mem_region(fb_info.fix.smem_start, fb_info.fix.smem_len);
		return -EIO;
	}

	printk(KERN_INFO "vsfb: framebuffer at 0x%lx, mapped to 0x%p, size %dk\n", fb_info.fix.smem_start, fb_info.screen_base, fb_info.fix.smem_len/1024);

	if (register_framebuffer(&fb_info) < 0){
		iounmap(fb_info.screen_base);
		release_mem_region(fb_info.fix.smem_start, fb_info.fix.smem_len);

		return -EINVAL;
	}

	return 0;
}

struct platform_driver vsfb_driver = {
        .driver = {
                .name = "vsfb",
        },
        .probe = vsfb_probe,
//        .remove = vsfb_remove,
};

static int __init vsfb_init(void)
{
        return platform_driver_register (&vsfb_driver);
        return 0;
}

#if 0
static void __exit tmio_mmc_exit(void)
{
         return platform_driver_unregister (&tmio_mmc_driver);
}
#endif

module_init(vsfb_init);

MODULE_AUTHOR("Ian Molton (based on acornfb by RMK and parts of anakinfb)");
MODULE_DESCRIPTION("Very Simple framebuffer driver");
MODULE_LICENSE("GPL");
