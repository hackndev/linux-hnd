/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * h3600 atmel micro companion support, touchscreen subdevice
 * Author : Alessandro Gardich <gremlin@gremlin.it>
 *
 */


#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>

#include <asm/hardware/micro.h>


struct ts_sample {
	unsigned short x;
	unsigned short y;
};


struct touchscreen_data {
	struct input_dev *input;
};

static micro_private_t *p_micro;
struct touchscreen_data *ts;

static void micro_ts_receive (int len, unsigned char *data) {
	if (len == 4) { 
		input_report_abs(ts->input, ABS_X, (data[2]<<8)+data[3]);
		input_report_abs(ts->input, ABS_Y, (data[0]<<8)+data[1]);
		input_report_abs(ts->input, ABS_PRESSURE, 1);
		input_report_key(ts->input, BTN_TOUCH, 0);
	}
	if (len == 0) {
		input_report_abs(ts->input, ABS_X, 0);
		input_report_abs(ts->input, ABS_Y, 0);
		input_report_abs(ts->input, ABS_PRESSURE, 0);
		input_report_key(ts->input, BTN_TOUCH, 1);
	}
	input_sync(ts->input);
}


static int micro_ts_probe (struct platform_device *pdev) {

	if (1) printk(KERN_ERR "micro touchscreen probe : begin\n");

	ts = kmalloc (sizeof (*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	memset (ts, 0, sizeof (*ts));

	p_micro = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, ts);
	//dev->driver_data = ts;
	
	ts->input = input_allocate_device();

	ts->input->evbit[0] = BIT(EV_ABS);	
	ts->input->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);

	ts->input->absmin[ABS_X] = 0;
	ts->input->absmin[ABS_Y] = 0;
	ts->input->absmin[ABS_PRESSURE] = 0;
	ts->input->absmax[ABS_X] = 1023;
	ts->input->absmax[ABS_Y] = 1023;
	ts->input->absmax[ABS_PRESSURE] = 1;

	ts->input->name = "micro ts";
	ts->input->private = ts;

	input_register_device (ts->input);

	{ /*--- callback ---*/
		//p_micro = dev_get_drvdata(&pdev->dev);
		spin_lock(p_micro->lock);
		p_micro->h_ts = micro_ts_receive;
		spin_unlock(p_micro->lock);
	}

	if (1) printk(KERN_ERR "micro touchscreen probe : end\n");
	return 0;
}

static int micro_ts_remove (struct platform_device *pdev) {
	struct touchscreen_data *ts;

	ts = platform_get_drvdata(pdev);

	spin_lock(p_micro->lock);
	p_micro->h_ts = NULL;
	spin_unlock(p_micro->lock);
	input_unregister_device (ts->input);
	kfree (ts);

	return 0;
}

static int micro_ts_suspend (struct platform_device *dev, pm_message_t statel) {
	spin_lock(p_micro->lock);
	p_micro->h_ts = NULL;
	spin_unlock(p_micro->lock);
	return 0;
}

static int micro_ts_resume (struct platform_device *dev) {
	spin_lock(p_micro->lock);
	p_micro->h_ts = micro_ts_receive;
	spin_unlock(p_micro->lock);
	return 0;
}

struct platform_driver micro_ts_device_driver = {
	.driver  = {
		.name    = "h3600-micro-ts",
	},
	.probe   = micro_ts_probe,
	.remove  = micro_ts_remove,
	.suspend = micro_ts_suspend,
	.resume  = micro_ts_resume,
};

static int micro_ts_init (void) 
{
	return platform_driver_register(&micro_ts_device_driver);
}

static void micro_ts_cleanup (void) 
{
	platform_driver_unregister (&micro_ts_device_driver);
}

module_init (micro_ts_init);
module_exit (micro_ts_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gremlin.it");
MODULE_DESCRIPTION("driver for iPAQ Atmel micro touchscreen");
