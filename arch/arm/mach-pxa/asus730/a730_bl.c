/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2006 Serge Nikolaenko
 *
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach-types.h>
#include <linux/backlight.h>    /* backlight_device */
#include <linux/err.h>

#include <asm/arch/asus730-gpio.h>
#include <asm/arch/sharpsl.h> /* for struct corgibl_machinfo */

#include "../generic.h"

#define A730_MAX_INTENSITY 0xff
#define A730_DEFAULT_INTENSITY (A730_MAX_INTENSITY / 4)

static void a730_set_bl_intensity(int intensity)
{
	printk("intensity=0x%x\n", intensity);

	pxa_gpio_mode(GPIO_NR_A730_BACKLIGHT_EN | GPIO_OUT);
	SET_A730_GPIO(BACKLIGHT_EN, intensity != 0);
	PWM_CTRL0 = 1;
	PWM_PWDUTY0 = intensity;
	PWM_PERVAL0 = A730_MAX_INTENSITY;

	if (intensity > 0) {
		pxa_set_cken(CKEN0_PWM0, 1);
	} else {
                pxa_set_cken(CKEN0_PWM0, 0);
	}
}

static struct corgibl_machinfo a730_bl_machinfo = {
        .default_intensity = A730_DEFAULT_INTENSITY,
        .limit_mask = 0xff,
        .max_intensity = A730_MAX_INTENSITY,
        .set_bl_intensity = a730_set_bl_intensity,
};

struct platform_device a730_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &a730_bl_machinfo,
	},
};

MODULE_AUTHOR("Serge Nikolaenko <mypal_hh@utl.ru>");
MODULE_DESCRIPTION("Backlight driver for ASUS A730(W)");
MODULE_LICENSE("GPL");
