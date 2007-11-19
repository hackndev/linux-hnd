/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * History:
 *
 * 2004-03-01   Eddi De Pieri      Adapted for htcuniversal using h3900_lcd.c 
 * 2004         Shawn Anderson     Lcd hacking on htcuniversal
 * see h3900_lcd.c for more history.
 *
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/arch/bitfield.h>  /* for pxa-regs.h (Fld, etc) */
#include <asm/arch/pxafb.h>     /* pxafb_mach_info, set_pxa_fb_info */
#include <asm/mach-types.h>     /* machine_is_htcuniversal */
#include <linux/lcd.h>          /* lcd_device */
#include <linux/err.h>
#include <linux/delay.h>

#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

static int saved_lcdpower=-1;

static int powerup_lcd(void)
{
	printk( KERN_INFO "htcuniversal powerup_lcd: called\n");

		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR1_ON, 0);
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR2_ON, 0);
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 0);
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_LCD_PWR4_ON, 0);
		asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_LCD_PWR5_ON, 0);
#if 1
		LCCR4|=LCCR4_PCDDIV;
#endif
		pxa_set_cken(CKEN16_LCD, 0);

		mdelay(100);
		asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_LCD_PWR5_ON, 1<<GPIOA_LCD_PWR5_ON);
		mdelay(5);
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 1<<GPIOB_LCD_PWR3_ON);
		mdelay(2);
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR1_ON, 1<<GPIOC_LCD_PWR1_ON);
		mdelay(2);
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR2_ON, 1<<GPIOC_LCD_PWR2_ON);
		mdelay(20);
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_LCD_PWR4_ON, 1<<GPIOD_LCD_PWR4_ON);
		mdelay(1);
		pxa_set_cken(CKEN16_LCD, 1);

		SET_HTCUNIVERSAL_GPIO(LCD1,1);
		SET_HTCUNIVERSAL_GPIO(LCD2,1);
 return 0;
}

static int powerdown_lcd(void)
{
	printk( KERN_INFO "htcuniversal powerdown_lcd: called\n");

#if 1
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR2_ON, 0);
		mdelay(100);
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_LCD_PWR4_ON, 0);
		mdelay(10);
		asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_LCD_PWR5_ON, 0);
		mdelay(1);
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 0);
		mdelay(1);
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR1_ON, 0);
		pxa_set_cken(CKEN16_LCD, 0);

		SET_HTCUNIVERSAL_GPIO(LCD1,0);
		SET_HTCUNIVERSAL_GPIO(LCD2,0);
#else
		pxa_set_cken(CKEN16_LCD, 0);

		SET_HTCUNIVERSAL_GPIO(LCD1,0);
		SET_HTCUNIVERSAL_GPIO(LCD2,0);

		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR2_ON, 0);
		mdelay(100);
		asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_LCD_PWR4_ON, 0);
		mdelay(10);
		asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_LCD_PWR5_ON, 0);
		mdelay(1);
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 0);
		mdelay(1);
		asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_LCD_PWR1_ON, 0);
#endif
 return 0;
}

static int htcuniversal_lcd_set_power(struct lcd_device *lm, int power)
{
	 /* Enable or disable power to the LCD (0: on; 4: off) */

	if ( power < 1 ) {
	
	powerup_lcd();

	} else {
	
	powerdown_lcd();

	}

	saved_lcdpower=power;	

	return 0;
}

static int htcuniversal_lcd_get_power(struct lcd_device *lm)
{
	/* Get the LCD panel power status (0: full on, 1..3: controller
	 * power on, flat panel power off, 4: full off) */

	if (saved_lcdpower == -1) 
	{
	 htcuniversal_lcd_set_power(lm, 4);
	 saved_lcdpower=4;
	}

 return saved_lcdpower;
}

static struct lcd_ops htcuniversal_lcd_properties =
{
	.get_power      = htcuniversal_lcd_get_power,
	.set_power      = htcuniversal_lcd_set_power,
};

static struct lcd_device *htcuniversal_lcd_dev;

static int htcuniversal_lcd_probe(struct platform_device * dev)
{
	htcuniversal_lcd_dev = lcd_device_register("pxa2xx-fb", NULL,
			&htcuniversal_lcd_properties);
	if (IS_ERR(htcuniversal_lcd_dev)) {
		printk("htcuniversal_lcd_probe: error registering devices\n");
		return -1;
	}

	return 0;
}

static int htcuniversal_lcd_remove(struct platform_device * dev)
{
	htcuniversal_lcd_set_power(htcuniversal_lcd_dev, 4);
	lcd_device_unregister(htcuniversal_lcd_dev);

	return 0;
}

static int htcuniversal_lcd_suspend(struct platform_device * dev, pm_message_t state)
{
//	printk("htcuniversal_lcd_suspend: called.\n");
	htcuniversal_lcd_set_power(htcuniversal_lcd_dev, 4);
	return 0;
}

static int htcuniversal_lcd_resume(struct platform_device * dev)
{
//	printk("htcuniversal_lcd_resume: called.\n");

	/* */
#if 1
	LCCR4|=LCCR4_PCDDIV;
#endif

	htcuniversal_lcd_set_power(htcuniversal_lcd_dev, 0);
	return 0;
}

static struct platform_driver htcuniversal_lcd_driver = {
	.driver = {
		.name     = "htcuniversal_lcd",
	},
	.probe    = htcuniversal_lcd_probe,
	.remove   = htcuniversal_lcd_remove,
	.suspend  = htcuniversal_lcd_suspend,
	.resume   = htcuniversal_lcd_resume,
};

static int htcuniversal_lcd_init(void)
{
	if (!machine_is_htcuniversal())
		return -ENODEV;

	return platform_driver_register(&htcuniversal_lcd_driver);
}

static void htcuniversal_lcd_exit(void)
{
	lcd_device_unregister(htcuniversal_lcd_dev);
	platform_driver_unregister(&htcuniversal_lcd_driver);
}

module_init(htcuniversal_lcd_init);
module_exit(htcuniversal_lcd_exit);

MODULE_AUTHOR("xanadux.org");
MODULE_DESCRIPTION("Framebuffer driver for HTC Universal");
MODULE_LICENSE("GPL");

