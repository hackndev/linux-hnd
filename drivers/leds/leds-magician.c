/*
 * HTC Magician GPIO LED driver
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

#include <asm/arch/magician.h>

static void magician_led_keys_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO103_MAGICIAN_LED_KP, value);
}

static void magician_led_vibra_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	gpio_set_value(GPIO22_MAGICIAN_VIBRA_EN, value);
}

static struct led_classdev magician_keys_led = {
	.name			= "magician:keys",
	.default_trigger	= "backlight",
	.brightness_set		= magician_led_keys_set,
};

static struct led_classdev magician_vibra_led = {
	.name			= "magician:vibra",
	.default_trigger	= "none",
	.brightness_set		= magician_led_vibra_set,
};

#ifdef CONFIG_PM
static int magician_led_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&magician_keys_led);
	led_classdev_suspend(&magician_vibra_led);

	return 0;
}

static int magician_led_resume(struct platform_device *dev)
{
	int i;

	led_classdev_resume(&magician_keys_led);
	led_classdev_resume(&magician_vibra_led);

	return 0;
}
#else
#define magician_led_suspend NULL
#define magician_led_resume NULL
#endif

static int magician_led_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &magician_keys_led);
	if (ret < 0)
		return ret;

	ret = led_classdev_register(&pdev->dev, &magician_vibra_led);
	if (ret < 0)
		led_classdev_unregister(&magician_keys_led);

	return ret;
}

static int magician_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&magician_vibra_led);
	led_classdev_unregister(&magician_keys_led);

	return 0;
}

static struct platform_driver magician_led_driver = {
	.probe		= magician_led_probe,
	.remove		= magician_led_remove,
	.suspend	= magician_led_suspend,
	.resume		= magician_led_resume,
	.driver		= {
		.name		= "magician-led",
	},
};

static int __init magician_led_init (void)
{
	return platform_driver_register(&magician_led_driver);
}

static void __exit magician_led_exit (void)
{
 	platform_driver_unregister(&magician_led_driver);
}

module_init (magician_led_init);
module_exit (magician_led_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("HTC Magician LED driver");
MODULE_LICENSE("GPL");
