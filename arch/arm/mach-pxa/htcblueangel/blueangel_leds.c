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
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>
#include <asm/mach-types.h>

#include "blueangel_leds.h"

static volatile void *virt;

struct blueangel_led_data {
	int                     hw_num;
	int                     registered;
	struct led_classdev	props;
};

static struct blueangel_led_data leds[] = {
	{
		.hw_num=HTC_WIFI_LED,
		.props  = {
			.name	       = "blueangel:wifi",
			.brightness_set = blueangel_led_brightness_set,
		}
	},
	{
		.hw_num=HTC_BT_LED,
		.props  = {
			.name	       = "blueangel:bt",
			.brightness_set = blueangel_led_brightness_set,
		}
	},
};

static int
led_read(int reg)
{
	volatile unsigned short *addr=((volatile unsigned short *)(virt+0xa));	
	volatile unsigned char *data=((volatile unsigned char *)(virt+0xc));	

	*addr |= 0x80;
	*addr = (*addr & 0xff80) | (reg & 0x7f);

	return *data;
}

static void
led_write(int reg, int val)
{
	volatile unsigned short *addr=((volatile unsigned short *)(virt+0xa));	
	volatile unsigned short *data=((volatile unsigned short *)(virt+0xc));	

	*addr = (*addr & 0xff80) | (reg & 0x7f);
	*addr &= 0xff7f;

	*data = (*data & 0xff00) | (val & 0xff);

}


void blueangel_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
 int i, hw_num=-1, duty_time=100, cycle_time=100;

	if (hw_num == HTC_BT_LED || hw_num == HTC_WIFI_LED) {
		if (value) {
			duty_time=(duty_time*16+500)/1000;
			cycle_time=(cycle_time*16+500)/1000;
		} else {
			duty_time=0;
			cycle_time=1;
		}
	}
 	
	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 if (!strcmp(leds[i].props.name,led_cdev->name)) 
	  hw_num=leds[i].hw_num;

	switch (hw_num) 
	{
	 case HTC_WIFI_LED:
		led_write(2, duty_time);
		led_write(3, cycle_time-duty_time);
		if (value) {
			led_write(4, led_read(4) | 0x2);
			led_write(0x20, led_read(0x20) & ~0x8);
		} else {
			led_write(4, led_read(4) &  ~0x2);
			led_write(0x20, led_read(0x20) | 0x8);
		}
		break;
	 case HTC_BT_LED:
		led_write(0, duty_time);
		led_write(1, cycle_time-duty_time);
		if (value) {
			led_write(4, led_read(4) | 0x1);
			led_write(0x20, led_read(0x20) & ~0x4);
		} else {
			led_write(4, led_read(4) &  ~0x1);
			led_write(0x20, led_read(0x20) | 0x4);
		}
		break;
	}

	printk("brightness=%d for the led=%d\n",value, hw_num);
}

static int blueangel_led_probe(struct platform_device *dev)
{
	int i,ret=0;

	printk("blueangel_led_probe\n");

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
	       ret = led_classdev_register(&dev->dev, &leds[i].props);
	       leds[i].registered = 1;

	       leds[i].props.brightness_set(&leds[i].props,LED_OFF);

	       if (unlikely(ret)) {
		       printk(KERN_WARNING "Unable to register blueangel_led %s\n", leds[i].props.name);
		       leds[i].registered = 0;
	       }
	}
	return ret;
}

static int blueangel_led_remove(struct platform_device *dev)
{
	int i;

	printk("blueangel_led_remove\n");

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 if (leds[i].registered)
	  led_classdev_unregister(&leds[i].props);

	return 0;
}

static int blueangel_led_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 led_classdev_suspend(&leds[i].props);

	return 0;
}

static int blueangel_led_resume(struct platform_device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++)
	 led_classdev_resume(&leds[i].props);
	
	return 0;
}


static struct platform_driver blueangel_led_driver = {
	.driver = {
		.name   = "blueangel-leds",
	},
	.probe  = blueangel_led_probe,
	.remove = blueangel_led_remove,
#ifdef CONFIG_PM
	.suspend = blueangel_led_suspend,
	.resume = blueangel_led_resume,
#endif
};

static int blueangel_led_init (void)
{
	if (!machine_is_blueangel() )
		return -ENODEV;
	virt=ioremap(0x11000000, 4096);

 	return platform_driver_register(&blueangel_led_driver);
}

static void blueangel_led_exit (void)
{
	iounmap(virt);
	platform_driver_unregister(&blueangel_led_driver);
}

module_init (blueangel_led_init);
module_exit (blueangel_led_exit);

MODULE_LICENSE("GPL");
