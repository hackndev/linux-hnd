/*
 * Buttons driver for Axim X50/X51(v).
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2007 Pierre Gaufillet
 *
 */

#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa27x_keyboard.h>

/****************************************************************
 * Keyboard
 ****************************************************************/

static struct pxa27x_keyboard_platform_data x50_kbd = {
    .nr_rows = 7,
    .nr_cols = 3,
    .keycodes = {
        {    /* row 0 */
            KEY_WLAN,
            -1,
            KEY_UP
        },
        {    /* row 1 */
            _KEY_RECORD,
            -1,
            KEY_DOWN
        },
        {    /* row 2 */
            -1,
            -1,
            -1
        },
        {    /* row 3 */
            _KEY_CALENDAR,
            -1,
            KEY_RIGHT
        },
        {    /* row 4 */
            _KEY_CONTACTS,
            -1,
            KEY_LEFT
        },
        {    /* row 5 */
            _KEY_MAIL,
            -1,
            KEY_ENTER
        },
        {    /* row 6 */
            _KEY_HOMEPAGE,
            -1,
            -1
        }},
};

static struct platform_device x50_pxa_keyboard = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev    =  {
		.platform_data  = &x50_kbd,
	},
};

static int __devinit aximx50_buttons_probe(struct platform_device *dev)
{
	platform_device_register(&x50_pxa_keyboard);
	return 0;
}

static struct platform_driver aximx50_buttons_driver = {
	.driver		= {
	    .name       = "aximx50-buttons",
	},
	.probe          = aximx50_buttons_probe,
};

static int __init aximx50_buttons_init(void)
{
	if (!machine_is_x50())
		return -ENODEV;

	return platform_driver_register(&aximx50_buttons_driver);
}

static void __exit aximx50_buttons_exit(void)
{
	platform_driver_unregister(&aximx50_buttons_driver);
}

module_init(aximx50_buttons_init);
module_exit(aximx50_buttons_exit);

MODULE_AUTHOR ("Pierre Gaufillet");
MODULE_DESCRIPTION ("Buttons support for Axim X50/X51(v)");
MODULE_LICENSE ("GPL");
