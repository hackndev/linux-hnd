/*
 * Backlight Driver for HP iPAQ hx2750
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx2750.h>

#define HX2750_MAX_INTENSITY      0x190
#define HX2750_DEFAULT_INTENSITY  0x082

static int hx2750_bl_powermode = FB_BLANK_UNBLANK;
static int current_intensity = 0;
static spinlock_t bl_lock = SPIN_LOCK_UNLOCKED;

static void hx2750_set_intensity(int intensity)
{
	unsigned long flags;

	if (hx2750_bl_powermode != FB_BLANK_UNBLANK) {
		intensity = 0;
	}

	spin_lock_irqsave(&bl_lock, flags);
	
	PWM_CTRL0 = 2; /* pre-scaler */
	PWM_PWDUTY0 = intensity; /* duty cycle */
	PWM_PERVAL0 = 0x1f4; /* period */

	if (intensity) {
			pxa_set_cken(CKEN0_PWM0,1);
			hx2750_set_egpio(HX2750_EGPIO_LCD_PWR | HX2750_EGPIO_BL_PWR);
	} else {
			pxa_set_cken(CKEN0_PWM0,0);
			hx2750_clear_egpio(HX2750_EGPIO_LCD_PWR | HX2750_EGPIO_BL_PWR);
	}
	
	spin_unlock_irqrestore(&bl_lock, flags);
}

static void hx2750_bl_blank(int blank)
{
	switch(blank) {

	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		if (hx2750_bl_powermode == FB_BLANK_UNBLANK) {
			hx2750_set_intensity(0);
			hx2750_bl_powermode = blank;
		}
		break;
	case FB_BLANK_UNBLANK:
		if (hx2750_bl_powermode != FB_BLANK_UNBLANK) {
			hx2750_bl_powermode = blank;
			hx2750_set_intensity(current_intensity);
		}
		break;
	}
}

#ifdef CONFIG_PM
static int hx2750_bl_suspend(struct device *dev, u32 state, u32 level)
{
	if (level == SUSPEND_POWER_DOWN)
		hx2750_bl_blank(FB_BLANK_POWERDOWN);
	return 0;
}

static int hx2750_bl_resume(struct device *dev, u32 level)
{
	if (level == RESUME_POWER_ON)
		hx2750_bl_blank(FB_BLANK_UNBLANK);
	return 0;
}
#else
#define hx2750_bl_suspend	NULL
#define hx2750_bl_resume	NULL
#endif


static int hx2750_bl_set_power(struct backlight_device *bd, int state)
{
	hx2750_bl_blank(state);
	return 0;
}

static int hx2750_bl_get_power(struct backlight_device *bd)
{
	return hx2750_bl_powermode;
}

static int hx2750_bl_set_intensity(struct backlight_device *bd, int intensity)
{
	if (intensity > HX2750_MAX_INTENSITY)
		intensity = HX2750_MAX_INTENSITY;
	hx2750_set_intensity(intensity);
	current_intensity=intensity;
	return 0;
}

static int hx2750_bl_get_intensity(struct backlight_device *bd)
{
	return current_intensity;
}

static struct backlight_properties hx2750_bl_data = {
	.owner          = THIS_MODULE,
	.get_power      = hx2750_bl_get_power,
	.set_power      = hx2750_bl_set_power,
	.max_brightness = HX2750_MAX_INTENSITY,
	.get_brightness = hx2750_bl_get_intensity,
	.set_brightness = hx2750_bl_set_intensity,
};

static struct backlight_device *hx2750_backlight_device;

static int __init hx2750_bl_probe(struct device *dev)
{
	hx2750_backlight_device = backlight_device_register ("hx2750-bl",
		NULL, &hx2750_bl_data);
	if (IS_ERR (hx2750_backlight_device))
		return PTR_ERR (hx2750_backlight_device);

	hx2750_bl_powermode = FB_BLANK_UNBLANK;
	hx2750_bl_set_intensity(NULL, HX2750_DEFAULT_INTENSITY);

	pxa_set_cken(CKEN0_PWM0,1);

	printk("hx2750 Backlight Driver Initialized.\n");
	return 0;
}

static int hx2750_bl_remove(struct device *dev)
{
	backlight_device_unregister(hx2750_backlight_device);

	hx2750_bl_set_intensity(NULL, 0);

	printk("hx2750 Backlight Driver Unloaded\n");
	return 0;
}

static struct device_driver hx2750_bl_driver = {
	.name		= "hx2750-bl",
	.bus		= &platform_bus_type,
	.probe		= hx2750_bl_probe,
	.remove		= hx2750_bl_remove,
	.suspend	= hx2750_bl_suspend,
	.resume		= hx2750_bl_resume,
};

static int __init hx2750_bl_init(void)
{
	return driver_register(&hx2750_bl_driver);
}

static void __exit hx2750_bl_exit(void)
{
 	driver_unregister(&hx2750_bl_driver);
}

module_init(hx2750_bl_init);
module_exit(hx2750_bl_exit);

MODULE_AUTHOR("Richard Purdie <rpurdie@o-hand.com>");
MODULE_DESCRIPTION("iPAQ hx2750 Backlight Driver");
MODULE_LICENSE("GPLv2");
