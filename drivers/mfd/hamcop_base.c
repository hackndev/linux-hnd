/*
 * Driver interface to the Samsung HAMCOP Companion chip
 *
 * HAMCOP: iPAQ H22xx
 *	  Specifications: http://www.handhelds.org/platforms/hp/ipaq-h22xx/
 *
 * Copyright © 2003, 2004 Compaq Computer Corporation.
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/soc-old.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/ds1wm.h>
#include <linux/mfd/hamcop_base.h>
#include <linux/mfd/samcop_adc.h>
#include <linux/touchscreen-adc.h>

#include <asm/types.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/irq.h>
#include <asm/arch/clock.h>

#include <asm/arch/h2200.h>
#include <asm/arch/h2200-gpio.h>
#include <asm/hardware/ipaq-hamcop.h>

#include <asm/hardware/samcop-dma.h>
#include <asm/hardware/samcop-sdi.h>
#include "../mmc/samcop_sdi.h"


struct hamcop_data
{
	int irq_base, irq_nr;
	void *mapping;
	spinlock_t gpio_lock;
	unsigned long irqmask;
	struct platform_device **devices;
	int ndevices;
	u16 save_clocks;

	int led_on[HAMCOP_NUMBER_OF_LEDS];
	struct clk *clk_led;
} *hd; /* XXX hd is a hack */

static int hamcop_remove (struct platform_device *pdev);

/***********************************************************************************/
/*      Register access                                                            */
/***********************************************************************************/

static inline void
hamcop_write_register (struct hamcop_data *hamcop, u32 addr, u16 value)
{
	__raw_writew (value, (unsigned long)hamcop->mapping + addr);
}

static inline u16
hamcop_read_register (struct hamcop_data *hamcop, u32 addr)
{
	return __raw_readw ((unsigned long)hamcop->mapping + addr);
}

/***********************************************************************************/
/*      ASIC IRQ demux                                                             */
/***********************************************************************************/

#define MAX_ASIC_ISR_LOOPS    1000

static void
hamcop_irq_demux (unsigned int irq, struct irq_desc *desc)
{
	int i;
	struct hamcop_data *hamcop;
	u32 pending;

	hamcop = desc->handler_data;

	for (i = 0 ; (i < MAX_ASIC_ISR_LOOPS); i++) {

		int asic_irq;

		pending = hamcop_read_register (hamcop, HAMCOP_IC_INTPND0);
		pending |= hamcop_read_register (hamcop, HAMCOP_IC_INTPND1) << 16;
		if (!pending)
			break;

		asic_irq = hamcop->irq_base + (31 - __builtin_clz (pending));
		desc = irq_desc + asic_irq;
		desc->handle_irq(asic_irq, desc);
	}

	if (unlikely (i >= MAX_ASIC_ISR_LOOPS && pending)) {
		u16 val;
		printk("%s: interrupt processing overrun pending=0x%08x\n", __FUNCTION__, pending);

		val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK0);
		val |= pending & 0xffff;
		hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0, val);

		val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK1);
		val |= (pending >> 16) & 0xffff;
		hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1, val);

		hamcop_write_register (hamcop, HAMCOP_IC_SRCPND0, pending & 0xffff);
		hamcop_write_register (hamcop, HAMCOP_IC_SRCPND1, (pending >> 16) & 0xffff);

		hamcop_write_register (hamcop, HAMCOP_IC_INTPND0, pending & 0xffff);
		hamcop_write_register (hamcop, HAMCOP_IC_INTPND1, (pending >> 16) & 0xffff);
	}
}

/***********************************************************************************/
/*      IRQ handling                                                               */
/***********************************************************************************/

/* ack <- IRQ is first serviced.
       mask <- IRQ is disabled.
     unmask <- IRQ is enabled */

static void
hamcop_asic_ack_ic_irq (unsigned int irq)
{
	struct hamcop_data *hamcop = get_irq_chip_data(irq);
	int mask = 1 << (irq - hamcop->irq_base);

	hamcop_write_register (hamcop, HAMCOP_IC_SRCPND0, mask & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_SRCPND1, (mask >> 16) & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTPND0, mask & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTPND1, (mask >> 16) & 0xffff);
}

static void
hamcop_asic_mask_ic_irq (unsigned int irq)
{
	struct hamcop_data *hamcop = get_irq_chip_data(irq);
	unsigned int mask = 1 << (irq - hamcop->irq_base);
	u16 val;

	val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK0);
	val |= mask & 0xffff;
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0, val);
	val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK1);
	val |= (mask >> 16) & 0xffff;
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1, val);
}

static void
hamcop_asic_unmask_ic_irq (unsigned int irq)
{
	struct hamcop_data *hamcop = get_irq_chip_data(irq);
	unsigned int mask = 1 << (irq - hamcop->irq_base);
	u16 val;

	val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK0);
	val &= ~(mask & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0, val);
	val = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK1);
	val &= ~((mask >> 16) & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1, val);
}

static struct irq_chip hamcop_ic_irq_chip = {
	.ack		= hamcop_asic_ack_ic_irq,
	.mask		= hamcop_asic_mask_ic_irq,
	.unmask		= hamcop_asic_unmask_ic_irq,
};

