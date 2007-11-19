/*
 * blueangel_ds1wm.c - HTC Blueangel DS1WM-in-AICr2 driver
 *
 * Copyright (C) 2006 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * 2006-12-17: Adapted to work for the Blueangel (Michael Horne <asylumed@gmail.com>)
 *	
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/mfd/asic3_base.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/ds1wm.h>

#include <asm/arch/htcblueangel-asic.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/clock.h>

static struct resource blueangel_ds1wm_resources[] = {
	[0] = {
		.start  = 0x11000000,
		.end	= 0x11000000 + 0x08,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = 0,
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	}

};

static struct ds1wm_platform_data blueangel_ds1wm_platform_data = {
	.bus_shift = 1,
};

static struct platform_device *blueangel_ds1wm;

static void blueangel_ds1wm_enable(struct clk *clock)
{
	printk ("blueangel_ds1wm: OWM_EN Low (active)\n");
	/* TODO: must find the proper init */
}

static void blueangel_ds1wm_disable(struct clk *clock)
{
	printk ("blueangel_ds1wm: OWM_EN High (inactive)\n");
	/* TODO: must find proper de-init */
}

static struct clk ds1wm_clk = {
        .name    = "ds1wm",
        .rate    = 16000000,
        .parent  = NULL,
	.enable  = blueangel_ds1wm_enable,
	.disable = blueangel_ds1wm_disable,
};

static int __devinit blueangel_ds1wm_init(void)
{
        int ret;
	int irq;
	printk("HTC Blueangel DS1WM driver\n");
	irq = asic3_irq_base(&blueangel_asic3.dev) + BLUEANGEL_OWM_IRQ;

	enable_irq(irq);
	blueangel_ds1wm_resources[1].start = irq;

	if (clk_register(&ds1wm_clk) < 0)
		printk(KERN_ERR "failed to register DS1WM clock\n");

        blueangel_ds1wm = platform_device_alloc("ds1wm", -1);
        if (!blueangel_ds1wm) {
		printk("blueangel_ds1wm: failed to allocate platform device\n");
		return -ENOMEM;
	}

	blueangel_ds1wm->num_resources = ARRAY_SIZE(blueangel_ds1wm_resources);
	blueangel_ds1wm->resource = blueangel_ds1wm_resources;
        blueangel_ds1wm->dev.platform_data = &blueangel_ds1wm_platform_data;

        ret = platform_device_add(blueangel_ds1wm);
        if (ret) {
                platform_device_put(blueangel_ds1wm);
		printk("blueangel_ds1wm: failed to add platform device\n");
	}

	return ret;
}

static void __devexit blueangel_ds1wm_exit(void)
{
	disable_irq(asic3_irq_base(&blueangel_asic3.dev) + BLUEANGEL_OWM_IRQ);
	platform_device_del(blueangel_ds1wm);
	clk_unregister(&ds1wm_clk);
}

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("DS1WM driver");
MODULE_LICENSE("GPL");

module_init(blueangel_ds1wm_init);
module_exit(blueangel_ds1wm_exit);
