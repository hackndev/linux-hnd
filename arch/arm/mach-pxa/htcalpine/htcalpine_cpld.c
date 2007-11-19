/*
 * Experimental driver for the CPLD in the HTC Alpine
 *
 * Copyright 2006 Philipp Zabel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>	/* KERN_ERR */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>

#include <asm/io.h>		/* ioremap */

/* debug */
#define CPLD_DEBUG 1
#if CPLD_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

static struct alpine_cpld_data
{
	void *mapping;
	u32 cached_egpio;	/* cached egpio registers */
} *cpld;

void htcalpine_cpld_egpio_set_value(int gpio, int val)
{
	int bit = 1<<gpio;
	int egpio;

	dbg("htcalpine_cpld_egpio_set_value(%d, %d)\n", gpio, val);

        if (val)
                cpld->cached_egpio |= bit;
        else
                cpld->cached_egpio &= ~bit;

	egpio = cpld->cached_egpio;
	dbg("egpio: 0x%08x", egpio);
        __raw_writew(egpio & 0x7ff, cpld->mapping+0x0);
        __raw_writew(((egpio<<10)>>21), cpld->mapping+0x2);
}

int htcalpine_cpld_egpio_get_value(int gpio)
{
	int bit = 1<<gpio;

	cpld->cached_egpio = (__raw_readw(cpld->mapping+0x0) & 0x7ff) |
			((__raw_readw(cpld->mapping+0x2) & 0x7ff) << 11);
	return cpld->cached_egpio & bit;
}

static int alpine_cpld_probe (struct platform_device *dev)
{
	struct resource *res;

	if (cpld)
		return -EBUSY;
	cpld = kmalloc (sizeof (struct alpine_cpld_data), GFP_KERNEL);
	if (!cpld)
		return -ENOMEM;
	memset (cpld, 0, sizeof (*cpld));

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	dbg("trying to ioremap CPLD chip at 0x%08x\n", res->start);
	cpld->mapping = ioremap (res->start, res->end - res->start);
	if (!cpld->mapping) {
		printk (KERN_ERR "alpine_cpld: couldn't ioremap\n");
		kfree (cpld);
		return -ENOMEM;
	}

	htcalpine_cpld_egpio_get_value(0);
	dbg("initial egpios: 0x%08x\n", cpld->cached_egpio);

	return 0;
}

static int alpine_cpld_remove (struct platform_device *dev)
{
	iounmap((void *)cpld->mapping);
	kfree(cpld);
	cpld = NULL;
	return 0;
}


struct platform_driver alpine_cpld_driver = {
	.probe    = alpine_cpld_probe,
	.remove   = alpine_cpld_remove,
	.driver   = {
		.name     = "htcalpine-cpld",
	},
};

static int alpine_cpld_init (void)
{
	return platform_driver_register (&alpine_cpld_driver);
}

static void alpine_cpld_exit (void)
{
	platform_driver_unregister (&alpine_cpld_driver);
}

module_init (alpine_cpld_init);
module_exit (alpine_cpld_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("Experimental driver for the CPLD in the HTC Alpine");
MODULE_SUPPORTED_DEVICE("htcalpine-cpld");

EXPORT_SYMBOL(htcalpine_cpld_egpio_set_value);