static void __init
hamcop_irq_init (struct hamcop_data *hamcop)
{
	int i;

	for (i = 0; i < HAMCOP_NR_IRQS; i++) {
		int irq = hamcop->irq_base + i;
		set_irq_chip (irq, &hamcop_ic_irq_chip);
		set_irq_chip_data(irq, hamcop);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	/* all ints off */
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0, 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1, 0xffff);

	/* clear out any pending irqs */
	for (i = 0; i < 32; i++) {
		if (hamcop_read_register (hamcop, HAMCOP_IC_INTPND0) == 0
		    && hamcop_read_register (hamcop, HAMCOP_IC_INTPND1) == 0)
			break;

		hamcop_write_register (hamcop, HAMCOP_IC_SRCPND0, 0xffff);
		hamcop_write_register (hamcop, HAMCOP_IC_SRCPND1, 0xffff);
		hamcop_write_register (hamcop, HAMCOP_IC_INTPND0, 0xffff);
		hamcop_write_register (hamcop, HAMCOP_IC_INTPND1, 0xffff);
	}

	set_irq_chained_handler (hamcop->irq_nr, hamcop_irq_demux);
	set_irq_type (hamcop->irq_nr, IRQT_FALLING);
	set_irq_data (hamcop->irq_nr, hamcop);
}

unsigned int
hamcop_irq_base (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop->irq_base;
}
EXPORT_SYMBOL(hamcop_irq_base);

/*************************************************************/

static void
hamcop_clock_enable (struct clk *clk)
{
	u16 val;
	unsigned long flags;

	local_irq_save (flags);

	val = hamcop_read_register (hd, HAMCOP_CPM_ClockControl);
	val |= clk->ctrlbit;
	hamcop_write_register (hd, HAMCOP_CPM_ClockControl, val);

	local_irq_restore (flags);
}

static void
hamcop_clock_disable (struct clk *clk)
{
	u16 val;
	unsigned long flags;

	local_irq_save (flags);

	val = hamcop_read_register (hd, HAMCOP_CPM_ClockControl);
	val &= ~clk->ctrlbit;
	hamcop_write_register (hd, HAMCOP_CPM_ClockControl, val);

	local_irq_restore (flags);
}

static int
hamcop_gclk (void *data)
{
	struct hamcop_data *hamcop = data;
	unsigned int mclk = get_memclk_frequency_10khz() * 10000;
	return (hamcop_read_register (hamcop, HAMCOP_CPM_ClockSleep) & HAMCOP_CPM_CLKSLEEP_CLKSEL) ? mclk : mclk / 2;
}

/*************************************************************/

/*
 * GPA accessors
 */

void
hamcop_set_gpio_a (struct device *dev, u32 mask, u16 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPA_DAT) & ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPA_DAT, val);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_a);

u16
hamcop_get_gpio_a (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPA_DAT);
}
EXPORT_SYMBOL(hamcop_get_gpio_a);

void
hamcop_set_gpio_a_con (struct device *dev, u16 mask, u16 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPA_CON) & ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPA_CON, val);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_a_con);

u16
hamcop_get_gpio_a_con (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPA_CON);
}
EXPORT_SYMBOL(hamcop_get_gpio_a_con);

void
hamcop_set_gpio_a_int (struct device *dev, u32 mask, u32 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPAINT0) |
	      (hamcop_read_register (hamcop, HAMCOP_GPIO_GPAINT1) << 16);
	val &= ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPAINT0, val & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPAINT1,
			       (val >> 16) & 0xffff);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_a_int);

u32 hamcop_get_gpio_a_int(struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPAINT0) |
	      (hamcop_read_register (hamcop, HAMCOP_GPIO_GPAINT1) << 16);
}
EXPORT_SYMBOL(hamcop_get_gpio_a_int);

void
hamcop_set_gpio_a_flt (struct device *dev, int gpa_n, u16 ctrl, u16 width)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPA_FLT0(gpa_n), ctrl);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPA_FLT1(gpa_n), width);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_a_flt);

/*
 * GPB accessors
 */

void
hamcop_set_gpio_b (struct device *dev, u32 mask, u16 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPB_DAT) & ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPB_DAT, val);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_b);

u16
hamcop_get_gpio_b (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPB_DAT);
}
EXPORT_SYMBOL(hamcop_get_gpio_b);

void
hamcop_set_gpio_b_con (struct device *dev, u16 mask, u16 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPB_CON) & ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPB_CON, val);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_b_con);

u16
hamcop_get_gpio_b_con (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPB_CON);
}
EXPORT_SYMBOL(hamcop_get_gpio_b_con);

void
hamcop_set_gpio_b_int (struct device *dev, u32 mask, u32 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_GPBINT0) |
	      (hamcop_read_register (hamcop, HAMCOP_GPIO_GPBINT1) << 16);
	val &= ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPBINT0, val & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPBINT1,
			       (val >> 16) & 0xffff);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_b_int);

u32 hamcop_get_gpio_b_int(struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_GPBINT0) |
	      (hamcop_read_register (hamcop, HAMCOP_GPIO_GPBINT1) << 16);
}
EXPORT_SYMBOL(hamcop_get_gpio_b_int);

void
hamcop_set_gpio_b_flt (struct device *dev, int gpa_n, u16 ctrl, u16 width)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave (&hamcop->gpio_lock, flags);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPB_FLT0(gpa_n), ctrl);
	hamcop_write_register (hamcop, HAMCOP_GPIO_GPB_FLT1(gpa_n), width);
	spin_unlock_irqrestore (&hamcop->gpio_lock, flags);
}
EXPORT_SYMBOL(hamcop_set_gpio_b_flt);


u16
hamcop_get_gpiomon (struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop_read_register (hamcop, HAMCOP_GPIO_MON);
}
EXPORT_SYMBOL(hamcop_get_gpiomon);

/*************************************************************
 Miscellaneous HAMCOP knobs
 *************************************************************/

void
hamcop_set_spucr (struct device *dev, u16 mask, u16 bits)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags;
	u16 val;

	local_irq_save (flags);
	val = hamcop_read_register (hamcop, HAMCOP_GPIO_SPUCR);
	val &= ~mask;
	val |= bits;
	hamcop_write_register (hamcop, HAMCOP_GPIO_SPUCR, val);
	local_irq_restore (flags);
}
EXPORT_SYMBOL(hamcop_set_spucr);

