/*
 * Buttons driver for HTC Universal
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.
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
#include <linux/mfd/asic3_base.h>
#include <asm/mach-types.h>
#include <asm/hardware/asic3_keys.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

static struct asic3_keys_button asic3_buttons[] = {
//{KEY_SCREEN,		ASIC3_GPIOA_IRQ_BASE+GPIOA_COVER_ROTATE_N,	1, 	"screen_cover", EV_SW},
//{KEY_SWITCHVIDEOMODE,	ASIC3_GPIOB_IRQ_BASE+GPIOB_CLAMSHELL_N,		1, 	"clamshell_rotate", EV_SW},
//{KEY_KBDILLUMTOGGLE,	ASIC3_GPIOB_IRQ_BASE+GPIOB_NIGHT_SENSOR,	1, 	"night_sensor", EV_SW},
{SW_LID,		ASIC3_GPIOA_IRQ_BASE+GPIOA_COVER_ROTATE_N,	1, 	"screen_cover", EV_SW},
{SW_TABLET_MODE,	ASIC3_GPIOB_IRQ_BASE+GPIOB_CLAMSHELL_N,		1, 	"clamshell_rotate", EV_SW},
//{SW_NIGHT_SENSOR,	ASIC3_GPIOB_IRQ_BASE+GPIOB_NIGHT_SENSOR,	1, 	"night_sensor", EV_SW},
{KEY_F10,		ASIC3_GPIOA_IRQ_BASE+GPIOA_BUTTON_BACKLIGHT_N,	1, 	"backlight_button"},
{KEY_RECORD,		ASIC3_GPIOA_IRQ_BASE+GPIOA_BUTTON_RECORD_N,	1, 	"record_button"},
{KEY_CAMERA,		ASIC3_GPIOA_IRQ_BASE+GPIOA_BUTTON_CAMERA_N,	1, 	"camera_button"},
{KEY_VOLUMEDOWN,	ASIC3_GPIOA_IRQ_BASE+GPIOA_VOL_UP_N,		1, 	"volume_slider_down"},
{KEY_VOLUMEUP,		ASIC3_GPIOA_IRQ_BASE+GPIOA_VOL_DOWN_N,		1, 	"volume_slider_up"},
{KEY_KPENTER,		ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_OK_N,		1, 	"select"},
{KEY_RIGHT,		ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_RIGHT_N,		1, 	"right"},
{KEY_LEFT,		ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_LEFT_N,		1, 	"left"},
{KEY_DOWN,		ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_DOWN_N,		1, 	"down"},
{KEY_UP,		ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_UP_N,		1, 	"up"},
};

static struct asic3_keys_platform_data asic3_keys_data = {
        .buttons = asic3_buttons,
        .nbuttons = ARRAY_SIZE(asic3_buttons),
        .asic3_dev = &htcuniversal_asic3.dev,
};

static struct platform_device htcuniversal_keys_asic3 = {
        .name = "asic3-keys",
        .dev = { .platform_data = &asic3_keys_data, }
};

static int __init htcuniversal_buttons_probe(struct platform_device *dev)
{
	platform_device_register(&htcuniversal_keys_asic3);
	return 0;
}

static struct platform_driver htcuniversal_buttons_driver = {
	.driver		= {
	    .name       = "htcuniversal_buttons",
	},
	.probe          = htcuniversal_buttons_probe,
};

static int __init htcuniversal_buttons_init(void)
{
	if (!machine_is_htcuniversal())
		return -ENODEV;

	return platform_driver_register(&htcuniversal_buttons_driver);
}

static void __exit htcuniversal_buttons_exit(void)
{
	platform_driver_unregister(&htcuniversal_buttons_driver);
}

module_init(htcuniversal_buttons_init);
module_exit(htcuniversal_buttons_exit);

MODULE_AUTHOR ("Joshua Wise, Pawel Kolodziejski, Paul Sokolosvky");
MODULE_DESCRIPTION ("Buttons support for HTC Universal");
MODULE_LICENSE ("GPL");
