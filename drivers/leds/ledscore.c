/*
 * linux/drivers/leds/ledscore.c
 *
 *   Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#warning This file is deprecated - please switch to mainline LED classdev
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/leds.h>

struct led_device {
	/* This protects the props field.*/
	spinlock_t lock;
	/* If props is NULL, the driver that registered this device has been unloaded */
	struct led_properties *props;

	unsigned long frequency; /* frequency of blinking, in milliseconds */
	int in_use; /* 1 if this device is in use by the kernel somewhere */
	
	struct class_device class_dev;
	struct timer_list *ktimer;
	struct list_head node;
};

#define to_led_device(d) container_of(d, struct led_device, class_dev)

static rwlock_t leds_list_lock = RW_LOCK_UNLOCKED;
static LIST_HEAD(leds_list);
static rwlock_t leds_interface_list_lock = RW_LOCK_UNLOCKED;
static LIST_HEAD(leds_interface_list);

static void leds_class_release(struct class_device *dev)
{
	struct led_device *d = to_led_device(dev);

	write_lock(&leds_list_lock);
		list_del(&d->node);
	write_unlock(&leds_list_lock);
	
	kfree(d);
}

static struct class leds_class = {
	.name		= "leds",
	.release	= leds_class_release,
};

static void leds_timer_function(unsigned long data)
{
	struct led_device *led_dev = (struct led_device *) data;
	unsigned long delay = 0;
	
	spin_lock(&led_dev->lock);
	if (led_dev->frequency) {
		delay = led_dev->frequency;
		if (likely(led_dev->props->brightness_get)) {
			unsigned long value;
			if (led_dev->props->brightness_get(led_dev->class_dev.dev, led_dev->props))
				value = 0;
			else
				value = 100;
			if (likely(led_dev->props->brightness_set))
				led_dev->props->brightness_set(led_dev->class_dev.dev, led_dev->props, value);
		}
	}
	spin_unlock(&led_dev->lock);

	if (delay)
		mod_timer(led_dev->ktimer, jiffies + msecs_to_jiffies(delay));
}

/* This function MUST be called with led_dev->lock held */
static int leds_enable_timer(struct led_device *led_dev)
{
	if (led_dev->frequency && led_dev->ktimer) {
		/* timer already created, just enable it */
		mod_timer(led_dev->ktimer, jiffies + msecs_to_jiffies(led_dev->frequency));
	} else if (led_dev->frequency && led_dev->ktimer == NULL) {
		/* create a new timer */
		led_dev->ktimer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
		if (led_dev->ktimer) {
			init_timer(led_dev->ktimer);
			led_dev->ktimer->function = leds_timer_function;
			led_dev->ktimer->data = (unsigned long) led_dev;
			led_dev->ktimer->expires = jiffies + msecs_to_jiffies(led_dev->frequency);
			add_timer(led_dev->ktimer);
		} else {
			led_dev->frequency = 0;
			return -ENOMEM;
		}
	}

	return 0;
}


static ssize_t leds_show_in_use(struct class_device *dev, char *buf)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = 0;

	spin_lock(&led_dev->lock);
	sprintf(buf, "%i\n", led_dev->in_use);
	ret = strlen(buf) + 1;
	spin_unlock(&led_dev->lock);

	return ret;
}

static CLASS_DEVICE_ATTR(in_use, 0444, leds_show_in_use, NULL);

static ssize_t leds_show_color(struct class_device *dev, char *buf)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = 0;
	
	spin_lock(&led_dev->lock);
	if (likely(led_dev->props)) {
		sprintf(buf, "%s\n", led_dev->props->color);
		ret = strlen(buf) + 1;
	}
	spin_unlock(&led_dev->lock);

	return ret;
}

static CLASS_DEVICE_ATTR(color, 0444, leds_show_color, NULL);

static ssize_t leds_show_current_color(struct class_device *dev, char *buf)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = 0;

	spin_lock(&led_dev->lock);
	if (likely(led_dev->props)) {
		if (led_dev->props->color_get) {
			sprintf(buf, "%u\n", led_dev->props->color_get(led_dev->class_dev.dev, led_dev->props));
			ret = strlen(buf) + 1;
		}
	}
	spin_unlock(&led_dev->lock);

	return ret;
}

static ssize_t leds_store_current_color(struct class_device *dev, const char *buf, size_t size)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = -EINVAL;
	char *after;

	unsigned long state = simple_strtoul(buf, &after, 10);
	if (after - buf > 0) {
		ret = after - buf;
		spin_lock(&led_dev->lock);
		if (led_dev->props && !led_dev->in_use) {
			if (led_dev->props->color_set) 
				led_dev->props->color_set(led_dev->class_dev.dev, led_dev->props, state);
		}
		spin_unlock(&led_dev->lock);
	}

	return ret;
}

