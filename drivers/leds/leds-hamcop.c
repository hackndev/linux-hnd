/*
 * LEDs support for the Samsung's HAMCOP.
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
#include <asm/hardware/hamcop_leds.h>
#include "leds.h"

#define PWM_RATE 5

/* hamcop_base function */
extern int hamcop_set_led(struct device *hamcop_dev, unsigned long rate,
                          int led_num, unsigned int duty_time,
                          unsigned int cycle_time);

static void hamcop_leds_set(struct led_classdev *led_cdev,
                            enum led_brightness b)
{
	struct hamcop_led *led = container_of(led_cdev, struct hamcop_led,
	                                     led_cdev);
	struct hamcop_leds_machinfo *machinfo = led->machinfo;
	struct device *hamcop_dev = &machinfo->hamcop_pdev->dev;

	pr_debug("%s:%s %d-%s %d\n", __FILE__, __FUNCTION__, led->hw_num,
	    led->led_cdev.name, b);

	if (b == LED_OFF)
		hamcop_set_led(hamcop_dev, PWM_RATE, led->hw_num, 0, 16);
	else {
		#ifdef CONFIG_LEDS_TRIGGER_HWTIMER
		if (led_cdev->trigger && led_cdev->trigger->is_led_supported &&
			       (led_cdev->trigger->is_led_supported(led_cdev) &
			        LED_SUPPORTS_HWTIMER)) {
			struct hwtimer_data *td = led_cdev->trigger_data;

			if (!td) return;

			hamcop_set_led(hamcop_dev, PWM_RATE, led->hw_num,
			  (PWM_RATE * td->delay_on)/1000,
			  (PWM_RATE * (td->delay_on + td->delay_off))/1000);
		}
		else
		#endif
		hamcop_set_led(hamcop_dev, PWM_RATE, led->hw_num, 16, 16);
	}

	return;
}

static int hamcop_leds_probe(struct platform_device *pdev)
{
	struct hamcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct hamcop_led *leds = machinfo->leds;
	int ret = 0, i;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	machinfo->leds_clk = clk_get(&pdev->dev, "led");
	if (IS_ERR(machinfo->leds_clk)) {
		printk(KERN_ERR "%s: failed to get clock\n", __FUNCTION__);
		ret = PTR_ERR(machinfo->leds_clk);
		goto clk_get_failed;
	}

	/* Turn on clocks early, for the case if trigger would enable
	 * led immediately after led_classdev_register(). */
	if (clk_enable(machinfo->leds_clk)) {
		printk(KERN_ERR "%s: failed to enable clock\n", __FUNCTION__);
		goto clk_enable_failed;
	}

	for (i = 0; i < machinfo->num_leds; i++) {
		leds[i].machinfo = machinfo;
		leds[i].led_cdev.brightness_set = hamcop_leds_set;
		ret = led_classdev_register(&pdev->dev, &leds[i].led_cdev);
		if (ret) {
			printk(KERN_ERR "can't register %s led\n",
			       leds[i].led_cdev.name);
			goto classdev_register_failed;
		}
	}

	return 0;

classdev_register_failed:
	while (--i >= 0)
		led_classdev_unregister(&leds[i].led_cdev);
	clk_disable(machinfo->leds_clk);
clk_enable_failed:
	clk_put(machinfo->leds_clk);
clk_get_failed:
	return ret;
}

static int hamcop_leds_remove(struct platform_device *pdev)
{
	struct hamcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct hamcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_unregister(&leds[i].led_cdev);

	clk_put(machinfo->leds_clk);

	return 0;
}

#ifdef CONFIG_PM
static int hamcop_leds_suspend(struct platform_device *pdev,
                               pm_message_t state)
{
	struct hamcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct hamcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_suspend(&leds[i].led_cdev);

	return 0;
}

static int hamcop_leds_resume(struct platform_device *pdev)
{
	struct hamcop_leds_machinfo *machinfo = pdev->dev.platform_data;
	struct hamcop_led *leds = machinfo->leds;
	int i = 0;

	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);

	for (i = 0; i < machinfo->num_leds; i++)
		led_classdev_resume(&leds[i].led_cdev);

	return 0;
}
#endif /* CONFIG_PM */

static struct platform_driver hamcop_leds_driver = {
	.probe = hamcop_leds_probe,
	.remove = hamcop_leds_remove,
#ifdef CONFIG_PM
	.suspend = hamcop_leds_suspend,
	.resume = hamcop_leds_resume,
#endif
	.driver = {
		.name = "hamcop-leds",
	},
};

static int __init hamcop_leds_init(void)
{
	pr_debug("%s:%s\n", __FILE__, __FUNCTION__);
	return platform_driver_register(&hamcop_leds_driver);
}

static void __exit hamcop_leds_exit(void)
{
	platform_driver_unregister(&hamcop_leds_driver);
	return;
}

module_init(hamcop_leds_init);
module_exit(hamcop_leds_exit);

MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru> ; Michal Panczyk <mpanczyk@gmail.com>");
MODULE_DESCRIPTION("Samsung's HAMCOP LEDs driver");
MODULE_LICENSE("GPL");
