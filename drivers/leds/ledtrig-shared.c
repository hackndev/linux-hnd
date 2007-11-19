/*
 * Trigger for the LED which is shared between devices.
 * 
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/leds.h>
#include "leds.h"

static void shared_trig_activate(struct led_classdev *led_cdev)
{
	struct led_trigger_shared *trig = container_of(led_cdev->trigger,
	                            struct led_trigger_shared, trigger);
	if (trig->cnt) led_set_brightness(led_cdev, LED_FULL);
	else led_set_brightness(led_cdev, LED_OFF);
	return;
}

static void shared_trig_deactivate(struct led_classdev *led_cdev)
{
	led_set_brightness(led_cdev, LED_OFF);
	return;
}

void led_trigger_event_shared(struct led_trigger_shared *trig,
                              enum led_brightness b)
{
	struct list_head *entry;

	if (!trig) return;

	read_lock(trig->trigger.leddev_list_lock);
	if (b == LED_FULL) {
		trig->cnt++;
	}
	else if (trig->cnt == 0 || --trig->cnt == 0) {
		b = LED_OFF;
	}
	else goto return_unlock;

	list_for_each(entry, &trig->trigger.led_cdevs) {
		struct led_classdev *led_cdev;
		led_cdev = list_entry(entry, struct led_classdev, trig_list);
		led_set_brightness(led_cdev, b);
	}
return_unlock:
	read_unlock(trig->trigger.leddev_list_lock);
	return;
}

void led_trigger_register_shared(const char *name,
                                 struct led_trigger_shared **trig)
{
	struct led_trigger_shared *_trig;

	_trig = kzalloc(sizeof(struct led_trigger_shared), GFP_KERNEL);
	if (_trig) {
		_trig->trigger.name = name;
		_trig->trigger.activate = shared_trig_activate;
		_trig->trigger.deactivate = shared_trig_deactivate;
		_trig->cnt = 0;
		led_trigger_register(&_trig->trigger);
	}
	*trig = _trig;

	return;
}

void led_trigger_unregister_shared(struct led_trigger_shared *trig)
{
	led_trigger_unregister(&trig->trigger);
	kfree(trig);
	return;
}

EXPORT_SYMBOL_GPL(led_trigger_event_shared);
EXPORT_SYMBOL_GPL(led_trigger_register_shared);
EXPORT_SYMBOL_GPL(led_trigger_unregister_shared);

MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("Trigger for the LED which is shared between devices");
MODULE_LICENSE("GPL");
