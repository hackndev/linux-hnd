/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * History:
 *
 * 2004-03-01   Eddi De Pieri      Adapted for h4000 using h3900_lcd.c 
 * 2004         Shawn Anderson     Lcd hacking on h4000
 * see h3900_lcd.c for more history.
 *
 */

#include <linux/types.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/dpm.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>

#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/arch/pxafb.h>     /* pxafb_mach_info, set_pxa_fb_info */
#include <asm/mach-types.h>     /* machine_is_h4000 */

#include <asm/arch/h4000.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>

extern struct platform_device h4000_asic3;

static int h4000_lcd_get_power(struct lcd_device *lm)
{
	if (asic3_get_gpio_status_b(&h4000_asic3.dev) & GPIOB_LCD_PCI) {
		if (asic3_get_gpio_out_b(&h4000_asic3.dev) & GPIOB_LCD_ON)
			return FB_BLANK_UNBLANK;
		else
			return FB_BLANK_VSYNC_SUSPEND;
	} else
		return FB_BLANK_POWERDOWN;
}

static int h4000_lcd_set_power(struct lcd_device *lm, int power)
{
	int i;

	if (power == FB_BLANK_UNBLANK) {
		DPM_DEBUG("h4000_lcd: Turning on\n");
		asic3_set_gpio_dir_d(&h4000_asic3.dev, GPIOD_PCO, 0);

		asic3_set_gpio_out_c(&h4000_asic3.dev,
				GPIOC_LCD_3V3_ON, GPIOC_LCD_3V3_ON);
		mdelay(25);
		asic3_set_gpio_out_c(&h4000_asic3.dev,
				GPIOC_LCD_5V_EN, GPIOC_LCD_5V_EN);
		mdelay(5);

		asic3_set_gpio_out_d(&h4000_asic3.dev, 
				GPIOD_LCD_RESET_N, GPIOD_LCD_RESET_N);
		mdelay(5);

		asic3_set_gpio_out_d(&h4000_asic3.dev,
				GPIOD_PCI, GPIOD_PCI);
		mdelay(20);

		for (i = 0; i < 20; i++) {
			if (asic3_get_gpio_status_d(&h4000_asic3.dev) & GPIOD_PCO)
				break;
			mdelay(20);
		}

		printk("LCD initialization delay: %d\n", i);

		asic3_set_gpio_out_b(&h4000_asic3.dev,
				GPIOB_LCD_PCI, GPIOB_LCD_PCI);
		mdelay(7);

		asic3_set_gpio_out_b(&h4000_asic3.dev,
				GPIOB_LCD_ON, GPIOB_LCD_ON);
		mdelay(7);

		asic3_set_gpio_out_c(&h4000_asic3.dev, 
				GPIOC_LCD_N3V_EN, GPIOC_LCD_N3V_EN);
		mdelay(2);

	} else {
		DPM_DEBUG("h4000_lcd: Turning off\n");
		asic3_set_gpio_out_d(&h4000_asic3.dev,
				GPIOD_PCI, 0);
		mdelay(10 + 60);

		for (i = 0; i < 20; i++) {
			if (!(asic3_get_gpio_status_d(&h4000_asic3.dev) & GPIOD_PCO))
				break;
			mdelay(20);
		}
		printk("LCD deinitialization delay: %d\n", i);

		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_LCD_N3V_EN, 0);
		mdelay(25);

		asic3_set_gpio_out_b(&h4000_asic3.dev, GPIOB_LCD_ON, 0);
		mdelay(25);

		asic3_set_gpio_out_b(&h4000_asic3.dev, GPIOB_LCD_PCI, 0);
		mdelay(2);
		
		asic3_set_gpio_out_d(&h4000_asic3.dev, 
				GPIOD_LCD_RESET_N, 0);
		mdelay(2 + 5);
		
		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_LCD_5V_EN, 0);
		mdelay(25);

		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_LCD_3V3_ON, 0);
		mdelay(10);

		asic3_set_gpio_dir_d(&h4000_asic3.dev, GPIOD_PCO, GPIOD_PCO);

		asic3_set_gpio_out_d(&h4000_asic3.dev,
				GPIOD_PCO, 0);
	}

	return 0;
}

static struct lcd_ops h4000_lcd_properties =
{
	.get_power      = h4000_lcd_get_power,
	.set_power      = h4000_lcd_set_power,
};

static struct lcd_device *h4000_lcd_dev;

static int h4000_lcd_probe(struct platform_device *pdev)
{
	h4000_lcd_dev = lcd_device_register("pxa2xx-fb", NULL,
			&h4000_lcd_properties);
	if (IS_ERR(h4000_lcd_dev)) {
		printk("h4000-lcd: Error registering device\n");
		return -1;
	}

	return 0;
}

static int h4000_lcd_remove(struct platform_device *pdev)
{
	h4000_lcd_set_power(h4000_lcd_dev, FB_BLANK_POWERDOWN);
	lcd_device_unregister(h4000_lcd_dev);

	return 0;
}

static int h4000_lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	h4000_lcd_set_power(h4000_lcd_dev, FB_BLANK_POWERDOWN);
	return 0;
}

static int h4000_lcd_resume(struct platform_device *pdev)
{
	h4000_lcd_set_power(h4000_lcd_dev, FB_BLANK_UNBLANK);
	return 0;
}

static struct platform_driver h4000_lcd_driver = {
	.driver	  = {
	    .name = "h4000-lcd",
	},
	.probe    = h4000_lcd_probe,
	.remove   = h4000_lcd_remove,
	.suspend  = h4000_lcd_suspend,
	.resume   = h4000_lcd_resume,
};

static int h4000_lcd_init(void)
{
	if (!h4000_machine_is_h4000())
		return -ENODEV;

	return platform_driver_register(&h4000_lcd_driver);
}

static void h4000_lcd_exit(void)
{
	platform_driver_unregister(&h4000_lcd_driver);
}

module_init(h4000_lcd_init);
module_exit(h4000_lcd_exit);

MODULE_AUTHOR("h4000 port team h4100-port@handhelds.org");
MODULE_DESCRIPTION("LCD driver for iPAQ H4000");
MODULE_LICENSE("GPL");
