/*
 * drivers/leds/h2200_leds.c
 *
 * Copyright (C) 2005 Matt Reimer <mreimer@vpop.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/arch/h2200.h>

struct h2200_led_data {
	unsigned long		led_num;
	int			registered;
	int			brightness;
	int			frequency;
	int			hamcop_duty_time;
	int			hamcop_cycle_time;
	struct led_properties	props;
};
#define to_h2200_led_data(d) container_of(d, struct h2200_led_data, props)

int h2200_led_brightness_get(struct device *dev, struct led_properties *props)
{
	struct h2200_led_data *data = to_h2200_led_data(props);

	return data->brightness;
}

void h2200_led_brightness_set(struct device *dev, struct led_properties *props, int value)
{
	struct h2200_led_data *data = to_h2200_led_data(props);
	unsigned long flags;

	if (value < 0) value = 0;
	data->brightness = value;
	local_irq_save(flags);
	if (data->brightness) {
		data->brightness = 100;
		if (!data->frequency) {
			data->hamcop_duty_time = 8;
			data->hamcop_cycle_time = 8;
		}
	} else {
		data->hamcop_duty_time = 0;
		data->hamcop_cycle_time = 0;
	}

	h2200_set_led(data->led_num, data->hamcop_duty_time,
		      data->hamcop_cycle_time);

	local_irq_restore(flags);
}

unsigned long h2200_led_frequency_get(struct device *dev, struct led_properties *props)
{
	struct h2200_led_data *data = to_h2200_led_data(props);

	return data->hamcop_cycle_time * 125; /* Convert back to ms. */
}

void h2200_led_frequency_set(struct device *dev, struct led_properties *props, unsigned long value)
{
	struct h2200_led_data *data = to_h2200_led_data(props);
	unsigned long flags;
	int freq_hamcop;

	if (value < 0) value = 0;
	data->frequency= value;
	freq_hamcop = value / 125; /* Convert from ms to 1/8 sec. */
	local_irq_save(flags);
	if (data->frequency) {
		data->brightness = 100;
		data->hamcop_duty_time  = freq_hamcop / 2;
		data->hamcop_cycle_time = freq_hamcop;
	} else {
		if (data->brightness) {
			data->hamcop_duty_time  = 8;
			data->hamcop_cycle_time = 8;
		} else {
			data->hamcop_duty_time  = 0;
			data->hamcop_cycle_time = 0;
		}
	}
	h2200_set_led(data->led_num, data->hamcop_duty_time,
		      data->hamcop_cycle_time);

	local_irq_restore(flags);
}

static struct h2200_led_data leds[] = {
	{
		.led_num = 0,
		.props	= {
			.owner		= THIS_MODULE,
			.name		= "power",
			.color		= "amber",
			.brightness_get	= h2200_led_brightness_get,
			.brightness_set	= h2200_led_brightness_set,
			.blink_frequency_get	= h2200_led_frequency_get,
			.blink_frequency_set	= h2200_led_frequency_set,
			.color_get	= NULL,
			.color_set	= NULL,
		}
	},
	{
		.led_num = 1,
		.props	= {
			.owner		= THIS_MODULE,
			.name		= "notify",
			.color		= "green",
			.brightness_get	= h2200_led_brightness_get,
			.brightness_set	= h2200_led_brightness_set,
			.blink_frequency_get	= h2200_led_frequency_get,
			.blink_frequency_set	= h2200_led_frequency_set,
			.color_get	= NULL,
			.color_set	= NULL,
		}
	},
	{
		.led_num = 2,
		.props	= {
			.owner		= THIS_MODULE,
			.name		= "bluetooth",
			.color		= "blue",
			.brightness_get	= h2200_led_brightness_get,
			.brightness_set	= h2200_led_brightness_set,
			.blink_frequency_get	= h2200_led_frequency_get,
			.blink_frequency_set	= h2200_led_frequency_set,
			.color_get	= NULL,
			.color_set	= NULL,
		}
	},
};

struct led_properties *h2200_power_led     = &leds[0].props;
struct led_properties *h2200_notify_led    = &leds[1].props;
struct led_properties *h2200_bluetooth_led = &leds[2].props;

EXPORT_SYMBOL(h2200_power_led);
EXPORT_SYMBOL(h2200_notify_led);
EXPORT_SYMBOL(h2200_bluetooth_led);

static int h2200_leds_probe(struct device *dev)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		ret = leds_device_register(dev, &leds[i].props);
		leds[i].registered = 1;
		if (unlikely(ret)) {
			printk(KERN_WARNING "Unable to register h2200 led %s\n", leds[i].props.color);
			leds[i].registered = 0;
		}
		h2200_set_led(i, 0, 0);
	}

	return ret;
}

static int h2200_leds_remove(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		if (leds[i].registered) {
			leds_device_unregister(&leds[i].props);
		}
	}
	return 0;
}

struct device_driver h2200_led_device_driver = {
	.name     = "hamcop leds",
	.bus	  = &platform_bus_type,
	.probe    = h2200_leds_probe,
	.remove   = h2200_leds_remove,
};

static int
h2200_led_init (void)
{
	return driver_register(&h2200_led_device_driver);
}

static void
h2200_led_exit (void)
{
	driver_unregister(&h2200_led_device_driver);
}

module_init(h2200_led_init);
module_exit(h2200_led_exit);

MODULE_AUTHOR("Michael Opdenacker <michaelo@handhelds.org>");
MODULE_DESCRIPTION("HP iPAQ h2200 LED driver");
MODULE_LICENSE("Dual BSD/GPL");
