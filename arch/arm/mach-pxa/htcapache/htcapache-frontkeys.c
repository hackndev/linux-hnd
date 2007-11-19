/*
 * Driver for keys on input lines attached to the HTC Apache
 * microcontroller interface.
 *
 * (c) Copyright 2006 Kevin O'Connor <kevin@koconnor.net>
 * Based on code: Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio_keys.h>

#include <asm/arch/hardware.h>

#include <asm/arch/htcapache-gpio.h>

static irqreturn_t
gpio_keys_isr(int irq, void *dev_id)
{
	int i;
	struct platform_device *pdev = dev_id;
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input = platform_get_drvdata(pdev);
	u32 regs;

	// Ack the interrupt.
	htcapache_write_mc_reg(3, 0);

	regs = ~(htcapache_read_mc_reg(4) | (htcapache_read_mc_reg(5) << 8));

	for (i = 0; i < pdata->nbuttons; i++) {
		int gpio = pdata->buttons[i].gpio;
		int state = !!(regs & (1<<gpio));
		input_report_key(input, pdata->buttons[i].keycode, state);
		input_sync(input);
	}

	return IRQ_HANDLED;
}

static int __devinit
gpio_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int i, ret;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	platform_set_drvdata(pdev, input);

	input->evbit[0] = BIT(EV_KEY);

	input->name = pdev->name;
//	input->phys = "gpio-keys/input0";
	input->cdev.dev = &pdev->dev;
	input->private = pdata;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	for (i = 0; i < pdata->nbuttons; i++) {
		int code = pdata->buttons[i].keycode;
		set_bit(code, input->keybit);
	}

	ret = input_register_device(input);
	if (ret < 0) {
		printk(KERN_ERR "Unable to register gpio-keys input device\n");
		goto fail;
	}

        ret = request_irq(IRQ_GPIO(GPIO_NR_HTCAPACHE_FKEY_IRQ)
			  , gpio_keys_isr
			  , IRQF_DISABLED | IRQF_TRIGGER_RISING
			    | IRQF_SAMPLE_RANDOM
			  , pdev->name, pdev);
	if (ret)
		goto fail;

	// Clear any queued up key events.
	htcapache_write_mc_reg(3, 0);

	return 0;

fail:
	input_free_device(input);
	return ret;
}

static int __devexit
gpio_keys_remove(struct platform_device *pdev)
{
	struct input_dev *input = platform_get_drvdata(pdev);

        free_irq(IRQ_GPIO(GPIO_NR_HTCAPACHE_FKEY_IRQ), pdev);
	input_unregister_device(input);
	return 0;
}

static struct platform_driver gpio_keys_device_driver = {
	.probe    = gpio_keys_probe,
	.remove   = __devexit_p(gpio_keys_remove),
	.driver = {
		.name     = "htcapache-frontkeys",
	}
};

static int __init
gpio_keys_init(void)
{
	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit
gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Keyboard driver for HTCApache front keypad");