void
hamcop_set_ifmode (struct device *dev, u16 mode)
{
	struct hamcop_data *hamcop = dev->driver_data;
	unsigned long flags;

	local_irq_save (flags);

	hamcop_write_register (hamcop, HAMCOP_CIF_IFMODE, mode);

	local_irq_restore (flags);
}
EXPORT_SYMBOL(hamcop_set_ifmode);


/*************************************************************
 HAMCOP SD hooks
 *************************************************************/

int
hamcop_dma_needs_bounce (struct device *dev, dma_addr_t addr, size_t size)
{
	return (addr + size >= H2200_HAMCOP_BASE + HAMCOP_SRAM_Base + HAMCOP_SRAM_Size);
}

static u64 hamcop_sdi_dmamask = 0xffffffffUL;


/* Note that HAMCOP's SRAM is controlled by the NAND clock. If the NAND or
   SD driver turns off the NAND clock while the other is still in use, there
   will be problems. Also, suspend/resume will not work if the SRAM is not
   powered up. */

static void
hamcop_sdi_init(struct device *dev)
{
	struct hamcop_data *hamcop = dev->parent->driver_data;
	struct samcop_sdi_data *plat = dev->platform_data;

	/* Initialize some run-time values. */
	plat->f_min = hamcop_gclk(hamcop) / 256; /* Divisor is SDIPRE + 1. */
	plat->f_max = hamcop_gclk(hamcop) / 1; /* SAMCOP docs: must be > 0 */
#ifdef HAMCOP_SDI_NOBOUNCE
	plat->dma_buf_virt = hamcop->mapping + HAMCOP_SRAM_Base;
#endif

	/* Configure dmabounce. */
	dev->dma_mask = &hamcop_sdi_dmamask;
	dev->coherent_dma_mask = 0xffffffffUL;
	dmabounce_register_dev(dev, 512, 4096);
	pxa_set_dma_needs_bounce(hamcop_dma_needs_bounce);
}

static void
hamcop_sdi_exit(struct device *dev)
{
	dmabounce_unregister_dev(dev);

}

static u32
hamcop_sdi_read_register(struct device *dev, u32 reg)
{
	struct hamcop_data *hamcop = dev->parent->driver_data;

	switch (reg) {

	case SAMCOP_SDICON:
		return hamcop_read_register(hamcop, HAMCOP_SDI_CON);
		break;

	case SAMCOP_SDIPRE:
		return hamcop_read_register(hamcop, HAMCOP_SDI_PRE);
		break;

	case SAMCOP_SDICMDARG:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_CMDARGH) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_CMDARGL);
		break;

	case SAMCOP_SDICMDCON:
		return hamcop_read_register(hamcop, HAMCOP_SDI_CMDCON);
		break;

	case SAMCOP_SDICMDSTAT:
		return hamcop_read_register(hamcop, HAMCOP_SDI_CMDSTAT);
		break;

	case SAMCOP_SDIRSP0:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_RSP0H) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_RSP0L);
		break;

	case SAMCOP_SDIRSP1:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_RSP1H) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_RSP1L);
		break;

	case SAMCOP_SDIRSP2:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_RSP2H) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_RSP2L);
		break;

	case SAMCOP_SDIRSP3:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_RSP3H) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_RSP3L);
		break;

	case SAMCOP_SDITIMER:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_TIMER1) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_TIMER2);
		break;

	case SAMCOP_SDIBSIZE:
		return hamcop_read_register(hamcop, HAMCOP_SDI_BSIZE);
		break;

	case SAMCOP_SDIDCON:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_DCON) << 12) |
		       (hamcop_read_register(hamcop, HAMCOP_SDI_BNUM) & 0xfff);
		break;

	case SAMCOP_SDIDCNT:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_DCNT1) << 12) |
		       (hamcop_read_register(hamcop, HAMCOP_SDI_DCNT2) & 0xfff);
		break;

	case SAMCOP_SDIDSTA:
		return hamcop_read_register(hamcop, HAMCOP_SDI_DSTA);
		break;

	case SAMCOP_SDIFSTA:
		return hamcop_read_register(hamcop, HAMCOP_SDI_FSTA);
		break;

	case SAMCOP_SDIDATA:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_DATA) << 16) |
			hamcop_read_register(hamcop, HAMCOP_SDI_DATA);
		break;

	case SAMCOP_SDIIMSK:
		return (hamcop_read_register(hamcop, HAMCOP_SDI_IMSK1) << 5) |
		       (hamcop_read_register(hamcop, HAMCOP_SDI_IMSK2) & 0x1f);
		break;
	}

	/* Error: unknown meta-register */
	return 0;
}