static CLASS_DEVICE_ATTR(current_color, 0444, leds_show_current_color, leds_store_current_color);

static ssize_t leds_show_brightness(struct class_device *dev, char *buf)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = 0;

	spin_lock(&led_dev->lock);
	if (likely(led_dev->props)) {
		if (likely(led_dev->props->brightness_get)) {
			sprintf(buf, "%u\n", 
				led_dev->props->brightness_get(led_dev->class_dev.dev, led_dev->props));
			ret = strlen(buf) + 1;
		}
	}
	spin_unlock(&led_dev->lock);

	return ret;
}

static ssize_t leds_store_brightness(struct class_device *dev, const char *buf, size_t size)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = -EINVAL;
	char *after;

	unsigned long state = simple_strtoul(buf, &after, 10);
	if (after - buf > 0) {
		ret = after - buf;
		spin_lock(&led_dev->lock);
		if (led_dev->props && !led_dev->in_use) {
			if (state > 100) state = 100;
			if (led_dev->props->brightness_set) 
				led_dev->props->brightness_set(led_dev->class_dev.dev, led_dev->props, state);
		}
		spin_unlock(&led_dev->lock);
	}

	return ret;
}

static CLASS_DEVICE_ATTR(brightness, 0644, leds_show_brightness, leds_store_brightness);

static ssize_t leds_show_frequency(struct class_device *dev, char *buf)
{
	struct led_device *led_dev = to_led_device(dev);
	ssize_t ret = 0;

	spin_lock(&led_dev->lock);
	if (likely(led_dev->props)) {
		if (led_dev->props->blink_frequency_get)
			sprintf(buf, "%lu\n", 
				led_dev->props->blink_frequency_get(led_dev->class_dev.dev, led_dev->props));
		else
			sprintf(buf, "%lu\n", led_dev->frequency);
		ret = strlen(buf) + 1;
	}
	spin_unlock(&led_dev->lock);

	return ret;
}

static ssize_t leds_store_frequency(struct class_device *dev, const char *buf, size_t size)
{
	struct led_device *led_dev = to_led_device(dev);
	int ret = -EINVAL, ret2;
	char *after;

	unsigned long state = simple_strtoul(buf, &after, 10);
	if (after - buf > 0) {
		ret = after - buf;
		spin_lock(&led_dev->lock);
		if (led_dev->props) {
			if (led_dev->props->blink_frequency_set) {
				led_dev->props->blink_frequency_set(
					led_dev->class_dev.dev, led_dev->props, state);
			} else {
				if (!led_dev->in_use) {
					led_dev->frequency = state;
					ret2 = leds_enable_timer(led_dev);
					if (ret2) ret = ret2;
				}
			}
		}
		spin_unlock(&led_dev->lock);
	}

	return ret;
}

static CLASS_DEVICE_ATTR(frequency, 0644, leds_show_frequency, leds_store_frequency);

/**
 * leds_device_register - register a new object of led_device class.
 * @dev: The device to register.
 * @prop: the led properties structure for this device.
 */
int leds_device_register(struct device *dev, struct led_properties *props)
{
	int rc;
	struct led_device *new_led;
	struct led_interface *interface;

	new_led = kmalloc (sizeof (struct led_device), GFP_KERNEL);
	if (unlikely (!new_led))
		return -ENOMEM;

	memset(new_led, 0, sizeof(struct led_device));

	spin_lock_init(&new_led->lock);
	new_led->props = props;
	props->led_dev = new_led;

	new_led->class_dev.class = &leds_class;
	new_led->class_dev.dev = dev;

	new_led->frequency = 0;
	new_led->in_use = 0;

	/* assign this led its name */
	strncpy(new_led->class_dev.class_id, props->name, sizeof(new_led->class_dev.class_id));

	rc = class_device_register (&new_led->class_dev);
	if (unlikely (rc)) {
		kfree (new_led);
		return rc;
	}

	/* register the attributes */
	class_device_create_file(&new_led->class_dev, &class_device_attr_in_use);
	class_device_create_file(&new_led->class_dev, &class_device_attr_color);
	class_device_create_file(&new_led->class_dev, &class_device_attr_current_color);
	class_device_create_file(&new_led->class_dev, &class_device_attr_brightness);
	class_device_create_file(&new_led->class_dev, &class_device_attr_frequency);

	/* add to the list of leds */
	write_lock(&leds_list_lock);
		list_add_tail(&new_led->node, &leds_list);
	write_unlock(&leds_list_lock);

	/* notify any interfaces */
	read_lock(&leds_interface_list_lock);
	list_for_each_entry(interface, &leds_interface_list, node) {
		if (interface->add)
			interface->add(dev, props);
	}
	read_unlock(&leds_interface_list_lock);

	printk(KERN_INFO "Registered led device: number=%s, color=%s\n", new_led->class_dev.class_id, props->color);

	return 0;
}
EXPORT_SYMBOL(leds_device_register);

