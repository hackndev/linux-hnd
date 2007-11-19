/*
 * LED Kernel Timer Trigger
 *
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include "leds.h"

struct timer_trig_data {
	unsigned long delay_on;		/* milliseconds on */
	unsigned long delay_off;	/* milliseconds off */
	struct timer_list timer;
};
struct led_trigger_timer {
	struct led_trigger trigger;
	int state;
};

void led_timer_trigger_event(struct led_trigger *trigger,
			enum led_brightness brightness)
{
	struct list_head *entry;
	struct led_classdev *led_cdev;
	struct timer_trig_data *timer_data;
	struct led_trigger_timer *timer_trig;
	if (!trigger)
		return;
	
	timer_trig = container_of(trigger, struct led_trigger_timer, trigger);
	timer_trig->state = brightness == LED_OFF ? 0 : 1;

	read_lock(&trigger->leddev_list_lock);
	list_for_each(entry, &trigger->led_cdevs) {
		led_cdev = list_entry(entry, struct led_classdev, trig_list);
		timer_data = led_cdev->trigger_data;
		if (brightness == LED_FULL)
			mod_timer(&timer_data->timer, jiffies +  HZ);
	}
	read_unlock(&trigger->leddev_list_lock);
}

static void led_timer_function(unsigned long data)
{
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct timer_trig_data *timer_data = led_cdev->trigger_data;
	struct led_trigger *trigger;
	struct led_trigger_timer *timer_trig;
	unsigned long brightness = LED_OFF;
	unsigned long delay = timer_data->delay_off;

	trigger = led_cdev->trigger;
	timer_trig = container_of(trigger, struct led_trigger_timer, trigger);

	if (!timer_data->delay_on || !timer_data->delay_off) {
		led_set_brightness(led_cdev, LED_OFF);
		return;
	}

	if (!led_cdev->brightness) {
		brightness = LED_FULL;
		delay = timer_data->delay_on;
	}

	if (!timer_trig->state)
		brightness = LED_OFF;

	led_set_brightness(led_cdev, brightness);

	if (timer_trig->state)
		mod_timer(&timer_data->timer, jiffies + msecs_to_jiffies(delay));
}

static ssize_t led_delay_on_show(struct class_device *dev, char *buf)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct timer_trig_data *timer_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timer_data->delay_on);

	return strlen(buf) + 1;
}

static ssize_t led_delay_on_store(struct class_device *dev, const char *buf,
				size_t size)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct timer_trig_data *timer_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timer_data->delay_on = state;
		mod_timer(&timer_data->timer, jiffies + 1);
		ret = count;
	}

	return ret;
}

static ssize_t led_delay_off_show(struct class_device *dev, char *buf)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct timer_trig_data *timer_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timer_data->delay_off);

	return strlen(buf) + 1;
}

static ssize_t led_delay_off_store(struct class_device *dev, const char *buf,
				size_t size)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct timer_trig_data *timer_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timer_data->delay_off = state;
		mod_timer(&timer_data->timer, jiffies + 1);
		ret = count;
	}

	return ret;
}

static CLASS_DEVICE_ATTR(delay_on, 0644, led_delay_on_show,
			led_delay_on_store);
static CLASS_DEVICE_ATTR(delay_off, 0644, led_delay_off_show,
			led_delay_off_store);

static void timer_trig_activate(struct led_classdev *led_cdev)
{
	struct timer_trig_data *timer_data;
	int rc;

	timer_data = kzalloc(sizeof(struct timer_trig_data), GFP_KERNEL);
	if (!timer_data)
		return;

	led_cdev->trigger_data = timer_data;

	timer_data->delay_on = 1000;
	timer_data->delay_off = 1000;
	init_timer(&timer_data->timer);
	timer_data->timer.function = led_timer_function;
	timer_data->timer.data = (unsigned long) led_cdev;

	rc = class_device_create_file(led_cdev->class_dev,
				&class_device_attr_delay_on);
	if (rc) goto err_out;
	rc = class_device_create_file(led_cdev->class_dev,
				&class_device_attr_delay_off);
	if (rc) goto err_out_delayon;

	mod_timer(&timer_data->timer, jiffies + 15);
	return;

err_out_delayon:
	class_device_remove_file(led_cdev->class_dev,
				&class_device_attr_delay_on);
err_out:
	led_cdev->trigger_data = NULL;
	kfree(timer_data);
}

static void timer_trig_deactivate(struct led_classdev *led_cdev)
{
	struct timer_trig_data *timer_data = led_cdev->trigger_data;

	if (timer_data) {
		class_device_remove_file(led_cdev->class_dev,
					&class_device_attr_delay_on);
		class_device_remove_file(led_cdev->class_dev,
					&class_device_attr_delay_off);
		del_timer_sync(&timer_data->timer);
		kfree(timer_data);
	}
}


void led_trigger_register_timer(const char *name,
                                  struct led_trigger **trig)
{
	struct led_trigger_timer *tmptrig_timer;
	
	tmptrig_timer = kzalloc(sizeof(struct led_trigger_timer), GFP_KERNEL);
	
	if (tmptrig_timer) {
		tmptrig_timer->state = 0;
		tmptrig_timer->trigger.name = name;
		tmptrig_timer->trigger.activate = timer_trig_activate;
		tmptrig_timer->trigger.deactivate = timer_trig_deactivate;
		tmptrig_timer->trigger.send_event = led_timer_trigger_event;

		led_trigger_register(&tmptrig_timer->trigger);
	}
	*trig = &tmptrig_timer->trigger;

	return;
}

void led_trigger_unregister_timer(struct led_trigger *trig)
{
	led_trigger_unregister(trig);
	kfree(trig);
	return;
}

EXPORT_SYMBOL_GPL(led_trigger_register_timer);
EXPORT_SYMBOL_GPL(led_trigger_unregister_timer);

static struct led_trigger_timer timer_trig = {
	.state = 1,
	.trigger = {
		.name     = "timer",
		.activate = timer_trig_activate,
		.deactivate = timer_trig_deactivate,
	}
};

static int __init timer_trig_init(void)
{
	return led_trigger_register(&timer_trig.trigger);
}

static void __exit timer_trig_exit(void)
{
	led_trigger_unregister(&timer_trig.trigger);
}

module_init(timer_trig_init);
module_exit(timer_trig_exit);

MODULE_AUTHOR("Richard Purdie <rpurdie@openedhand.com>");
MODULE_DESCRIPTION("Timer LED trigger");
MODULE_LICENSE("GPL");