static void
hamcop_sdi_write_register(struct device *dev, u32 reg, u32 val)
{
	struct hamcop_data *hamcop = dev->parent->driver_data;

	switch (reg) {

	case SAMCOP_SDICON:
		hamcop_write_register(hamcop, HAMCOP_SDI_CON, val);
		break;

	case SAMCOP_SDIPRE:
		hamcop_write_register(hamcop, HAMCOP_SDI_PRE, val);
		break;

	case SAMCOP_SDICMDARG:
		hamcop_write_register(hamcop, HAMCOP_SDI_CMDARGH, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_CMDARGL, val & 0xffff);
		break;

	case SAMCOP_SDICMDCON:
		hamcop_write_register(hamcop, HAMCOP_SDI_CMDCON, val);
		break;

	case SAMCOP_SDICMDSTAT:
		hamcop_write_register(hamcop, HAMCOP_SDI_CMDSTAT, val);
		break;

	case SAMCOP_SDIRSP0:
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP0H, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP0L, val & 0xffff);
		break;

	case SAMCOP_SDIRSP1:
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP1H, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP1L, val & 0xffff);
		break;

	case SAMCOP_SDIRSP2:
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP2H, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP2L, val & 0xffff);
		break;

	case SAMCOP_SDIRSP3:
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP3H, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_RSP3L, val & 0xffff);
		break;

	case SAMCOP_SDITIMER:
		hamcop_write_register(hamcop, HAMCOP_SDI_TIMER1, val >> 16);
		hamcop_write_register(hamcop, HAMCOP_SDI_TIMER2, val & 0xffff);
		break;

	case SAMCOP_SDIBSIZE:
		hamcop_write_register(hamcop, HAMCOP_SDI_BSIZE, val);
		break;

	case SAMCOP_SDIDCON:
		hamcop_write_register(hamcop, HAMCOP_SDI_BNUM, val & 0xfff);
		hamcop_write_register(hamcop, HAMCOP_SDI_DCON, val >> 12);
		break;

	case SAMCOP_SDIDCNT:
		hamcop_write_register(hamcop, HAMCOP_SDI_DCNT1, val >> 12);
		hamcop_write_register(hamcop, HAMCOP_SDI_DCNT2, val & 0xfff);
		break;

	case SAMCOP_SDIDSTA:
		hamcop_write_register(hamcop, HAMCOP_SDI_DSTA, val);
		break;

	case SAMCOP_SDIFSTA:
		hamcop_write_register(hamcop, HAMCOP_SDI_FSTA, val);
		break;

	case SAMCOP_SDIDATA:
		hamcop_write_register(hamcop, HAMCOP_SDI_DATA, val & 0xffff);
		hamcop_write_register(hamcop, HAMCOP_SDI_DATA, val >> 16);
		break;

	case SAMCOP_SDIIMSK:
		hamcop_write_register(hamcop, HAMCOP_SDI_IMSK1, val >> 5);
		hamcop_write_register(hamcop, HAMCOP_SDI_IMSK2, val & 0x1f);
		break;
	}

}

static void
hamcop_sdi_card_power(struct device *dev, int on, int clock_req)
{
	struct hamcop_data *hamcop = dev->parent->driver_data;
	u16 sdi_psc, sdi_con;

	/* Set power. */
	sdi_con = hamcop_read_register(hamcop, HAMCOP_SDI_CON);

	if (on) {
		/* XXX Configure SD_DETECT_N and SD_POWER_EN GPIOs? */
		SET_H2200_GPIO(SD_POWER_EN, 1);

		hamcop_set_spucr(dev->parent, HAMCOP_GPIO_SPUCR_PU_SDCM |
				 HAMCOP_GPIO_SPUCR_PU_SDDA, 0);

		/* XXX Should we only do this if
		       mmc->ios.bus_width == MMC_BUS_WIDTH_4? */
		sdi_con &= ~HAMCOP_SDI_CON_CLOCKTYPE_MMC;

		sdi_con |= SAMCOP_SDICON_FIFORESET;

	} else {

		SET_H2200_GPIO(SD_POWER_EN, 0);
		hamcop_set_spucr(dev->parent,
				 HAMCOP_GPIO_SPUCR_PU_SDCM |
				 HAMCOP_GPIO_SPUCR_PU_SDDA,
				 HAMCOP_GPIO_SPUCR_PU_SDCM |
				 HAMCOP_GPIO_SPUCR_PU_SDDA);
	}

	/* Set clock. */
	sdi_psc = clock_req ? (hamcop_gclk(hamcop) / clock_req) - 1 : 0;
	if (hamcop_gclk(hamcop) * (sdi_psc + 1) > clock_req)
		sdi_psc++;

	if (sdi_psc > 255)
		sdi_psc = 255;

	hamcop_write_register(hamcop, HAMCOP_SDI_CON, sdi_con);
	hamcop_write_register(hamcop, HAMCOP_SDI_PRE, sdi_psc);

	/* Wait 74 SDCLKs to allow the card to initialize. */
	udelay((74 * (sdi_psc + 1) * 1000000) / hamcop_gclk(hamcop));
}


static struct samcop_sdi_data hamcop_sdi_data = {
	.init		= hamcop_sdi_init,
	.exit		= hamcop_sdi_exit,
	.read_reg	= hamcop_sdi_read_register,
	.write_reg	= hamcop_sdi_write_register,
	.card_power	= hamcop_sdi_card_power,

	/* card detect IRQ */
	.irq_cd		= H2200_IRQ(SD_DETECT_N),

	/* These two get filled in by hamcop_sdi_init(). */
	.f_min		= 0,
	.f_max		= 0,

	/* HAMCOP has 16KB of SRAM, so 16K / 512 = 32 sectors max. */
	.max_sectors	= 32,

	.timeout	= HAMCOP_SDI_TIMER_MAX,

	/* DMA settings */
	.dma_chan	= 0, /* HAMCOP has only one DMA channel */
	.dma_devaddr	= (void *)HAMCOP_SDI_DATA,
#ifdef HAMCOP_SDI_NOBOUNCE
	.dma_buf_size	= HAMCOP_SRAM_Size,
	.dma_buf_phys	= HAMCOP_SRAM_Base,
	.dma_buf_virt	= 0, /* gets filled in by hamcop_sdi_init() */
#endif
	.hwsrcsel	= HAMCOP_DCON_CH0_SD_UNSHIFTED << SAMCOP_DCON_SRCSHIFT,
	.xfer_unit	= 2, /* two bytes per DMA transfer */
};

/*************************************************************
 HAMCOP DMA hooks
 *************************************************************/

