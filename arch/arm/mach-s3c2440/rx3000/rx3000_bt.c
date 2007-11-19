/* 
 * Copyright 2006 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Bluetooth driver for HP iPAQ RX3000
 *
 * Based on hx4700_bt.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/dpm.h>
#include <linux/leds.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <linux/mfd/asic3_base.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/rx3000.h>
#include <asm/arch/rx3000-asic3.h>

extern struct platform_device s3c_device_asic3;

static void rx3000_bt_power(int power)
{
    if (power) {
	DPM_DEBUG("rx3000_bt: Turning on\n");
	/* configre uart0 gpios */
	s3c2410_gpio_cfgpin(S3C2410_GPH0, S3C2410_GPH0_nCTS0);
	s3c2410_gpio_cfgpin(S3C2410_GPH1, S3C2410_GPH1_nRTS0);
	s3c2410_gpio_cfgpin(S3C2410_GPH2, S3C2410_GPH2_TXD0);
	s3c2410_gpio_cfgpin(S3C2410_GPH3, S3C2410_GPH3_RXD0);

	/* enable bluetooth clk */
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA15, ASIC3_GPA15);
	asic3_set_gpio_alt_fn_a(&s3c_device_asic3.dev, ASIC3_GPA15, ASIC3_GPA15);
	mdelay(5);
		
	/* power on */
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC12, ASIC3_GPC12);
	mdelay(5);

	/* reset up */
	s3c2410_gpio_setpin(S3C2410_GPA3, 1);
	
	led_trigger_event_shared(rx3000_radio_trig, LED_FULL);
    } else {
	DPM_DEBUG("rx3000_bt: Turning off\n");
	/* reset down */
	s3c2410_gpio_setpin(S3C2410_GPA3, 0);
	mdelay(5);
	
	/* power off */
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC12, 0);
	mdelay(5);

	/* disable bluetooth clk */
	asic3_set_gpio_alt_fn_a(&s3c_device_asic3.dev, ASIC3_GPA15, 0);
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA15, 0);

	led_trigger_event_shared(rx3000_radio_trig, LED_OFF);

	/* configre uart0 gpios */
	s3c2410_gpio_cfgpin(S3C2410_GPH0, S3C2410_GPH0_OUTP);
	s3c2410_gpio_cfgpin(S3C2410_GPH1, S3C2410_GPH1_OUTP);
	s3c2410_gpio_cfgpin(S3C2410_GPH2, S3C2410_GPH2_OUTP);
	s3c2410_gpio_cfgpin(S3C2410_GPH3, S3C2410_GPH3_OUTP);
    }
}

static ssize_t rx3000_bt_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", (asic3_get_gpio_out_c(&s3c_device_asic3.dev) & ASIC3_GPC12) ? 1 : 0);
}

static ssize_t rx3000_bt_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if ((simple_strtoul(buf, NULL, 0)) == 0)
		rx3000_bt_power(0);
    	else
		rx3000_bt_power(1);

	return size;
}

#ifdef CONFIG_PM
static int rx3000_bt_suspend(struct platform_device *pdev, pm_message_t state)
{
	rx3000_bt_power(0);
        return 0;
}

static int rx3000_bt_resume(struct platform_device *pdev)
{
	rx3000_bt_power(0);
        return 0;
}
#endif

static DEVICE_ATTR(power_control, 0644, rx3000_bt_power_show, rx3000_bt_power_store);

static int rx3000_bt_probe(struct platform_device *pdev)
{
	int err;

	rx3000_bt_power(0);

	err = device_create_file(&pdev->dev, &dev_attr_power_control);
	if (err < 0)
		return -ENODEV;

	return 0;
}

static int rx3000_bt_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_power_control);
	rx3000_bt_power(0);
	return 0;
}

static struct platform_driver rx3000_bt_driver = {
	.driver		= {	
		.name	= "rx3000-bt",
	},
	.probe		= rx3000_bt_probe,
	.remove		= rx3000_bt_remove,
#ifdef CONFIG_PM
	.suspend	= rx3000_bt_suspend,
	.resume		= rx3000_bt_resume,
#endif
};

static int __init rx3000_bt_init(void)
{
	printk(KERN_INFO "iPAQ RX3000 Bluetooth Driver\n");
	platform_driver_register(&rx3000_bt_driver);
	
	return 0;
}

static void __exit rx3000_bt_exit(void)
{
        platform_driver_unregister(&rx3000_bt_driver);
}

module_init(rx3000_bt_init);
module_exit(rx3000_bt_exit);

MODULE_AUTHOR("Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("Bluetooth driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
