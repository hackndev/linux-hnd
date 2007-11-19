/*
 * linux/arch/arm/common/tc6387xb_soc.c
 *
 * Toshiba TC6387XB support
 * Copyright (c) 2005 Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains TC6387XB base support.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/tmio_mmc.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>

#include <linux/mfd/tc6387xb.h>
#include "soc-core.h"

struct tc6387xb_data {
	int irq;
	struct tc6387xb_platform_data *platform;
};

static void tc6387xb_hwinit(struct platform_device *dev)
{
	struct tc6387xb_data *data = platform_get_drvdata(dev);

	if (data && data->platform && data->platform->hw_init)
		data->platform->hw_init();

}

#ifdef CONFIG_PM

static int tc6387xb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct tc6387xb_data *data = platform_get_drvdata(dev);

	if (data && data->platform && data->platform->suspend)
		data->platform->suspend();

	return 0;
}

static int tc6387xb_resume(struct platform_device *dev)
{
	struct tc6387xb_data *data = platform_get_drvdata(dev);

	if (data && data->platform && data->platform->resume)
		data->platform->resume();

	return 0;
}
#endif

static struct resource tc6387xb_mmc_resources[] = {
        {
                .name = "control",
                .start = TC6387XB_MMC_CTL_BASE,
                .end   = TC6387XB_MMC_CTL_BASE + 0x1ff,
                .flags = IORESOURCE_MEM,
        },
        {
                .name = "config",
                .start = TC6387XB_MMC_CNF_BASE,
                .end   = TC6387XB_MMC_CNF_BASE + 0xff,
                .flags = IORESOURCE_MEM,
        },
        {
                .start = TC6387XB_MMC_IRQ,
                .end   = TC6387XB_MMC_IRQ,
                .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
        },
};

static struct soc_device_data tc6387xb_devices[] = {
        {
                .name = "tmio_mmc",
                .res = tc6387xb_mmc_resources,
                .num_resources = ARRAY_SIZE(tc6387xb_mmc_resources),
        },
};


static int tc6387xb_probe(struct platform_device *pdev)
{
	struct tc6387xb_data *data;

	data = kmalloc(sizeof(struct tc6387xb_data), GFP_KERNEL);
	if(!data)
		return -ENOMEM;

	data->irq = pdev->resource[1].start;
	data->platform = pdev->dev.platform_data;
	platform_set_drvdata(pdev, data);

	tc6387xb_hwinit(pdev);
	
	soc_add_devices(pdev, tc6387xb_devices, ARRAY_SIZE(tc6387xb_devices), &pdev->resource[0], 0, data->irq);

	/* Init finished. */
	return 0;
}

static int tc6387xb_remove(struct platform_device *dev)
{
//	struct tc6387xb_data *tchip = platform_get_drvdata(dev);
//	int i;

	/* Free the subdevice resources */

	// Free IRQ ?

	//FIXME - put chip to sleep?
	
	/* Free core resources */

	return 0;
}


static struct platform_driver tc6387xb_platform_driver = {
	.driver = {
		.name		= "tc6387xb",
	},
	.probe		= tc6387xb_probe,
	.remove		= tc6387xb_remove,
#ifdef CONFIG_PM
	.suspend	= tc6387xb_suspend,
	.resume		= tc6387xb_resume,
#endif
};


static int __init tc6387xb_init(void)
{
	return platform_driver_register (&tc6387xb_platform_driver);
}

static void __exit tc6387xb_exit(void)
{
	platform_driver_unregister(&tc6387xb_platform_driver);
}

#ifdef MODULE
module_init(tc6387xb_init);
#else	/* start early for dependencies */
subsys_initcall(tc6387xb_init);
#endif
module_exit(tc6387xb_exit);

MODULE_DESCRIPTION("Toshiba TC6387XB core driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ian Molton");
