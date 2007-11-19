/* 
 * Copyright 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Backlight driver for HP iPAQ RX3000
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <linux/mfd/asic3_base.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-timer.h>
#include <asm/arch/rx3000-asic3.h>

#include <asm/arch/lcd.h>

#include <asm/plat-s3c24xx/devs.h>

extern struct platform_device s3c_device_asic3;

static void rx3000_backlight_power(int on)
{
	s3c2410_gpio_setpin(S3C2410_GPB0, 0);
	s3c2410_gpio_cfgpin(S3C2410_GPB0,
			    (on) ? S3C2410_GPB0_TOUT0 : S3C2410_GPB0_OUTP);
}

static void rx3000_set_brightness(int tcmpb0)
{
	unsigned long tcfg0;
	unsigned long tcfg1;
	unsigned long tcon;

	tcfg0 = readl(S3C2410_TCFG0);
	tcfg1 = readl(S3C2410_TCFG1);

	tcfg0 &= ~S3C2410_TCFG_PRESCALER0_MASK;
	tcfg0 |= 0x0;

	tcfg1 &= ~S3C2410_TCFG1_MUX0_MASK;
	tcfg1 |= S3C2410_TCFG1_MUX0_DIV2;

	writel(tcfg0, S3C2410_TCFG0);
	writel(tcfg1, S3C2410_TCFG1);
	writel(0x3e8, S3C2410_TCNTB(0));

	tcon = readl(S3C2410_TCON);
	tcon &= ~0x0F;
	tcon |= S3C2410_TCON_T0RELOAD;
	tcon |= S3C2410_TCON_T0MANUALUPD;
	writel(tcon, S3C2410_TCON);

	writel(0x3e8, S3C2410_TCNTB(0));
	writel(tcmpb0, S3C2410_TCMPB(0));

	/* start the timer running */
	tcon |= S3C2410_TCON_T0START;
	tcon &= ~S3C2410_TCON_T0MANUALUPD;
	writel(tcon, S3C2410_TCON);
}

static void rx3000_lcd_power(int on)
{
	if (on) {
    		s3c2410_gpio_cfgpin(S3C2410_GPC1, S3C2410_GPC1_VCLK);
		s3c2410_gpio_cfgpin(S3C2410_GPC4, S3C2410_GPC4_VM);
		mdelay(10);
		asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC10, ASIC3_GPC10);
		asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC9, ASIC3_GPC9);
		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB12, ASIC3_GPB12);
		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB11, ASIC3_GPB11);
		mdelay(10);
		s3c2410_gpio_cfgpin(S3C2410_GPB0, S3C2410_GPB0_TOUT0);
		s3c2410_gpio_cfgpin(S3C2410_GPB1, S3C2410_GPB1_TOUT1);
	} else {
		s3c2410_gpio_cfgpin(S3C2410_GPB0, S3C2410_GPB0_OUTP);
		s3c2410_gpio_setpin(S3C2410_GPB0, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPB1, S3C2410_GPB1_OUTP);
		s3c2410_gpio_setpin(S3C2410_GPB1, 0);
		s3c2410_gpio_setpin(S3C2410_GPB6, 0);
		s3c2410_gpio_setpin(S3C2410_GPC0, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC9, 0);
		mdelay(15);
		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB12, 0);
		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB11, 0);
		mdelay(15);
		s3c2410_gpio_cfgpin(S3C2410_GPC1, S3C2410_GPC1_OUTP);
		s3c2410_gpio_cfgpin(S3C2410_GPC4, S3C2410_GPC4_OUTP);
		s3c2410_gpio_setpin(S3C2410_GPC1, 0);
		s3c2410_gpio_setpin(S3C2410_GPC4, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC10, 0);
		s3c2410_gpio_setpin(S3C2410_GPC5, 0);
	}
}

static struct s3c2410_bl_mach_info rx3000_blcfg __initdata = {

	.backlight_max          = 0x384,
	.backlight_default      = 0x1c2,
	.backlight_power	= rx3000_backlight_power,
	.set_brightness		= rx3000_set_brightness,
	.lcd_power		= rx3000_lcd_power,
};

static int rx3000_bl_probe(struct platform_device *pdev)
{
        set_s3c2410bl_info(&rx3000_blcfg);

        platform_device_register(&s3c_device_bl);
        return 0;
}

static int rx3000_bl_remove(struct platform_device *pdev)
{
        platform_device_unregister(&s3c_device_bl);
        return 0;
}

static struct platform_driver rx3000_bl_driver = {
        .driver         = {
                .name   = "rx3000-bl",
        },
        .probe          = rx3000_bl_probe,
        .remove         = rx3000_bl_remove,
};

static int __init rx3000_bl_init(void)
{
        platform_driver_register(&rx3000_bl_driver);
        return 0;
}

static void __exit rx3000_bl_exit(void)
{
        platform_driver_unregister(&rx3000_bl_driver);
}

module_init(rx3000_bl_init);
module_exit(rx3000_bl_exit);

MODULE_AUTHOR("Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("Backlight driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
