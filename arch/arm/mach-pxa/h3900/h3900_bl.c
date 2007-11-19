/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2006 Paul Sokolosvky
 * Based on the code by Jamie Hicks and others
 *
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/mach-types.h>     /* machine_is_h3900 */
#include <linux/corgi_bl.h>
#include <linux/err.h>

#include <linux/mfd/asic2_base.h>
#include <linux/mfd/asic3_base.h>
#include <asm/hardware/ipaq-asic2.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch/h3900.h>
#include <asm/arch/h3900-gpio.h>

#define H3900_DEFAULT_INTENSITY (0x100 / 4)

extern struct platform_device h3900_asic3, h3900_asic2;

static void h3900_set_bl_intensity(int intensity)
{
	if (intensity > 0) {
                asic2_clock_enable(&h3900_asic2.dev, ASIC2_CLOCK_PWM | ASIC2_CLOCK_EX1,
                                         ASIC2_CLOCK_PWM | ASIC2_CLOCK_EX1);
                asic2_set_pwm(&h3900_asic2.dev, _IPAQ_ASIC2_PWM_0_Base, intensity, 0x100,
                                    PWM_TIMEBASE_ENABLE | 0x8);
                asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_FL_PWR_ON, GPIO3_FL_PWR_ON);
	} else {
                asic3_set_gpio_out_b (&h3900_asic3.dev, GPIO3_FL_PWR_ON, 0);
                asic2_set_pwm(&h3900_asic2.dev, _IPAQ_ASIC2_PWM_0_Base, 0, 0, 0);
                asic2_clock_enable(&h3900_asic2.dev, ASIC2_CLOCK_PWM, 0);
	}
}


static struct corgibl_machinfo h3900_bl_machinfo = {
        .default_intensity = H3900_DEFAULT_INTENSITY,
        .limit_mask = 0xffff,
        .max_intensity = 0x100,
        .set_bl_intensity = h3900_set_bl_intensity,
};

struct platform_device h3900_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &h3900_bl_machinfo,
	},
};

MODULE_AUTHOR("Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("Backlight driver for iPAQ h3900");
MODULE_LICENSE("GPL");

