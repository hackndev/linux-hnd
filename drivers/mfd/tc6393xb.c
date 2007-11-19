/*
 * linux/arch/arm/common/tc6393xb_soc.c
 *
 * Toshiba TC6393 support
 * Copyright (c) 2005 Dirk Opfer
 * Copyright (c) 2005 Ian Molton <spyro@f2s.com>
 *
 *	Based on code written by Sharp/Lineo for 2.4 kernels
 *	Based on locomo.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains all generic TC6393 support.
 *
 * 19/03/2005 IM Fixed IRQ code
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
#include <linux/mfd/tmio_nand.h>
#include <linux/mfd/tmio_ohci.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/arch/pxa-regs.h>

#include <linux/mfd/tc6393.h>
#include "soc-core.h"

#define platform_get_platdata(_dev)      ((_dev)->dev.platform_data)

struct tc6393xb_data
{
	int irq_base, irq_nr;
	void *mapbase;
	struct platform_device *devices;
	int ndevices;
};

/* Setup the TC6393XB NAND flash controllers configuration registers */
static void tc6393xb_nand_hwinit(struct platform_device *sdev) {
	
	struct tc6393xb_data *chip = platform_get_drvdata(sdev);
	
	/* Sequence: 
	 * SMD Buffer ON (gpio related)
	 * Enable the clock (SCRUNEN)
	 * Set the ctl reg base address
	 * Enable the ctl reg
	 * Configure power control (control bt PCNT[1,0] 4ms startup delay)
	 */
	printk("nand_hwinit(0x%08lx)\n", (unsigned long)chip->mapbase);
	/* (89h) SMD Buffer ON By TC6393XB SystemConfig gpibfc1*/
	writew(0xff, chip->mapbase + TC6393_SYS_GPIBCR1);

//	return 0;
}

static void tc6393xb_nand_suspend(struct platform_device *sdev) {
	printk("nand_suspend()\n");
//	return 0;
}

static void tc6393xb_nand_resume(struct platform_device *sdev) {
	printk("nand_resume()\n");
//	return 0;
}

static struct tmio_nand_hwconfig tc6393xb_nand_hwconfig = {
	.hwinit  = tc6393xb_nand_hwinit,
	.suspend = tc6393xb_nand_suspend,
	.resume  = tc6393xb_nand_resume,
};

static void tc6393xb_mmc_set_clock(struct platform_device *sdev, int state) {
	struct tc6393xb_data *chip = platform_get_drvdata(sdev);
	unsigned char tmp;

	printk("mmc_set_clock(0x%08lx)\n", (unsigned long)chip->mapbase);
	if(state == MMC_CLOCK_ENABLED){
		tmp = readw(chip->mapbase + TC6393_SYS_GPIBCR1);
		writew(tmp | CK32KEN, chip->mapbase + TC6393_SYS_GPIBCR1);
	//FIXME - need to disable too...
	}
}

static struct tmio_mmc_hwconfig tc6393xb_mmc_hwconfig = {
        .set_mmc_clock  = tc6393xb_mmc_set_clock,
};

#if 0
static void tc6393xb_ohci_hwinit(struct platform_device *sdev) {
	struct tc6393xb_data *chip =  platform_get_drvdata(sdev);
	unsigned char tmp;

	// enable usb clock - sysconf + 0x98, halfword, bit D1 (0x2)
	// FIXME - setting this reg could use some locking...
	tmp = readw(chip->mapbase + TC6393_SYS_GPIBCR1);
	writew(tmp | USBCLK, chip->mapbase + TC6393_SYS_GPIBCR1);
	// enable usb function - sysconf + 0xe0  bit D0 (0x1)
	writeb(0x1, chip->mapbase + 0xe0);
}

static struct tmio_ohci_hwconfig tc6393xb_ohci_hwconfig = {
        .hwinit  = tc6393xb_ohci_hwinit,
};
#endif

