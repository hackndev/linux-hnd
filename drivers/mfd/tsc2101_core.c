/*
 * TI TSC2102 Common Code
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
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/mfd/tsc2101.h>
#include <linux/platform_device.h>

struct tsc2101 tsc2101;


void tsc2101_lock(void)
{
	spin_lock_irqsave(&tsc2101.lock, tsc2101.lock_flags);
}

EXPORT_SYMBOL(tsc2101_lock);

void tsc2101_unlock(void)
{
	spin_unlock_irqrestore(&tsc2101.lock, tsc2101.lock_flags);
}

EXPORT_SYMBOL(tsc2101_unlock);

void tsc2101_readdata(void)
{
	u32 status;
	unsigned long flags;

	/* FIXME: is spinlock USEFUL here? */
	spin_lock_irqsave(&tsc2101.lock, flags);

	status=tsc2101_regread(&tsc2101, TSC2101_REG_STATUS);

	spin_unlock_irqrestore(&tsc2101.lock, flags);

	if (tsc2101.ts)
		tsc2101.ts->readdata(status);

	if (tsc2101.meas)
		if(tsc2101.meas->readdata(status)) {
			/* FIXME: is spinlock USEFUL here? */
			spin_lock_irqsave(&tsc2101.lock, flags);

			/* Switch back to touchscreen autoscan */
			tsc2101_regwrite(&tsc2101, TSC2101_REG_ADC, TSC2101_ADC_DEFAULT | TSC2101_ADC_PSM | TSC2101_ADC_ADMODE(0x2));

			spin_unlock_irqrestore(&tsc2101.lock, flags);
		}
}

EXPORT_SYMBOL(tsc2101_readdata);

int tsc2101_register(struct tsc2101_driver *drv, int dev_type)
{
	switch (dev_type) {
		case TSC2101_DEV_TS:	tsc2101.ts = drv; break;
		case TSC2101_DEV_SND:	tsc2101.snd = drv; break;
		case TSC2101_DEV_MEAS:	tsc2101.meas = drv; break;
	};
	return drv->probe(&tsc2101);
}

int tsc2101_unregister(struct tsc2101_driver *drv,int dev_type)
{
	switch (dev_type) {
		case TSC2101_DEV_TS:	tsc2101.ts = NULL; break;
		case TSC2101_DEV_SND:	tsc2101.snd = NULL; break;
		case TSC2101_DEV_MEAS:	tsc2101.meas = NULL; break;
	};
	return drv->remove();
}

EXPORT_SYMBOL(tsc2101_register);
EXPORT_SYMBOL(tsc2101_unregister);

static int tsc2101_suspend(struct platform_device *dev, pm_message_t state)
{
	if (tsc2101.ts)
		tsc2101.ts->suspend(state);
	if (tsc2101.snd)
		tsc2101.snd->suspend(state);
	if (tsc2101.meas)
		tsc2101.meas->suspend(state);
	
	tsc2101.platform->suspend();
	return 0;
}

static int tsc2101_resume(struct platform_device *dev)
{
	tsc2101.platform->resume();

	if (tsc2101.meas)
		tsc2101.meas->resume();
	if (tsc2101.snd)
		tsc2101.snd->resume();
	if (tsc2101.ts)
		tsc2101.ts->resume();

	return 0;
}


static int tsc2101_probe(struct platform_device *dev)
{
	tsc2101.ts = NULL;
	tsc2101.snd = NULL;
	tsc2101.meas = NULL;
	platform_set_drvdata(dev,&tsc2101);
	spin_lock_init(&tsc2101.lock);
	tsc2101.platform = dev->dev.platform_data;
	tsc2101_readdata();
	return 0;
}

static  int tsc2101_remove(struct platform_device *dev)
{
	return 0;
}


static struct platform_driver tsc2101_driver = {
	.driver		= {
		.name 		= "tsc2101",
		.owner		= THIS_MODULE,
		.bus 		= &platform_bus_type,
	},
	.probe		= tsc2101_probe,
	.remove		= tsc2101_remove,
	.suspend	= tsc2101_suspend,
	.resume		= tsc2101_resume,
};

static int __init tsc2101_init(void)
{
	return platform_driver_register(&tsc2101_driver);
}

static void __exit tsc2101_exit(void)
{
	platform_driver_unregister(&tsc2101_driver);
}

module_init(tsc2101_init);
module_exit(tsc2101_exit);

MODULE_AUTHOR("Tomas Cech <sleep_walker@suse.cz>");
MODULE_DESCRIPTION("tsc2101 core driver");
MODULE_LICENSE("GPL");
