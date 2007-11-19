/*
 * Buttons driver for iPAQ h4150/h4350.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2005 Pawel Kolodziejski
 * Copyright (C) 2003 Joshua Wise
 *
 */

#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <asm/mach-types.h>
#include <asm/arch/htcsable-gpio.h>

static struct gpio_keys_button pxa_buttons[] = {
	{ KEY_POWER,	GPIO_NR_HTCSABLE_KEY_ON_N,	1, "Power button"	},
};

static struct gpio_keys_platform_data gpio_keys_data = {
        .buttons = pxa_buttons,
        .nbuttons = ARRAY_SIZE(pxa_buttons),
};

static struct platform_device htcsable_keys_gpio = {
        .name = "gpio-keys",
        .dev = { .platform_data = &gpio_keys_data, }
};

static int __init htcsable_buttons_probe(struct platform_device *dev)
{
	platform_device_register(&htcsable_keys_gpio);
	return 0;
}

static struct platform_driver htcsable_buttons_driver = {
  .driver = {
    .name           = "htcsable_buttons",
  },
	.probe          = htcsable_buttons_probe,
};

static int __init htcsable_buttons_init(void)
{
	if (!machine_is_hw6900())
		return -ENODEV;

	return platform_driver_register(&htcsable_buttons_driver);
}

static void __exit htcsable_buttons_exit(void)
{
	platform_driver_unregister(&htcsable_buttons_driver);
}

module_init(htcsable_buttons_init);
module_exit(htcsable_buttons_exit);

MODULE_AUTHOR ("Joshua Wise, Pawel Kolodziejski, Paul Sokolosvky, Luke Kenneth Casson Leighton");
MODULE_DESCRIPTION ("Buttons support for iPAQ htcsable");
MODULE_LICENSE ("GPL");

