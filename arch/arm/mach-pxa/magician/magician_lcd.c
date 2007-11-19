/*
 * LCD driver for HTC Magician
 *
 * Copyright (C) 2006, Philipp Zabel
 *
 * based on previous work by
 *	Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>,
 *	Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>

#include <asm/arch/magician.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>

static int lcd_select;

static struct pxafb_mode_info magician_pxafb_modes_samsung[] = {
{
	.pixclock	= 96153,
	.bpp		= 16,
	.xres		= 240,
	.yres		= 320,
	.hsync_len	= 4,
	.vsync_len	= 1,
	.left_margin	= 20,
	.upper_margin	= 7,
	.right_margin	= 8,
	.lower_margin	= 8,
	.sync		= 0,
},
};

static struct pxafb_mode_info magician_pxafb_modes_toppoly[] = {
{
	.pixclock	= 96153,
	.bpp		= 16,
	.xres		= 240,
	.yres		= 320,
	.hsync_len	= 11,
	.vsync_len	= 3,
	.left_margin	= 19,
	.upper_margin	= 2,
	.right_margin	= 10,
	.lower_margin	= 2,
	.sync		= 0,
},
};

static void samsung_lcd_power(int on, struct fb_var_screeninfo *si);
static void toppoly_lcd_power(int on, struct fb_var_screeninfo *si);

/* Samsung LTP280QV */
static struct pxafb_mach_info magician_fb_info_samsung = {
	.modes		= magician_pxafb_modes_samsung,
	.num_modes	= 1,
	.fixed_modes	= 1,
	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_PixFlEdg,
	.pxafb_backlight_power = NULL,
	.pxafb_lcd_power = samsung_lcd_power,
};

/* Toppoly TD028STEB1 */
static struct pxafb_mach_info magician_fb_info_toppoly = {
	.modes		= magician_pxafb_modes_toppoly,
	.num_modes	= 1,
	.fixed_modes	= 1,
	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_PixRsEdg,
	.pxafb_backlight_power = NULL,
	.pxafb_lcd_power = toppoly_lcd_power,
};

static void samsung_lcd_power(int on, struct fb_var_screeninfo *si)
{
	/* FIXME: implement samsung power switching */
	printk("samsung LCD power o%s\n", on ? "n" : "ff");
	return;
}

static void toppoly_lcd_power(int on, struct fb_var_screeninfo *si)
{
	printk("toppoly LCD power o%s\n", on ? "n" : "ff");

	if (on) {
		printk(KERN_DEBUG "toppoly power on\n");
		/*
		pxa_gpio_mode(GPIO74_LCD_FCLK_MD);
		pxa_gpio_mode(GPIO75_LCD_LCLK_MD); // done by pxafb
		*/
		gpio_set_value(EGPIO_MAGICIAN_TOPPOLY_POWER, 1);
		pxa_gpio_mode(GPIO104_MAGICIAN_LCD_POWER_1_MD);
		pxa_gpio_mode(GPIO104_MAGICIAN_LCD_POWER_1_MD);
		pxa_gpio_mode(GPIO104_MAGICIAN_LCD_POWER_1_MD);

		gpio_set_value(GPIO106_MAGICIAN_LCD_POWER_3, 1);
		udelay(2000);
		gpio_set_value(EGPIO_MAGICIAN_LCD_POWER, 1);
		udelay(2000);
		/* LCCR0 = 0x04000081; // <-- already done by pxafb_enable_controller */
		udelay(2000);
		gpio_set_value(GPIO104_MAGICIAN_LCD_POWER_1, 1);
		udelay(2000);
		gpio_set_value(GPIO105_MAGICIAN_LCD_POWER_2, 1);
	} else {
		printk(KERN_DEBUG "toppoly power off\n");
		/* LCCR0 = 0x00000000; // <-- will be done by pxafb_disable_controller afterwards?! */
		mdelay(15);
		gpio_set_value(GPIO105_MAGICIAN_LCD_POWER_2, 0);
		udelay(500);
		gpio_set_value(GPIO104_MAGICIAN_LCD_POWER_1, 0);
		udelay(1000);
		gpio_set_value(GPIO106_MAGICIAN_LCD_POWER_3, 0);
		gpio_set_value(EGPIO_MAGICIAN_LCD_POWER, 0);
	}
}

static int magician_lcd_probe(struct platform_device *dev)
{
	volatile u8 *p;

	printk(KERN_NOTICE "HTC Magician LCD driver\n");

	/* Check which LCD we have */
	p = (volatile u8 *)ioremap_nocache(PXA_CS3_PHYS, 0x1000);
	if (p) {
		lcd_select = p[0x14] & 0x8;
		printk("magician_lcd: lcd_select = %d (%s)\n", lcd_select,
		       lcd_select ? "samsung" : "toppoly");
		iounmap((void *)p);
	} else
		printk("magician_lcd: lcd_select ioremap failed\n");

	if (lcd_select)
		set_pxa_fb_info(&magician_fb_info_samsung);
	else
		set_pxa_fb_info(&magician_fb_info_toppoly);
	return 0;
}

static int magician_lcd_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_PM
static int magician_lcd_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int magician_lcd_resume(struct platform_device *dev)
{
	return 0;
}
#endif

static struct platform_driver magician_lcd_driver = {
	.driver = {
		.name = "magician-lcd",
	},
	.probe = magician_lcd_probe,
	.remove = magician_lcd_remove,
#ifdef CONFIG_PM
	.suspend = magician_lcd_suspend,
	.resume = magician_lcd_resume,
#endif
};

static __init int magician_lcd_init(void)
{
	if (!machine_is_magician())
		return -ENODEV;

	return platform_driver_register(&magician_lcd_driver);
}

static __exit void magician_lcd_exit(void)
{
	platform_driver_unregister(&magician_lcd_driver);
}

module_init(magician_lcd_init);
module_exit(magician_lcd_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("HTC Magician LCD driver");
MODULE_LICENSE("GPL");
