/*
 * LEDs support for HTC PASIC3 devices.
 *
 * Copyright (c) 2007 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/mfd/pasic3.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>

/*
 * dump all secondary registers
 */
void pasic3_dump_registers(struct device *dev)
{
	int i;
	printk("pasic3 secondary register dump:\n00: ");
	for (i = 0x00; i < 0x10; i++)
		printk("%02x ", pasic3_read_register(dev, i));
	printk("\n10: ");
	for (i = 0x10; i < 0x20; i++)
		printk("%02x ", pasic3_read_register(dev, i));
	printk("\n20: ");
	for (i = 0x20; i < 0x30; i++)
		printk("%02x ", pasic3_read_register(dev, i));
	printk("\n");
}

/*
 * The purpose of the following two functions is unknown.
 */
static void pasic3_enable_led_mask(struct device *dev, int mask)
{
	int r20, r21, r22;

	r20 = pasic3_read_register(dev, 0x20);
	r21 = pasic3_read_register(dev, 0x21);
	r22 = pasic3_read_register(dev, 0x22);
	r20 &= ~mask;
	pasic3_write_register(dev, 0x20, r20);
	pasic3_write_register(dev, 0x21, r21);
	pasic3_write_register(dev, 0x22, r22);
}

/*
 * The purpose of the following function is unknown.
 */
static void pasic3_disable_led_mask(struct device *dev, int mask)
{
	int r20, r21, r22;

	r20 = pasic3_read_register(dev, 0x20);
	r21 = pasic3_read_register(dev, 0x21);
	r22 = pasic3_read_register(dev, 0x22);
	r20 |= mask;
	r21 |= mask;	/* what are 0x21 and 0x22 good for? */
	r22 &= ~mask;	/* the blueangel driver doesn't seem to need them */
	pasic3_write_register(dev, 0x20, r20);
	pasic3_write_register(dev, 0x21, r21);
	pasic3_write_register(dev, 0x22, r22);
}

/*
 * led_classdev callback to set the brightness of a pasic3_led
 */
static void pasic3_led_set(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct pasic3_led *led = container_of(led_cdev, struct pasic3_led, led);
	struct pasic3_leds_machinfo *pdata = led->pdata;
	struct device *dev = led_cdev->class_dev->dev->parent;

	int led_reg = pdata->num_leds * 2; /* after on and off time registers */
	int state;

	state = pasic3_read_register(dev, led_reg);

	if (value) {
#ifdef CONFIG_LEDS_TRIGGER_HWTIMER
		if (led_cdev->trigger && led_cdev->trigger->is_led_supported &&
				(led_cdev->trigger->is_led_supported(led_cdev) &
				 LED_SUPPORTS_HWTIMER)) {
			struct hwtimer_data *td = led_cdev->trigger_data;
			if (!td)
				return;
			/* 0x10 = ~1s --> ((ms*16+500)/1000) */
			pasic3_write_register(dev, led->hw_num*2,   (td->delay_on  * 16 + 500) / 1000);
			pasic3_write_register(dev, led->hw_num*2+1, (td->delay_off * 16 + 500) / 1000);

		}
#endif
		pasic3_enable_led_mask(dev, led->mask);
		if ((value > LED_HALF) && (led->bit2))
			state = (state & ~(1<<led->hw_num)) | led->bit2;
		else
			state = (state & ~led->bit2) | (1<<led->hw_num);
	} else {
		pasic3_disable_led_mask(dev, led->mask);
		state &= ~((1<<led->hw_num) | led->bit2);
	}

	pasic3_write_register(dev, led_reg, state);
}

#ifdef CONFIG_PM
static int pasic3_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pasic3_leds_machinfo *pdata = pdev->dev.platform_data;
	int i;

	if (pdata->power_gpio)
		gpio_set_value(pdata->power_gpio, 0);

	for (i = 0; i < pdata->num_leds; i++)
		led_classdev_suspend(&(pdata->leds[i].led));

	return 0;
}

static int pasic3_led_resume(struct platform_device *pdev)
{
	struct pasic3_leds_machinfo *pdata = pdev->dev.platform_data;
	int i;
	
	if (pdata->power_gpio)
		gpio_set_value(pdata->power_gpio, 1);

	for (i = 0; i < pdata->num_leds; i++)
		led_classdev_resume(&(pdata->leds[i].led));

	return 0;
}
#else
#define pasic3_led_suspend NULL
#define pasic3_led_resume NULL
#endif

static int pasic3_led_probe(struct platform_device *pdev)
{
	struct pasic3_leds_machinfo *pdata = pdev->dev.platform_data;
	struct device *dev = pdev->dev.parent;
	int ret;
	int i;

	for (i = 0; i < pdata->num_leds; i++) {
		pdata->leds[i].led.brightness_set = pasic3_led_set;
		pdata->leds[i].pdata = pdata;
		ret = led_classdev_register(&pdev->dev, &(pdata->leds[i].led));
		if (ret < 0)
			goto pasic3_err;
	}

	if (pdata->power_gpio)
		gpio_set_value(pdata->power_gpio, 1);

	pasic3_write_register(dev, 0x00,0); /* led0 on time */
	pasic3_write_register(dev, 0x01,0); /* led0 off time */
	pasic3_write_register(dev, 0x02,0); /* led1 on time, 0x10 = ~1s ((ms*16+500)/1000) */
	pasic3_write_register(dev, 0x03,0); /* led1 off time, */
	pasic3_write_register(dev, 0x04,0); /* led2 on time, (cycle time = on + off time) */
	pasic3_write_register(dev, 0x05,0); /* led2 off time, */

	pasic3_disable_led_mask(dev, PASIC3_MASK_LED0 | PASIC3_MASK_LED1 | PASIC3_MASK_LED2);

	pasic3_write_register(dev, 0x06, pasic3_read_register(dev, 0x06) & ~0x3f);

	pasic3_dump_registers(dev);

	return 0;

pasic3_err:
	for (i = i-1; i >= 0; i--)
		led_classdev_unregister(&(pdata->leds[i].led));
	return ret;
}

static int pasic3_led_remove(struct platform_device *pdev)
{
	struct pasic3_leds_machinfo *pdata = pdev->dev.platform_data;
	int i;

	if (pdata->power_gpio)
		gpio_set_value(pdata->power_gpio, 0);

	for (i = 0; i < pdata->num_leds; i++)
		led_classdev_unregister(&(pdata->leds[i].led));

	return 0;
}

static struct platform_driver pasic3_led_driver = {
	.probe		= pasic3_led_probe,
	.remove		= pasic3_led_remove,
	.suspend	= pasic3_led_suspend,
	.resume		= pasic3_led_resume,
	.driver		= {
		.name		= "pasic3-led",
	},
};

static int __init pasic3_led_init (void)
{
	return platform_driver_register(&pasic3_led_driver);
}

static void __exit pasic3_led_exit (void)
{
 	platform_driver_unregister(&pasic3_led_driver);
}

module_init (pasic3_led_init);
module_exit (pasic3_led_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("HTC PASIC3 LED driver");
MODULE_LICENSE("GPL");

