/*
 *  linux/arch/arm/mach-pxa/roverp1_lcd.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2003 Adam Turowski
 *  Copyright(C) 2004 Vitaliy Sardyko
 *  Copyright(C) 2004 Konstantine A Beklemishev
 *
 *  2003-12-03: Adam Turowski
 *		initial code.
 *  2004-11-07: Vitaliy Sardyko
 *		updated to 2.6.7
 *  2005-01-10: Konstantine A Beklemishev
 *		adapted for RoverPC P1 (MitacMio336)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

/* #include <asm/arch/roverp1-init.h>
 * #include <asm/arch/roverp1-gpio.h> */
#include <linux/fb.h>
#include <asm/arch/pxafb.h>

#define DEBUG 1



#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif


static int roverp1_lcd_set_power (struct lcd_device *lm, int setp)
{
	return 0;
}

static int roverp1_lcd_get_power (struct lcd_device *lm)
{
	return 0;
} 
struct lcd_ops roverp1_lcd_properties =
{
	.set_power = roverp1_lcd_set_power,
	.get_power = roverp1_lcd_get_power,
}; 

static void roverp1_backlight_power(int on)
{
}


static struct pxafb_mach_info roverp1_fb_info  = {
    	.pixclock   =	156551,
	.bpp	    =	16,
	.xres	    =	240,
	.yres	    =	320,
	.hsync_len  =	24,
	.vsync_len  =	2,
	.left_margin=	20,
	.upper_margin = 5,
	.right_margin = 12,
	.lower_margin = 5,
        .sync	    =	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	.lccr0 = 0x003008f0,
	.lccr3 = 0x04400008,
        .pxafb_backlight_power	= roverp1_backlight_power
};

static int roverp1_backlight_set_power (struct backlight_device *bm, int on)
{
	return 0;
}

static int roverp1_backlight_get_power (struct backlight_device *bm)
{
	return 0;
}

static struct backlight_properties roverp1_backlight_properties =
{
	.owner         = THIS_MODULE,
	.set_power     = roverp1_backlight_set_power,
	.get_power     = roverp1_backlight_get_power,
};

static struct lcd_device *pxafb_lcd_device;
static struct backlight_device *pxafb_backlight_device;

int  roverp1_lcd_init (void)
{
	set_pxa_fb_info (&roverp1_fb_info);

	pxafb_lcd_device = lcd_device_register("pxafb", NULL, &roverp1_lcd_properties);
	if (IS_ERR (pxafb_lcd_device))
	{
		DPRINTK(" LCD device doesn't registered, booting failed !\n");
		return IS_ERR (pxafb_lcd_device);
	}

	pxafb_backlight_device = backlight_device_register("pxafb", NULL, &roverp1_backlight_properties);

	return IS_ERR (pxafb_backlight_device);
}


static void roverp1_lcd_exit (void)
{
	lcd_device_unregister (pxafb_lcd_device);
	backlight_device_unregister (pxafb_backlight_device);
}

module_init(roverp1_lcd_init);
module_exit(roverp1_lcd_exit);

MODULE_AUTHOR("Adam Turowski, Konstantine A Beklemishev");
MODULE_DESCRIPTION("LCD driver for RoverPC P1 (MitacMio336)");
MODULE_LICENSE("GPL");
