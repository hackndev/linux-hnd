/*
 *  Palm LCD Border switch
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

#include <asm/arch/palmlcd-border.h>
#include <asm/arch/gpio.h>

struct palmlcd_border_pdata *pdata;

static ssize_t palmlcd_border_write(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	signed long state = simple_strtol(buf, NULL, 10);
	
	if ( state >= 1 )
	    __gpio_set_value(pdata->select_gpio, 1);
	else
	    __gpio_set_value(pdata->select_gpio, 0);

	msleep(50);
	__gpio_set_value(pdata->switch_gpio, 1);

	if ((state == 1) || (state == 0)) /* default - switch border on/off */
	    msleep(200);
	else { /* hidden functionality - colored border */
	    if (state >= 0)
		msleep(state);
	    else
		msleep(-state);
	}

	__gpio_set_value(pdata->switch_gpio, 0);

	return count;
}

static ssize_t palmlcd_border_read(struct device *dev, 
				  struct device_attribute *attr,
				  char *buf)
{
	return strlcpy(buf, __gpio_get_value(pdata->select_gpio) ? "1\n" : "0\n", 3);
}

static DEVICE_ATTR(border_power, 0644, palmlcd_border_read, palmlcd_border_write);

static struct attribute *palmlcd_border_attrs[] = {
	&dev_attr_border_power.attr,
	NULL
};

static struct attribute_group palmlcd_border_attr_group = {
	.attrs	= palmlcd_border_attrs,
};

static int __devinit palmlcd_border_probe(struct platform_device *pdev)
{
	if(!pdev->dev.platform_data)
	    return -ENODEV;

	pdata = pdev->dev.platform_data;

	return sysfs_create_group(&pdev->dev.kobj, &palmlcd_border_attr_group);
}

static int palmlcd_border_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &palmlcd_border_attr_group);
	return 0;
}

static struct platform_driver palmlcd_border_driver = {
	.probe 		= palmlcd_border_probe,
	.remove		= palmlcd_border_remove,
	.suspend 	= NULL,
	.resume		= NULL,
	.driver = {
		.name 	= "palmlcd-border",
	}
};

static int __init palmlcd_border_init(void)
{
	return platform_driver_register(&palmlcd_border_driver);
}

static void palmlcd_border_exit(void)
{
	platform_driver_unregister(&palmlcd_border_driver);
}

module_init(palmlcd_border_init);
module_exit(palmlcd_border_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("Palm LCD Border switch");
