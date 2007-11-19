/*
 * Driver for diagonal joypads on GPIO lines capable of generating interrupts.
 *
 * Copyright 2007 Milan Plzik, based on gpio-keys.c (c) Phil Blundell 2005
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
#include <linux/gpiodev.h>
#include <linux/gpiodev_diagonal.h>

#define DIRECTIONS 5

/* Standard NEWS ordering, plus action key */
static int keycodes[DIRECTIONS] = {KEY_UP, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_ENTER};

static void debounce_handler (unsigned long data)
{
	struct gpiodev_diagonal_platform_data *pdata = (void*)data;
	struct input_dev *input = pdata->input;
	char keys[DIRECTIONS] = { 0, };
	int i, j, m = 0;

	/* evaluate all pressed buttons */
	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio *gpio = &pdata->buttons[i].gpio;
		int state = (gpiodev_get_value(gpio) ? 1 : 0) ^ (pdata->buttons[i].active_low);
		if (state)
			for (j = 0 ; j < DIRECTIONS; j++) {
				if ( (1 << j) & pdata->buttons[i].directions )
					keys[j]++;
			};
	};	
	
	for (i = 0; i < DIRECTIONS - 1; i++)
		if (keys[i] > m) m = keys[i];

	for (i = 0; i < DIRECTIONS - 1; i++) {
		/* if any key was pressed, send also keypress events */
		if ((keys[i] == m) && (m > 0))
			input_report_key(input, keycodes[i], 1);
		else 
			input_report_key(input, keycodes[i], 0);
	};

	if (!pdata->dir_disables_enter)
		input_report_key(input, keycodes[DIRECTIONS - 1], keys[DIRECTIONS - 1] > 0);
	else
		/* m is nonzero if any of direction buttons is pressed */
		input_report_key(input, keycodes[DIRECTIONS - 1], (keys[DIRECTIONS - 1] > 0) & (m == 0));

	input_sync(input);
};

static irqreturn_t gpiodev_diagonal_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct gpiodev_diagonal_platform_data *pdata = pdev->dev.platform_data;

	/* Give some time for other GPIOs to trigger */
	mod_timer(&pdata->debounce_timer, jiffies + msecs_to_jiffies (50));

	return IRQ_HANDLED;
}

static int __devinit gpiodev_diagonal_probe(struct platform_device *pdev)
{
	struct gpiodev_diagonal_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int i, error;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;
	pdata->input = input;

	platform_set_drvdata(pdev, input);

	input->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);

	input->name = pdev->name;
	input->phys = "gpiodev-diagonal/input0";
	input->cdev.dev = &pdev->dev;
	input->private = pdata;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;


	setup_timer (&pdata->debounce_timer, debounce_handler, (unsigned long) pdata);
	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpiodev_to_irq(&(pdata->buttons[i].gpio));

		set_irq_type(irq, IRQ_TYPE_EDGE_BOTH);
		error = request_irq(irq, gpiodev_diagonal_isr, IRQF_SAMPLE_RANDOM | IRQF_SHARED,
				     pdata->buttons[i].desc ? pdata->buttons[i].desc : "gpiodev_diagonal",
				     pdev);
		if (error) {
			printk(KERN_ERR "gpiodev-diagonal: unable to claim irq %d; error %d\n",
				irq, error);
			goto fail;
		}
	}

	for (i = 0; i < 5; i++) {
		set_bit(keycodes[i], input->keybit);
	};

	error = input_register_device(input);
	if (error) {
		printk(KERN_ERR "Unable to register gpiodev-diagonal input device\n");
		goto fail;
	}


	return 0;

 fail:
	for (i = i - 1; i >= 0; i--)
		free_irq(gpiodev_to_irq(&pdata->buttons[i].gpio), pdev);

	input_free_device(input);
	
	del_timer_sync(&pdata->debounce_timer);

	return error;
}

static int __devexit gpiodev_diagonal_remove(struct platform_device *pdev)
{
	struct gpiodev_diagonal_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input = platform_get_drvdata(pdev);
	int i;

	del_timer_sync(&pdata->debounce_timer);

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpiodev_to_irq(&pdata->buttons[i].gpio);
		free_irq(irq, pdev);
	}

	input_unregister_device(input);

	return 0;
}

struct platform_driver gpiodev_diagonal_device_driver = {
	.probe		= gpiodev_diagonal_probe,
	.remove		= __devexit_p(gpiodev_diagonal_remove),
	.driver		= {
		.name	= "gpiodev-diagonal",
	}
};

static int __init gpiodev_diagonal_init(void)
{
	return platform_driver_register(&gpiodev_diagonal_device_driver);
}

static void __exit gpiodev_diagonal_exit(void)
{
	platform_driver_unregister(&gpiodev_diagonal_device_driver);
}

module_init(gpiodev_diagonal_init);
module_exit(gpiodev_diagonal_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Milan Plzik <mmp@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for diagonal joypads on GPIOs");
