/*
 * Palm Treo 650 LEDs
 *
 * TODO: Change keypad into a backlight device so brightness can be varied.
 * 
 * Author: Alex Osborne <bobofdoom@gmail.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch/palmt650-gpio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

#define ASIC6_LED_0_Base 0x80
#define ASIC6_LED_1_Base 0x90
#define ASIC6_LED_2_Base 0xA0

#define ASIC6_LED_TimeBase	0x00
#define ASIC6_LED_PeriodTime	0x04 /* might be wrong */
#define ASIC6_LED_DutyTime	0x08 /* might be wrong */
#define ASIC6_LED_AutoStopCount	0x0c

/* LED TimeBase bits */
#include <asm/hardware/ipaq-asic2.h>

static void *asic6_base;

static void asic6_write(u32 reg, u16 val)
{
	*((volatile u16*)((u32)asic6_base+reg)) = val;
}

static u16 asic6_read(u32 reg)
{
	return *((volatile u16*)((u32)asic6_base+reg));
}

static void asic6_led_set(u32 base, enum led_brightness value)
{
	/* we want simply full-on / full-off for now, so clear all the other
	 * registers 
	 */
	asic6_write(base | 0x2, 0x0);
	asic6_write(base | 0x6, 0x0);
	asic6_write(base | ASIC6_LED_PeriodTime, 0x0);
	asic6_write(base | ASIC6_LED_AutoStopCount, 0x0);
	asic6_write(base | ASIC6_LED_DutyTime, 0x1);
	asic6_write(base | ASIC6_LED_TimeBase, value ? 0x10 : 0);
}

static void palmt650_led_green_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	asic6_led_set(ASIC6_LED_0_Base, value);
}

static void palmt650_led_red_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	asic6_led_set(ASIC6_LED_1_Base, value);
}

static void palmt650_led_keypad_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	asic6_led_set(ASIC6_LED_2_Base, value);
}

static void palmt650_led_vibra_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	SET_PALMT650_GPIO(VIBRATE_EN, value);
}

static struct led_classdev palmt650_green_led = {
	.name			= "palmt650:green",
	.brightness_set		= palmt650_led_green_set,
};

static struct led_classdev palmt650_red_led = {
	.name			= "palmt650:red",
	.brightness_set		= palmt650_led_red_set,
};

static struct led_classdev palmt650_keypad_led = {
	.name			= "palmt650:keypad",
	.brightness_set		= palmt650_led_keypad_set,
};

static struct led_classdev palmt650_vibra_led = {
	.name			= "palmt650:vibra",
	.brightness_set		= palmt650_led_vibra_set,
};

#ifdef CONFIG_PM

static int palmt650_led_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&palmt650_green_led);
	led_classdev_suspend(&palmt650_red_led);
	led_classdev_suspend(&palmt650_keypad_led);
	led_classdev_suspend(&palmt650_vibra_led);
	return 0;
}

static int palmt650_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&palmt650_green_led);
	led_classdev_resume(&palmt650_red_led);
	led_classdev_resume(&palmt650_keypad_led);
	led_classdev_resume(&palmt650_vibra_led);
	return 0;
}
#endif

static int palmt650_led_probe(struct platform_device *pdev)
{
	int ret;

	ret = led_classdev_register(&pdev->dev, &palmt650_green_led);
	if (ret < 0)
		goto green_err;

	ret = led_classdev_register(&pdev->dev, &palmt650_red_led);
	if (ret < 0)
		goto red_err;

	ret = led_classdev_register(&pdev->dev, &palmt650_keypad_led);
	if (ret < 0)
		goto keypad_err;

	ret = led_classdev_register(&pdev->dev, &palmt650_vibra_led);
	if (ret < 0)
		goto vibra_err;

	asic6_base = ioremap_nocache(0x08000000, 0x1000);
	
	/* enable LED controllers, possibly clock EN reg.*/
	asic6_write(0x66, 7); 

	/* enable green LED, possibly GPIO related reg. */
	asic6_write(0x10, 0xc0);

	/* enable red LED + keypad control, possibly GPIO related reg. */
	asic6_write(0x12, 0xe3);

	/* FIXME: should be moved to seperate phone driver */
/*
	printk("Attempting to reset phone module...\n");
	asic6_write(0x48, asic6_read(0x48) & ~0x40); 
	msleep(5);
	asic6_write(0x48, asic6_read(0x48) | 0x40);
*/

	return ret;
vibra_err:
	led_classdev_unregister(&palmt650_keypad_led);
keypad_err:	
	led_classdev_unregister(&palmt650_red_led);
red_err:
	led_classdev_unregister(&palmt650_green_led);
green_err:
	return ret;
}

static int palmt650_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&palmt650_green_led);
	led_classdev_unregister(&palmt650_red_led);
	led_classdev_unregister(&palmt650_keypad_led);
	led_classdev_unregister(&palmt650_vibra_led);
	return 0;
}

static struct platform_driver palmt650_led_driver = {
	.probe		= palmt650_led_probe,
	.remove		= palmt650_led_remove,
#ifdef CONFIG_PM
	.suspend	= palmt650_led_suspend,
	.resume		= palmt650_led_resume,
#endif
	.driver		= {
		.name		= "palmt650-led",
	},
};

static int __init palmt650_led_init(void)
{
	return platform_driver_register(&palmt650_led_driver);
}

static void __exit palmt650_led_exit(void)
{
 	platform_driver_unregister(&palmt650_led_driver);
}

module_init(palmt650_led_init);
module_exit(palmt650_led_exit);

MODULE_AUTHOR("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION("Palm Treo 650 LED driver");
MODULE_LICENSE("GPL");
