/*
 * 
 * Driver for RS232 transceiver on the serial port.
 *
 * Copyright 2007 Paul Sokolovsky
 * Copyright 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/dpm.h>
#include <linux/rs232_serial.h>

static void serial_detect(struct rs232_serial_pdata *pdata)
{
	int connected = gpiodev_get_value(&pdata->detect_gpio);
	if (!connected)
		DPM_DEBUG("rs232-serial: Turning off port\n");
	dpm_power(&pdata->power_ctrl, connected);
	if (connected)
		DPM_DEBUG("rs232-serial: Turning on port\n");
}

static void serial_change_task_handler(struct work_struct *work)
{
	struct rs232_serial_pdata *pdata = container_of(work, struct rs232_serial_pdata, debounce_task.work);
	serial_detect(pdata);
}

static int serial_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct rs232_serial_pdata *pdata = pdev->dev.platform_data;
	schedule_delayed_work(&pdata->debounce_task, 100); /* debounce */
	return IRQ_HANDLED;
}


static int rs232_serial_resume(struct platform_device *pdev)
{
	struct rs232_serial_pdata *pdata = pdev->dev.platform_data;
	/* check for changes to serial that may have occurred */
	serial_detect(pdata);
	return 0;
}

static int rs232_serial_probe(struct platform_device *pdev)
{
	struct rs232_serial_pdata *pdata = pdev->dev.platform_data;

	int irq = gpiodev_to_irq(&pdata->detect_gpio);
	if (irq == -1) {
		printk(KERN_ERR "rs232-serial: Detection pin does not generate irqs\n");
		return -EINVAL;
	}
	if (request_irq(irq, serial_isr, IRQF_DISABLED | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "rs232-serial", pdev) != 0) {
		printk(KERN_ERR "rs232-serial: Unable to configure detect irq.\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&pdata->debounce_task, serial_change_task_handler);

	/* Detect current state */
	serial_detect(pdata);

	return 0;
}

static int rs232_serial_remove(struct platform_device *pdev)
{
        struct rs232_serial_pdata *pdata = pdev->dev.platform_data;
	free_irq(gpiodev_to_irq(&pdata->detect_gpio), pdev);
	return 0;
}

static struct platform_driver rs232_serial_driver = {
	.driver	  = {
		.name     = "rs232-serial",
	},
	.probe	= rs232_serial_probe,
	.remove	= rs232_serial_remove,
	.resume = rs232_serial_resume,
};

static int __init rs232_serial_init(void)
{
	return platform_driver_register(&rs232_serial_driver);
}

static void __exit rs232_serial_exit(void)
{
	platform_driver_unregister(&rs232_serial_driver);
}

module_init(rs232_serial_init);
module_exit(rs232_serial_exit);

MODULE_AUTHOR("Todd Blumer, Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("RS232 serial port driver");
MODULE_LICENSE("GPL");