static u32
hamcop_dma_read_register(void __iomem *regs, u32 reg)
{
	u32 ret;

	switch (reg) {

	case SAMCOP_DMA_DISRC:
		ret =  __raw_readw(regs + _HAMCOP_DMA_DISRC0) << 29 |
		       __raw_readw(regs + _HAMCOP_DMA_DISRC1);
		break;

	case SAMCOP_DMA_DIDST:
		ret = __raw_readw(regs + _HAMCOP_DMA_DIDST0) << 29 |
		      __raw_readw(regs + _HAMCOP_DMA_DIDST1);
		break;

	case SAMCOP_DMA_DCON:
		ret = __raw_readw(regs + _HAMCOP_DMA_DCON0) << 20 |
		      __raw_readw(regs + _HAMCOP_DMA_DCON1);
		break;

	case SAMCOP_DMA_DSTAT:
		ret = __raw_readw(regs + _HAMCOP_DMA_DSTAT0) << 20 |
		      __raw_readw(regs + _HAMCOP_DMA_DSTAT1);
		break;

	case SAMCOP_DMA_DCSRC:
		ret = __raw_readw(regs + _HAMCOP_DMA_DCSRC);
		break;

	case SAMCOP_DMA_DCDST:
		ret = __raw_readw(regs + _HAMCOP_DMA_DCDST);
		break;

	case SAMCOP_DMA_DMASKTRIG:
		ret = __raw_readw(regs + _HAMCOP_DMA_DMASKTRIG);
		break;

	default:
		ret = 0;
		break;
	}

	return ret;
}


static void
hamcop_dma_write_register(void __iomem *regs, u32 reg, u32 val)
{
	switch (reg) {

	case SAMCOP_DMA_DISRC:
		__raw_writew(val >> 29,    regs + _HAMCOP_DMA_DISRC0);
		__raw_writew(val & 0x7fff, regs + _HAMCOP_DMA_DISRC1);
		break;

	case SAMCOP_DMA_DIDST:
		__raw_writew(val >> 29,    regs + _HAMCOP_DMA_DIDST0);
		__raw_writew(val & 0x7fff, regs + _HAMCOP_DMA_DIDST1);
		break;

	case SAMCOP_DMA_DCON:
		__raw_writew(val >> 20,    regs + _HAMCOP_DMA_DCON0);
		__raw_writew(val & 0xffff, regs + _HAMCOP_DMA_DCON1);
		break;

	case SAMCOP_DMA_DSTAT:
		__raw_writew(val >> 20,    regs + _HAMCOP_DMA_DSTAT0);
		__raw_writew(val & 0xffff, regs + _HAMCOP_DMA_DSTAT1);
		break;

	case SAMCOP_DMA_DCSRC:
		__raw_writew(val & 0x7fff, regs + _HAMCOP_DMA_DCSRC);
		break;

	case SAMCOP_DMA_DCDST:
		__raw_writew(val & 0x7fff, regs + _HAMCOP_DMA_DCDST);
		break;

	case SAMCOP_DMA_DMASKTRIG:
		__raw_writew(val, regs + _HAMCOP_DMA_DMASKTRIG);
		break;
	}
}

static struct samcop_dma_plat_data hamcop_dma_plat_data = {
	.n_channels	= 1,
	.read_reg	= hamcop_dma_read_register,
	.write_reg	= hamcop_dma_write_register,
};

/*************************************************************
 HAMCOP SRAM
 *************************************************************/

/* Returns a pointer to HAMCOP's SRAM in virtual address space. */
u8 *
hamcop_sram(struct device *dev)
{
	struct hamcop_data *hamcop = dev->driver_data;

	return hamcop->mapping + HAMCOP_SRAM_Base;
}
EXPORT_SYMBOL(hamcop_sram);

/*************************************************************
 HAMCOP LEDs
 *************************************************************/

/* Sets the given LED to the given value
 *
 * led_num: 0 to 2.
 * duty_time (unit: 1/8th s, max 31): 	time on
 * cycle_time (unit: 1/8th s, max 31): 	time on + time off
 *
 * If duty_time <= 0 or cycle_time <= 0, the LED is turned off
 *
 * Example: hamcop_set_led (dev, 2, 2, 4);
 *
 * LED controller specifications:
 * http://www.handhelds.org/platforms/hp/ipaq-h22xx/12-led_driver.pdf
 *
 * Blinking for a fixed number of times not implemented.
 */

/* Prescale value: divider for the led clock */
/* 4096 x 1 / 32768 = 1/8 s when using RTC clock (32.768KHz) */

#define LED_PRESCALER (32768 / 1/8)

