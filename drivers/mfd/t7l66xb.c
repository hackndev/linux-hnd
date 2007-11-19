/*
 * linux/arch/arm/common/t7l66xb_soc.c
 *
 * Toshiba T7L66XB support
 * Copyright (c) 2005 Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains T7L66XB base support.
 *
 * This is based on Dirks and my work on the tc6393xb SoC.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/tmio_mmc.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/arch/pxa-regs.h>

#include <linux/mfd/t7l66xb.h>
#include "soc-core.h"

#define platform_get_platdata(_dev)      ((_dev)->dev.platform_data)

struct t7l66xb_data
{
	int irq_base, irq_nr;
	void *mapbase;
	struct platform_device *devices;
	int ndevices;
};

struct tmio_ohci_hwconfig t7l66xb_ohci_hwconfig = {
	.start = NULL,
};

static struct resource t7l66xb_mmc_resources[] = {
	{
		.name = "control",
		.start = T7L66XB_MMC_CTL_BASE,
		.end   = T7L66XB_MMC_CTL_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "config",
		.start = T7L66XB_MMC_CNF_BASE,
		.end   = T7L66XB_MMC_CNF_BASE + 0xff,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = T7L66XB_MMC_IRQ,
		.end = T7L66XB_MMC_IRQ,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct soc_device_data t7l66xb_devices[] = {
	{
		.name = "tmio_mmc",
		.res = t7l66xb_mmc_resources,
		.num_resources = ARRAY_SIZE(t7l66xb_mmc_resources),
	},
};

#if 0
static struct t7l66xb_block t7l66xb_blocks[] = {
	{
		.name  = "tmio_mmc",
		.cnf_start = T7L66XB_MMC_CNF_BASE,
		.cnf_end   = T7L66XB_MMC_CNF_BASE + 0xff,
		.ctl_start = T7L66XB_MMC_CTL_BASE,
		.ctl_end   = T7L66XB_MMC_CTL_BASE + 0x1ff,
		.irq       = T7L66XB_MMC_IRQ,
	},
	{
		.name = "tmio_nand",
		.cnf_start = T7L66XB_NAND_CNF_BASE,
		.cnf_end   = T7L66XB_NAND_CNF_BASE + 0xff,
		.ctl_start = T7L66XB_NAND_CTL_BASE,
		.ctl_end   = T7L66XB_NAND_CTL_BASE + 0xff,
		.irq       = -1,
	},
	{
		.name  = "tmio_ohci",
		.ctl_start = T7L66XB_USB_CTL_BASE,
		.ctl_end   = T7L66XB_USB_CTL_BASE + 0xff,
		.cnf_start = T7L66XB_USB_CNF_BASE,
                .cnf_end   = T7L66XB_USB_CNF_BASE + 0xff,
		.irq   = T7L66XB_OHCI_IRQ,
		.hwconfig  = &t7l66xb_ohci_hwconfig,
	},
};
#endif


/* Handle the T7L66XB interrupt mux */
static void t7l66xb_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int req, i;
	struct t7l66xb_data *data = get_irq_data(irq);

	/* Acknowledge the parent IRQ */
	desc->chip->ack(irq);
  
	while ( (req = (readb(data->mapbase + T7L66XB_SYS_ISR)
			& ~(readb(data->mapbase + T7L66XB_SYS_IMR)))) ) {
		for (i = 0; i <= 7; i++) {
			int dev_irq = data->irq_base + i;
			struct irq_desc *d = NULL;
			if ((req & (1<<i))) {
				d = irq_desc + dev_irq;
				d->handle_irq(dev_irq, d);
			}
		}
	}
}


static void t7l66xb_mask_irq(unsigned int irq)
{
	struct t7l66xb_data *data = get_irq_chip_data(irq);

	writeb(readb(data->mapbase + T7L66XB_SYS_IMR) | 1 << (irq - data->irq_base), data->mapbase + T7L66XB_SYS_IMR);
}

static void t7l66xb_unmask_irq(unsigned int irq)
{
	struct t7l66xb_data *data = get_irq_chip_data(irq);

	writeb(readb(data->mapbase + T7L66XB_SYS_IMR) & ~( 1 << (irq - data->irq_base)),data->mapbase + T7L66XB_SYS_IMR);
}

static struct irq_chip t7l66xb_chip = {
	.name   = "t7l66xb",
	.ack	= t7l66xb_mask_irq,
	.mask	= t7l66xb_mask_irq,
	.unmask	= t7l66xb_unmask_irq,
};

