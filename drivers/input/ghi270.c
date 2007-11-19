/*
 * Trigger the input-key LED whenever a key is pressed.
 *
 * Copyright 2007 SDG Systems LLC
 * Derived from drivers/input/power.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/leds.h>

#define KEY_LED_DURATION    (2 * HZ)
static int keylight_duration = KEY_LED_DURATION;
module_param(keylight_duration, int, 0644);
MODULE_PARM_DESC(keylight_duration, "How long (jiffies) to keep keyboard lit");

static struct input_handler ghi270_handler;
static unsigned long jiffies_last_event;

DEFINE_LED_TRIGGER(ledtrig_keylight);

static void ledtrig_keylight_timerfunc(unsigned long data)
{
	if (time_after(jiffies, jiffies_last_event))
		led_trigger_event(ledtrig_keylight, LED_OFF);
}

static DEFINE_TIMER(ledtrig_keylight_timer, ledtrig_keylight_timerfunc, 0, 0);

/* Turn on the keypad backlight and leave it on until keylight_duration
 * expires.
 */
static void ghi270_event(struct input_handle *handle, unsigned int type,
		         unsigned int code, int down)
{
	if (type == EV_KEY) {
		jiffies_last_event = jiffies;
		led_trigger_event(ledtrig_keylight, LED_FULL);
		mod_timer(&ledtrig_keylight_timer, jiffies + keylight_duration);
	}
	return;
}

static struct input_handle *ghi270_connect(struct input_handler *handler,
					   struct input_dev *dev,
					   const struct input_device_id *id)
{
	struct input_handle *handle;

	if (!(handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL)))
		return NULL;

	handle->dev = dev;
	handle->handler = handler;

	input_open_device(handle);

	return handle;
}

static void ghi270_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	kfree(handle);
}

static const struct input_device_id ghi270_ids[] = {
	{
		/* We want to see all key events. */
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT(EV_KEY) },
	},
	{ }, 	/* Terminating entry */
};

MODULE_DEVICE_TABLE(input, power_ids);

static struct input_handler ghi270_handler = {
	.event =	ghi270_event,
	.connect =	ghi270_connect,
	.disconnect =	ghi270_disconnect,
	.name =		"ghi270-input",
	.id_table =	ghi270_ids,
};

static int __init ghi270_input_init(void)
{
	led_trigger_register_simple("input-key", &ledtrig_keylight);
	return input_register_handler(&ghi270_handler);
}

static void __exit ghi270_input_exit(void)
{
	input_unregister_handler(&ghi270_handler);
	led_trigger_unregister_simple(ledtrig_keylight);
}

module_init(ghi270_input_init);
module_exit(ghi270_input_exit);

MODULE_AUTHOR("Matt Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("input event monitor");
MODULE_LICENSE("GPL");
