/*
 * LEDs interface for Grayhill GHI270
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */
 
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ghi270.h>

#define GHI270_KEYPAD_MAX_BRIGHTNESS 255

void ghi270_led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness value)
{
	PWM_CTRL2   = 0x3f;
	PWM_PERVAL2 = GHI270_KEYPAD_MAX_BRIGHTNESS - 1;
	PWM_PWDUTY2 = GHI270_KEYPAD_MAX_BRIGHTNESS - value;
}

void ghi270_green_led_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness value)
{
	gpio_set_value(GHI270_GPIO93_GREEN_LED, value > LED_OFF ? 1 : 0);
}

void ghi270_red_led_brightness_set(struct led_classdev *led_cdev,
				   enum led_brightness value)
{
	gpio_set_value(GHI270_GPIO94_RED_LED, value > LED_OFF ? 1 : 0);
}

void ghi270_debug_led_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness value)
{
	/* gpio level 0 = on */
	gpio_set_value(GHI270_GPIO41_DEBUG_LED, value > LED_OFF ? 0 : 1);
}

void ghi270_vibrator_set(struct led_classdev *led_cdev,
			 enum led_brightness value)
{
	gpio_set_value(GHI270_H_GPIO87_VIBRATOR, value > LED_OFF ? 1 : 0);
}

static struct led_classdev ghi270_leds[] = {

	/* keypad green */
	{
		.name		= "ghi270:green",
		.brightness_set = ghi270_green_led_brightness_set,
		/* .default_trigger = "nand-disk", */
	},

	/* keypad red */
	{
		.name		= "ghi270:red",
		.brightness_set = ghi270_red_led_brightness_set,
	},

	/* keypad backlight */
	{
		.name		= "ghi270keypad:backlight",
		.brightness_set = ghi270_led_brightness_set,
		.default_trigger = "input-key",
	},

	/* debug LED on the mainboard, not visible outside the case */
	{
		.name		= "ghi270debug:green",
		.brightness_set = ghi270_debug_led_brightness_set,
	},

	/* vibrator */
	{
		.name		= "ghi270:vibrator",
		.brightness_set = ghi270_vibrator_set,
	}
};


static int ghi270_led_probe(struct platform_device *pdev)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(ghi270_leds); i++) {
		if(machine_is_ghi270hg()
		    && (ghi270_leds[i].brightness_set == ghi270_vibrator_set)) {
			/* No vibrator on HG */
			continue;
		}
		ret = led_classdev_register(&pdev->dev, &ghi270_leds[i]);
		if (ret) {
			printk(KERN_ERR "Error %d registering led %s\n",
			       ret, ghi270_leds[i].name);
			goto err;
		}

		/* Turn it off. */
		ghi270_leds[i].brightness_set(&ghi270_leds[i], 0);
	}

	return 0;

err:
	while (--i >= 0)
		led_classdev_unregister(&ghi270_leds[i]);
	return ret;
}

static int ghi270_led_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ghi270_leds); i++)
		led_classdev_unregister(&ghi270_leds[i]);

	return 0;
}

static int ghi270_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ghi270_leds); i++)
		led_classdev_suspend(&ghi270_leds[i]);

	return 0;
}

static int ghi270_led_resume(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ghi270_leds); i++)
		led_classdev_resume(&ghi270_leds[i]);

	return 0;
}


static struct platform_driver ghi270_led_driver = {
	.driver = {
		.name   = "ghi270-leds",
	},
	.probe  = ghi270_led_probe,
	.remove = ghi270_led_remove,
#ifdef CONFIG_PM
	.suspend = ghi270_led_suspend,
	.resume = ghi270_led_resume,
#endif
};

static int ghi270_led_init(void)
{
	if (!(machine_is_ghi270() || machine_is_ghi270hg()))
		return -ENODEV;

	return platform_driver_register(&ghi270_led_driver);
}

static void ghi270_led_exit(void)
{
	platform_driver_unregister(&ghi270_led_driver);
}

module_init(ghi270_led_init);
module_exit(ghi270_led_exit);

MODULE_LICENSE("GPL");
/* vim600: set noexpandtab sw=8 : */
