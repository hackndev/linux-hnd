/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2006 Paul Sokolosvky
 * Copyright (C) 2006 Luke Kenneth Casson Leighton
 *
 * Based on code from older versions of h4000_lcd.c
 * Based on code from h4000_bl.c
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/mach-types.h>     /* machine_is_htcsable */
#include <linux/corgi_bl.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#define HTCSABLE_MAX_INTENSITY 0x3ff

static void htcsable_set_bl_intensity(int intensity)
{
  printk("htcsable_set_bl_intensity: %d\n", intensity);

	/* LCD brightness is driven by PWM0.
	 * We'll set the pre-scaler to 8, and the period to 1024, this
	 * means the backlight refresh rate will be 3686400/(8*1024) =
	 * 450 Hz which is quite enough.
	 */
	PWM_CTRL0 = 7;            /* pre-scaler */
	PWM_PWDUTY0 = intensity; /* duty cycle */
	PWM_PERVAL0 = HTCSABLE_MAX_INTENSITY;      /* period */

	if (intensity > 0) {
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
			GPIOD_LCD_BACKLIGHT, GPIOD_LCD_BACKLIGHT);
    mdelay(15);
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
			1<<14, 1<<14);
    mdelay(15);
		pxa_set_cken(CKEN0_PWM0, 1);
	} else {
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
			GPIOD_LCD_BACKLIGHT, 0);
    mdelay(15);
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
			1<<14, 0);
    mdelay(15);
		pxa_set_cken(CKEN0_PWM0, 0);
	}
}


static struct corgibl_machinfo htcsable_bl_machinfo = {
        .default_intensity = HTCSABLE_MAX_INTENSITY / 4,
        .limit_mask = 0xffff,
        .max_intensity = HTCSABLE_MAX_INTENSITY,
        .set_bl_intensity = htcsable_set_bl_intensity,
};

struct platform_device htcsable_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &htcsable_bl_machinfo,
	},
};

MODULE_AUTHOR("Paul Sokolovsky <pmiscml@gmail.com>, Luke Kenneth Casson Leighton");
MODULE_DESCRIPTION("Backlight driver for iPAQ HTCSABLE");
MODULE_LICENSE("GPL");

