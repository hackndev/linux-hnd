/*
 * Palm LifeDrive LED Driver
 *
 * Author: Marek Vasut <marek.vasut@gmail.com>
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
#include <asm/arch/palmld-gpio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/scoop.h>

static void palmldled_amber_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	SET_PALMLD_GPIO(ORANGE_LED, value ? 1 : 0);
}

static void palmldled_green_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	SET_PALMLD_GPIO(GREEN_LED, value ? 1 : 0);
}

static struct led_classdev palmld_amber_led = {
	.name			= "amber",
	.brightness_set		= palmldled_amber_set,
};

static struct led_classdev palmld_green_led = {
	.name			= "green",
	.brightness_set		= palmldled_green_set,
};

#ifdef CONFIG_PM
static int palmldled_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&palmld_amber_led);
	led_classdev_suspend(&palmld_green_led);
	return 0;
}

static int palmldled_resume(struct platform_device *dev)
{
	led_classdev_resume(&palmld_amber_led);
	led_classdev_resume(&palmld_green_led);
	return 0;
}
#endif

static int palmldled_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &palmld_amber_led);
	if (ret < 0)
		return ret;

	ret = led_classdev_register(&pdev->dev, &palmld_green_led);
	if (ret < 0)
		led_classdev_unregister(&palmld_amber_led);

	return ret;
}

static int palmldled_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&palmld_amber_led);
	led_classdev_unregister(&palmld_green_led);
	return 0;
}

static struct platform_driver palmldled_driver = {
	.probe		= palmldled_probe,
	.remove		= palmldled_remove,
#ifdef CONFIG_PM
	.suspend	= palmldled_suspend,
	.resume		= palmldled_resume,
#endif
	.driver		= {
		.name		= "palmld-led",
	},
};

static int __init palmldled_init(void)
{
	return platform_driver_register(&palmldled_driver);
}

static void __exit palmldled_exit(void)
{
 	platform_driver_unregister(&palmldled_driver);
}

module_init(palmldled_init);
module_exit(palmldled_exit);

MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("Palm LifeDrive LED driver");
MODULE_LICENSE("GPL");