/**
 * leds_device_unregister - unregisters a object of led_properties class.
 * @props: the property to unreigister
 *
 * Unregisters a previously registered via leds_device_register object.
 */
void leds_device_unregister(struct led_properties *props)
{
	struct led_device *led_dev;
	struct led_interface *interface;

	if (!props || !props->led_dev)
		return;

	led_dev = props->led_dev;

	/* notify interfaces device is going away */
	read_lock(&leds_interface_list_lock);
	list_for_each_entry(interface, &leds_interface_list, node) {
		if (interface->remove)
			interface->remove(led_dev->class_dev.dev, props);
	}
	read_unlock(&leds_interface_list_lock);

	class_device_remove_file (&led_dev->class_dev, &class_device_attr_frequency);
	class_device_remove_file (&led_dev->class_dev, &class_device_attr_brightness);
	class_device_remove_file (&led_dev->class_dev, &class_device_attr_current_color);
	class_device_remove_file (&led_dev->class_dev, &class_device_attr_color);
	class_device_remove_file (&led_dev->class_dev, &class_device_attr_in_use);

	spin_lock(&led_dev->lock);
	led_dev->props = NULL;
	props->led_dev = NULL;
	spin_unlock(&led_dev->lock);

	if (led_dev->ktimer) {
		del_timer_sync(led_dev->ktimer);
		kfree(led_dev->ktimer);
		led_dev->ktimer = NULL;
	}

	class_device_unregister(&led_dev->class_dev);
}
EXPORT_SYMBOL(leds_device_unregister);

int leds_acquire(struct led_properties *led)
{
	int ret = -EBUSY;

	spin_lock(&led->led_dev->lock);
	if (!led->led_dev->in_use) {
		led->led_dev->in_use = 1;
		/* Disable the userspace blinking, if any */
		led->led_dev->frequency = 0;
		ret = 0;
	}
	spin_unlock(&led->led_dev->lock);

	return ret;
}
EXPORT_SYMBOL(leds_acquire);
	
void leds_release(struct led_properties *led)
{
	spin_lock(&led->led_dev->lock);
	led->led_dev->in_use = 0;
	/* Disable the kernel blinking, if any */
	led->led_dev->frequency = 0;
	spin_unlock(&led->led_dev->lock);
}
EXPORT_SYMBOL(leds_release);

/* Sets the frequency of the led in milliseconds.
 * Only call this function after leds_acquire returns true
 */
int leds_set_frequency(struct led_properties *led, unsigned long frequency)
{
	int ret = 0;
	
	spin_lock(&led->led_dev->lock);

	if (led->blink_frequency_set) {
		led->blink_frequency_set(led->led_dev->class_dev.dev, led, frequency);
	} else {
		if (!led->led_dev->in_use)
			return -EINVAL;
	
		led->led_dev->frequency = frequency;
		ret = leds_enable_timer(led->led_dev);
	}

	spin_unlock(&led->led_dev->lock);

	return ret;
}
EXPORT_SYMBOL(leds_set_frequency);

int leds_interface_register(struct led_interface *interface)
{
	struct led_device *led_dev;

	write_lock(&leds_interface_list_lock);
	list_add_tail(&interface->node, &leds_interface_list);

	read_lock(&leds_list);
	list_for_each_entry(led_dev, &leds_list, node) {
		spin_lock(&led_dev->lock);
		if (led_dev->props) {
			interface->add(led_dev->class_dev.dev, led_dev->props);
		}
		spin_unlock(&led_dev->lock);
	}
	read_unlock(&leds_list);

	write_unlock(&leds_interface_list_lock);

	return 0;
}
EXPORT_SYMBOL(leds_interface_register);

void leds_interface_unregister(struct led_interface *interface)
{
	write_lock(&leds_interface_list_lock);
	list_del(&interface->node);
	write_unlock(&leds_interface_list_lock);
}
EXPORT_SYMBOL(leds_interface_unregister);

static int __init leds_init(void)
{
	/* initialize the class device */
	return class_register(&leds_class);
}
subsys_initcall(leds_init);

static void __exit leds_exit(void)
{
	class_unregister(&leds_class);
}
module_exit(leds_exit);

MODULE_AUTHOR("John Lenz");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED core class interface");