static struct resource tc6393xb_mmc_resources[] = {
	{
		.name = "control",
		.start = TC6393XB_MMC_CTL_BASE,
		.end   = TC6393XB_MMC_CTL_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "config",
		.start = TC6393XB_MMC_CNF_BASE,
		.end   = TC6393XB_MMC_CNF_BASE + 0xff,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TC6393XB_MMC_IRQ,
		.end   = TC6393XB_MMC_IRQ,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct resource tc6393xb_nand_resources[] = {
	{
		.name = "control",
		.start = TC6393XB_NAND_CTL_BASE,
		.end   = TC6393XB_NAND_CTL_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "config",
		.start = TC6393XB_NAND_CNF_BASE,
		.end   = TC6393XB_NAND_CNF_BASE + 0xff,
		.flags = IORESOURCE_MEM,
	},
};

static struct soc_device_data tc6393xb_devices[] = {
	{
		.name = "tmio_mmc",
		.res = tc6393xb_mmc_resources,
		.num_resources = ARRAY_SIZE(tc6393xb_mmc_resources),
		.hwconfig = &tc6393xb_mmc_hwconfig,
	},
	{
		.name = "tmio-nand",
		.res = tc6393xb_nand_resources,
		.num_resources = ARRAY_SIZE(tc6393xb_nand_resources),
		.hwconfig = &tc6393xb_nand_hwconfig,
	},
};

/** TC6393 interrupt handling stuff.
 * NOTE: TC6393 has a 1 to many mapping on all of its IRQs.
 * that is, there is only one real hardware interrupt
 * we determine which interrupt it is by reading some IO memory.
 * We have two levels of expansion, first in the handler for the
 * hardware interrupt we generate an interrupt
 * IRQ_TC6393_*_BASE and those handlers generate more interrupts
 *
 */
static void tc6393xb_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int req, i;
	struct tc6393xb_data *data = get_irq_data(irq);
	
	/* Acknowledge the parent IRQ */
	desc->chip->ack(irq);
  
