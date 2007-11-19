/*
 * drivers/soc/soc-core.c
 *
 * core SoC support
 * Copyright (c) 2006 Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains functionality used by many SoC type devices.
 *
 * Created: 2006-11-28
 *
 */

#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "soc-core.h"

void soc_free_devices(struct platform_device *devices, int nr_devs)
{
	struct platform_device *dev = devices;
	int i;

	for (i = 0; i < nr_devs; i++) {
		struct resource *res = dev->resource;
		platform_device_unregister(dev++);
		kfree(res);
	}
	kfree(devices);
}
EXPORT_SYMBOL_GPL(soc_free_devices);

#define SIGNED_SHIFT(val, shift) ((shift) >= 0 ? ((val) << (shift)) : ((val) >> -(shift)))

struct platform_device *soc_add_devices(struct platform_device *dev,
					struct soc_device_data *soc, int nr_devs,
					struct resource *mem,
					int relative_addr_shift, int irq_base)
{
	struct platform_device *devices;
	int i, r, base;

	devices = kzalloc(nr_devs * sizeof(struct platform_device), GFP_KERNEL);
	if (!devices)
		return NULL;

	for (i = 0; i < nr_devs; i++) {
		struct platform_device *sdev = &devices[i];
		struct soc_device_data *blk = &soc[i];
		struct resource *res;

		sdev->id = -1;
		sdev->name = blk->name;

		sdev->dev.parent = &dev->dev;
		sdev->dev.platform_data = (void *)blk->hwconfig;
		sdev->num_resources = blk->num_resources;

		/* Allocate space for the subdevice resources */
		res = kzalloc(blk->num_resources * sizeof (struct resource), GFP_KERNEL);
		if (!res)
			goto fail;

		for (r = 0; r < blk->num_resources; r++) {
			res[r].name = blk->res[r].name; // Fixme - should copy

			/* Find out base to use */
			base = 0;
			if (blk->res[r].flags & IORESOURCE_MEM) {
				base = mem->start;
			} else if ((blk->res[r].flags & IORESOURCE_IRQ) &&
				(blk->res[r].flags & IORESOURCE_IRQ_SOC_SUBDEVICE)) {
				base = irq_base;
			}

			/* Adjust resource */
			if (blk->res[r].flags & IORESOURCE_MEM) {
				res[r].parent = mem;
				res[r].start = base + SIGNED_SHIFT(blk->res[r].start, relative_addr_shift);
				res[r].end   = base + SIGNED_SHIFT(blk->res[r].end,   relative_addr_shift);
			} else {
				res[r].start = base + blk->res[r].start;
				res[r].end   = base + blk->res[r].end;
			}
			res[r].flags = blk->res[r].flags;
		}

		sdev->resource = res;
		if (platform_device_register(sdev)) {
			kfree(res);
			goto fail;
		}

		printk(KERN_INFO "SoC: registering %s\n", blk->name);
	}
	return devices;

fail:
	soc_free_devices(devices, i + 1);
	return NULL;
}
EXPORT_SYMBOL_GPL(soc_add_devices);
