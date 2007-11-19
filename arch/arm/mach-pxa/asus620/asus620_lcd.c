/*
 *  linux/arch/arm/mach-pxa/asus620_lcd.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2003 Adam Turowski
 *  Copyright(C) 2004 Vitaliy Sardyko
 *
 *  2003-12-03: Adam Turowski
 *		initial code.
 *  2003-23-09: Nicolas Pouillon
 * 		Added TCON detection code
 *  2004-11-07: Vitaliy Sardyko
 *		updated to 2.6.7
 *  2006-09-01: Vincent Benony
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <asm/hardware.h>

#include <linux/fb.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus620-gpio.h>
#include <asm/mach-types.h>

#define DEBUG 1

#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

static int has_tcon;
static int lcd_power = 0;

static int asus620_lcd_set_power (struct lcd_device *lm, int setp)
{
	DPRINTK("pwer : %d\n", setp);
	lcd_power = setp;
	/* It's a bit hacky, but this is the only func called after reinit of gpios */
//	pxa_gpio_mode (GPIO_NR_A620_TCON_HERE_N|GPIO_IN);
//	pxa_gpio_mode (GPIO_NR_A620_TCON_EN|(has_tcon?GPIO_ALT_FN_2_OUT:GPIO_OUT));

	switch (setp) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
//			asus620_gpo_set(GPO_A620_LCD_ENABLE);
			asus620_gpo_set(GPIO_NR_A620_TCON_HERE_N);
			asus620_gpo_set(GPO_A620_LCD_POWER3);
			mdelay(30);
			asus620_gpo_set(GPO_A620_LCD_POWER1);
			mdelay(30);
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
                        break;
		case FB_BLANK_POWERDOWN:
			asus620_gpo_clear(GPO_A620_LCD_POWER1);
			mdelay(65);
			asus620_gpo_clear(GPO_A620_LCD_POWER3);
//			asus620_gpo_clear(GPO_A620_LCD_ENABLE);
			asus620_gpo_clear(GPIO_NR_A620_TCON_HERE_N);
			break;
	}

	return 0;
}

static int asus620_lcd_get_power (struct lcd_device *lm)
{
	return lcd_power;
} 

struct lcd_ops asus620_lcd_properties =
{
	.set_power = asus620_lcd_set_power,
	.get_power = asus620_lcd_get_power
}; 
/*
  Adam's asus620: (probably with TCON)
 LCCR0: 0x003000f9 -- ENB | LDM | SFM | IUM | EFM | PAS | BM | OUM
 LCCR1: 0x15150cef -- PPL=0xef, HSW=0x3, ELW=0x15, BLW=0x15
 LCCR2: 0x06060d3f -- LPP=0x13f, VSW=0x3, EFW=0x6, BFW=0x6
 LCCR3: 0x04700007 -- PCD=0x7, ACB=0x0, API=0x0, VSP, HSP, PCP, BPP=0x4

  Nipo's asus620: (No TCON)
 LCCR0: 0x001000f9 -- ENB | LDM | SFM | IUM | EFM | PAS | BM
 LCCR1: 0x0a0afcef -- PPL=0xef, HSW=0xfc, ELW=0x0a, BLW=0x0a
 LCCR2: 0x0303153f -- LPP=0x13f, VSW=0x3, EFW=0x3, BFW=0x3
 LCCR3: 0x04700007 -- PCD=0x7, ACB=0x0, API=0x0, VSP, HSP, PCP, BPP=0x4 
*/

static struct pxafb_mode_info asus620_mode_tcon = {
        .pixclock =     171521,
        .bpp =          16,
        .xres =         240,
        .yres =         320,
        .hsync_len =    4,
        .vsync_len =    4,
        .left_margin =  22,
        .upper_margin = 6,
        .right_margin = 22,
        .lower_margin = 6,
        .sync =         0,
};

static struct pxafb_mach_info asus620_fb_info_tcon = {
	.modes = &asus620_mode_tcon,
	.num_modes = 1,
        .lccr0 =        ( LCCR0_LDM | LCCR0_SFM | LCCR0_IUM | LCCR0_EFM |
						  LCCR0_PAS | LCCR0_QDM | LCCR0_BM | LCCR0_OUM ),
        .lccr3 =        ( LCCR3_HorSnchL | LCCR3_VrtSnchL | LCCR3_16BPP |
						  LCCR3_PCP ),
};

static struct pxafb_mode_info asus620_mode_notcon = {
	.pixclock =	171521,
	.bpp =		16,
	.xres =		240,
	.yres =		320,
	.hsync_len =	252,
	.vsync_len =	4,
	.left_margin =	11,
	.upper_margin =	4,
	.right_margin =	11,
	.lower_margin =	4,
	.sync =		0,
};

static struct pxafb_mach_info asus620_fb_info_notcon = {
	.modes = &asus620_mode_notcon,
	.num_modes = 1,
	.lccr0 =	( LCCR0_ENB | LCCR0_LDM | LCCR0_SFM | LCCR0_IUM | LCCR0_EFM |
				  LCCR0_PAS | LCCR0_BM ),
	.lccr3 =	( LCCR3_HorSnchL | LCCR3_VrtSnchL | LCCR3_16BPP | LCCR3_PCP ),
};

static struct lcd_device *pxafb_lcd_device;

int asus620_lcd_init (void)
{
	void *info;

	if (!machine_is_a620())
		return -ENODEV;

	printk( "Registering Asus A620 FrameBuffer device\n");

	pxa_gpio_mode (GPIO_NR_A620_TCON_HERE_N|GPIO_IN);
	has_tcon = GET_A620_GPIO(TCON_HERE_N);

	if (has_tcon)
	{
		info = &asus620_fb_info_tcon;
		pxa_gpio_mode (GPIO_NR_A620_TCON_EN|GPIO_ALT_FN_2_OUT);
		printk(" TCON here\n");
	}
	else
	{
		info = &asus620_fb_info_notcon;
		pxa_gpio_mode (GPIO_NR_A620_TCON_EN|GPIO_OUT);
		printk(" TCON absent\n");
	}
	
	pxafb_lcd_device = lcd_device_register("pxafb", info, &asus620_lcd_properties);

	if (IS_ERR(pxafb_lcd_device))
	{
		printk(" LCD device registering failed !\n");
		return PTR_ERR(pxafb_lcd_device);
	}

	set_pxa_fb_info(info);

	printk(" LCD and backlight devices successfully registered\n");
	return 0;
}

static void asus620_lcd_exit (void)
{
	lcd_device_unregister (pxafb_lcd_device);
}

module_init(asus620_lcd_init);
module_exit(asus620_lcd_exit);

MODULE_AUTHOR("Adam Turowski, Nicolas Pouillon, Vincent Benony");
MODULE_DESCRIPTION("LCD driver for Asus 620");
MODULE_LICENSE("GPL");
