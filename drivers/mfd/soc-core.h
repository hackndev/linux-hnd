/*
 * drivers/soc/soc-core.h
 *
 * core SoC support
 * Copyright (c) 2006 Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains prototypes for the functions in soc-core.c
 *
 * Created: 2006-11-28
 *
 */

struct soc_device_data {
	char *name;
	struct resource *res;
	int num_resources;
	void *hwconfig; /* platform_data to pass to the subdevice */
};

struct platform_device *soc_add_devices(struct platform_device *dev,
					struct soc_device_data *soc, int n_devs,
					struct resource *mem,
					int relative_addr_shift, int irq_base);

void soc_free_devices(struct platform_device *devices, int nr_devs);

