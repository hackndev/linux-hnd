/*
 *  Palm T|X LCD Border switch
 *
 *  Copyright (C) 2007 Marek Vasut <marek.vasut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/arch/hardware.h>
#include <asm/arch/palmtx-gpio.h>

static ssize_t palmtx_border_write(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	signed long state = simple_strtol(buf, NULL, 10);
	
	if ( state >= 1 )
	    SET_PALMTX_GPIO(BORDER_SELECT, 1);
	else
	    SET_PALMTX_GPIO(BORDER_SELECT, 0);

	msleep(50);
	SET_PALMTX_GPIO(BORDER_SWITCH, 1);

	if ((state == 1) || (state == 0)) /* default - switch border on/off */
	    msleep(200);
	else { /* hidden functionality - colored border */
	    if (state >= 0)
		msleep(state);
	    else
		msleep(-state);
	}

	SET_PALMTX_GPIO(BORDER_SWITCH, 0);

	return count;
}

static ssize_t palmtx_border_read(struct device *dev, 
				  struct device_attribute *attr,
				  char *buf)
{
	return strlcpy(buf, GET_PALMTX_GPIO(BORDER_SELECT) ? "1\n" : "0\n", 3);
}

static DEVICE_ATTR(border_power, 0644, palmtx_border_read, palmtx_border_write);

static struct attribute *palmtx_border_attrs[] = {
	&dev_attr_border_power.attr,
	NULL
};

static struct attribute_group palmtx_border_attr_group = {
	.attrs	= palmtx_border_attrs,
};

static int __devinit palmtx_border_probe(struct platform_device *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &palmtx_border_attr_group);
}

static int palmtx_border_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &palmtx_border_attr_group);
	return 0;
}

static struct platform_driver palmtx_border_driver = {
	.probe 		= palmtx_border_probe,
	.remove		= palmtx_border_remove,
	.suspend 	= NULL,
	.resume		= NULL,
	.driver = {
		.name 	= "palmtx-border",
	}
};

static int __init palmtx_border_init(void)
{
	return platform_driver_register(&palmtx_border_driver);
}

static void palmtx_border_exit(void)
{
	platform_driver_unregister(&palmtx_border_driver);
}

module_init(palmtx_border_init);
module_exit(palmtx_border_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("Palm T|X Border switch");