/* Install the IRQ handler */
static void t7l66xb_setup_irq(struct t7l66xb_data *tchip)
{
	int i;

	for (i = 0; i < T7L66XB_NR_IRQS; i++) {
		int irq = tchip->irq_base + i;
		set_irq_chip (irq, &t7l66xb_chip);
		set_irq_chip_data (irq, tchip);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	set_irq_data (tchip->irq_nr, tchip);
        set_irq_chained_handler (tchip->irq_nr, t7l66xb_irq_handler);
	set_irq_type (tchip->irq_nr, IRQT_FALLING);
}

static void t7l66xb_hwinit(struct platform_device *dev)
{
	struct t7l66xb_platform_data *pdata = platform_get_platdata(dev);

	if (pdata && pdata->hw_init)
		pdata->hw_init();
}


#ifdef CONFIG_PM

static int t7l66xb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct t7l66xb_platform_data *pdata = platform_get_platdata(dev);


	if (pdata && pdata->suspend)
		pdata->suspend();

	return 0;
}

static int t7l66xb_resume(struct platform_device *dev)
{
	struct t7l66xb_platform_data *pdata = platform_get_platdata(dev);

	if (pdata && pdata->resume)
		pdata->resume();

	t7l66xb_hwinit(dev);

	return 0;
}
#endif

static int t7l66xb_probe(struct platform_device *dev)
{
	struct t7l66xb_platform_data *pdata = dev->dev.platform_data;
	unsigned long pbase = (unsigned long)dev->resource[0].start;
	unsigned long plen = dev->resource[0].end - dev->resource[0].start;
	int err = -ENOMEM;
	struct t7l66xb_data *data;

	data = kmalloc (sizeof (struct t7l66xb_data), GFP_KERNEL);
	if (!data)
		goto out;

	data->irq_base = pdata->irq_base;
	data->irq_nr   = dev->resource[1].start;

	if (!data->irq_base) {
		printk("t7166xb: uninitialized irq_base!\n");
		goto out_free_data;
	}

	data->mapbase = ioremap(pbase, plen);
	if(!data->mapbase)
		goto out_free_irqs;

	platform_set_drvdata(dev, data);
	t7l66xb_setup_irq(data);
	t7l66xb_hwinit(dev);

	/* Mask IRQs  -- should we mask/unmask on suspend/resume? */
	writew(0xbf, data->mapbase + T7L66XB_SYS_IMR);

	printk(KERN_INFO "%s rev %d @ 0x%08lx using irq %d-%d on irq %d\n",
	       dev->name,  readw(data->mapbase + T7L66XB_SYS_RIDR),
	       (unsigned long)data->mapbase, data->irq_base,
	       data->irq_base + T7L66XB_NR_IRQS - 1, data->irq_nr);

	data->devices = soc_add_devices(dev, t7l66xb_devices,
	                                ARRAY_SIZE(t7l66xb_devices),
	                                &dev->resource[0], 0, data->irq_base);

	if(!data->devices){
		printk(KERN_INFO "%s: Failed to allocate devices!\n",
		       dev->name);
		goto out_free_devices;
	}

	return 0;

out_free_devices:
// FIXME!!!	t7l66xb_remove_devices(data);
out_free_irqs:
out_free_data:
	kfree(data);
out:
	return err;
}

static int t7l66xb_remove(struct platform_device *dev)
{
	struct t7l66xb_data *tchip = platform_get_drvdata(dev);
	int i;

	/* Free the subdevice resources */
	for (i = 0; i < tchip->ndevices; i++) {
		platform_device_unregister (&tchip->devices[i]);
		kfree (tchip->devices[i].resource);
	}

	/* Take down IRQ handling */
        for (i = 0; i < T7L66XB_NR_IRQS; i++) {
                int irq = i + tchip->irq_base;
                set_irq_handler (irq, NULL);
                set_irq_chip (irq, NULL);
                set_irq_chip_data (irq, NULL);
        }
	set_irq_chained_handler (tchip->irq_nr, NULL);
	// Free IRQ ?

	//FIXME - put chip to sleep?
	
	/* Free core resources */
	iounmap (tchip->mapbase);
//	t7l66xb_remove_devices(tchip); FIXME!!!
	kfree (tchip);

	return 0;
}


static struct platform_driver t7l66xb_platform_driver = {
	.driver = {
		.name		= "t7l66xb",
	},
	.probe		= t7l66xb_probe,
	.remove		= t7l66xb_remove,
#ifdef CONFIG_PM
	.suspend	= t7l66xb_suspend,
	.resume		= t7l66xb_resume,
#endif
};


static int __init t7l66xb_init(void)
{
	int retval = 0;
		
	retval = platform_driver_register (&t7l66xb_platform_driver);
	return retval;
}

static void __exit t7l66xb_exit(void)
{
	platform_driver_unregister(&t7l66xb_platform_driver);
}

#ifdef MODULE
module_init(t7l66xb_init);
#else	/* start early for dependencies */
subsys_initcall(t7l66xb_init);
#endif
module_exit(t7l66xb_exit);

MODULE_DESCRIPTION("Toshiba T7L66XB core driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dirk Opfer and Ian Molton");
