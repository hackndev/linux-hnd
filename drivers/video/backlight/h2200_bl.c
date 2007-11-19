/*
 * LCD backlight support for iPAQ H2200
 *
 * Copyright (c) 2004 Andrew Zabolotny
 * Copyright (c) 2004-6 Matt Reimer
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/platform_device.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h2200-gpio.h>
#include <asm/arch/sharpsl.h>

#define H2200_MAX_INTENSITY 0x3ff

static
void h2200_set_bl_intensity(int intensity)
{
	if (intensity < 7) intensity = 0;

	PWM_CTRL0 = 1;
	PWM_PERVAL0 = H2200_MAX_INTENSITY;
	PWM_PWDUTY0 = intensity;

	if (intensity) {
		pxa_gpio_mode(GPIO16_PWM0_MD);
		pxa_set_cken(CKEN0_PWM0, 1);
		SET_H2200_GPIO(BACKLIGHT_ON, 1);
	} else {
		pxa_set_cken(CKEN0_PWM0, 0);
		SET_H2200_GPIO(BACKLIGHT_ON, 0);
	}
}

static struct corgibl_machinfo h2200_bl_machinfo = {
	.max_intensity = H2200_MAX_INTENSITY,
	.default_intensity = H2200_MAX_INTENSITY,
	.limit_mask = 0xff, /* limit brightness to about 1/4 on low battery */
	.set_bl_intensity = h2200_set_bl_intensity,
};

struct platform_device h2200_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &h2200_bl_machinfo,
	},
};

static int __init h2200_bl_init(void)
{
	return platform_device_register(&h2200_bl);
}

static void __exit h2200_bl_exit(void)
{
	platform_device_unregister(&h2200_bl);
}

module_init(h2200_bl_init);
module_exit(h2200_bl_exit);

MODULE_AUTHOR("Andrew Zabolotny, Matt Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("Backlight driver for iPAQ H2200");
MODULE_LICENSE("GPL");
