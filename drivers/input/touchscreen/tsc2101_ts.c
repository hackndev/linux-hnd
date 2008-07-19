/*
 * Texas Instruments TSC2101 Touchscreen Driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/mfd/tsc2101.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>


static struct tsc2101_driver tsc2101_ts_driver;
static struct tsc2101_ts tsc2101_ts;

static struct tsc2101_ts_event ts_data;

void tsc2101_ts_report(struct tsc2101_ts *tsc2101_ts, int x, int y, int p, int pendown)
{
	input_report_abs(tsc2101_ts->inputdevice, ABS_X, x);
	input_report_abs(tsc2101_ts->inputdevice, ABS_Y, y);
	input_report_abs(tsc2101_ts->inputdevice, ABS_PRESSURE, p);
	input_report_key(tsc2101_ts->inputdevice, BTN_TOUCH, pendown);
	input_sync(tsc2101_ts->inputdevice);

	printk(KERN_DEBUG "tsc2101_ts: to input device: %d, %d, %d - pen is %s\n",x,y,p,pendown ? "down" : "up");
	return;
}

/* FIXME: should I really call tsc2101_readdata() and not only tsc2101_ts->readdata() ? */
static u8 tsc2101_ts_readdata(u32 status)
{
	u32 values[4];
	int z1,z2;

	if (status & (TSC2101_STATUS_XSTAT | TSC2101_STATUS_YSTAT | TSC2101_STATUS_Z1STAT 
		| TSC2101_STATUS_Z2STAT)) {
		/* Read X, Y, Z1 and Z2 */
		tsc2101_ts_driver.tsc->platform->send(TSC2101_READ, TSC2101_REG_X, &values[0], 4);
	
		ts_data.x=values[0];
		ts_data.y=values[1];
		z1=values[2];
		z2=values[3];
	
		/* Calculate Pressure */
		if ((z1 != 0) && (ts_data.x!=0) && (ts_data.y!=0)) 
			ts_data.p = ((ts_data.x * (z2 -z1) / z1));
		else
			ts_data.p=0;
		printk(KERN_DEBUG "tsc2101_ts: read values x=%d, y=%d, p=%d\n",ts_data.x,ts_data.y,ts_data.p);
	} else {
		printk(KERN_DEBUG "tsc2101_ts: data not for touchscreen\n");
	}
	return 0;
}

static void tsc2101_ts_enable(struct tsc2101 *devdata)
{
	unsigned long flags;

	/* touchscreen data initialization */
	ts_data.x = 0;
	ts_data.y = 0;
	ts_data.p = 0;

	spin_lock_irqsave(&tsc2101_ts_driver.tsc->lock, flags);

	//tsc2101_regwrite(devdata, TSC2101_REG_RESETCTL, 0xbb00);
	
	/* PINTDAV is data available only */
	tsc2101_regwrite(devdata, TSC2101_REG_STATUS, 0x2000);

	/* disable buffer mode */
	tsc2101_regwrite(devdata, TSC2101_REG_BUFMODE, 0x0);

	/* use internal reference, 100 usec power-up delay,
	 * power down between conversions, 1.25V internal reference */
	tsc2101_regwrite(devdata, TSC2101_REG_REF, 0x16);
			   
	/* enable touch detection, 84usec precharge time, 32 usec sense time */
	tsc2101_regwrite(devdata, TSC2101_REG_CONFIG, 0x08);
			   
	/* 3 msec conversion delays, 12 MHz MClk  */
	tsc2101_regwrite(devdata, TSC2101_REG_DELAY, 0x0900 | 0xc);
	
	/*
	 * TSC2101-controlled conversions
	 * 12-bit samples
	 * continuous X,Y,Z1,Z2 scan mode
	 * average (mean) 4 samples per coordinate
	 * 1 MHz internal conversion clock
	 * 500 usec panel voltage stabilization delay
	 */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));

	spin_unlock_irqrestore(&tsc2101_ts_driver.tsc->lock, flags);

	return;
}


static void tsc2101_ts_disable(struct tsc2101 *devdata)
{
	/* stop conversions and power down */
	tsc2101_regwrite(devdata, TSC2101_REG_ADC, 0x4000);
}