void
hamcop_set_led (struct device *dev, int led_num, int duty_time, int cycle_time)
{
	struct hamcop_data *hamcop = dev->driver_data;
	u16 tmp_con0, con0, con1;
	int leds_on, i;

	if (led_num >= HAMCOP_NUMBER_OF_LEDS) {
		printk (KERN_WARNING "hamcop_set_led: illegal LED number %d, must be < %d\n", led_num, HAMCOP_NUMBER_OF_LEDS);
		return;
	}

	if (duty_time > 0 && cycle_time > 0) {

		if (cycle_time > 31) {
			printk (KERN_WARNING "hamcop_set_led: cycle_time > 32");
			cycle_time = 31;
		}

		if (duty_time > cycle_time) {
			printk (KERN_WARNING "hamcop_set_led: duty_time > cycle_time");
			duty_time = cycle_time;
		}

		con0 = HAMCOP_LED_LEDNCON0_ENB_ON		    |
		       HAMCOP_LED_LEDNCON0_INITV_ON		    |
		       (duty_time << HAMCOP_LED_LEDNCON0_CMP_SHIFT) |
		       HAMCOP_LED_LEDNCON0_CNTEN_OFF		    |
		       HAMCOP_LED_LEDNCON0_CLK_RTC;

		con1 = cycle_time << HAMCOP_LED_LEDNCON1_DIV_SHIFT;

		hamcop->led_on[led_num] = 1;

	} else {
		con0 = HAMCOP_LED_LEDNCON0_ENB_OFF;
		con1 = 0;
		hamcop->led_on[led_num] = 0;
	}


	/* Power on the LED controller */
	/* If this is not done, setting registers won't work */
	clk_enable(hamcop->clk_led);

	/* Initialize the LED0_CON0 buffer with the led_clk setting,
	 * which is common to all LEDS. Also initialise LEDPS0 and LEDPS1,
	 * which are common too. As this driver doesn't change these settings,
	 * this can just be done once when the led controller is powered on.
	 */

	/* led_ps bits 0 to 15 */
	hamcop_write_register(hamcop, HAMCOP_LED_LEDPS0,
			      LED_PRESCALER & 0xffff);

	/* led_ps bits 16 to 26 */
	hamcop_write_register(hamcop, HAMCOP_LED_LEDPS1,
			      (LED_PRESCALER >> 16) & 0x03ff);

	/* Use RTC as the clock source. */
	tmp_con0 = hamcop_read_register(hamcop, HAMCOP_LEDN_CON0(0));
	tmp_con0 &= ~HAMCOP_LED_LEDNCON0_CLK_MASK;
	hamcop_write_register(hamcop, HAMCOP_LEDN_CON0(0), tmp_con0);

	/* Set LED control registers */
	hamcop_write_register(hamcop, HAMCOP_LEDN_CON0(led_num), con0);
	hamcop_write_register(hamcop, HAMCOP_LEDN_CON1(led_num), con1);

	/* If no LEDs are active, turn off the LED controller to save power. */
	leds_on = 0;
	for (i = 0; i < HAMCOP_NUMBER_OF_LEDS; i++)
		leds_on |= hamcop->led_on[i];

	if (!leds_on)
		clk_disable(hamcop->clk_led);
}
EXPORT_SYMBOL(hamcop_set_led);

/*************************************************************
 HAMCOP clocks
 *************************************************************/

/* base clocks */

static struct clk clk_g = {
	.name		= "gclk",
	.rate		= 0,
	.parent		= NULL,
};

/* clock definitions */

static struct clk hamcop_clocks[] = {
	{ .name    = "keypad",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_KEYPAD_CLKEN
	},
	{ .name    = "dma",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_DMAC_CLKEN
	},
	{ .name    = "nand",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_NAND_CLKEN
	},
	{ .name    = "led",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_LED_CLKEN
	},
	{ .name    = "rtc",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_RTC_CLKEN
	},
	{ .name    = "sdi",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_SD_CLKEN
	},
	{ .name    = "ds1wm",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_1WIRE_CLKEN
	},
	{ .name    = "adc",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_ADC_CLKEN
	},
	{ .name    = "gpio",
	  .parent  = &clk_g,
	  .enable  = hamcop_clock_enable,
	  .disable = hamcop_clock_disable,
	  .ctrlbit = HAMCOP_CPM_CLKCON_GPIO_CLKEN
	},
	{ .name    = "watchdog",
	  .parent  = &clk_g,
	  .ctrlbit = 0
	}
};

/*************************************************************/

static struct samcop_adc_platform_data hamcop_adc_platform_data = {
	.delay = 3000,
};

static struct tsadc_platform_data tsadc_pdata = {
	.x_pin = "samcop adc:x",
	.y_pin = "samcop adc:y",
	.z1_pin = "samcop adc:z1",
	.z2_pin = "samcop adc:z2",
	.num_xy_samples = 1,
	.num_z_samples = 1,
	.delayed_pressure = 1,
	.max_jitter = 10,
};

struct ds1wm_platform_data hamcop_ds1wm_plat_data = {
	.bus_shift = 2,
};

struct hamcop_block
{
	platform_device_id id;
	char *name;
	unsigned long start, end;
	unsigned long irq;
	void *platform_data;
};

static struct hamcop_block hamcop_blocks[] = {
	{
		.id    = { -1 },
		.name  = "samcop adc",
		.start = _HAMCOP_ADC_Base,
		.end   = _HAMCOP_ADC_Base + _HAMCOP_ADC_Size,
		.irq   = _IRQ_HAMCOP_ADC,
		.platform_data = &hamcop_adc_platform_data,
	},
	{
		.id = { -1 },
		.name = "ts-adc",
		.platform_data = &tsadc_pdata,
		.irq = _IRQ_HAMCOP_ADCTS,
	},
	{
		.id    = { -1 },
		.name  = "hamcop nand",
		.start = _HAMCOP_NF_Base,
		.end   = _HAMCOP_NF_Base + _HAMCOP_NF_Size,
		.irq   = -1, /* XXX until RnB interrupt code is written */
	},
	{
		.id    = { -1 },
		.name  = "ds1wm",
		.start = _HAMCOP_OWM_Base,
		.end   = _HAMCOP_OWM_Base + _HAMCOP_OWM_Size,
		.irq   = _IRQ_HAMCOP_ONEWIRE,
		.platform_data = &hamcop_ds1wm_plat_data,
	},
#if 0
// LEDs device must be defined and parametrized by machine
	{
		.id    = { -1 },
		.name  = "hamcop leds",
		.start = _HAMCOP_LED_Base,
		.end   = _HAMCOP_LED_Base + _HAMCOP_LED_Size,
		.irq   = -1,
	},
#endif
	{
		.id    = { -1 },
		.name  = "samcop dma",
		.start = _HAMCOP_DMA_Base,
		.end   = _HAMCOP_DMA_Base + _HAMCOP_DMA_Size,
		.irq   = _IRQ_HAMCOP_DMA,
		.platform_data = &hamcop_dma_plat_data,
	},
	{
		.id    = { -1 },
		.name  = "samcop sdi",
		.start = _HAMCOP_SDI_Base,
		.end   = _HAMCOP_SDI_Base + _HAMCOP_SDI_Size,
		.irq   = _IRQ_HAMCOP_SDHC,
		.platform_data = &hamcop_sdi_data,
	},
};

