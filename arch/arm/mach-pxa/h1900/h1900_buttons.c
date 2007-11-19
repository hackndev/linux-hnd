/*
 * HP iPAQ h1910/h1915 Buttons driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
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
#include <linux/mfd/asic3_base.h>
#include <linux/gpio_keys.h>
#include <asm/mach-types.h>
#include <asm/hardware/asic3_keys.h>
#include <asm/arch/h1900-gpio.h>
#include <asm/arch/h1900-asic.h>

extern struct platform_device h1900_asic3;

static struct gpio_keys_button pxa_buttons[] = {
	{ KEY_POWER,	GPIO_NR_H1900_POWER_BUTTON_N,	1, "Power button"	},
	{ KEY_ENTER,	GPIO_NR_H1900_ACTION_BUTTON_N,	1, "Action button"	},
	{ KEY_UP,	GPIO_NR_H1900_UP_BUTTON_N,	1, "Up button"		},
	{ KEY_DOWN,	GPIO_NR_H1900_DOWN_BUTTON_N,	1, "Down button"	},
	{ KEY_LEFT,	GPIO_NR_H1900_LEFT_BUTTON_N,	1, "Left button"	},
	{ KEY_RIGHT,	GPIO_NR_H1900_RIGHT_BUTTON_N,	1, "Right button"	},
};

static struct asic3_keys_button asic3_buttons[] = {
        { _KEY_RECORD,    H1900_RECORD_BTN_IRQ,		1, "Record button" },
        { _KEY_HOMEPAGE,  H1900_HOME_BTN_IRQ,		1, "Home button" },
        { _KEY_MAIL,      H1900_MAIL_BTN_IRQ,		1, "Mail button" },
        { _KEY_CONTACTS,  H1900_CONTACTS_BTN_IRQ, 	1, "Contacts button" },
        { _KEY_CALENDAR,  H1900_CALENDAR_BTN_IRQ,	1, "Calendar button" },
};

static struct gpio_keys_platform_data gpio_keys_data = {
        .buttons = pxa_buttons,
        .nbuttons = ARRAY_SIZE(pxa_buttons),
};

static struct asic3_keys_platform_data asic3_keys_data = {
        .buttons = asic3_buttons,
        .nbuttons = ARRAY_SIZE(asic3_buttons),
        .asic3_dev = &h1900_asic3.dev,
};

static struct platform_device h1910_keys_gpio = {
        .name = "gpio-keys",
        .dev = { .platform_data = &gpio_keys_data, }
};

static struct platform_device h1910_keys_asic3 = {
        .name = "asic3-keys",
        .dev = { .platform_data = &asic3_keys_data, }
};

static int __init h1910_buttons_probe(struct platform_device *dev)
{
	platform_device_register(&h1910_keys_gpio);
	platform_device_register(&h1910_keys_asic3);
	return 0;
}

static struct platform_driver h1910_buttons_driver = {
	.driver		= {
	    .name       = "h1900-buttons",
	},
	.probe          = h1910_buttons_probe,
};

static int __init h1910_buttons_init(void)
{
	if (!machine_is_h1900())
		return -ENODEV;

	return platform_driver_register(&h1910_buttons_driver);
}

static void __exit h1910_buttons_exit(void)
{
	platform_driver_unregister(&h1910_buttons_driver);
}

module_init(h1910_buttons_init);
module_exit(h1910_buttons_exit);

MODULE_AUTHOR ("Joshua Wise, Pawel Kolodziejski, Paul Sokolovsky");
MODULE_DESCRIPTION ("Buttons support for HP iPAQ h1910/h1915");
MODULE_LICENSE ("GPL");
