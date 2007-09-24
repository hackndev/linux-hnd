/*
 * Palm Zire72 LED Driver
 *
 * Author: Jan Herman <2hp@seznam.cz>
 
 * Based on driver from Marek Vasut (Palm Life Drive LED Driver)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <asm/mach-types.h>
#include <asm/arch/palmz72-gpio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/scoop.h>

static void palmz72led_green_set(struct led_classdev *led_cdev,
				 enum led_brightness value)
{
	SET_PALMZ72_GPIO(LED, value ? 1 : 0);
}

static struct led_classdev palmz72_green_led = {
	.name			= "led",
	.brightness_set		= palmz72led_green_set,
};

#ifdef CONFIG_PM
static int palmz72led_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&palmz72_green_led);
	return 0;
}

static int palmz72led_resume(struct platform_device *dev)
{
	led_classdev_resume(&palmz72_green_led);
	return 0;
}
#endif

static int palmz72led_probe(struct platform_device *pdev)
{
	int ret;
	ret = led_classdev_register(&pdev->dev, &palmz72_green_led);
	if (ret < 0)
		led_classdev_unregister(&palmz72_green_led);

	return ret;
}

static int palmz72led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&palmz72_green_led);
	return 0;
}

static struct platform_driver palmz72led_driver = {
	.probe		= palmz72led_probe,
	.remove		= palmz72led_remove,
#ifdef CONFIG_PM
	.suspend	= palmz72led_suspend,
	.resume		= palmz72led_resume,
#endif
	.driver		= {
	.name		= "palmz72-led",
	},
};

static int __init palmz72led_init(void)
{
	return platform_driver_register(&palmz72led_driver);
}

static void __exit palmz72led_exit(void)
{
 	platform_driver_unregister(&palmz72led_driver);
}

module_init(palmz72led_init);
module_exit(palmz72led_exit);

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>");
MODULE_DESCRIPTION("Palm Zire72 LED driver");
MODULE_LICENSE("GPL");
