/* e740_lcd.c
 *
 * (c) Ian Molton 2005
 *
 * This file contains the definitions for the LCD timings and functions
 * to control the LCD power / frontlighting via the w100fb driver.
 *
 */
#include <linux/platform_device.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/err.h>


#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-gpio.h>

#include <video/w100fb.h>

/* ------------------- backlight driver ------------------ */

static int current_brightness = 0;

static int e740_bl_update_status (struct backlight_device *bd)
{
	int intensity = bd->props.brightness;
	u32 gpio;

	if (bd->props.power != FB_BLANK_UNBLANK)
                intensity = 0;
        if (bd->props.fb_blank != FB_BLANK_UNBLANK)
                intensity = 0;

	current_brightness=intensity;

	gpio = w100fb_gpio_read(W100_GPIO_PORT_B);
	gpio &= ~0x31;
	if(intensity)
		gpio |= (((intensity-1) & 0x3) << 4) | 1;
	w100fb_gpio_write(W100_GPIO_PORT_B, gpio);

        return 0;
}

static int e740_bl_get_brightness (struct backlight_device *bl)
{
        return current_brightness;
}

static struct backlight_ops e740_bl_info = {
	.update_status  = e740_bl_update_status,
//        .max_brightness = 4,
        .get_brightness = e740_bl_get_brightness,
};

/* -------------------------- LCD driver -----------------------*/

static int lcd_power;

void __e740_lcd_set_power(int power)
{
	int gpio;
	gpio = w100fb_gpio_read(W100_GPIO_PORT_B);
	if(power)
        	gpio &= ~0xe;
	else
		gpio |= 0xe;
        w100fb_gpio_write(W100_GPIO_PORT_B, gpio);
}

static int e740_lcd_set_power (struct lcd_device *ld, int state)
{
	lcd_power = state;
	__e740_lcd_set_power(lcd_power);

	return 0;
}

static int e740_lcd_get_power (struct lcd_device *ld){
	return lcd_power;
}

static struct lcd_ops e740_lcd_info = {
        .get_power      = e740_lcd_get_power,
        .set_power      = e740_lcd_set_power,
};

/* ----------------------- device declarations -------------------------- */

#ifdef CONFIG_PM
static int e740_lcd_hook_suspend(struct platform_device *dev, pm_message_t state)
{
		__e740_lcd_set_power(1);
//		__e740_set_brightness(NULL, 0);
        return 0;
}

static int e740_lcd_hook_resume(struct platform_device *dev)
{
//                e740_bl_set_power(NULL, FB_BLANK_UNBLANK);
		__e740_lcd_set_power(lcd_power);
        return 0;
}
#endif


static struct backlight_device *e740_bl_device;
static struct lcd_device *e740_lcd_device;

static int e740_lcd_hook_probe (struct platform_device *dev) {
	e740_lcd_device = lcd_device_register("w100fb", NULL, &e740_lcd_info);
	e740_bl_device  = backlight_device_register("w100fb", NULL, NULL, &e740_bl_info);
	return 0;
}

static int e740_lcd_hook_remove(struct platform_device *dev)
{
        lcd_device_unregister(e740_lcd_device);
        backlight_device_unregister(e740_bl_device);
	return 0;
}

static struct platform_driver e740_lcd_hook_driver = {
	.driver = {
        	.name           = "e740-lcd-hook",
	},
        .probe          = e740_lcd_hook_probe,
        .remove         = e740_lcd_hook_remove,
#ifdef CONFIG_PM
        .suspend        = e740_lcd_hook_suspend,
        .resume         = e740_lcd_hook_resume,
#endif
};

static int e740_bl_init (void) {
        return platform_driver_register(&e740_lcd_hook_driver);
}

static void e740_bl_exit (void)
{
	platform_driver_unregister(&e740_lcd_hook_driver);
}

module_init (e740_bl_init);
module_exit (e740_bl_exit);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("e740 lcd driver");
MODULE_LICENSE("GPLv2");
