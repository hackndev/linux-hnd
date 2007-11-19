/*
 * HP iPAQ h1910/h1915 LCD Driver
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 * Copyright (C) 2003-2004 Joshua Wise
 * Copyright (C) 2000-2003 Hewlett-Packard Company.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * History (deprecated):
 *
 * 2004-06-01   Joshua Wise        Adapted to new LED API
 * 2004-06-01   Joshua Wise        Re-adapted for hp iPAQ h1900
 * ????-??-??   ?                  Converted to h3900_lcd.c
 * 2003-05-14   Joshua Wise        Adapted for the HP iPAQ H1900
 * 2002-08-23   Jamey Hicks        Adapted for use with PXA250-based iPAQs
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 *                                 and abstracted EGPIO interface.
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
#include <linux/corgi_bl.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/arch/pxafb.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h1900-asic.h>
#include <asm/arch/h1900-gpio.h>

#include <linux/mfd/asic3_base.h>

extern struct platform_device h1900_asic3;
#define asic3 &h1900_asic3.dev

int h1900_lcd_set_power(struct lcd_device *lm, int level)
{
	switch (level) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
            		asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_5V, GPIO3_H1900_LCD_5V);
			mdelay(20);
            		asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_PCI, GPIO3_H1900_LCD_PCI);
			mdelay(20);
            		asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_3V, GPIO3_H1900_LCD_3V);
			GPSR(GPIO_NR_H1900_VGL_EN) = GPIO_bit(GPIO_NR_H1900_VGL_EN);
			GPSR(GPIO_NR_H1900_LCD_SOMETHING) = GPIO_bit(GPIO_NR_H1900_LCD_SOMETHING);
			mdelay(10);
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			break;
		case FB_BLANK_POWERDOWN:
			GPCR(GPIO_NR_H1900_LCD_SOMETHING) = GPIO_bit(GPIO_NR_H1900_LCD_SOMETHING);
			GPCR(GPIO_NR_H1900_VGL_EN) = GPIO_bit(GPIO_NR_H1900_VGL_EN);
			mdelay(1);
			asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_3V, 0);
			asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_PCI, 0);
			asic3_set_gpio_out_c(asic3, GPIO3_H1900_LCD_5V, 0);
			break;
	}

	return 0;
}

static int h1900_lcd_get_power(struct lcd_device *lm)
{
	if (asic3_get_gpio_out_c(asic3) & GPIO3_H1900_LCD_5V)
		return FB_BLANK_UNBLANK;
	else
		return FB_BLANK_POWERDOWN;
}

static struct lcd_ops h1900_lcd_properties = {
	.set_power      = h1900_lcd_set_power,
	.get_power      = h1900_lcd_get_power,
};

static struct pxafb_mode_info h1910_lcd_modes[] = {
{
	.pixclock =     221039,
	.bpp =          16,
	.xres =         240,
	.yres =         320,
	.hsync_len =    3,
	.vsync_len =    5,
	.left_margin =  25,
	.upper_margin = 6,
	.right_margin = 22,
	.lower_margin = 11,
	.sync =         0,
}
};

static struct pxafb_mach_info h1900_fb_info = {
	.modes          = h1910_lcd_modes,
	.num_modes      = ARRAY_SIZE(h1910_lcd_modes),
	.lccr0 =        (LCCR0_PAS),
};

static struct lcd_device *h1900_lcd_device;

static int h1900_lcd_probe(struct platform_device *dev)
{
	h1900_lcd_device = lcd_device_register("pxafb", NULL, &h1900_lcd_properties);

	if (IS_ERR(h1900_lcd_device))
		return PTR_ERR(h1900_lcd_device);

	h1900_lcd_set_power(h1900_lcd_device, FB_BLANK_UNBLANK);

	return 0;
}

static int h1900_lcd_remove(struct platform_device *pdev)
{
	h1900_lcd_set_power(h1900_lcd_device, FB_BLANK_POWERDOWN);
	lcd_device_unregister(h1900_lcd_device);

	return 0;
}

static int h1900_lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	h1900_lcd_set_power(h1900_lcd_device, FB_BLANK_POWERDOWN);

	return 0;
}

static int h1900_lcd_resume(struct platform_device *pdev)
{
	h1900_lcd_set_power(h1900_lcd_device, FB_BLANK_UNBLANK);

	return 0;
}

static struct platform_driver h1900_lcd_driver = {
	.driver	=	{
		.name	= "h1900-lcd",
	},
	.probe		= h1900_lcd_probe,
	.remove		= h1900_lcd_remove,
	.suspend	= h1900_lcd_suspend,
	.resume		= h1900_lcd_resume,
};

static __init int h1900_lcd_init(void)
{
	if (!machine_is_h1900())
		return -ENODEV;

	set_pxa_fb_info(&h1900_fb_info);

	return platform_driver_register(&h1900_lcd_driver);
}

static __exit void h1900_lcd_exit(void)
{
	platform_driver_unregister(&h1900_lcd_driver);
}

module_init(h1900_lcd_init);
module_exit(h1900_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("HP iPAQ h1910/h1915 LCD glue driver");
