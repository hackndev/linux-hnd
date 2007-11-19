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

struct hamcop_leds_machinfo;

struct hamcop_led {
	struct led_classdev led_cdev;
	int hw_num;
	struct hamcop_leds_machinfo *machinfo;
};

struct hamcop_leds_machinfo {
	int num_leds;
	struct hamcop_led *leds;
	struct platform_device *hamcop_pdev;

	/* private */
	struct clk *leds_clk;
};

extern int hamcop_leds_register(void);
extern void hamcop_leds_unregister(void);
