/*
 *  Palm TX LCD driver
 *
 *  Based on Asus MyPal 716 LCD and Backlight driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>

#include <asm/arch/palmtx-gpio.h>
#include <asm/arch/palmtx-init.h>

static int lcd_power;

int palmtx_lcd_set_power(struct lcd_device *lm, int level)
{
	switch (level) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
		  // this is very likely incomplete !!!
		  	printk("palmtx_lcd: turning LCD on\n");
			SET_GPIO(GPIO_NR_PALMTX_LCD_POWER, 1);
			mdelay(70);
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
                        break;
		case FB_BLANK_POWERDOWN:
		  // this is very likely incomplete !!!
			printk("palmtx_lcd: turning LCD off\n");
			SET_GPIO(GPIO_NR_PALMTX_LCD_POWER, 0);
			mdelay(65);
			break;
	}

	lcd_power = level;

	return 0;
}

static int palmtx_lcd_get_power(struct lcd_device *lm)
{
	printk("palmtx_lcd: power is set to %d", lcd_power);
	return lcd_power;
} 

struct lcd_properties palmtx_lcd_properties =
{
	.owner		= THIS_MODULE,
	.set_power	= palmtx_lcd_set_power,
	.get_power	= palmtx_lcd_get_power,
}; 

static struct lcd_device *pxafb_lcd_device;


int palmtx_lcd_probe(struct device *dev)
{
	if (!machine_is_xscale_palmtx())
		return -ENODEV;

	pxafb_lcd_device = lcd_device_register("pxafb", NULL, &palmtx_lcd_properties);

	if (IS_ERR(pxafb_lcd_device)){
	  printk("palmtx_lcd_probe: cannot register LCD device\n");
	  return -ENOMEM;
	}

	printk ("palmtx LCD driver registered\n");
	
	return 0;
}

static int palmtx_lcd_remove(struct device *dev)
{
	lcd_device_unregister(pxafb_lcd_device);
	printk ("palmtx LCD driver unregistered\n");
	return 0;
}

#ifdef CONFIG_PM

static int palmtx_lcd_suspend(struct device *dev, pm_message_t state)
{
	palmtx_lcd_set_power(pxafb_lcd_device, FB_BLANK_UNBLANK);
	return 0;
}

static int palmtx_lcd_resume(struct device *dev)
{
	palmtx_lcd_set_power(pxafb_lcd_device, FB_BLANK_POWERDOWN);							
	return 0;
}
#endif

static struct device_driver palmtx_lcd_driver = {
	.name           = "palmtx-lcd",
	.bus            = &platform_bus_type,
	.probe          = palmtx_lcd_probe,
	.remove         = palmtx_lcd_remove,
#ifdef CONFIG_PM
	.suspend        = palmtx_lcd_suspend,
	.resume         = palmtx_lcd_resume,
#endif
};

static int palmtx_lcd_init(void)
{
	if (!machine_is_xscale_palmtx())
		return -ENODEV;

	return driver_register(&palmtx_lcd_driver);
}

static void palmtx_lcd_exit(void)
{
	lcd_device_unregister(pxafb_lcd_device);
	driver_unregister(&palmtx_lcd_driver);
}

module_init(palmtx_lcd_init);
module_exit(palmtx_lcd_exit);

MODULE_DESCRIPTION("LCD driver for Palm TX");
MODULE_LICENSE("GPL");