static void hamcop_release (struct device *dev)
{
	struct platform_device *sdev = to_platform_device (dev);
	kfree (sdev->resource);
	kfree (sdev);
}

static int hamcop_probe (struct platform_device *pdev)
{
	struct hamcop_platform_data *pdata = pdev->dev.platform_data;
	struct hamcop_data *hamcop;
	struct resource *hamcop_mem;
	int i, rc;

	hamcop = kmalloc (sizeof (struct hamcop_data), GFP_KERNEL);
	if (unlikely (!hamcop))
		goto error0;
	hd = hamcop;
	memset (hamcop, 0, sizeof (*hamcop));
	platform_set_drvdata(pdev, hamcop);

	hamcop->irq_nr = platform_get_irq(pdev, 0);
	hamcop_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	hamcop->irq_base = pdata->irq_base;
	if (unlikely(hamcop->irq_base == 0)) {
		printk ("hamcop: uninitialized irq_base!\n");
		goto error1;
	}
	hamcop->mapping = ioremap ((unsigned long)hamcop_mem->start, HAMCOP_MAP_SIZE);
	if (unlikely (!hamcop->mapping)) {
		printk ("hamcop: couldn't ioremap\n");
		goto error1;
	}

	/* Tell the DMA system about HAMCOP's SRAM, which the samcop_sdi
	   driver uses. */
	if (dma_declare_coherent_memory(&pdev->dev,
	    hamcop_mem->start + HAMCOP_SRAM_Base,
	    HAMCOP_SRAM_Base, HAMCOP_SRAM_Size,
	    DMA_MEMORY_MAP | DMA_MEMORY_INCLUDES_CHILDREN |
	    DMA_MEMORY_EXCLUSIVE) != DMA_MEMORY_MAP) {
		printk ("hamcop: couldn't declare coherent dma memory\n");
		goto error3;
	}

	printk ("%s: using irq %d-%d on irq %d\n", pdev->name, hamcop->irq_base, hamcop->irq_base + HAMCOP_NR_IRQS - 1, hamcop->irq_nr);

	/* Initalize HAMCOP. */
	hamcop_write_register (hamcop, HAMCOP_CIF_IFMODE,
			       HAMCOP_CIF_IFMODE_VLIO);

	hamcop_write_register (hamcop, HAMCOP_CPM_ClockControl,
			       HAMCOP_CPM_CLKCON_PLLON_CLKEN |
			       HAMCOP_CPM_CLKCON_NAND_CLKEN  |
			       HAMCOP_CPM_CLKCON_GPIO_CLKEN);

	hamcop_write_register (hamcop, HAMCOP_CPM_ClockSleep,
			       pdata->clocksleep);

	hamcop_write_register (hamcop, HAMCOP_CPM_PllControl,
			       pdata->pllcontrol);

	/* 0x54 means enable only CLKOUT3M (3.6864MHz). */
	hamcop_write_register (hamcop, HAMCOP_GPIO_CLKOUTCON, 0x54);

	hamcop_irq_init (hamcop);

	/* Register HAMCOP's clocks. */
	clk_g.rate = hamcop_gclk(hamcop);

	if (clk_register(&clk_g) < 0)
		printk(KERN_ERR "failed to register HAMCOP gclk\n");

	for (i = 0; i < ARRAY_SIZE(hamcop_clocks); i++) {
		rc = clk_register(&hamcop_clocks[i]);
		if (rc < 0)
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       hamcop_clocks[i].name, rc);
	}

	/* Configured the LEDs. */
	hamcop->clk_led = clk_get(&pdev->dev, "led");
	if (IS_ERR(hamcop->clk_led)) {
		printk(KERN_ERR "failed to get led clock\n");
		rc = -ENOENT;
		goto error;
	}

	h2200_led_hook = hamcop_set_led;

	/* Add all the sub-devices. */
	hamcop->ndevices = ARRAY_SIZE (hamcop_blocks);
	hamcop->devices = kmalloc (hamcop->ndevices * sizeof (struct platform_device *), GFP_KERNEL);
	if (unlikely (!hamcop->devices))
		goto enomem;
	memset (hamcop->devices, 0, hamcop->ndevices * sizeof (struct platform_device *));

	for (i = 0; i < hamcop->ndevices; i++) {
		struct platform_device *sdev;
		struct hamcop_block *blk = &hamcop_blocks[i];
		struct resource *res;

		sdev = kmalloc (sizeof (*sdev), GFP_KERNEL);
		if (unlikely (!sdev))
			goto enomem;
		memset (sdev, 0, sizeof (*sdev));
		sdev->id = hamcop_blocks[i].id.id;
		sdev->dev.parent = &pdev->dev;
		sdev->dev.release = hamcop_release;
		sdev->dev.platform_data = hamcop_blocks[i].platform_data;
		sdev->num_resources = (blk->irq == -1) ? 1 : 2;
		sdev->name = blk->name;
		res = kmalloc (sdev->num_resources * sizeof (struct resource), GFP_KERNEL);
		if (unlikely (!res))
			goto enomem;
		sdev->resource = res;
		memset (res, 0, sdev->num_resources * sizeof (struct resource));
		res[0].start = blk->start + hamcop_mem->start;
		res[0].end = blk->end + hamcop_mem->start;
		res[0].flags = IORESOURCE_MEM;
		res[0].parent = hamcop_mem;
		if (blk->irq != -1) {
			res[1].start = blk->irq + hamcop->irq_base;
			res[1].end = res[1].start;
			res[1].flags = IORESOURCE_IRQ;
		}
		sdev->dev.dma_mem = pdev->dev.dma_mem;
		rc = platform_device_register (sdev);
		if (unlikely (rc != 0)) {
			printk ("hamcop: could not register %s\n", blk->name);
			kfree (sdev->resource);
			kfree (sdev);
			goto error;
		}
		hamcop->devices[i] = sdev;
	}

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			pdata->child_devs[i]->dev.parent = &pdev->dev;
			platform_device_register(pdata->child_devs[i]);
		}
	}

	return 0;

 enomem:
	rc = -ENOMEM;
 error:
	hamcop_remove (pdev);
	return rc;

 error3:
	iounmap(hamcop->mapping);
 error1:
	kfree (hamcop);
 error0:
	return -ENOMEM;
}

