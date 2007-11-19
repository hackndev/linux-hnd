/*
 * LEDs support for HTC ASIC3 devices.
 *
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include "leds.h"

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include <asm/mach-types.h>
#include <asm/hardware/asic3_leds.h>

#ifdef DEBUG
#define dbg(msg, ...) printk(msg, __VA_ARGS__)
#else
#define dbg(msg, ...)
#endif

static
void asic3_leds_set(struct led_classdev *led_cdev, enum led_brightness b)
{
	struct asic3_led *led = container_of(led_cdev, struct asic3_led,
	                                     led_cdev);
	struct asic3_leds_machinfo *machinfo = led->machinfo;
	struct device *asic3_dev = &machinfo->asic3_pdev->dev;

	dbg("%s:%s %d(%d)-%s %d\n", __FILE__, __FUNCTION__, led->hw_num,
	    led->gpio_num, led->led_cdev.name, b);

	if (led->hw_num == -1) {
		asic3_gpio_set_value(asic3_dev, led->gpio_num, b);
		return;
	}

	if (b == LED_OFF) {
		asic3_set_led(asic3_dev, led->hw_num, 0, 16, 6);
		asic3_set_gpio_out_c(asic3_dev, led->hw_num, 0);
	}
	else {
		asic3_set_gpio_out_c(asic3_dev, led->hw_num, led->hw_num);
		#ifdef CONFIG_LEDS_TRIGGER_HWTIMER
		if (led_cdev->trigger && led_cdev->trigger->is_led_supported &&
			       (led_cdev->trigger->is_led_supported(led_cdev) &
			        LED_SUPPORTS_HWTIMER)) {
			struct hwtimer_data *td = led_cdev->trigger_data;
			if (!td) return;
			asic3_set_led(asic3_dev, led->hw_num, td->delay_on/8,
					   (td->delay_on + td->delay_off)/8, 6);
		}
		else 
		#endif
		asic3_set_led(asic3_dev, led->hw_num, 16, 16, 6);
	}

	return;
}

static
int asic3_leds_probe(struct platform_device *pdev)
{
	struct asic3_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct asic3_led *leds = machinfo->leds;
	int ret, i = 0;

	dbg("%s:%s\n", __FILE__, __FUNCTION__);

	// Turn on clocks early, for the case if trigger would enable
	// led immediately after led_classdev_register().
	asic3_set_clock_cdex(&machinfo->asic3_pdev->dev,
		CLOCK_CDEX_LED0 | CLOCK_CDEX_LED1 | CLOCK_CDEX_LED2,
		CLOCK_CDEX_LED0 | CLOCK_CDEX_LED1 | CLOCK_CDEX_LED2);

	for (i = 0; i < machinfo->num_leds; i++) {
		leds[i].machinfo = machinfo;
		leds[i].led_cdev.brightness_set = asic3_leds_set;
		ret = led_classdev_register(&pdev->dev, &leds[i].led_cdev);
		if (ret) {
			printk(KERN_ERR "Error: can't register %s led\n",
			       leds[i].led_cdev.name);
			goto out_err;
		}
	}
	
	return 0;

out_err:
	while (--i >= 0) led_classdev_unregister(&leds[i].led_cdev);

	asic3_set_clock_cdex(&machinfo->asic3_pdev->dev,
		CLOCK_CDEX_LED0 | CLOCK_CDEX_LED1 | CLOCK_CDEX_LED2,
		0               | 0               | 0);

	return ret;
}

static
int asic3_leds_remove(struct platform_device *pdev)
{
	struct asic3_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct asic3_led *leds = machinfo->leds;
	int i = 0;

	dbg("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_unregister(&leds[i].led_cdev);
	
	asic3_set_clock_cdex(&machinfo->asic3_pdev->dev,
		CLOCK_CDEX_LED0 | CLOCK_CDEX_LED1 | CLOCK_CDEX_LED2,
		0               | 0               | 0);

	return 0;
}

#ifdef CONFIG_PM

static
int asic3_leds_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct asic3_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct asic3_led *leds = machinfo->leds;
	int i = 0;

	dbg("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_suspend(&leds[i].led_cdev);

	return 0;
}

static
int asic3_leds_resume(struct platform_device *pdev)
{
	struct asic3_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct asic3_led *leds = machinfo->leds;
	int i = 0;

	dbg("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_resume(&leds[i].led_cdev);

	return 0;
}

#endif

static
struct platform_driver asic3_leds_driver = {
	.probe = asic3_leds_probe,
	.remove = asic3_leds_remove,
#ifdef CONFIG_PM
	.suspend = asic3_leds_suspend,
	.resume = asic3_leds_resume,
#endif
	.driver = {
		.name = "asic3-leds",
	},
};

static int __init asic3_leds_init(void)
{
	dbg("%s:%s\n", __FILE__, __FUNCTION__);
	return platform_driver_register(&asic3_leds_driver);
}

static void __exit asic3_leds_exit(void)
{
	platform_driver_unregister(&asic3_leds_driver);
}

module_init(asic3_leds_init);
module_exit(asic3_leds_exit);

MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("HTC ASIC3 LEDs driver");
MODULE_LICENSE("GPL");