	while ( (req = (readb(data->mapbase + TC6393_SYS_ISR)
			& ~(readb(data->mapbase + TC6393_SYS_IMR)))) ) {
		for (i = 0; i <= 7; i++) {
			int dev_irq = data->irq_base + i;
			struct irq_desc *d = NULL;
			if ((req & (1<<i))) {
				if(i != 1)  printk("IRQ! from %d\n", i);
				d = irq_desc + dev_irq;
				d->handle_irq(dev_irq, d);
			}
		}
	}
}

static void tc6393xb_mask_irq(unsigned int irq)
{
	struct tc6393xb_data *tc6393 = get_irq_chip_data(irq);

	writeb(readb(tc6393->mapbase + TC6393_SYS_IMR) | 1 << (irq - tc6393->irq_base),tc6393->mapbase + TC6393_SYS_IMR);
}

static void tc6393xb_unmask_irq(unsigned int irq)
{
	struct tc6393xb_data *tc6393 = get_irq_chip_data(irq);

	writeb(readb(tc6393->mapbase + TC6393_SYS_IMR) & ~( 1 << (irq - tc6393->irq_base)),tc6393->mapbase + TC6393_SYS_IMR);
}

static struct irq_chip tc6393xb_chip = {
	.ack	= tc6393xb_mask_irq,
	.mask	= tc6393xb_mask_irq,
	.unmask	= tc6393xb_unmask_irq,
};


static void tc6393xb_setup_irq(struct tc6393xb_data *tchip)
{
	int i;
	
	for (i = 0; i < TC6393XB_NR_IRQS; i++) {
		int irq = tchip->irq_base + i;
		set_irq_chip (irq, &tc6393xb_chip);
		set_irq_chip_data (irq, tchip);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	set_irq_data (tchip->irq_nr, tchip);
	set_irq_chained_handler (tchip->irq_nr, tc6393xb_irq_handler);
	set_irq_type (tchip->irq_nr, IRQT_FALLING);
}

void tc6393xb_hwinit(struct platform_device *dev)
{
	struct tc6393xb_platform_data *pdata = platform_get_platdata(dev);
	struct tc6393xb_data *tchip =  platform_get_drvdata(dev);

	if(!pdata || !tchip){
		BUG_ON("no driver data!\n");
		return;
	}

	if (pdata->hw_init)
		pdata->hw_init();
		
	writew(0, tchip->mapbase + TC6393_SYS_FER);

	/* Clock setting */
	writew(pdata->sys_pll2cr,   tchip->mapbase + TC6393_SYS_PLL2CR);
	writew(pdata->sys_ccr,      tchip->mapbase + TC6393_SYS_CCR);
	writew(pdata->sys_mcr,      tchip->mapbase + TC6393_SYS_MCR);

  	/* GPIO */
	writew(pdata->sys_gper,     tchip->mapbase + TC6393_SYS_GPER);
	writew(pdata->sys_gpodsr1,  tchip->mapbase + TC6393_SYS_GPODSR1);
	writew(pdata->sys_gpooecr1, tchip->mapbase + TC6393_SYS_GPOOECR1);
}


#ifdef CONFIG_PM

static int tc6393xb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct tc6393xb_platform_data *pdata = platform_get_platdata(dev);

	if (pdata && pdata->suspend)
		pdata->suspend();

	return 0;
}

static int tc6393xb_resume(struct platform_device *dev)
{
	struct tc6393xb_platform_data *pdata = platform_get_platdata(dev);

	if (pdata && pdata->resume)
		pdata->resume();

	tc6393xb_hwinit(dev);

	return 0;
}

#else
#define tc6393xb_suspend NULL
#define tc6393xb_resume NULL
#endif

static int tc6393xb_probe(struct platform_device *dev)
{
	struct tc6393xb_platform_data *pdata = dev->dev.platform_data;
	unsigned long pbase = (unsigned long)dev->resource[0].start;
	unsigned long plen = dev->resource[0].end - dev->resource[0].start;
	int err = -ENOMEM;
	struct tc6393xb_data *data;

	data = kmalloc (sizeof (struct tc6393xb_data), GFP_KERNEL);
	if (!data)
		goto out;

	data->irq_base = pdata->irq_base;
	data->irq_nr = dev->resource[1].start;

	if (!data->irq_base) {
		printk("tc6393xb: uninitialized irq_base!\n");
		goto out_free_data;
	}

	data->mapbase = ioremap(pbase, plen);
	if(!data->mapbase)
		goto out_free_irqs;

	platform_set_drvdata(dev, data);
	tc6393xb_setup_irq (data);
	tc6393xb_hwinit(dev);

	/* Enable (but mask!) our IRQs */
	writew(0,    data->mapbase + TC6393_SYS_IRR);
	writew(0xbf, data->mapbase + TC6393_SYS_IMR);

	printk(KERN_INFO "%s rev %d @ 0x%08lx using irq %d-%d on irq %d\n",
	       dev->name,  readw(data->mapbase + TC6393_SYS_RIDR),
	       (unsigned long)data->mapbase, data->irq_base,
	       data->irq_base + TC6393XB_NR_IRQS - 1, data->irq_nr);

	data->devices =  soc_add_devices(dev, tc6393xb_devices,
	                                ARRAY_SIZE(tc6393xb_devices),
	                                &dev->resource[0], 0, data->irq_base);

	if(!data->devices) {
		printk(KERN_INFO "%s: Failed to allocate devices!\n",
		       dev->name);
		goto out_free_devices;
	}

	return 0;

out_free_devices:
//FIXME	tc6393xb_remove_devices(data);
out_free_irqs:
out_free_data:
	kfree(data);
out:
	return err;
}

static int tc6393xb_remove(struct platform_device *dev)
{
	struct tc6393xb_data *tchip = platform_get_drvdata(dev);
	int i;

	/* Free the subdevice resources */
        for (i = 0; i < tchip->ndevices; i++) {
                platform_device_unregister (&tchip->devices[i]);
                kfree (tchip->devices[i].resource);
        }

	/* Take down IRQ handling */
	for (i = 0; i < TC6393XB_NR_IRQS; i++) {
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
//FIXME        tc6393xb_remove_devices(tchip);
        kfree (tchip);

	return 0;
}


static struct platform_driver tc6393xb_device_driver = {
	.driver = {
		.name		= "tc6393xb",
	},
	.probe		= tc6393xb_probe,
	.remove		= tc6393xb_remove,
#ifdef CONFIG_PM
	.suspend	= tc6393xb_suspend,
	.resume		= tc6393xb_resume,
#endif
};


static int __init tc6393xb_init(void)
{
	int retval = 0;
	retval = platform_driver_register (&tc6393xb_device_driver);
	return retval;
}

static void __exit tc6393xb_exit(void)
{
	platform_driver_unregister(&tc6393xb_device_driver);
}

#ifdef MODULE
module_init(tc6393xb_init);
#else	/* start early for dependencies */
subsys_initcall(tc6393xb_init);
#endif
module_exit(tc6393xb_exit);

MODULE_DESCRIPTION("Toshiba TC6393 core driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dirk Opfer and Ian Molton");
