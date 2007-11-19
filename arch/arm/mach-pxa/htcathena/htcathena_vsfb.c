/*
 * htcathena_lcd.c
 *
 * Based on the code
 * (c) 2006 Ian Molton <spyro@f2s.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 *
 */

#include <linux/vsfb.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <asm/mach-types.h>

static struct fb_var_screeninfo htcathena_vsfb_var = {
        .xres           = 480,
        .yres           = 640,
        .xres_virtual   = 480,
        .yres_virtual   = 640,
        .bits_per_pixel = 16,
        .red            = { 11, 5, 0 },
        .green          = {  5, 6, 0 },
        .blue           = {  0, 5, 0 },
        .activate       = FB_ACTIVATE_NOW,
        .height         = -1,
        .width          = -1,
        .vmode          = FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo htcathena_vsfb_fix = {
        .id             = "vsfb",
        .smem_start     = 0x04800000,
        .smem_len       = 480*640*2,
        .type           = FB_TYPE_PACKED_PIXELS,
        .visual         = FB_VISUAL_TRUECOLOR,
        .line_length    = 480*2,
        .accel          = FB_ACCEL_NONE,
};

static struct vsfb_deviceinfo htcathena_vsfb_deviceinfo = {
	.fix = &htcathena_vsfb_fix,
	.var = &htcathena_vsfb_var,
};

static struct platform_device htcathena_vsfb_device = {
        .name           = "vsfb",
        .id             = -1,
        .dev            = {
                .platform_data  = &htcathena_vsfb_deviceinfo,
        },
};

static int htcathena_lcd_init (void) {

	if (!machine_is_htcathena ())
		return -ENODEV;

	platform_device_register(&htcathena_vsfb_device);

	return 0;
}

module_init (htcathena_lcd_init);

MODULE_DESCRIPTION("HTC Athena VSFB driver");
MODULE_LICENSE("GPL");

