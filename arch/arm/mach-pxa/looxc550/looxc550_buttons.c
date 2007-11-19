/*
 *  linux/arch/arm/mach-pxa/looxc550/looxc550_buttons.c
 *
 *  Keyboard definitions for FSC LOOX C550
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/looxc550.h>


/**************************** Keyboard **************************/

static struct pxa27x_keyboard_platform_data looxc550_buttons = {
    .nr_rows = 5,
    .nr_cols = 2,
    .keycodes = {
	{	_KEY_RECORD,
		KEY_UP
	},
	{	_KEY_CALENDAR,
		KEY_DOWN
        },
	{	KEY_RIGHT,
		_KEY_MAIL
	},
	{	_KEY_CONTACTS,
		KEY_LEFT
        },
	{	_KEY_HOMEPAGE,
		KEY_ENTER
        },
    },
    .gpio_modes = {
	GPIO_NR_LOOXC550_KP_MKIN0_MD,
	GPIO_NR_LOOXC550_KP_MKIN1_MD,
	GPIO_NR_LOOXC550_KP_MKIN2_MD,
	GPIO_NR_LOOXC550_KP_MKIN3_MD,
	GPIO_NR_LOOXC550_KP_MKIN4_MD,
	GPIO_NR_LOOXC550_KP_MKOUT0_MD,
	GPIO_NR_LOOXC550_KP_MKOUT1_MD,
    }
};

static struct platform_device looxc550_pxa_keyboard = {
	.name	= "pxa27x-keyboard",
	.id	= -1,
	.dev	= {
		.platform_data = &looxc550_buttons
	}
};

/*
Power button connected to GPIO 0, so we need use gpio-keys
*/

static struct gpio_keys_button looxc550_gpio_buttons[] = {
	{ KEY_POWER, GPIO_NR_LOOXC550_KEYPWR, 0, "Power button"}
};

static struct gpio_keys_platform_data looxc550_gpio_keys_data = {
	.buttons = looxc550_gpio_buttons,
	.nbuttons = ARRAY_SIZE(looxc550_gpio_buttons)
};

static struct platform_device looxc550_gpio_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &looxc550_gpio_keys_data
	}
};

/*********************************************************************/

static int __devinit looxc550_buttons_probe(struct platform_device *dev)
{
	platform_device_register(&looxc550_pxa_keyboard);
	platform_device_register(&looxc550_gpio_keys);
        return 0;
}

static struct platform_driver looxc550_buttons_driver = {
	.driver	= {
	    .name= "looxc550-buttons",
	},
	.probe	= looxc550_buttons_probe
};

static int __init looxc550_buttons_init(void)
{
	if (!machine_is_looxc550())
	    return -ENODEV;
	return platform_driver_register(&looxc550_buttons_driver);
}

static void __exit looxc550_buttons_exit(void)
{
	platform_driver_unregister(&looxc550_buttons_driver);
}

module_init(looxc550_buttons_init);
module_exit(looxc550_buttons_exit);

MODULE_DESCRIPTION ("Buttons support for FSC Loox C550");
MODULE_LICENSE ("GPL");
