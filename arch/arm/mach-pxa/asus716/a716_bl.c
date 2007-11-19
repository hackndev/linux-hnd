/*
 *  Asus MyPal A716 Backlight glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/corgi_bl.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>
#include <linux/fb.h>
#include <asm/arch/pxafb.h>

static void a716_set_bl_intensity(int intensity)
{
	PWM_CTRL0 = 0;
	PWM_PWDUTY0 = ((intensity * 220) / 255) + 35;
	PWM_PERVAL0 = 255;

	if (intensity > 0) {
		CKEN |= CKEN0_PWM0;
		a716_gpo_set(GPO_A716_BACKLIGHT);
		mdelay(50);
	} else {
		CKEN &= ~CKEN0_PWM0;
		a716_gpo_clear(GPO_A716_BACKLIGHT);
		mdelay(30);
	}
}

static struct corgibl_machinfo a716_bl_machinfo = {
	.default_intensity = 255 / 4,
	.limit_mask = 0x1f,
	.max_intensity = 255,
	.set_bl_intensity = a716_set_bl_intensity,
};

struct platform_device a716_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &a716_bl_machinfo,
	},
};

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 Backlight glue driver");
MODULE_LICENSE("GPL");
