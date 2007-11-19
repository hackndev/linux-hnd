/*
 * Hardware definitions for Compaq iPAQ H36xx Handheld Computers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Alessandro Gardich.
 *
 * History : based on 2.4 drivers.
 *
 * 2005-11-xx	Alessandro Gardich, let start.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/arch/h3600.h>
#include <asm/hardware/ipaq-ops.h>

#include <../arch/arm/mach-sa1100/generic.h>

static int h3600_lcd_power; 

/*
 * helper for sa1100fb
 */
static void __h3600_lcd_power(int enable )
{
	assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_POWER, enable);
	assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_ENABLE, enable);

	return;
}

/*--- sa1100fb END ---*/


static int h3600_lcd_set_power( struct lcd_device *lm, int level )
{
	h3600_lcd_power = level;	

	if (level < 1) {
		assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_POWER, 1);
	} else {
		assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_POWER, 0);
	}

	if (level < 4) {
		assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_ENABLE, 1);
	} else {
		assign_ipaqsa_egpio(IPAQ_EGPIO_LCD_ENABLE, 0);
	}

	return 0;
}

static int h3600_lcd_get_power( struct lcd_device *lm)
{
	return h3600_lcd_power;
}


static struct lcd_ops h3600_lcd_ops = {
	.get_power	= h3600_lcd_get_power,
	.set_power	= h3600_lcd_set_power,
};

static struct lcd_device *h3600_lcd_device;

static int h3600_lcd_probe (struct platform_device *pdev)
{
	if (! machine_is_h3600 ())
		return -ENODEV;

	h3600_lcd_device = lcd_device_register("sa1100fb", NULL, &h3600_lcd_ops);
	if (IS_ERR (h3600_lcd_device))
		return PTR_ERR (h3600_lcd_device);

	h3600_lcd_set_power (h3600_lcd_device, 0);

	sa1100fb_lcd_power = __h3600_lcd_power;

	return 0;
}

static int h3600_lcd_remove (struct platform_device *pdev)
{
	h3600_lcd_set_power(h3600_lcd_device, 4);
	lcd_device_unregister (h3600_lcd_device);

	sa1100fb_lcd_power = NULL;

	return 0;
}

static int h3600_lcd_suspend ( struct platform_device *pdev, pm_message_t msg) 
{
	h3600_lcd_set_power(h3600_lcd_device, 4);
	return 0;
}

static int h3600_lcd_resume (struct platform_device *pdev) 
{
	h3600_lcd_set_power(h3600_lcd_device, 0);
	return 0;
}

struct platform_driver h3600_lcd_driver = {
	.driver   = { 
		.name     = "h3600-lcd",
	},
	.probe    = h3600_lcd_probe,
	.remove   = h3600_lcd_remove,
	.suspend  = h3600_lcd_suspend,
	.resume   = h3600_lcd_resume,
};

static int __init h3600_lcd_init(void)
{
	int rc;

	if (!machine_is_h3600())
		return -ENODEV;

	rc = platform_driver_register(&h3600_lcd_driver);

	return rc;
}

static void __exit h3600_lcd_exit(void)
{
	lcd_device_unregister(h3600_lcd_device);
	platform_driver_unregister(&h3600_lcd_driver);
}

module_init (h3600_lcd_init);
module_exit (h3600_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Gardich <gremlin-4KDpiRHFOM2onA0d6jMUrA@public.gmane.org>");
MODULE_DESCRIPTION("iPAQ h3600 LCD driver");

