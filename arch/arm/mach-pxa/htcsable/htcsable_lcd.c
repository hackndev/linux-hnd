/*
 * HTC Sable (ipaq hw6915) LCD driver
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright not listed but probably the people below at least.
 * Copyright (C) 2006 Luke Kenneth Casson Leighton (lkcl@lkcl.net)
 *
 * History:
 *
 * 2004-03-01   Eddi De Pieri      Adapted for h4000 using h3900_lcd.c 
 * 2004         Shawn Anderson     Lcd hacking on h4000
 * see h3900_lcd.c for more history.
 * 2006nov26    lkcl               Adapted for htc sable
 *
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/arch/bitfield.h>  /* for pxa-regs.h (Fld, etc) */
#include <asm/arch/pxafb.h>     /* pxafb_mach_info, set_pxa_fb_info */
#include <asm/mach-types.h>     /* machine_is_hw6900 */
#include <linux/lcd.h>          /* lcd_device */
#include <linux/backlight.h>    /* backlight_device */
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

static int saved_lcdpower = -1;

static int htcsable_lcd_set_power(struct lcd_device *lm, int power)
{
	 /* Enable or disable power to the LCD (0: on; 4: off) */

	saved_lcdpower = power;

	if ( power < 1 ) {
#if 0
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
				GPIOD_LCD_BACKLIGHT, GPIOD_LCD_BACKLIGHT);
		pxa_set_cken(CKEN0_PWM0, 1); /* this is backlight */
		mdelay(15);
#endif
		pxa_set_cken(CKEN16_LCD, 1);
		mdelay(50);
		asic3_set_gpio_out_a(&htcsable_asic3.dev,
				GPIOA_LCD_PWR_5, GPIOA_LCD_PWR_5);
		mdelay(15);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 
				GPIOC_LCD_PWR_1, GPIOC_LCD_PWR_1);
		mdelay(15);
		asic3_set_gpio_out_c(&htcsable_asic3.dev,
				GPIOC_LCD_PWR_2, GPIOC_LCD_PWR_2);

	} else {
#if 0
		asic3_set_gpio_out_d(&htcsable_asic3.dev,
				GPIOD_LCD_BACKLIGHT, 0);
		pxa_set_cken(CKEN0_PWM0, 0); /* this is backlight */
#endif
		mdelay(15);
		pxa_set_cken(CKEN16_LCD, 0);
		mdelay(50);
		asic3_set_gpio_out_a(&htcsable_asic3.dev,
				GPIOA_LCD_PWR_5, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 
				GPIOC_LCD_PWR_1, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&htcsable_asic3.dev,
				GPIOC_LCD_PWR_2, 0);
	}

	return 0;
}

static int htcsable_lcd_get_power(struct lcd_device *lm)
{
	if (saved_lcdpower == -1)
	{
		htcsable_lcd_set_power(lm, 4);
		saved_lcdpower = 4;
	}

	return saved_lcdpower;
}

static struct lcd_ops htcsable_lcd_properties =
{
	.get_power      = htcsable_lcd_get_power,
	.set_power      = htcsable_lcd_set_power,
};

static struct lcd_device *htcsable_lcd_dev;

static int htcsable_lcd_probe(struct platform_device * dev)
{
	htcsable_lcd_dev = lcd_device_register("pxa2xx-fb", NULL,
			&htcsable_lcd_properties);
	if (IS_ERR(htcsable_lcd_dev)) {
		printk("htcsable-lcd: Error registering device\n");
		return -1;
	}

	htcsable_lcd_set_power(htcsable_lcd_dev, 0);
	return 0;
}

static int htcsable_lcd_remove(struct platform_device * dev)
{
	htcsable_lcd_set_power(htcsable_lcd_dev, 4);
	lcd_device_unregister(htcsable_lcd_dev);

	return 0;
}

static int htcsable_lcd_suspend(struct platform_device * dev, pm_message_t state)
{
	htcsable_lcd_set_power(htcsable_lcd_dev, 4);
	return 0;
}

static int htcsable_lcd_resume(struct platform_device * dev)
{
	htcsable_lcd_set_power(htcsable_lcd_dev, 0);
	return 0;
}

static struct platform_driver htcsable_lcd_driver = {
	.driver = {
		.name     = "htcsable_lcd",
	},
	.probe    = htcsable_lcd_probe,
	.remove   = htcsable_lcd_remove,
	.suspend  = htcsable_lcd_suspend,
	.resume   = htcsable_lcd_resume,
};

static int htcsable_lcd_init(void)
{
	if (!machine_is_hw6900())
		return -ENODEV;

	return platform_driver_register(&htcsable_lcd_driver);
}

static void htcsable_lcd_exit(void)
{
	lcd_device_unregister(htcsable_lcd_dev);
	platform_driver_unregister(&htcsable_lcd_driver);
}

module_init(htcsable_lcd_init);
module_exit(htcsable_lcd_exit);

MODULE_AUTHOR("h4000 port team h4100-port@handhelds.org; Luke Kenneth Casson Leighton");
MODULE_DESCRIPTION("LCD driver for iPAQ hw6915");
MODULE_LICENSE("GPL");

/* vim: set ts=8 tw=80 shiftwidth=8 noet: */

