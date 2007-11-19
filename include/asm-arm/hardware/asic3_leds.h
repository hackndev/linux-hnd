/*
 * LEDs support for HTC ASIC3 devices.
 *
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

struct asic3_leds_machinfo;

struct asic3_led {
	struct led_classdev led_cdev;
	int hw_num;	/* Number of "hardware-accelerated" led */
	int gpio_num;	/* Number of GPIO if hw_num == -1 */
	struct asic3_leds_machinfo *machinfo;
};

struct asic3_leds_machinfo {
	int num_leds;
	struct asic3_led *leds;
	struct platform_device *asic3_pdev;
};

extern int asic3_leds_register(void);
extern void asic3_leds_unregister(void);

