/*
 * LEDs support for Samsung's SAMCOP devices.
 *
 * Copyright (c) 2007  Anton Vorontsov <cbou@mail.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/leds.h>

struct samcop_leds_machinfo;

struct samcop_led {
	struct led_classdev led_cdev;
	int hw_num;
	struct samcop_leds_machinfo *machinfo;
};

struct samcop_leds_machinfo {
	int num_leds;
	struct samcop_led *leds;

	/* private */
	struct clk *leds_clk;
};

extern int samcop_leds_register(void);
extern void samcop_leds_unregister(void);
