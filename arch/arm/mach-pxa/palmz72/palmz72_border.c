/*
 *  	PalmOne Zire 72 LCD Border switch
 * 		(Based on Palm LD LCD border switch) 
 *
 *  	Author: Marek Vasut <marek.vasut@gmail.com>
 * 		Modification for Palm Zire 72:	Jan Herman <z72ka@hackndev.com>
 * 
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/arch/hardware.h>
#include <asm/arch/palmz72-gpio.h>

static ssize_t palmz72_border_write(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	signed long state = simple_strtol(buf, NULL, 10);
	
	if ( state >= 1 )
	    SET_PALMZ72_GPIO(BORDER_SELECT, 1);
	else
	    SET_PALMZ72_GPIO(BORDER_SELECT, 0);

	msleep(50);
	SET_PALMZ72_GPIO(BORDER_SWITCH, 1);

	if ((state == 1) || (state == 0)) /* default - switch border on/off */
	    msleep(200);
	else { /* hidden functionality - colored border */
	    if (state >= 0)
		msleep(state);
	    else
		msleep(-state);
	}

	SET_PALMZ72_GPIO(BORDER_SWITCH, 0);

	return count;
}

static ssize_t palmz72_border_read(struct device *dev, 
				  struct device_attribute *attr,
				  char *buf)
{
	return strlcpy(buf, GET_PALMZ72_GPIO(BORDER_SELECT) ? "1\n" : "0\n", 3);
}

static DEVICE_ATTR(border_power, 0644, palmz72_border_read, palmz72_border_write);

static struct attribute *palmz72_border_attrs[] = {
	&dev_attr_border_power.attr,
	NULL
};

static struct attribute_group palmz72_border_attr_group = {
	.attrs	= palmz72_border_attrs,
};

static int __devinit palmz72_border_probe(struct platform_device *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &palmz72_border_attr_group);
}

static int palmz72_border_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &palmz72_border_attr_group);
	return 0;
}

static struct platform_driver palmz72_border_driver = {
	.probe 		= palmz72_border_probe,
	.remove		= palmz72_border_remove,
	.suspend 	= NULL,
	.resume		= NULL,
	.driver = {
		.name 	= "palmz72-border",
	}
};

static int __init palmz72_border_init(void)
{
	printk(KERN_INFO "LCD border driver registered\n");
	return platform_driver_register(&palmz72_border_driver);
}

static void palmz72_border_exit(void)
{
	printk(KERN_INFO "LCD border driver unregistered\n");
	platform_driver_unregister(&palmz72_border_driver);
}

module_init(palmz72_border_init);
module_exit(palmz72_border_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jan Herman <z72ka@hackndev.com>");
MODULE_DESCRIPTION("PalmOne Zire72 Border switch");
