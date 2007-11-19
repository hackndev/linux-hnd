/*
 * e330_lcd.c
 *
 * (c) 2006 Ian Molton <spyro@f2s.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/vsfb.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <asm/mach-types.h>

static struct fb_var_screeninfo e330_vsfb_var = {
        .xres           = 240,
        .yres           = 320,
        .xres_virtual   = 240,
        .yres_virtual   = 320,
        .bits_per_pixel = 16,
        .red            = { 11, 5, 0 },
        .green          = {  5, 6, 0 },
        .blue           = {  0, 5, 0 },
        .activate       = FB_ACTIVATE_NOW,
        .height         = -1,
        .width          = -1,
        .vmode          = FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo e330_vsfb_fix = {
        .id             = "vsfb",
        .smem_start     = 0x0c042000,
        .smem_len       = 240*320*2,
        .type           = FB_TYPE_PACKED_PIXELS,
        .visual         = FB_VISUAL_TRUECOLOR,
        .line_length    = 240*2,        // For gods sake fix this
        .accel          = FB_ACCEL_NONE,
};

static struct vsfb_deviceinfo e330_vsfb_deviceinfo = {
	.fix = &e330_vsfb_fix,
	.var = &e330_vsfb_var,
};

static struct platform_device e330_vsfb_device = {
        .name           = "vsfb",
        .id             = -1,
        .dev            = {
                .platform_data  = &e330_vsfb_deviceinfo,
        },
};

static int e330_lcd_init (void) {

	if (!machine_is_e330 ())
		return -ENODEV;

	platform_device_register(&e330_vsfb_device);

	return 0;
}

module_init (e330_lcd_init);
//module_exit (e740_lcd_exit);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("e330 lcd driver");
MODULE_LICENSE("GPLv2");

