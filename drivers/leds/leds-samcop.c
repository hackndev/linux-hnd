/*
 * LEDs support for the Samsung's SAMCOP.
 *
 * Copyright (c) 2007  Anton Vorontsov <cbou@mail.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <asm/hardware/samcop_leds.h>
#include "leds.h"

#define PWM_RATE 5

/* samcop_base function */
extern int samcop_set_led(struct device *samcop_dev, unsigned long rate,
                          int led_num, unsigned int duty_time,
                          unsigned int cycle_time);

static void samcop_leds_set(struct led_classdev *led_cdev,
                            enum led_brightness b)
{
	struct samcop_led *led = container_of(led_cdev, struct samcop_led,
	                                     led_cdev);
	struct device *samcop_dev = led_cdev->class_dev->dev->parent;

	pr_debug("%s:%s %d-%s %d\n", __FILE__, __FUNCTION__, led->hw_num,
	    led->led_cdev.name, b);

	if (b == LED_OFF)
		samcop_set_led(samcop_dev, PWM_RATE, led->hw_num, 0, 16);
	else {
		#ifdef CONFIG_LEDS_TRIGGER_HWTIMER
		if (led_cdev->trigger && led_cdev->trigger->is_led_supported &&
			       (led_cdev->trigger->is_led_supported(led_cdev) &
			        LED_SUPPORTS_HWTIMER)) {
			struct hwtimer_data *td = led_cdev->trigger_data;

			if (!td) return;

			samcop_set_led(samcop_dev, PWM_RATE, led->hw_num,
			  (PWM_RATE * td->delay_on)/1000,
			  (PWM_RATE * (td->delay_on + td->delay_off))/1000);
		}
		else
		#endif
		samcop_set_led(samcop_dev, PWM_RATE, led->hw_num, 16, 16);
	}

	return;
}

static int samcop_leds_probe(struct platform_device *pdev)
{
	struct samcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct samcop_led *leds = machinfo->leds;
	int ret = 0, i;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	machinfo->leds_clk = clk_get(&pdev->dev, "led");
	if (IS_ERR(machinfo->leds_clk)) {
		printk("%s: failed to get clock\n", __FUNCTION__);
		ret = PTR_ERR(machinfo->leds_clk);
		goto clk_get_failed;
	}

	/* Turn on clocks early, for the case if trigger would enable
	 * led immediately after led_classdev_register(). */
	if (clk_enable(machinfo->leds_clk)) {
		printk("%s: failed to enable clock\n", __FUNCTION__);
		goto clk_enable_failed;
	}

	for (i = 0; i < machinfo->num_leds; i++) {
		leds[i].machinfo = machinfo;
		leds[i].led_cdev.brightness_set = samcop_leds_set;
		ret = led_classdev_register(&pdev->dev, &leds[i].led_cdev);
		if (ret) {
			printk("can't register %s led\n",
			       leds[i].led_cdev.name);
			goto classdev_register_failed;
		}
	}

	goto success;

classdev_register_failed:
	while (--i >= 0)
		led_classdev_unregister(&leds[i].led_cdev);
	clk_disable(machinfo->leds_clk);
clk_enable_failed:
	clk_put(machinfo->leds_clk);
clk_get_failed:
success:
	return ret;
}

static int samcop_leds_remove(struct platform_device *pdev)
{
	struct samcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct samcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_unregister(&leds[i].led_cdev);

	clk_put(machinfo->leds_clk);

	return 0;
}

#ifdef CONFIG_PM
static int samcop_leds_suspend(struct platform_device *pdev,
                               pm_message_t state)
{
	struct samcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct samcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_suspend(&leds[i].led_cdev);

	return 0;
}

static int samcop_leds_resume(struct platform_device *pdev)
{
	struct samcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct samcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_resume(&leds[i].led_cdev);

	return 0;
}
#endif /* CONFIG_PM */

static
struct platform_driver samcop_leds_driver = {
	.probe = samcop_leds_probe,
	.remove = samcop_leds_remove,
#ifdef CONFIG_PM
	.suspend = samcop_leds_suspend,
	.resume = samcop_leds_resume,
#endif
	.driver = {
		.name = "samcop-leds",
	},
};

static int __init samcop_leds_init(void)
{
	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);
	return platform_driver_register(&samcop_leds_driver);
}

static void __exit samcop_leds_exit(void)
{
	platform_driver_unregister(&samcop_leds_driver);
}

module_init(samcop_leds_init);
module_exit(samcop_leds_exit);

MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("Samsung's SAMCOP LEDs driver");
MODULE_LICENSE("GPL");
