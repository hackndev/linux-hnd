/*
 * linux/drivers/video/backlight/s3c2410_lcd.c
 * Copyright (c) Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C2410 LCD Controller Backlight Driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <asm/arch/lcd.h>
#include <asm/arch/regs-gpio.h>

struct s3c2410bl_devs {
	struct backlight_device *bl;
	struct lcd_device	*lcd;
};

static char suspended;

static int s3c2410bl_get_lcd_power(struct lcd_device *lcd)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&lcd->class_dev);

	if (info)
		return info->lcd_power_value;

	return 0;
}

static int s3c2410bl_set_lcd_power(struct lcd_device *lcd, int power)
{
	struct s3c2410_bl_mach_info *info;
	int lcd_power = 1;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&lcd->class_dev);

	if (info && info->lcd_power) {
		info->lcd_power_value = power;
		if (power != FB_BLANK_UNBLANK)
			lcd_power = 0;
		if (suspended)
			lcd_power = 0;
		info->lcd_power(lcd_power);
	}

	return 0;
}
static int s3c2410bl_get_bl_brightness(struct backlight_device *bl)
{
	struct s3c2410_bl_mach_info *info;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	if(info)
		return info->brightness_value;

	return 0;
}

static int s3c2410bl_set_bl_brightness(struct backlight_device *bl)
{
	struct s3c2410_bl_mach_info *info;
	int brightness = bl->props.brightness;
	int power = 1;

	info = (struct s3c2410_bl_mach_info *)class_get_devdata(&bl->class_dev);

	if (bl->props.power != FB_BLANK_UNBLANK)
		power = 0;
	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		power = 0;
	if (suspended)
		power = 0;

	if (info && info->set_brightness) {
		if ( brightness )
		{
			info->brightness_value = brightness;
			info->set_brightness(brightness);
		}
		else
			power = 0;
	}
	if (info && info->backlight_power) {
		info->backlight_power_value = power;
		info->backlight_power(power);
	}

	return 0;
}

static int is_s3c2410fb(struct fb_info *info)
{
	return (!strcmp(info->fix.id,"s3c2410fb"));
}

static struct backlight_ops s3c2410bl_ops = {
	.get_brightness = s3c2410bl_get_bl_brightness,
	.update_status	= s3c2410bl_set_bl_brightness,
	.check_fb 	= is_s3c2410fb
};
static struct lcd_ops s3c2410lcd_ops = {
	.get_power	= s3c2410bl_get_lcd_power,
	.set_power	= s3c2410bl_set_lcd_power,
	.check_fb	= is_s3c2410fb
};

static int __init s3c2410bl_probe(struct platform_device *pdev)
{
	struct s3c2410bl_devs *devs;
	struct s3c2410_bl_mach_info *info;

	suspended = 0;

	info = ( struct s3c2410_bl_mach_info *)pdev->dev.platform_data;

	if (!info) {
		printk(KERN_ERR "Hm... too bad : no platform data for bl\n");
		return -EINVAL;
	}

	devs = (struct s3c2410bl_devs *)kmalloc(sizeof(*devs),GFP_KERNEL);
	if (!devs) {
		return -ENOMEM;
	}

	/* Register the backlight device */
	devs->bl = backlight_device_register ("s3c2410-bl", &pdev->dev, info, \
		&s3c2410bl_ops);

	if (IS_ERR (devs->bl)) {
		kfree(devs);
		return PTR_ERR (devs->bl);
	}

	devs->bl->props.max_brightness = info->backlight_max;

	/* Set default brightness */
	devs->bl->props.power = FB_BLANK_UNBLANK;
	devs->bl->props.brightness = info->backlight_default;
	s3c2410bl_set_bl_brightness(devs->bl);

	devs->lcd = lcd_device_register("s3c2410-lcd", info, &s3c2410lcd_ops);

	if (IS_ERR (devs->lcd)) {
		backlight_device_unregister(devs->bl);
		kfree(devs);
		return PTR_ERR (devs->lcd);
	}
	platform_set_drvdata(pdev, devs);

	printk(KERN_ERR "s3c2410 Backlight Driver Initialized.\n");
	return 0;
}

static int s3c2410bl_remove(struct platform_device *pdev)
{
	struct s3c2410bl_devs *devs = platform_get_drvdata(pdev);

	if (devs) {
		if (devs->bl) {
			devs->bl->props.power = FB_BLANK_POWERDOWN;
			s3c2410bl_set_bl_brightness(devs->bl);
			backlight_device_unregister(devs->bl);
		}
		if (devs->lcd) {
			lcd_device_unregister(devs->lcd);
		}
		kfree(devs);
	}
	
	printk("s3c2410 Backlight Driver Unloaded\n");

	return 0;

}

#ifdef CONFIG_PM
static int s3c2410bl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct s3c2410bl_devs *devs = platform_get_drvdata(pdev);
	struct s3c2410_bl_mach_info *info = (struct s3c2410_bl_mach_info *)pdev->dev.platform_data;

	if (devs) {
		suspended = 1;
		s3c2410bl_set_bl_brightness(devs->bl);
		s3c2410bl_set_lcd_power(devs->lcd, info->lcd_power_value);
	}

	return 0;
}

static int s3c2410bl_resume(struct platform_device *pdev)
{
	struct s3c2410bl_devs *devs = platform_get_drvdata(pdev);
	struct s3c2410_bl_mach_info *info = (struct s3c2410_bl_mach_info *)pdev->dev.platform_data;

	if (devs) {
		suspended = 0;
		s3c2410bl_set_lcd_power(devs->lcd, info->lcd_power_value);
		s3c2410bl_set_bl_brightness(devs->bl);
	}
	return 0;
}
#else
#define s3c2410bl_suspend NULL
#define s3c2410bl_resume  NULL
#endif

static struct platform_driver s3c2410bl_driver = {
	.driver		= {
		.name	= "s3c2410-bl",
		.owner	= THIS_MODULE,
	},
	.probe		= s3c2410bl_probe,
	.remove		= s3c2410bl_remove,
	.suspend        = s3c2410bl_suspend,
	.resume         = s3c2410bl_resume,
};



static int __init s3c2410bl_init(void)
{
	return platform_driver_register(&s3c2410bl_driver);
}

static void __exit s3c2410bl_cleanup(void)
{
	platform_driver_unregister(&s3c2410bl_driver);
}

module_init(s3c2410bl_init);
module_exit(s3c2410bl_cleanup);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 Backlight Driver");
MODULE_LICENSE("GPLv2");

