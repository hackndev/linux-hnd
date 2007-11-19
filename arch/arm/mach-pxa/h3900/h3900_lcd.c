/*
 * LCD driver for h3900
 *
 * Copyright 2003 Phil Blundell
 * Copyright 2006 Paul Sokolovsky
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/arch/h3900.h>

extern struct platform_device h3900_asic3;

static int h3900_lcd_set_power(struct lcd_device *lm, int power)
{
	if (power == FB_BLANK_UNBLANK) {
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_ON, GPIO3_LCD_ON);
		mdelay(3);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_PCI, GPIO3_LCD_PCI);
		mdelay(7);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_5V_ON, GPIO3_LCD_5V_ON);
		mdelay(1);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_NV_ON, GPIO3_LCD_NV_ON);
		mdelay(7);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_9V_ON, GPIO3_LCD_9V_ON);
		mdelay(10);
	} else {
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_PCI, 0);
		mdelay(20);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_NV_ON, 0);
		mdelay(181);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_9V_ON, 0);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_5V_ON, 0);
		mdelay(11);
		asic3_set_gpio_out_b(&h3900_asic3.dev, GPIO3_LCD_ON, 0);
		mdelay(10);
	}
	return 0;
}

static int h3900_lcd_get_power( struct lcd_device *lm )
{
	if (asic3_get_gpio_out_b(&h3900_asic3.dev) & GPIO3_LCD_PCI) {
		if (asic3_get_gpio_out_b(&h3900_asic3.dev) & GPIO3_LCD_ON)
			return FB_BLANK_UNBLANK;
		else
			return FB_BLANK_VSYNC_SUSPEND;
	} else
		return FB_BLANK_POWERDOWN;
}

static struct lcd_ops h3900_lcd_properties = {
	.set_power	= h3900_lcd_set_power,
	.get_power	= h3900_lcd_get_power,
};

static struct lcd_device *pxafb_lcd_device;

static int h3900_lcd_probe(struct platform_device *pdev)
{
	pxafb_lcd_device = lcd_device_register("pxafb", NULL, &h3900_lcd_properties);
	if (IS_ERR (pxafb_lcd_device))
		return PTR_ERR (pxafb_lcd_device);

	h3900_lcd_set_power (pxafb_lcd_device, FB_BLANK_UNBLANK);

        return 0;
}

static int h3900_lcd_remove(struct platform_device *pdev)
{
	h3900_lcd_set_power(pxafb_lcd_device, FB_BLANK_POWERDOWN);
        lcd_device_unregister(pxafb_lcd_device);

        return 0;
}

static int h3900_lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
        h3900_lcd_set_power(pxafb_lcd_device, FB_BLANK_POWERDOWN);
        return 0;
}

static int h3900_lcd_resume(struct platform_device *pdev)
{
        h3900_lcd_set_power(pxafb_lcd_device, FB_BLANK_UNBLANK);
        return 0;
}

static struct platform_driver h3900_lcd_driver = {
	.driver   = {
	    .name     = "h3900-lcd",
	},
        .probe    = h3900_lcd_probe,
        .remove   = h3900_lcd_remove,
        .suspend  = h3900_lcd_suspend,
        .resume   = h3900_lcd_resume,
};


static int
h3900_lcd_init (void)
{
	if (!machine_is_h3900())
		return -ENODEV;

        return platform_driver_register(&h3900_lcd_driver);
}

static void
h3900_lcd_exit (void)
{
        platform_driver_unregister(&h3900_lcd_driver);
}

module_init (h3900_lcd_init);
module_exit (h3900_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>, Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("iPAQ h3900 LCD driver");
