/*
 * HP iPAQ h1910/h1915 Backlight driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/backlight.h>
#include <linux/corgi_bl.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/arch/pxafb.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h1900-asic.h>
#include <asm/arch/h1900-gpio.h>

#include <linux/mfd/asic3_base.h>

extern struct platform_device h1900_asic3;
#define asic3 &h1900_asic3.dev

static void h1900_set_bl_intensity(int intensity)
{
	PWM_CTRL0 = 0;
	PWM_PWDUTY0 = (intensity * 183) / 255;
	PWM_PERVAL0 = 183;

	if (intensity > 0) {
		GPSR(GPIO_NR_H1900_LCD_PWM) = GPIO_bit(GPIO_NR_H1900_LCD_PWM);
		CKEN |= CKEN0_PWM0;
		mdelay(50);
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_BACKLIGHT_POWER, GPIO3_H1900_BACKLIGHT_POWER);
	} else {
		GPCR(GPIO_NR_H1900_LCD_PWM) = GPIO_bit(GPIO_NR_H1900_LCD_PWM);
		CKEN &= ~CKEN0_PWM0;
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_BACKLIGHT_POWER, 0);
	}
}

static struct corgibl_machinfo h1900_bl_machinfo = {
        .default_intensity = 255 / 4,
        .limit_mask = 0x1f,
        .max_intensity = 255,
        .set_bl_intensity = h1900_set_bl_intensity,
};

struct platform_device h1900_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &h1900_bl_machinfo,
	},
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("iPAQ h1910/h1915 Backlight glue driver");
