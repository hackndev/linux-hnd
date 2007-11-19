/*
 * LED Hardware Timer Trigger
 * 
 * Copyright 2006-2007 Anton Vorontsov <cbou@mail.ru>
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Original author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/ctype.h>
#include "leds.h"

static ssize_t led_delay_on_show(struct class_device *dev, char *buf)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct hwtimer_data *timer_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timer_data->delay_on);

	return strlen(buf) + 1;
}

static ssize_t led_delay_on_store(struct class_device *dev, const char *buf,
                                  size_t size)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct hwtimer_data *timer_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timer_data->delay_on = state;
		if (!timer_data->delay_on || !timer_data->delay_off)
			led_set_brightness(led_cdev, LED_OFF);
		else
			led_set_brightness(led_cdev, LED_FULL);
		ret = count;
	}

	return ret;
}

static ssize_t led_delay_off_show(struct class_device *dev, char *buf)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct hwtimer_data *timer_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timer_data->delay_off);

	return strlen(buf) + 1;
}

static ssize_t led_delay_off_store(struct class_device *dev, const char *buf,
                                   size_t size)
{
	struct led_classdev *led_cdev = class_get_devdata(dev);
	struct hwtimer_data *timer_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timer_data->delay_off = state;
		if (!timer_data->delay_on || !timer_data->delay_off)
			led_set_brightness(led_cdev, LED_OFF);
		else
			led_set_brightness(led_cdev, LED_FULL);
		ret = count;
	}

	return ret;
}

static CLASS_DEVICE_ATTR(delay_on, 0644, led_delay_on_show,
                         led_delay_on_store);
static CLASS_DEVICE_ATTR(delay_off, 0644, led_delay_off_show,
                         led_delay_off_store);

static void hwtimer_trig_activate(struct led_classdev *led_cdev)
{
	int err;
	struct hwtimer_data *timer_data;

	timer_data = kzalloc(sizeof(struct hwtimer_data), GFP_KERNEL);
	if (!timer_data) {
		led_cdev->trigger_data = NULL;
		return;
	}

	timer_data->delay_on = 1000;
	timer_data->delay_off = 1000;

	led_cdev->trigger_data = timer_data;

	err = class_device_create_file(led_cdev->class_dev,
	                               &class_device_attr_delay_on);
	if (err) goto err_delay_on;

	err = class_device_create_file(led_cdev->class_dev,
	                               &class_device_attr_delay_off);
	if (err) goto err_delay_off;

	return;

err_delay_off:
	class_device_remove_file(led_cdev->class_dev,
	                         &class_device_attr_delay_on);
err_delay_on:
	led_cdev->trigger_data = NULL;
	kfree(timer_data);
	printk(KERN_ERR "ledtrig-hwtimer: activate failed\n");
	return;
}

static void hwtimer_trig_deactivate(struct led_classdev *led_cdev)
{
	struct hwtimer_data *timer_data = led_cdev->trigger_data;

	if (timer_data) {
		class_device_remove_file(led_cdev->class_dev,
					&class_device_attr_delay_on);
		class_device_remove_file(led_cdev->class_dev,
					&class_device_attr_delay_off);
		kfree(timer_data);
	}

	led_cdev->trigger_data = NULL;
	return;
}

static int hwtimer_trig_led_supported(struct led_classdev *led_cdev)
{
	if (led_cdev->flags & LED_SUPPORTS_HWTIMER)
		return LED_SUPPORTS_HWTIMER;
	return 0;
}

void led_trigger_register_hwtimer(const char *name,
                                  struct led_trigger **trig)
{
	struct led_trigger *tmptrig;

	tmptrig = kzalloc(sizeof(struct led_trigger), GFP_KERNEL);
	if (tmptrig) {
		tmptrig->name = name;
		tmptrig->activate = hwtimer_trig_activate;
		tmptrig->deactivate = hwtimer_trig_deactivate;
		tmptrig->is_led_supported = hwtimer_trig_led_supported;
		led_trigger_register(tmptrig);
	}
	*trig = tmptrig;

	return;
}

void led_trigger_unregister_hwtimer(struct led_trigger *trig)
{
	led_trigger_unregister(trig);
	kfree(trig);
	return;
}

EXPORT_SYMBOL_GPL(led_trigger_register_hwtimer);
EXPORT_SYMBOL_GPL(led_trigger_unregister_hwtimer);

static struct led_trigger timer_led_trigger = {
	.name     = "hwtimer",
	.activate = hwtimer_trig_activate,
	.deactivate = hwtimer_trig_deactivate,
	.is_led_supported = hwtimer_trig_led_supported,
};

static int __init hwtimer_trig_init(void)
{
	return led_trigger_register(&timer_led_trigger);
}

static void __exit hwtimer_trig_exit(void)
{
	led_trigger_unregister(&timer_led_trigger);
}

module_init(hwtimer_trig_init);
module_exit(hwtimer_trig_exit);

MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("LED Hardware Timer Trigger");
MODULE_LICENSE("GPL");
