/*
 * Palm Tungsten|T3 LED Driver
 *
 * Author: Tomas Cech <Tomas.Cech@matfyz.cz>
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
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/scoop.h>
#include <asm/arch/tps65010.h>


struct palmtt3led_work_type {
	struct work_struct work;
	int led1;
	int led2;
	int vibra;
} palmtt3led_work;


static void tps650101_scheduled_leds(struct work_struct *work)
{
	tps65010_set_led(LED1,palmtt3led_work.led1);
	tps65010_set_led(LED2,palmtt3led_work.led2);
	tps65010_set_vib(palmtt3led_work.vibra);
}

static void palmtt3led_red_set(struct led_classdev *led_cdev,
			       enum led_brightness value)
{
	palmtt3led_work.led2 = value ? ON : OFF;
	schedule_work(&(palmtt3led_work.work));
}

static void palmtt3led_green_set(struct led_classdev *led_cdev,
				 enum led_brightness value)
{
	/* NOTE: This is set to OFF, not to OFF... 
	   It shows charging status - plugged/unplugged... */
	palmtt3led_work.led1 = value ? ON : UNDER_CHG_CTRL;
	schedule_work(&(palmtt3led_work.work));
}

static void palmtt3led_red_blink_set(struct led_classdev *led_cdev,
				     enum led_brightness value)
{
	palmtt3led_work.led2 = value ? BLINK : OFF;
	schedule_work(&(palmtt3led_work.work));
}

static void palmtt3led_green_blink_set(struct led_classdev *led_cdev,
				       enum led_brightness value)
{
	palmtt3led_work.led1 = value ? BLINK : OFF;
	schedule_work(&(palmtt3led_work.work));
}

static void palmtt3_vibra_set(struct led_classdev *led_cdev,
			      enum led_brightness value)
{
	palmtt3led_work.vibra = value ? ON : OFF;
	schedule_work(&(palmtt3led_work.work));
}


static struct led_classdev palmtt3_red_led = {
	.name			= "red",
	.brightness_set		= palmtt3led_red_set,
	.default_trigger	= "mmc-card",
};

static struct led_classdev palmtt3_green_led = {
	.name			= "green",
	.brightness_set		= palmtt3led_green_set,
};

static struct led_classdev palmtt3_red_blink_led = {
	.name			= "red_blink",
	.brightness_set		= palmtt3led_red_blink_set,
};

static struct led_classdev palmtt3_green_blink_led = {
	.name			= "green_blink",
	.brightness_set		= palmtt3led_green_blink_set,
};

static struct led_classdev palmtt3_vibra = {
	.name			= "vibra",
	.brightness_set		= palmtt3_vibra_set,
};


#ifdef CONFIG_PM
static int palmtt3led_suspend(struct platform_device *dev,
			      pm_message_t state)
{
	led_classdev_suspend(&palmtt3_red_led);
	led_classdev_suspend(&palmtt3_green_led);
	led_classdev_suspend(&palmtt3_red_blink_led);
	led_classdev_suspend(&palmtt3_green_blink_led);
	led_classdev_suspend(&palmtt3_vibra);
	return 0;
}

static int palmtt3led_suspend_late(struct platform_device *dev,
				   pm_message_t state)
{
	tps65010_set_led(LED1, OFF);
	tps65010_set_led(LED2, OFF);
	tps65010_set_vib(OFF);
	return 0;
}

static int palmtt3led_resume_early(struct platform_device *dev)
{
	schedule_work(&(palmtt3led_work.work));
	return 0;
}

static int palmtt3led_resume(struct platform_device *dev)
{
	led_classdev_resume(&palmtt3_red_led);
	led_classdev_resume(&palmtt3_green_led);
	led_classdev_resume(&palmtt3_red_blink_led);
	led_classdev_resume(&palmtt3_green_blink_led);
	led_classdev_resume(&palmtt3_vibra);
	return 0;
}
#endif

static int palmtt3led_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &palmtt3_red_led);
	if (ret < 0)
		return ret;

	ret = led_classdev_register(&pdev->dev, &palmtt3_green_led);
	if (ret < 0)
		led_classdev_unregister(&palmtt3_red_led);

	ret = led_classdev_register(&pdev->dev, &palmtt3_red_blink_led);
	if (ret < 0) {
		led_classdev_unregister(&palmtt3_red_led);
		led_classdev_unregister(&palmtt3_green_led);
	}

	ret = led_classdev_register(&pdev->dev, &palmtt3_green_blink_led);
	if (ret < 0) {
		led_classdev_unregister(&palmtt3_red_led);
		led_classdev_unregister(&palmtt3_green_led);
		led_classdev_unregister(&palmtt3_red_blink_led);
	}
	ret = led_classdev_register(&pdev->dev, &palmtt3_vibra);
	if (ret < 0) {
		led_classdev_unregister(&palmtt3_red_led);
		led_classdev_unregister(&palmtt3_green_led);
		led_classdev_unregister(&palmtt3_red_blink_led);
		led_classdev_unregister(&palmtt3_green_blink_led);
	}

	palmtt3led_work.led1 = UNDER_CHG_CTRL;
	palmtt3led_work.led2 = OFF;
	palmtt3led_work.vibra = OFF;
	INIT_WORK(&(palmtt3led_work.work),tps650101_scheduled_leds);
	return ret;
}

static int palmtt3led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&palmtt3_red_led);
	led_classdev_unregister(&palmtt3_green_led);
	led_classdev_unregister(&palmtt3_red_blink_led);
	led_classdev_unregister(&palmtt3_green_blink_led);
	led_classdev_unregister(&palmtt3_vibra);
	return 0;
}

static struct platform_driver palmtt3led_driver = {
	.probe		= palmtt3led_probe,
	.remove		= palmtt3led_remove,
#ifdef CONFIG_PM
	.suspend	= palmtt3led_suspend,
	.resume		= palmtt3led_resume,
	.suspend_late	= palmtt3led_suspend_late,
	.resume_early	= palmtt3led_resume_early,
#endif
	.driver		= {
		.name		= "palmtt3-led",
	},
};

static int __init palmtt3led_init(void)
{
	return platform_driver_register(&palmtt3led_driver);
}

static void __exit palmtt3led_exit(void)
{
 	platform_driver_unregister(&palmtt3led_driver);
}

module_init(palmtt3led_init);
module_exit(palmtt3led_exit);

MODULE_AUTHOR("Tomas Cech <Tomas.Cech@matfyz.cz");
MODULE_DESCRIPTION("Palm Tungsten|T3 LED driver");
MODULE_LICENSE("GPL");
