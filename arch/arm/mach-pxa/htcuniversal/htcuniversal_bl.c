/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2006 Paul Sokolosvky
 * Based on code from older versions of htcuniversal_lcd.c
 *
 */

#include <linux/types.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <asm/arch/pxa-regs.h>
#include <asm/mach-types.h>     /* machine_is_htcuniversal */
#include <linux/corgi_bl.h>
#include <linux/err.h>

#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#define HTCUNIVERSAL_MAX_INTENSITY 0xc7

static void htcuniversal_set_bl_intensity(int intensity)
{
	PWM_CTRL1 = 1;            /* pre-scaler */
	PWM_PWDUTY1 = intensity; /* duty cycle */
	PWM_PERVAL1 = HTCUNIVERSAL_MAX_INTENSITY+1;      /* period */

	if (intensity > 0) {
		pxa_set_cken(CKEN1_PWM1, 1);
                asic3_set_gpio_out_d(&htcuniversal_asic3.dev,
                        (1<<GPIOD_FL_PWR_ON), (1<<GPIOD_FL_PWR_ON));
	} else {
		pxa_set_cken(CKEN1_PWM1, 0);
                asic3_set_gpio_out_d(&htcuniversal_asic3.dev,
                        (1<<GPIOD_FL_PWR_ON), 0);
	}
}


static struct corgibl_machinfo htcuniversal_bl_machinfo = {
        .default_intensity = HTCUNIVERSAL_MAX_INTENSITY / 4,
        .limit_mask = 0xff,
        .max_intensity = HTCUNIVERSAL_MAX_INTENSITY,
        .set_bl_intensity = htcuniversal_set_bl_intensity,
};

struct platform_device htcuniversal_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &htcuniversal_bl_machinfo,
	},
};

MODULE_AUTHOR("Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("Backlight driver for HTC Universal");
MODULE_LICENSE("GPL");
