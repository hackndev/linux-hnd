/*
 * non-ASIC3 LED interface for HTC Universal.
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
#include <linux/mfd/asic3_base.h>

#include <asm/io.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>
#include <asm/mach-types.h>

#include "htcuniversal_leds.h"

struct htcuniversal_led_data {
	int                     hw_num;
	int                     registered;
	struct led_classdev	props;
};

static struct htcuniversal_led_data leds[] = {
	{
		.hw_num=HTC_PHONE_BL_LED,
		.props  = {
			.name	       = "phone:redgreen",
			.brightness_set = htcuniversal_led_brightness_set,
		}
	},
	{
		.hw_num=HTC_KEYBD_BL_LED,
		.props  = {
			.name	       = "keyboard:rosa",
			.brightness_set = htcuniversal_led_brightness_set,
		}
	},
	{
		.hw_num=HTC_VIBRA,
		.props  = {
			.name	       = "phone:vibra",
			.brightness_set = htcuniversal_led_brightness_set,
		}
	},
	{
		.hw_num=HTC_CAM_FLASH_LED,
		.props  = {
			.name	       = "flashlight:white",
			.brightness_set = htcuniversal_led_brightness_set,
		}
	},
};


void htcuniversal_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
 int i, hw_num=-1;

 	
	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 if (!strcmp(leds[i].props.name,led_cdev->name)) 
	  hw_num=leds[i].hw_num;

	switch (hw_num) 
	{
	 case HTC_PHONE_BL_LED:
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BL_KEYP_PWR_ON, (value > LED_OFF) ? (1<<GPIOD_BL_KEYP_PWR_ON) : 0);
		break;
	 case HTC_KEYBD_BL_LED:
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BL_KEYB_PWR_ON, (value > LED_OFF) ? (1<<GPIOD_BL_KEYB_PWR_ON) : 0);
		break;
	 case HTC_VIBRA:
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_VIBRA_PWR_ON, (value > LED_OFF) ? (1<<GPIOD_VIBRA_PWR_ON) : 0);
		break;
	 case HTC_CAM_FLASH_LED:
		asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_FLASHLIGHT, (value > LED_OFF) ? (1<<GPIOA_FLASHLIGHT) : 0);
		break;
	}

	printk("brightness=%d for the led=%d\n",value, hw_num);
}

static int htcuniversal_led_probe(struct platform_device *dev)
{
	int i,ret=0;

	printk("htcuniversal_led_probe\n");

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
	       ret = led_classdev_register(&dev->dev, &leds[i].props);
	       leds[i].registered = 1;

	       leds[i].props.brightness_set(&leds[i].props,LED_OFF);

	       if (unlikely(ret)) {
		       printk(KERN_WARNING "Unable to register htcuniversal_led %s\n", leds[i].props.name);
		       leds[i].registered = 0;
	       }
	}
	return ret;
}

static int htcuniversal_led_remove(struct platform_device *dev)
{
	int i;

	printk("htcuniversal_led_remove\n");

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 if (leds[i].registered)
	  led_classdev_unregister(&leds[i].props);

	return 0;
}

static int htcuniversal_led_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 led_classdev_suspend(&leds[i].props);

	return 0;
}

static int htcuniversal_led_resume(struct platform_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 led_classdev_resume(&leds[i].props);
	
	return 0;
}


static struct platform_driver htcuniversal_led_driver = {
	.driver = {
		.name   = "htcuniversal_led",
	},
	.probe  = htcuniversal_led_probe,
	.remove = htcuniversal_led_remove,
#ifdef CONFIG_PM
	.suspend = htcuniversal_led_suspend,
	.resume = htcuniversal_led_resume,
#endif
};

static int htcuniversal_led_init (void)
{
	if (!machine_is_htcuniversal() )
		return -ENODEV;

 return platform_driver_register(&htcuniversal_led_driver);
}

static void htcuniversal_led_exit (void)
{
	platform_driver_unregister(&htcuniversal_led_driver);
}

module_init (htcuniversal_led_init);
module_exit (htcuniversal_led_exit);

MODULE_LICENSE("GPL");
