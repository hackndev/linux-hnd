/*
 * HTC Treo680 GPIO LED driver
 *
 * Copyright (c) 2006 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/io.h>
#include <asm/gpio.h>

#include <asm/arch/palmt680-gpio.h>

static void treo680_led_keys_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO_NR_PALMT680_KEYB_BL, value);
}

static void treo680_led_vibra_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO_NR_PALMT680_VIBRA, value);
}

static void treo680_led_red_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO_NR_PALMT680_RED_LED, value);
}

static void treo680_led_green_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO_NR_PALMT680_GREEN_LED, value);
}


static struct led_classdev treo680_keys_led = {
	.name			= "treo680:keys",
	.default_trigger	= "backlight",
	.brightness_set		= treo680_led_keys_set,
};

static struct led_classdev treo680_vibra_led = {
	.name			= "treo680:vibra",
	.default_trigger	= "none",
	.brightness_set		= treo680_led_vibra_set,
};

static struct led_classdev treo680_red_led = {
	.name			= "treo680:red",
	.default_trigger	= "none",
	.brightness_set		= treo680_led_red_set,
};

static struct led_classdev treo680_green_led = {
	.name			= "treo680:green",
	.default_trigger	= "none",
	.brightness_set		= treo680_led_green_set,
};


#ifdef CONFIG_PM
static int treo680_led_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&treo680_red_led);
	led_classdev_suspend(&treo680_green_led);
	led_classdev_suspend(&treo680_keys_led);
	led_classdev_suspend(&treo680_vibra_led);

	return 0;
}

static int treo680_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&treo680_red_led);
	led_classdev_resume(&treo680_green_led);
	led_classdev_resume(&treo680_keys_led);
	led_classdev_resume(&treo680_vibra_led);

	return 0;
}
#else
#define treo680_led_suspend NULL
#define treo680_led_resume NULL
#endif

static int treo680_led_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &treo680_keys_led);
	if (ret < 0)
		return ret;

	ret = led_classdev_register(&pdev->dev, &treo680_vibra_led);
	if (ret < 0)
		goto vibrafail;

	ret = led_classdev_register(&pdev->dev, &treo680_red_led);
	if (ret < 0)
		goto redfail;

	ret = led_classdev_register(&pdev->dev, &treo680_green_led);
	if (ret < 0)
		goto greenfail;

	return ret;

greenfail:
	led_classdev_unregister(&treo680_red_led);
redfail:
	led_classdev_unregister(&treo680_vibra_led);
vibrafail:
	led_classdev_unregister(&treo680_keys_led);

	return ret;
}

static int treo680_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&treo680_vibra_led);
	led_classdev_unregister(&treo680_keys_led);
	led_classdev_unregister(&treo680_red_led);
	led_classdev_unregister(&treo680_green_led);

	return 0;
}

static struct platform_driver treo680_led_driver = {
	.probe		= treo680_led_probe,
	.remove		= treo680_led_remove,
	.suspend	= treo680_led_suspend,
	.resume		= treo680_led_resume,
	.driver		= {
		.name		= "palmt680-led",
	},
};

static int __init treo680_led_init (void)
{
	return platform_driver_register(&treo680_led_driver);
}

static void __exit treo680_led_exit (void)
{
 	platform_driver_unregister(&treo680_led_driver);
}

module_init (treo680_led_init);
module_exit (treo680_led_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("HTC treo680 LED driver");
MODULE_LICENSE("GPL");
