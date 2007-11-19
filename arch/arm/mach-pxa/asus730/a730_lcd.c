/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/types.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <linux/platform_device.h>
#include <asm/arch/pxa-regs.h>  /* LCCR[0,1,2,3]* */
#include <asm/arch/bitfield.h>  /* for pxa-regs.h (Fld, etc) */
#include <asm/arch/pxafb.h>     /* pxafb_mach_info, set_pxa_fb_info */
#include <asm/mach-types.h>     /* machine_is_a730 */
#include <linux/lcd.h>          /* lcd_device */
#include <linux/backlight.h>    /* backlight_device */
#include <linux/fb.h>
#include <linux/lcd.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <asm/arch/asus730-gpio.h>
#include <asm/arch/pxa-regs.h>

extern void pca9535_write_output(u16);
extern u32 pca9535_read_input(void);

static int a730_lcd_get_power(struct lcd_device *lm)
{
    //return (pca9535_read_input() & 0x1);
    /*int i;
    int p = pca9535_read_input();*/
    int i, p = GET_A730_GPIO(LCD_EN);
    if ((p & 0x1)) i = 0;
    else if (!(p & 0x1)) i = 4;
    printk("%s: input=0x%x, i=0x%x\n", __FUNCTION__, p, i);
    return i;
}

static int a730_lcd_set_power(struct lcd_device *lm, int power)
{
	//u16 i = pca9535_read_input();
	printk("lcd power: %d\n", power);
	pxa_gpio_mode(GPIO_NR_A730_LCD_EN | GPIO_OUT);
	switch (power)
	{
		case FB_BLANK_UNBLANK://0
			//i |= 0x1;
			//pca9535_write_output(i);
			SET_A730_GPIO(LCD_EN, 0);
			break;
		case FB_BLANK_NORMAL://1
		case FB_BLANK_VSYNC_SUSPEND://2
		case FB_BLANK_HSYNC_SUSPEND://3
		case FB_BLANK_POWERDOWN://4
			//i &= ~0x1;
			SET_A730_GPIO(LCD_EN, 1);
			//pca9535_write_output(i);
			break;
	}
	
	return 0;
}

static struct lcd_ops a730_lcd_properties =
{
	.get_power      = a730_lcd_get_power,
	.set_power      = a730_lcd_set_power,
};

static struct lcd_device *a730_lcd_dev = NULL;

static int a730_lcd_probe(struct platform_device *pdev)
{
	//pxa_gpio_mode(GPIO_NR_A730_LCD_EN | GPIO_OUT);
	a730_lcd_dev = lcd_device_register("pxa2xx-fb", NULL, &a730_lcd_properties);
	
	if (IS_ERR(a730_lcd_dev)) {
		printk("a730-lcd: Error registering device\n");
		return -1;
	}

	return 0;
}

static int a730_lcd_remove(struct platform_device *pdev)
{
	a730_lcd_set_power(a730_lcd_dev, 4);
	lcd_device_unregister(a730_lcd_dev);
	return 0;
}

static int a730_lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	a730_lcd_set_power(a730_lcd_dev, 4);
	return 0;
}

static int a730_lcd_resume(struct platform_device *pdev)
{
	a730_lcd_set_power(a730_lcd_dev, 0);
	return 0;
}

/*********************************************/

static struct platform_driver a730_lcd_driver = {
	.driver = {
	    .name     = "a730-lcd",
	},
	.probe    = a730_lcd_probe,
	.remove   = a730_lcd_remove,
	.suspend  = a730_lcd_suspend,
	.resume   = a730_lcd_resume,
};

static int a730_lcd_init(void)
{
	if (!machine_is_a730()) return -ENODEV;

	return platform_driver_register(&a730_lcd_driver);
	
	return 0;
}

static void a730_lcd_exit(void)
{
	lcd_device_unregister(a730_lcd_dev);
	platform_driver_unregister(&a730_lcd_driver);
}

module_init(a730_lcd_init);
module_exit(a730_lcd_exit);

MODULE_AUTHOR("Serge Nikolaenko <mypal_hh@utl.ru>");
MODULE_DESCRIPTION("LCD handling for Asus MyPal A730(W)");
MODULE_LICENSE("GPL");
