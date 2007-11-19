/*
 * LED Backlight Trigger
 *
 * Copyright (c) 2007 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/leds.h>

DEFINE_LED_TRIGGER(ledtrig_backlight);

void ledtrig_backlight_update_status(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	led_trigger_event(ledtrig_backlight, intensity ? LED_FULL : LED_OFF);
}
EXPORT_SYMBOL(ledtrig_backlight_update_status);

static int __init ledtrig_backlight_init(void)
{
	led_trigger_register_simple("backlight", &ledtrig_backlight);
	return 0;
}

static void __exit ledtrig_backlight_exit(void)
{
	led_trigger_unregister_simple(ledtrig_backlight);
}

module_init(ledtrig_backlight_init);
module_exit(ledtrig_backlight_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("LED Backlight Trigger");
MODULE_LICENSE("GPL");
