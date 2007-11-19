/*
 * linux/drivers/leds/locomo.c
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#warning This file is deprecated - please switch to mainline LED classdev
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds.h>

#include <asm/hardware.h>
#include <asm/hardware/locomo.h>

struct locomoled_data {
	unsigned long		offset;
	int			registered;
	int		brightness;
	struct led_properties	props;
};
#define to_locomoled_data(d) container_of(d, struct locomoled_data, props)

int locomoled_brightness_get(struct device *dev, struct led_properties *props)
{
	struct locomoled_data *data = to_locomoled_data(props);

	return data->brightness;
}

void locomoled_brightness_set(struct device *dev, struct led_properties *props, int value)
{
	struct locomo_dev *locomo_dev = LOCOMO_DEV(dev);
	struct locomoled_data *data = to_locomoled_data(props);
	
	unsigned long flags;

	if (value < 0) value = 0;
	data->brightness = value;
	local_irq_save(flags);
	if (data->brightness) {
		data->brightness = 100;
		locomo_writel(LOCOMO_LPT_TOFH, locomo_dev->mapbase + data->offset);
	} else
		locomo_writel(LOCOMO_LPT_TOFL, locomo_dev->mapbase + data->offset);
	local_irq_restore(flags);
}

static struct locomoled_data leds[] = {
	{
		.offset	= LOCOMO_LPT0,
		.props	= {
			.owner			= THIS_MODULE,
			.name			= "power",
			.color			= "amber",
			.brightness_get		= locomoled_brightness_get,
			.brightness_set		= locomoled_brightness_set,
			.color_get		= NULL,
			.color_set		= NULL,
			.blink_frequency_get	= NULL,
			.blink_frequency_set	= NULL,
		}
	},
	{
		.offset	= LOCOMO_LPT1,
		.props	= {
			.owner			= THIS_MODULE,
			.name			= "mail",
			.color			= "green",
			.brightness_get		= locomoled_brightness_get,
			.brightness_set		= locomoled_brightness_set,
			.color_get		= NULL,
			.color_set		= NULL,
			.blink_frequency_get	= NULL,
			.blink_frequency_set	= NULL,
		}
	},
};

static int locomoled_probe(struct locomo_dev *dev)
{
	int i, ret = 0;
	
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		ret = leds_device_register(&dev->dev, &leds[i].props);
		leds[i].registered = 1;
		if (unlikely(ret)) {
			printk(KERN_WARNING "Unable to register locomo led %s\n", leds[i].props.color);
			leds[i].registered = 0;
		}
	}
	
	return ret;
}

static int locomoled_remove(struct locomo_dev *dev) {
	int i;
	
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		if (leds[i].registered) {
			leds_device_unregister(&leds[i].props);
		}
	}
	return 0;
}

static struct locomo_driver locomoled_driver = {
	.drv = {
		.name = "locomoled"
	},
	.devid	= LOCOMO_DEVID_LED,
	.probe	= locomoled_probe,
	.remove	= locomoled_remove,
};

static int __init locomoled_init(void) {
	return locomo_driver_register(&locomoled_driver);
}
module_init(locomoled_init);

MODULE_AUTHOR("John Lenz <lenz@cs.wisc.edu>");
MODULE_DESCRIPTION("Locomo LED driver");
MODULE_LICENSE("GPL");