static void ts_interrupt_main(struct tsc2101 *dev, int isTimer, struct pt_regs *regs)
{
	struct tsc2101_ts *devdata = dev->ts->priv;
	unsigned long flags;

	/* FIXME: is spinlock USEFUL here? */
	spin_lock_irqsave(&tsc2101_ts_driver.tsc->lock, flags);

	if (dev->platform->pendown()) {
		/* if touchscreen is just touched */
		devdata->pendown = 1;
		tsc2101_readdata();
		tsc2101_ts_report(devdata, ts_data.x, ts_data.y, ts_data.p, /*1*/ !!ts_data.p);
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
	} else if (devdata->pendown > 0 && devdata->pendown < 3) {
		/* if touchscreen was touched short time ago and it's possible that it's only surface indirectness */
		mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		devdata->pendown++;
	} else {
		/* if touchscreen was touched long time ago - it was probably released */
		if (devdata->pendown) {
			tsc2101_readdata();
			ts_data.x = 0;
			ts_data.y = 0;
			ts_data.p = 0;
			tsc2101_ts_report(devdata, 0, 0, 0, 0);
			}
		devdata->pendown = 0;

		enable_irq(dev->platform->irq);
//		set_irq_type(dev->platform->irq,IRQT_FALLING);

		/* This must be checked after set_irq_type() to make sure no data was missed */
		if (dev->platform->pendown()) {
			tsc2101_readdata();
			mod_timer(&(devdata->ts_timer), jiffies + HZ / 100);
		} 
	}
	
	spin_unlock_irqrestore(&tsc2101_ts_driver.tsc->lock, flags);
}


static void tsc2101_ts_timer(unsigned long data)
{
	ts_interrupt_main(tsc2101_ts_driver.tsc, TSC2101_TIMER, NULL);
	printk(KERN_DEBUG "tsc2101_ts: timer handled\n");
}

static irqreturn_t tsc2101_ts_handler(int irq, void *dev_id)
{
//	set_irq_type(tsc2101_ts_driver.tsc->platform->irq,IRQT_NOEDGE);
	disable_irq(tsc2101_ts_driver.tsc->platform->irq);
	ts_interrupt_main(tsc2101_ts_driver.tsc, TSC2101_IRQ, NULL);
	printk(KERN_DEBUG "tsc2101_ts: IRQ handled\n");
	return IRQ_HANDLED;
}

static int tsc2101_ts_suspend(pm_message_t state)
{
	tsc2101_ts_disable(tsc2101_ts_driver.tsc);
	return 0;
}

static int tsc2101_ts_resume(void)
{
	tsc2101_ts_enable(tsc2101_ts_driver.tsc);
	return 0;
}



static int tsc2101_ts_probe(struct tsc2101 *tsc)
{
	struct input_dev *input_dev;
	int ret = -ENODEV;

	if (!tsc)
		return ret;

	tsc2101_ts_driver.tsc = tsc;
	tsc2101_ts_driver.priv = (void *) &tsc2101_ts;

	init_timer(&tsc2101_ts.ts_timer);
	tsc2101_ts.ts_timer.data = (unsigned long) &tsc2101_ts;
	tsc2101_ts.ts_timer.function = tsc2101_ts_timer;
	
	input_dev = input_allocate_device();

	if (!input_dev)
		goto input_err;

	tsc2101_ts.inputdevice = input_dev;

	input_dev->name = "tsc2101_ts";
	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	input_dev->keybit[LONG(BTN_TOUCH)] |= BIT(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X, X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, PRESSURE_MIN, PRESSURE_MAX, 0, 0);
	
	ret = input_register_device(tsc2101_ts.inputdevice);
	if (ret)
		goto input_err;


	/* request irq */
	if (request_irq(tsc->platform->irq, tsc2101_ts_handler, 0, "tsc2101_ts", tsc))
		goto irq_err;

	printk(KERN_NOTICE "tsc2101_ts: IRQ %d allocated\n",tsc->platform->irq);
	tsc2101_ts_enable(tsc);

	set_irq_type(tsc->platform->irq,IRQT_FALLING);

	/* Check there is no pending data */
	tsc2101_readdata();

	printk(KERN_NOTICE "tsc2101_ts: touchscreen driver initialized\n");

	return 0;
irq_err:
	printk(KERN_ERR "tsc2101_ts: Could not allocate touchscreen IRQ!\n");	
	ret = -EINVAL;
input_err:
	input_free_device(tsc2101_ts.inputdevice);
	return ret;

}

static int tsc2101_ts_remove(void)
{
	free_irq(tsc2101_ts_driver.tsc->platform->irq, tsc2101_ts_driver.tsc);
	del_timer_sync(&tsc2101_ts.ts_timer);
	input_unregister_device(tsc2101_ts.inputdevice);
	input_free_device(tsc2101_ts.inputdevice);
	tsc2101_ts_disable(tsc2101_ts_driver.tsc);
	printk("tsc2101_ts: touchscreen driver removed\n");
	return 0;
}

static int __init tsc2101_ts_init(void)
{
	return tsc2101_register(&tsc2101_ts_driver,TSC2101_DEV_TS);
}

static void __exit tsc2101_ts_exit(void)
{
	tsc2101_unregister(&tsc2101_ts_driver,TSC2101_DEV_TS);
}

static struct tsc2101_driver tsc2101_ts_driver = {
	.suspend	= tsc2101_ts_suspend,
	.resume		= tsc2101_ts_resume,
	.probe		= tsc2101_ts_probe,
	.remove		= tsc2101_ts_remove,
	.readdata	= tsc2101_ts_readdata
};

module_init(tsc2101_ts_init);
module_exit(tsc2101_ts_exit);

MODULE_LICENSE("GPL");
