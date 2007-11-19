/*
 *  Asus MyPal A716 LCD glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *  Copyright (C) 2004 Vitaliy Sardyko
 *  Copyright (C) 2003 Adam Turowski
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/asus716-gpio.h>

int a716_lcd_set_power(struct lcd_device *lm, int level)
{
	switch (level) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
			a716_gpo_set(GPO_A716_LCD_ENABLE);
			a716_gpo_set(GPO_A716_LCD_POWER3);
			mdelay(30);
			a716_gpo_set(GPO_A716_LCD_POWER1);
			mdelay(30);
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
                        break;
		case FB_BLANK_POWERDOWN:
			a716_gpo_clear(GPO_A716_LCD_POWER1);
			mdelay(65);
			a716_gpo_clear(GPO_A716_LCD_POWER3);
			a716_gpo_clear(GPO_A716_LCD_ENABLE);
			break;
	}

	return 0;
}

static int a716_lcd_get_power(struct lcd_device *lm)
{
	if (a716_gpo_get() & GPO_A716_LCD_ENABLE)
		return FB_BLANK_UNBLANK;
	else
		return FB_BLANK_POWERDOWN;
}

struct lcd_ops a716_lcd_properties =
{
	.set_power	= a716_lcd_set_power,
	.get_power	= a716_lcd_get_power,
}; 

static struct pxafb_mode_info a716_lcd_modes[] = {
{
	.pixclock =     171521,
	.bpp =          16,
	.xres =         240,
	.yres =         320,
	.hsync_len =    64,
	.vsync_len =    6,
	.left_margin =  11,
	.upper_margin = 4,
	.right_margin = 11,
	.lower_margin = 4,
	.sync =         0,
}
};

static struct pxafb_mach_info a716_fb_info = {
	.modes          = a716_lcd_modes,
	.num_modes      = ARRAY_SIZE(a716_lcd_modes),
	.lccr0 =        (LCCR0_PAS),
};

static struct lcd_device *a716_lcd_dev;

static int a716_lcd_probe(struct platform_device *pdev)
{
	a716_lcd_dev = lcd_device_register("pxafb", NULL, &a716_lcd_properties);

	if (IS_ERR(a716_lcd_dev))
		return PTR_ERR(a716_lcd_dev);

	a716_lcd_set_power(a716_lcd_dev, FB_BLANK_UNBLANK);

	return 0;
}

static int a716_lcd_remove(struct platform_device *pdev)
{
        a716_lcd_set_power(a716_lcd_dev, FB_BLANK_POWERDOWN);
	lcd_device_unregister(a716_lcd_dev);

	return 0;
}

static int a716_lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	a716_lcd_set_power(a716_lcd_dev, FB_BLANK_POWERDOWN);

	return 0;
}

static int a716_lcd_resume(struct platform_device *pdev)
{
	a716_lcd_set_power(a716_lcd_dev, FB_BLANK_UNBLANK);

        return 0;
}

static struct platform_driver a716_lcd_driver = {
	.driver =       {
		.name   = "a716-lcd",
	},
	.probe          = a716_lcd_probe,
	.remove         = a716_lcd_remove,
	.suspend        = a716_lcd_suspend,
	.resume         = a716_lcd_resume,
};

static __init int a716_lcd_init(void)
{
	if (!machine_is_a716())
		return -ENODEV;

	set_pxa_fb_info(&a716_fb_info);

	return platform_driver_register(&a716_lcd_driver);
}

static __exit void a716_lcd_exit(void)
{
	platform_driver_unregister(&a716_lcd_driver);
}

module_init(a716_lcd_init);
module_exit(a716_lcd_exit);

MODULE_AUTHOR("Vitaliy Sardyko, Adam Turowski, Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 LCD glue driver");
MODULE_LICENSE("GPL");
