/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2006 Paul Sokolosvky
 *
 */

#include <linux/types.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <asm/arch/pxa-regs.h>
#include <linux/corgi_bl.h>
#include <linux/err.h>

#include <asm/arch/htcapache-gpio.h>

#define MAX_INTENSITY 0xc7

static void set_bl_intensity(int intensity)
{
	PWM_CTRL0 = 1;            /* pre-scaler */
	PWM_PWDUTY0 = intensity; /* duty cycle */
	PWM_PERVAL0 = MAX_INTENSITY+1;      /* period */

	if (intensity > 0) {
		pxa_set_cken(CKEN0_PWM0, 1);
	} else {
		pxa_set_cken(CKEN0_PWM0, 0);
	}
}

static struct corgibl_machinfo bl_machinfo = {
        .default_intensity = MAX_INTENSITY / 4,
        .limit_mask = 0xff,
        .max_intensity = MAX_INTENSITY,
        .set_bl_intensity = set_bl_intensity,
};

struct platform_device htcapache_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &bl_machinfo,
	},
};

MODULE_AUTHOR("Kevin O'Connor <kevin@koconnor.net>");
MODULE_DESCRIPTION("Backlight driver for HTC Apache");
MODULE_LICENSE("GPL");