static int hamcop_remove(struct platform_device *pdev)
{
	struct hamcop_platform_data *pdata = pdev->dev.platform_data;
	struct hamcop_data *hamcop = platform_get_drvdata(pdev);
	int i;
	u8 *bootloader;

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			platform_device_unregister(pdata->child_devs[i]);
		}
	}

	for (i = 0; i < HAMCOP_NR_IRQS; i++) {
		int irq = i + hamcop->irq_base;
		set_irq_flags(irq, 0);
		set_irq_handler (irq, NULL);
		set_irq_chip (irq, NULL);
		set_irq_chip_data(irq, NULL);
	}

	set_irq_chained_handler (hamcop->irq_nr, NULL);

	if (hamcop->devices) {
		for (i = 0; i < hamcop->ndevices; i++) {
			if (hamcop->devices[i])
				platform_device_unregister (hamcop->devices[i]);
		}
		kfree (hamcop->devices);
	}

	for (i = 0; i < ARRAY_SIZE(hamcop_clocks); i++)
		clk_unregister(&hamcop_clocks[i]);
	clk_unregister(&clk_g);

	dma_release_declared_memory(&pdev->dev);

	/* Copy bootloader back to SRAM so reset will work.
	 * Other drivers that write to SRAM (e.g. samcop_sdi) should
	 * be shut down. */

	bootloader = get_hamcop_bootloader();
	if (bootloader)
		memcpy(hamcop->mapping + HAMCOP_SRAM_Base, bootloader,
		       HAMCOP_SRAM_Size);

	iounmap (hamcop->mapping);

	kfree (hamcop);

	return 0;
}

/* Apparently shutdown should only do the minimum to quiesce the device,
 * ignoring locks etc. */
static void
hamcop_shutdown (struct platform_device *pdev)
{
        struct hamcop_data *hamcop = platform_get_drvdata(pdev);
	u8 *bootloader;

	bootloader = get_hamcop_bootloader();
	if (bootloader)
		memcpy(hamcop->mapping + HAMCOP_SRAM_Base, bootloader,
		       HAMCOP_SRAM_Size);
}

static int
hamcop_suspend (struct platform_device *pdev, pm_message_t state)
{
        struct hamcop_data *hamcop = platform_get_drvdata(pdev);
	u8 *bootloader;

	hamcop->save_clocks = hamcop_read_register (hamcop, HAMCOP_CPM_ClockControl);

	hamcop->irqmask = hamcop_read_register (hamcop, HAMCOP_IC_INTMSK0);
	hamcop->irqmask |= (hamcop_read_register (hamcop, HAMCOP_IC_INTMSK1) << 16);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0, 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1, 0xffff);

	/* Copy bootloader back to SRAM so reset/resume will work. */
	bootloader = get_hamcop_bootloader();
	if (bootloader)
		memcpy(hamcop->mapping + HAMCOP_SRAM_Base, bootloader,
		       HAMCOP_SRAM_Size);

	return 0;
}

static int
hamcop_resume (struct platform_device *pdev)
{
        struct hamcop_data *hamcop = platform_get_drvdata(pdev);

	hamcop_set_ifmode(&pdev->dev, HAMCOP_CIF_IFMODE_VLIO);

	hamcop_write_register (hamcop, HAMCOP_CIF_IFMODE,
			       HAMCOP_CIF_IFMODE_VLIO);

	hamcop_write_register (hamcop, HAMCOP_CPM_ClockControl,
			       hamcop->save_clocks);

	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK0,
			       hamcop->irqmask & 0xffff);
	hamcop_write_register (hamcop, HAMCOP_IC_INTMSK1,
			       (hamcop->irqmask >> 16) & 0xffff);

	return 0;
}

static struct platform_driver hamcop_device_driver = {
        .driver = {
	.name		= "hamcop",
        },

	.probe		= hamcop_probe,
	.remove		= hamcop_remove,
	.suspend	= hamcop_suspend,
	.resume		= hamcop_resume,
	.shutdown	= hamcop_shutdown,
};

static int __init
hamcop_base_init (void)
{
	int retval = 0;
	retval = platform_driver_register (&hamcop_device_driver);
	return retval;
}

static void __exit
hamcop_base_exit (void)
{
	platform_driver_unregister (&hamcop_device_driver);
}

#ifdef MODULE
module_init(hamcop_base_init);
#else	/* start early for dependencies */
subsys_initcall(hamcop_base_init);
#endif
module_exit(hamcop_base_exit);

MODULE_AUTHOR("Jamey Hicks <jamey@handhelds.org>");
MODULE_DESCRIPTION("Base SoC driver for Samsung HAMCOP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE("hamcop");
