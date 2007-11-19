/*
 * Core driver for HTC PASIC3 LED/DS1WM chip.
 *
 * Copyright (C) 2006 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/clk.h>
#include <linux/ds1wm.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mfd/pasic3.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/clock.h>
#include <asm/gpio.h>

struct pasic3_data {
	void __iomem *mapping;
	unsigned int bus_shift;

	struct device *dev;

	struct platform_device *mmc_dev;
};

/*
 * write to a secondary register on the PASIC3
 */
void pasic3_write_register(struct device *dev, u32 reg, u8 val)
{
	struct pasic3_data *asic = dev->driver_data;
	int bus_shift = asic->bus_shift;
	void *addr = asic->mapping + (5 << bus_shift);
	void *data = asic->mapping + (6 << bus_shift);

	__raw_writew((__raw_readw(addr) & 0xff80) | (reg & 0x7f), addr);
	__raw_writew((__raw_readw(addr) & 0xff7f), addr); /* write mode */

	__raw_writeb((__raw_readb(data) & 0xff00) | (val & 0xff), data);
}
EXPORT_SYMBOL(pasic3_write_register); /* for leds-pasic3 */

/*
 * read from a secondary register on the PASIC3
 */
u8 pasic3_read_register(struct device *dev, u32 reg)
{
	struct pasic3_data *asic = dev->driver_data;
	int bus_shift = asic->bus_shift;
	void *addr = asic->mapping + (5 << bus_shift);
	void *data = asic->mapping + (6 << bus_shift);

	__raw_writew((__raw_readw(addr) | 0x80), addr); /* read mode */
	__raw_writew((__raw_readw(addr) & 0xff80) | (reg & 0x7f), addr);

	return __raw_readb(data);
}
EXPORT_SYMBOL(pasic3_read_register); /* for leds-pasic3 */

/*
 * DS1WM
 */

static struct clk ds1wm_clk;

static void ds1wm_clock_enable(struct clk *clock)
{
	struct pasic3_data *asic = (struct pasic3_data *)clock->parent->ctrlbit;

	/* I don't know how to enable the 4MHz OWM clock here */

	/* FIXME: use this to detect magician/pasic3 for now */
	if (ds1wm_clk.rate == 4000000) {
		pasic3_write_register(asic->dev, 0x2a, 0x08); /* sets up the 4MHz clock? */
	}
}

static void ds1wm_clock_disable(struct clk *clock)
{
	/* I don't know how to disable the 4MHz OWM clock here */
}

static void ds1wm_enable(struct platform_device *ds1wm_dev)
{
	struct device *dev = ds1wm_dev->dev.parent;
	int c;

	/* FIXME: use this to detect magician/pasic3 for now */
	if (ds1wm_clk.rate == 4000000) {
		c = pasic3_read_register(dev, 0x28);
		pasic3_write_register(dev, 0x28, c & 0x7f);

		pr_debug ("pasic3: DS1WM OWM_EN low (active) %02x\n", c & 0x7f);
	}
}

static void ds1wm_disable(struct platform_device *ds1wm_dev)
{
	struct device *dev = ds1wm_dev->dev.parent;
	int c;

	/* FIXME: use this to detect magician/pasic3 for now */
	if (ds1wm_clk.rate == 4000000) {
		c = pasic3_read_register(dev, 0x28);
		pasic3_write_register(dev, 0x28, c | 0x80);

		pr_debug ("pasic3: DS1WM OWM_EN high (inactive) %02x\n", c | 0x80);
	}
}

static struct ds1wm_platform_data __devinit ds1wm_platform_data = {
	.bus_shift = 2,
	.enable = ds1wm_enable,
	.disable = ds1wm_disable,
};

static struct platform_device ds1wm = {
	.name		= "ds1wm",
	.id		= -1,
	.dev = {
		.platform_data = &ds1wm_platform_data,
	},
};

static struct clk clk_g = {
	.name    = "gclk",
	.rate    = 0,
	.parent  = NULL,
};

static struct clk ds1wm_clk = {
        .name    = "ds1wm",
        .rate    = 4000000,
        .parent  = &clk_g,
	.enable  = ds1wm_clock_enable,
	.disable = ds1wm_clock_disable,
};

static int pasic3_probe(struct platform_device *pdev)
{
	struct pasic3_platform_data *pdata = pdev->dev.platform_data;
	struct pasic3_data *asic;
        int ret;
	int i;

	asic = kzalloc(sizeof(struct pasic3_data), GFP_KERNEL);
	if (!asic)
		return -ENOMEM;

	platform_set_drvdata(pdev, asic);
	asic->dev = &pdev->dev;

	if (pdata && pdata->bus_shift)
		asic->bus_shift = pdata->bus_shift;
	else
		asic->bus_shift = 2;

	if (pdata && pdata->clock_rate)
		ds1wm_clk.rate = pdata->clock_rate;

	asic->mapping = ioremap(pdev->resource[0].start, 6*(1<<asic->bus_shift));
	if (!asic->mapping) {
		printk(KERN_ERR "pasic3: couldn't ioremap PASIC3\n");
		kfree (asic);
		return -ENOMEM;
	}

	clk_g.ctrlbit = (unsigned int)asic;
	if (clk_register(&clk_g) < 0)
		printk(KERN_ERR "pasic3: failed to register PASIC3 gclk\n");

	if (clk_register(&ds1wm_clk) < 0)
		printk(KERN_ERR "pasic3: failed to register DS1WM clock\n");

	ds1wm_platform_data.bus_shift = asic->bus_shift;

	ds1wm.num_resources = pdev->num_resources;
	ds1wm.resource = pdev->resource;
	ds1wm.dev.parent = &pdev->dev;

	ret = platform_device_register(&ds1wm);

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			pdata->child_devs[i]->dev.parent = &pdev->dev;
			ret = platform_device_register(pdata->child_devs[i]);
		}
	}

        return ret;
}

static int pasic3_remove(struct platform_device *pdev)
{
	struct pasic3_platform_data *pdata = pdev->dev.platform_data;
	struct pasic3_data *asic = platform_get_drvdata(pdev);
	int i;

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			platform_device_unregister(pdata->child_devs[i]);
		}
	}

	platform_device_unregister(&ds1wm);

	clk_unregister(&ds1wm_clk);
	clk_unregister(&clk_g);

	iounmap(asic->mapping);
	kfree(asic);
	return 0;
}

static struct platform_driver pasic3_driver = {
	.driver		= {
		.name	= "pasic3",
	},
	.probe		= pasic3_probe,
	.remove		= pasic3_remove,
};

static int __init pasic3_base_init(void)
{
	return platform_driver_register(&pasic3_driver);
}

static void __exit pasic3_base_exit(void)
{
	platform_driver_unregister(&pasic3_driver);
}

module_init(pasic3_base_init);
module_exit(pasic3_base_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("Core driver for HTC PASIC3");
MODULE_LICENSE("GPL");

