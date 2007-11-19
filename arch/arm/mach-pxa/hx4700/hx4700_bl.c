/*
 * LCD backlight support for iPAQ HX4700
 *
 * Copyright (c) 2005 Ian Molton
 * Copyright (c) 2005 SDG Systems, LLC
 * Copyright (c) 2006 Todd Blumer <todd@sdgsystems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 2006-10-07    Anton Vorontsov <cbou@mail.ru>
 * Convert backlight control code to use corgi-bl driver.
 */

#include <linux/platform_device.h>
#include <linux/corgi_bl.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

static
void hx4700_set_bl_intensity(int intensity)
{
	if (intensity < 7) intensity = 0;
	PWM_CTRL1 = 1;
	PWM_PERVAL1 = 0xC8;
	PWM_PWDUTY1 = intensity;
	return;
}

static
struct corgibl_machinfo hx4700_bl_machinfo = {
	.max_intensity = 0xC8,
	.default_intensity = 0xC8/4,
	.limit_mask = 0xFF,
	.set_bl_intensity = hx4700_set_bl_intensity
};

struct platform_device hx4700_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &hx4700_bl_machinfo
	}
};
