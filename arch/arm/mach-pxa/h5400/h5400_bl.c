/*
 * Backlight support for iPAQ H5400
 *
 * Copyright (c) 2007 Anton Vorontsov <cbou@mail.ru>
 * Copyright (c) 2003 Keith Packard
 * Copyright (c) 2003, 2005 Phil Blundell
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * History:
 * 2007-02-18  Anton Vorontsov  Converted to corgi-bl, PDA generic driver.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/corgi_bl.h>
#include <linux/mfd/mq11xx.h>

#include <asm/arch/h5400-asic.h>
#include <linux/mfd/samcop_base.h>

extern struct mediaq11xx_base *mq_base;

static void mq1100_set_pwm(struct mediaq11xx_base *base, unsigned char chan,
                           unsigned char level)
{
	unsigned long d;
	int shift = chan ? 24 : 8;

	d = base->regs->FP.pulse_width_mod_control;
	d &= ~(0xff << shift);
	d |= level << shift;
	base->regs->FP.pulse_width_mod_control = d;

	return;
}

static void h5400_bl_set_intensity(int intensity)
{
	samcop_set_gpio_b(&h5400_samcop.dev,
	                  SAMCOP_GPIO_GPB_BACKLIGHT_POWER_ON, intensity ?
	                  SAMCOP_GPIO_GPB_BACKLIGHT_POWER_ON : 0);

	mq1100_set_pwm(mq_base, 0, (0x3ff - intensity) >> 2);
	return;
}

static void h5400_bl_null_release(struct device *dev)
{
	return;
}

static struct corgibl_machinfo h5400_bl_info = {
	.max_intensity = 0x3ff,
	.default_intensity = 0x3ff/16,
	.limit_mask = 0x3ff/128,
	.set_bl_intensity = h5400_bl_set_intensity,
};

static struct platform_device corgibl_pdev = {
	.name = "corgi-bl",
	.id = -1,
	.dev = {
		.platform_data = &h5400_bl_info,
		.release = h5400_bl_null_release,
	},
};

static int __init h5400_bl_init(void)
{
	return platform_device_register(&corgibl_pdev);
}

static void __exit h5400_bl_exit(void)
{
	platform_device_unregister(&corgibl_pdev);
	return;
}

module_init(h5400_bl_init);
module_exit(h5400_bl_exit);
MODULE_LICENSE("GPL");
