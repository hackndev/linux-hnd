/*
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
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
 *
 * History:
 *
 * 2002-08-23   Jamey Hicks        GPIO and IRQ support for iPAQ H5400
 *
 */

#include <linux/module.h>
#include <linux/init.h>
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
#include <linux/ds1wm.h>
#include <linux/touchscreen-adc.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/clk.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/h5400-asic.h>
#include <asm/arch/h5400-gpio.h>
#include <asm/hardware/ipaq-samcop.h>
#include <linux/mfd/samcop_base.h>
#include <asm/hardware/samcop-sdi.h>
#include <asm/hardware/samcop-dma.h>
#include <asm/arch/pxa-dmabounce.h>
#include "../mmc/samcop_sdi.h"

#include <asm/arch/irq.h>
#include <asm/arch/clock.h>
#include <asm/types.h>

struct samcop_data
{
	struct device *dev;
	int irq_base, irq_nr;
	void *mapping;
	spinlock_t gpio_lock;
	unsigned long irqmask;
	struct platform_device **devices;
	int ndevices;
} *samcop_device_data;	/* XXX samcop_device_data is a hack */

static void samcop_set_gpio_bit(struct device *dev, unsigned gpio, int val);
static int samcop_get_gpio_bit(struct device *dev, unsigned gpio);
static int samcop_gpio_to_irq(struct device *dev, unsigned gpio);

static int samcop_remove(struct platform_device *pdev);
static int samcop_gclk(void *ptr);
static void samcop_set_gpio_a_pullup(struct device *dev, u32 mask, u32 bits);
static void samcop_set_gpio_a_con(struct device *dev, unsigned int idx, u32 mask, u32 bits);
void samcop_set_gpio_int(struct device *dev, unsigned int idx, u32 mask, u32 bits);
void samcop_set_gpio_int_enable(struct device *dev, unsigned int idx, u32 mask, u32 bits);
void samcop_set_gpio_filter_config(struct device *dev, unsigned int idx, u32 mask,
			       u32 bits);

/***********************************************************************************/
/*      Register access                                                            */
/***********************************************************************************/

static inline void samcop_write_register(struct samcop_data *samcop, unsigned long addr, unsigned long value)
{
	__raw_writel(value, (unsigned long)samcop->mapping + addr);
}

static inline unsigned long samcop_read_register(struct samcop_data *samcop, unsigned long addr)
{
	return __raw_readl((unsigned long)samcop->mapping + addr);
}

/***********************************************************************************/
/*      gpiodev handling                                                          */
/***********************************************************************************/
static void samcop_set_gpio_bit(struct device *dev, unsigned gpio, int val)
{
	u32 mask = _SAMCOP_GPIO_BIT(gpio);
	u32 bitval = 0;
	if (val) bitval = mask;

	pr_debug("%s(%d, %d)\n", __FUNCTION__, gpio, val);

	switch _SAMCOP_GPIO_BANK(gpio) {
		case _SAMCOP_GPIO_BANK_A:
			samcop_set_gpio_a(dev, mask, bitval);
			return;
		case _SAMCOP_GPIO_BANK_B:
			samcop_set_gpio_b(dev, mask, bitval);
			return;
	};

	printk("%s: invalid GPIO value 0x%x", __FUNCTION__, gpio);
};

static int samcop_get_gpio_bit(struct device *dev, unsigned gpio)
{
	u32 mask = _SAMCOP_GPIO_BIT(gpio);

	pr_debug("%s(%d)\n", __FUNCTION__, gpio);

	switch _SAMCOP_GPIO_BANK(gpio) {
		case _SAMCOP_GPIO_BANK_A:
			return samcop_get_gpio_a(dev) & mask;
		case _SAMCOP_GPIO_BANK_B:
			return samcop_get_gpio_b(dev) & mask;
	};

	printk("%s: invalid GPIO value 0x%x", __FUNCTION__, gpio);
	return -1;
};

static int samcop_gpio_to_irq(struct device *dev, unsigned gpio)
{
	struct samcop_data *samcop = dev_get_drvdata(dev);
	u32 irq_base = samcop->irq_base;

	pr_debug("%s(%d)\n", __FUNCTION__, gpio);

	/*
	 * Find the GPIO interrupt we want by checking the given GPIO
	 * pin against what is actually wired up
	 */
	if ((gpio >= SAMCOP_GPA_APPBTN1) && (gpio <= SAMCOP_GPA_APPBTN4))
		return irq_base + gpio - (SAMCOP_GPA_APPBTN1 & 0x1f);
	else if (gpio == SAMCOP_GPA_RECORD)
		return irq_base + _IRQ_SAMCOP_RECORD;
	else if ((gpio >= SAMCOP_GPA_JOYSTICK1) && (gpio <= SAMCOP_GPA_RESET))
		return irq_base + SAMCOP_GPIO_IRQ_START + gpio - (SAMCOP_GPA_JOYSTICK1 & 0x1f);
	else if (gpio == SAMCOP_GPA_OPT_RESET)
		return irq_base + SAMCOP_GPIO_IRQ_START + _IRQ_SAMCOP_GPIO_GPA18;
	else if (_SAMCOP_GPIO_BANK(gpio) == _SAMCOP_GPIO_BANK_B)
		return irq_base + _IRQ_SAMCOP_GPIO_GPB12 + (gpio & 0x1f) - (SAMCOP_GPB_WAN1 & 0x1f);

	printk("%s: invalid GPIO value 0x%x", __FUNCTION__, gpio);
	return -1;
};

/***********************************************************************************/
/*      ASIC IRQ demux                                                             */
/***********************************************************************************/

#define MAX_ASIC_ISR_LOOPS    1000

static void samcop_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	int i;
	struct samcop_data *samcop;
	u32 pending;

	// Tell processor to acknowledge its interrupt
	if (desc->chip->ack)
		desc->chip->ack(irq);
	samcop = desc->handler_data;

	if (0)
		printk("%s: interrupt received\n", __FUNCTION__);

	for (i = 0 ; (i < MAX_ASIC_ISR_LOOPS); i++) {
		int asic_irq;

		pending = samcop_read_register(samcop, SAMCOP_IC_INTPND);
		if (!pending)
			break;

		asic_irq = samcop->irq_base + (31 - __builtin_clz(pending));
		desc = irq_desc + asic_irq;
		desc->handle_irq(asic_irq, desc);
	}

	if (desc->chip->end)
		desc->chip->end(irq);

	if (i >= MAX_ASIC_ISR_LOOPS && pending) {
		u32 val;

		printk(KERN_WARNING
		       "%s: interrupt processing overrun, pending=0x%08x. "
		       "Masking interrupt\n", __FUNCTION__, pending);
		val = samcop_read_register(samcop, SAMCOP_IC_INTMSK);
		samcop_write_register(samcop, SAMCOP_IC_INTMSK, val | pending);
		samcop_write_register(samcop, SAMCOP_IC_SRCPND, pending);
		samcop_write_register(samcop, SAMCOP_IC_INTPND, pending);
	}
}

/***********************************************************************************/
/*      ASIC EPS IRQ demux                                                         */
/***********************************************************************************/

static u32 eps_irq_mask[] = {
	SAMCOP_PCMCIA_EPS_CD0_N, /* IRQ_GPIO_SAMCOP_EPS_CD0 */
	SAMCOP_PCMCIA_EPS_CD1_N, /* IRQ_GPIO_SAMCOP_EPS_CD1 */
	SAMCOP_PCMCIA_EPS_IRQ0_N, /* IRQ_GPIO_SAMCOP_EPS_IRQ0 */
	SAMCOP_PCMCIA_EPS_IRQ1_N, /* IRQ_GPIO_SAMCOP_EPS_IRQ1 */
	(SAMCOP_PCMCIA_EPS_ODET0_N|SAMCOP_PCMCIA_EPS_ODET1_N),/* IRQ_GPIO_SAMCOP_EPS_ODET */
	SAMCOP_PCMCIA_EPS_BATT_FLT/* IRQ_GPIO_SAMCOP_EPS_BATT_FAULT */
};

static void samcop_asic_eps_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	int i;
	struct samcop_data *samcop;
	u32 eps_pending;

	samcop = desc->handler_data;

	if (0) printk("%s: interrupt received irq=%d\n", __FUNCTION__, irq);

	for (i = 0 ; (i < MAX_ASIC_ISR_LOOPS) ; i++) {
		int j;
		eps_pending = samcop_read_register(samcop, SAMCOP_PCMCIA_IP);
		if (!eps_pending)
			break;
		if (0) printk("%s: eps_pending=0x%08x\n", __FUNCTION__, eps_pending);
		for (j = 0 ; j < SAMCOP_EPS_IRQ_COUNT ; j++)
			if (eps_pending & eps_irq_mask[j]) {
				int eps_irq = samcop->irq_base + j + SAMCOP_EPS_IRQ_START;
				desc = irq_desc + eps_irq;
				desc->handle_irq(eps_irq, desc);
			}
	}

	if (eps_pending)
		printk("%s: interrupt processing overrun pending=0x%08x\n", __FUNCTION__, eps_pending);
}

/***********************************************************************************/
/*      ASIC GPIO IRQ demux                                                        */
/***********************************************************************************/

static void samcop_asic_gpio_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	u32 pending;
	struct samcop_data *samcop;
	int loop;

	samcop = desc->handler_data;

	for (loop = 0; loop < MAX_ASIC_ISR_LOOPS; loop++)
	{
		int i;
		pending = samcop_read_register(samcop, SAMCOP_GPIO_INTPND);

		if (!pending)
			break;

		for (i = 0; i < SAMCOP_GPIO_IRQ_COUNT; i++) {
			if (pending & (1 << i)) {
				int gpio_irq = samcop->irq_base + i + SAMCOP_GPIO_IRQ_START;
				desc = irq_desc + gpio_irq;
				desc->handle_irq(gpio_irq, desc);
			}
		}
	}

	if (pending)
		printk("%s: demux overrun, pending=%08x\n", __FUNCTION__, pending);
}

/***********************************************************************************/
/*      IRQ handling                                                               */
/***********************************************************************************/

/* ack <- IRQ is first serviced.
       mask <- IRQ is disabled.
     unmask <- IRQ is enabled */

static void samcop_asic_ack_ic_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = 1 << (irq - SAMCOP_IC_IRQ_START - samcop->irq_base);

	samcop_write_register(samcop, SAMCOP_IC_SRCPND, mask);
	samcop_write_register(samcop, SAMCOP_IC_INTPND, mask);
}

static void samcop_asic_mask_ic_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = 1 << (irq - SAMCOP_IC_IRQ_START - samcop->irq_base);
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_IC_INTMSK);
	val |= mask;
	samcop_write_register(samcop, SAMCOP_IC_INTMSK, val);
}

static void samcop_asic_unmask_ic_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = 1 << (irq - SAMCOP_IC_IRQ_START - samcop->irq_base);
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_IC_INTMSK);
	val &= ~mask;
	samcop_write_register(samcop, SAMCOP_IC_INTMSK, val);
}

static struct {
	int irq;
	int gpa_con_idx;
	int gpa_con_n;
	int gpioint_idx;
	int gpioint_n;
	int enint1_n;
}
ic_ints_tbl[] = {
	{_IRQ_SAMCOP_BUTTON_1,    0,  0,   0, 0,   0},
	{_IRQ_SAMCOP_BUTTON_2,    0,  1,   0, 1,   1},
	{_IRQ_SAMCOP_BUTTON_3,    0,  2,   0, 2,   2},
	{_IRQ_SAMCOP_BUTTON_4,    0,  3,   0, 3,   3},
	{_IRQ_SAMCOP_RECORD,      0, 12,   1, 4,   4},
	{_IRQ_SAMCOP_WAKEUP1,     0, 13,   1, 5,   5},
	{_IRQ_SAMCOP_WAKEUP2,     0, 14,   1, 6,   6},
	{_IRQ_SAMCOP_WAKEUP3,     0, 15,   1, 7,   7},
	{_IRQ_SAMCOP_SD_WAKEUP,   1,  0,   2, 0,   8},
};

static ssize_t ic_ints_tbl_offset(unsigned int irq)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(ic_ints_tbl); i++) {
		if (ic_ints_tbl[i].irq == irq)
			return i;
	}
	return -1;
}

static int samcop_asic_set_type_ic_irq(unsigned int irq, unsigned int type)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	const int irqn = irq - SAMCOP_IC_IRQ_START - samcop->irq_base;
	const ssize_t offset = ic_ints_tbl_offset(irqn);
	int n;
	int idx;
	int irq_type;
	int irq_mask;
	int irq_fmask;

	if (offset < 0)
		goto no_need_to_handle;

	n = ic_ints_tbl[offset].gpioint_n;
	idx = ic_ints_tbl[offset].gpioint_idx;

	if (type & (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING))
		irq_type = SAMCOP_GPIO_IRQT_BOTHEDGE(n);
	else if (type & IRQF_TRIGGER_RISING)
		irq_type = SAMCOP_GPIO_IRQT_RISING(n);
	else if (type & IRQF_TRIGGER_FALLING)
		irq_type = SAMCOP_GPIO_IRQT_FALLING(n);
	else if (type & IRQF_TRIGGER_LOW)
		irq_type = SAMCOP_GPIO_IRQT_LOW(n);
	else if (type & IRQF_TRIGGER_HIGH)
		irq_type = SAMCOP_GPIO_IRQT_HIGH(n);
	else return -EINVAL;

	irq_mask = SAMCOP_GPIO_IRQT_MASK(n);
	irq_fmask = SAMCOP_GPIO_IRQF_MASK(n);

	samcop_set_gpio_int(samcop->dev, idx, irq_mask, irq_type);
	samcop_set_gpio_int(samcop->dev, idx, irq_fmask, 0);

	pr_debug("%s: %x %x %x\n", __FUNCTION__, idx, irq_mask, irq_type);

no_need_to_handle:
	return 0;
}

static unsigned int samcop_asic_startup_ic_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	const int irqn = irq - SAMCOP_IC_IRQ_START - samcop->irq_base;
	const ssize_t offset = ic_ints_tbl_offset(irqn);
	int con_idx;
	int con_n;
	int en_n;

	if (offset < 0)
		goto no_need_to_handle;

	en_n = ic_ints_tbl[offset].enint1_n;
	con_idx = ic_ints_tbl[offset].gpa_con_idx;
	con_n = ic_ints_tbl[offset].gpa_con_n;

	samcop_set_gpio_int_enable(samcop->dev, 0,
				   SAMCOP_GPIO_ENINT_MASK(en_n),
				   SAMCOP_GPIO_ENINT_MASK(en_n));
	samcop_set_gpio_a_con(samcop->dev, con_idx,
			      SAMCOP_GPIO_GPx_CON_MASK(con_n),
			      SAMCOP_GPIO_GPx_CON_MODE(con_n, EXTIN));

	pr_debug("%s (en):  %x %x\n", __FUNCTION__, 0, en_n);
	pr_debug("%s (con): %x %x\n", __FUNCTION__, con_idx, con_n);

no_need_to_handle:
	irq_desc[irq].chip->enable(irq);
	return 0;
}

static void samcop_asic_ack_eps_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = eps_irq_mask[irq - SAMCOP_EPS_IRQ_START - samcop->irq_base];

	samcop_write_register(samcop, SAMCOP_PCMCIA_IP, mask);
	/* acknowledge PCMCIA interrupt in INTPND/SRCPND */
	samcop_write_register(samcop, SAMCOP_IC_SRCPND, 1 << _SAMCOP_IC_INT_PCMCIA);
	samcop_write_register(samcop, SAMCOP_IC_INTPND, 1 << _SAMCOP_IC_INT_PCMCIA);
}

static void samcop_asic_mask_eps_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = eps_irq_mask[irq - SAMCOP_EPS_IRQ_START - samcop->irq_base];
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_PCMCIA_IC);
	val &= ~mask;
	samcop_write_register(samcop, SAMCOP_PCMCIA_IC, val);
}

static void samcop_asic_unmask_eps_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	int mask = eps_irq_mask[irq - SAMCOP_EPS_IRQ_START - samcop->irq_base];
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_PCMCIA_IC);
	val |= mask;
	samcop_write_register(samcop, SAMCOP_PCMCIA_IC, val);
}

static void samcop_asic_ack_gpio_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);

	/* acknowledge GPIO_INTPND interrupt */
	samcop_write_register(samcop, SAMCOP_GPIO_INTPND,
			       1 << (irq - SAMCOP_GPIO_IRQ_START - samcop->irq_base));

	/* acknowledge also GPIO interrupt in INTPND/SRCPND */
	samcop_write_register(samcop, SAMCOP_IC_SRCPND, 1 << _SAMCOP_IC_INT_GPIO);
	samcop_write_register(samcop, SAMCOP_IC_INTPND, 1 << _SAMCOP_IC_INT_GPIO);
}

static void samcop_asic_mask_gpio_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	u32 mask = 1 << (irq - SAMCOP_GPIO_IRQ_START - samcop->irq_base);
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_GPIO_ENINT2);
	val &= ~mask;
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT2, val);
}

static void samcop_asic_unmask_gpio_irq(unsigned int irq)
{
	struct samcop_data *samcop = get_irq_chip_data(irq);
	u32 mask = 1 << (irq - SAMCOP_GPIO_IRQ_START - samcop->irq_base);
	u32 val;

	val = samcop_read_register(samcop, SAMCOP_GPIO_ENINT2);
	val |= mask;
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT2, val);
}

static struct irq_chip samcop_ic_irq_chip = {
	.ack		= samcop_asic_ack_ic_irq,
	.mask		= samcop_asic_mask_ic_irq,
	.unmask		= samcop_asic_unmask_ic_irq,
	.startup        = samcop_asic_startup_ic_irq,
	.set_type       = samcop_asic_set_type_ic_irq,
};

static struct irq_chip samcop_eps_irq_chip = {
	.ack		= samcop_asic_ack_eps_irq,
	.mask		= samcop_asic_mask_eps_irq,
	.unmask		= samcop_asic_unmask_eps_irq,
};

static struct irq_chip samcop_gpio_irq_chip = {
	.ack		= samcop_asic_ack_gpio_irq,
	.mask		= samcop_asic_mask_gpio_irq,
	.unmask		= samcop_asic_unmask_gpio_irq,
};

static void __init samcop_irq_init(struct samcop_data *samcop)
{
	int i;

	/* mask all interrupts, this will cause the processor to ignore all interrupts from SAMCOP */
	samcop_write_register(samcop, SAMCOP_IC_INTMSK, 0xffffffff);

	/* clear out any pending irqs */
	for (i = 0; i < 32; i++) {
		if (samcop_read_register(samcop, SAMCOP_IC_INTPND) == 0) {
			printk(KERN_INFO
			       "%s: no interrupts pending, looped %d %s. "
			       "Continuing\n", __FUNCTION__,
			       i + 1, ((i + 1) ? "time" : "times"));
			break;
		}
		samcop_write_register(samcop, SAMCOP_IC_SRCPND, 0xffffffff);
		samcop_write_register(samcop, SAMCOP_IC_INTPND, 0xffffffff);
	}

	samcop_write_register(samcop, SAMCOP_PCMCIA_IC, 0);	/* nothing enabled */
	samcop_write_register(samcop, SAMCOP_PCMCIA_IP, 0xff);	/* clear anything here now */
	samcop_write_register(samcop, SAMCOP_PCMCIA_IM, 0x2555);

	samcop_write_register(samcop, SAMCOP_GPIO_ENINT1, 0);
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT2, 0);
	samcop_write_register(samcop, SAMCOP_GPIO_INTPND, 0x3fff);

	for (i = 0; i < SAMCOP_IC_IRQ_COUNT; i++) {
		int irq = samcop->irq_base + i + SAMCOP_IC_IRQ_START;
		set_irq_chip_data(irq, samcop);
		set_irq_chip(irq, &samcop_ic_irq_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	for (i = 0; i < SAMCOP_EPS_IRQ_COUNT; i++) {
		int irq = samcop->irq_base + i + SAMCOP_EPS_IRQ_START;
		set_irq_chip_data(irq, samcop);
		set_irq_chip(irq, &samcop_eps_irq_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	for (i = 0; i < SAMCOP_GPIO_IRQ_COUNT; i++) {
		int irq = samcop->irq_base + i + SAMCOP_GPIO_IRQ_START;
		set_irq_chip_data(irq, samcop);
		set_irq_chip(irq, &samcop_gpio_irq_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	set_irq_data(samcop->irq_base + _IRQ_SAMCOP_PCMCIA, samcop);
	set_irq_chained_handler(samcop->irq_base + _IRQ_SAMCOP_PCMCIA,
				 samcop_asic_eps_irq_demux);
	set_irq_data(samcop->irq_base + _IRQ_SAMCOP_GPIO, samcop);
	set_irq_chained_handler(samcop->irq_base + _IRQ_SAMCOP_GPIO,
				 samcop_asic_gpio_irq_demux);

	set_irq_data(samcop->irq_nr, samcop);
	set_irq_type(samcop->irq_nr, IRQT_FALLING);
	set_irq_chained_handler(samcop->irq_nr, samcop_irq_demux);
}

/*************************************************************
 * SAMCOP SD interface hooks and initialization
 *************************************************************/
int samcop_dma_needs_bounce(struct device *dev, dma_addr_t addr, size_t size)
{
	return (addr + size >= H5400_SAMCOP_BASE + _SAMCOP_SRAM_Base + SAMCOP_SRAM_SIZE);
}

static u64 samcop_sdi_dmamask = 0xffffffffUL;

static void samcop_sdi_init(struct device *dev)
{
	struct samcop_data *samcop = dev->parent->driver_data;
	struct samcop_sdi_data *plat = dev->platform_data;

	/* Init run-time values */
	plat->f_min = samcop_gclk(samcop) / 256; /* Divisor is SDIPRE max + 1 */
	plat->f_max = samcop_gclk(samcop) / 2;   /* Divisor is SDIPRE min + 1 */
	plat->dma_devaddr = (void *) _SAMCOP_SDI_Base + SAMCOP_SDIDATA;

	/* provide the right card detect irq to the mmc subsystem by
	 * applying the samcop irq_base offset
	 */
	plat->irq_cd = _IRQ_SAMCOP_SD_WAKEUP + samcop->irq_base;

	/* setup DMA (including dma_bounce) */
	dev->dma_mask = &samcop_sdi_dmamask; /* spread the address range wide
					      * so we can DMA bounce wherever
					      * we want.
					      */
	dev->coherent_dma_mask = samcop_sdi_dmamask;
	dmabounce_register_dev(dev, 512, 4096);
	pxa_set_dma_needs_bounce(samcop_dma_needs_bounce);
}

static void samcop_sdi_exit(struct device *dev)
{
	dmabounce_unregister_dev(dev);
}

static u32 samcop_sdi_read_register(struct device *dev, u32 reg)
{
	struct samcop_data *samcop = dev->parent->driver_data;

	return samcop_read_register(samcop, _SAMCOP_SDI_Base + reg);
}

static void samcop_sdi_write_register(struct device *dev, u32 reg, u32 val)
{
	struct samcop_data *samcop = dev->parent->driver_data;

	samcop_write_register(samcop, _SAMCOP_SDI_Base + reg, val);
}

static void samcop_sdi_card_power(struct device *dev, int on, int clock_req)
{
	struct samcop_data *samcop = dev->parent->driver_data;
	u32 sdi_psc = 1, sdi_con;
	int clock_rate;

	/* Set power */
	sdi_con = samcop_sdi_read_register(dev, SAMCOP_SDICON);
#ifdef CONFIG_ARCH_H5400
	/* apply proper power setting to the sd slot */
	if (machine_is_h5400())
		SET_H5400_GPIO(POWER_SD_N, on != 0);
#endif

	if (on) {
		/* enable pullups */
		samcop_set_spcr(dev->parent, SAMCOP_GPIO_SPCR_SDPUCR, 0);
		samcop_set_gpio_a_pullup(dev->parent, SAMCOP_GPIO_GPA_SD_DETECT_N, 0);

		/* enable the SD clock prescaler and clear the fifo */
		sdi_con |= SAMCOP_SDICON_FIFORESET | SAMCOP_SDICON_CLOCKTYPE;
	} else {
		/* remove power from the sd slot */
#ifdef CONFIG_ARCH_H5400
		if (machine_is_h5400())
			SET_H5400_GPIO(POWER_SD_N, 0);
#endif

		/* disable pullups */
		samcop_set_spcr(dev->parent, SAMCOP_GPIO_SPCR_SDPUCR,
				SAMCOP_GPIO_SPCR_SDPUCR);
		samcop_set_gpio_a_pullup(dev->parent,
					 SAMCOP_GPIO_GPA_SD_DETECT_N,
					 SAMCOP_GPIO_GPA_SD_DETECT_N);

		sdi_con &= ~SAMCOP_SDICON_CLOCKTYPE;
	}

	clock_rate = samcop_gclk(samcop);

	if (clock_req == 0) {
		/* Don't set the prescaler or enable the card clock if the mmc
		 * subsystem is sending a 0 clock rate.
		 */
		sdi_con &= ~SAMCOP_SDICON_CLOCKTYPE;
	} else if (clock_req < (clock_rate / 256)) {
		printk(KERN_WARNING
		       "%s: MMC subsystem requesting bogus clock rate %d. "
		       "Hardcoding prescaler to 255\n",
		       __FUNCTION__, clock_req);
		sdi_psc = 255;
	} else {
		sdi_psc = clock_rate / clock_req - 1;
		if (sdi_psc > 255) {
			printk(KERN_WARNING
			       "%s: calculated prescaler %d is too high. "
			       "Hardcoding prescaler to 255\n",
			       __FUNCTION__, sdi_psc);
			sdi_psc = 255;
		} else if (sdi_psc < 1) {
			printk(KERN_WARNING
			       "%s: calculated prescaler %d is too low. "
			       "Hardcoding prescaler to 1\n",
			      __FUNCTION__, sdi_psc);
			sdi_psc = 1;
		}
	}

	samcop_sdi_write_register(dev, SAMCOP_SDICON, sdi_con);

	if (clock_req != 0 && on) {
		if (clock_rate / (sdi_psc + 1) > clock_req && sdi_psc < 255)
			sdi_psc++;
		samcop_sdi_write_register(dev, SAMCOP_SDIPRE, sdi_psc);
		/* wait for the prescribed time that the card requires to init */
		if ((clock_rate / sdi_psc) > 1000000UL)
			ndelay(sdi_psc * 1000000000UL / clock_rate * SAMCOP_CARD_INIT_CYCLES);
		else
			udelay(sdi_psc * 1000000UL / clock_rate * SAMCOP_CARD_INIT_CYCLES);
	}
}

static struct samcop_sdi_data samcop_sdi_data = {
	.init		  = samcop_sdi_init,
	.exit		  = samcop_sdi_exit,
	.read_reg	  = samcop_sdi_read_register,
	.write_reg	  = samcop_sdi_write_register,
	.card_power	  = samcop_sdi_card_power,
	/* card detect IRQ. Macro is for the samcop number,
	 * use samcop->irq_base + irq_cd to get the irq system number
	 */
	.irq_cd		  = 0, /* this will be set by sdi_init */
	.f_min		  = 0, /* this will be set by sdi_init */
	.f_max		  = 0, /* this will be set by sdi_init */
	/* SAMCOP has 32KB of SRAM, 32KB / 512B (per sector) = 64 sectors max.*/
	.max_sectors  = 64,
	.timeout	  = SAMCOP_SDI_TIMER_MAX,

	/* DMA specific settings */
	.dma_chan	  = 0, /* hardcoded for now */
	.dma_devaddr  = (void *)_SAMCOP_SDI_Base + SAMCOP_SDIDATA,
	.hwsrcsel	  = SAMCOP_DCON_CH0_SD,
	/* SAMCOP supports 32bit transfers, 32/8 = 4 bytes per transfer unit */
	.xfer_unit	  = 4,
};

/*************************************************************
 * SAMCOP DMA interface hooks and initialization
 *************************************************************/

static struct samcop_dma_plat_data samcop_dma_plat_data = {
	.n_channels = 2,
};

/*************************************************************
 * SAMCOP LEDs
 *************************************************************/

int samcop_set_led(struct device *samcop_dev, unsigned long rate,
		   int led_num, unsigned int duty_time,
		   unsigned int cycle_time)
{
	int con;
	int psc;

	cycle_time &= 0x1F;
	duty_time &= 0x1F;
	if (duty_time > cycle_time)
		duty_time = cycle_time;

	psc = samcop_gclk(samcop_dev->driver_data) / rate;
	samcop_write_register(samcop_dev->driver_data, SAMCOP_LED_LEDPS, psc);

	con = duty_time ? 0x3 | (duty_time << 4) | (cycle_time << 12) : 0;

	samcop_write_register(samcop_dev->driver_data,
			      SAMCOP_LED_CONTROL(led_num), con);

	return 0;
}
EXPORT_SYMBOL(samcop_set_led);

/*************************************************************/

int samcop_clock_enable(struct clk* clk, int enable)
{
	unsigned long flags, val;

	local_irq_save(flags);
	val = samcop_read_register(samcop_device_data, SAMCOP_CPM_ClockControl);

	/* UCLK is different from the rest (0 = on, 1 = off), so we
	 * special-case it here. */
	if (strcmp(clk->name, "uclk") == 0)
		enable = enable ? 0 : 1;

	if (enable)
		val |= clk->ctrlbit;
	else
		val &= ~clk->ctrlbit;
	samcop_write_register(samcop_device_data, SAMCOP_CPM_ClockControl, val);
	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(samcop_clock_enable);


static void samcop_clock_en(struct clk* clk)
{
	if (samcop_clock_enable(clk, 1))
		printk(KERN_ERR "Error while enabling clock %s", clk->name);
}

static void samcop_clock_dis(struct clk* clk)
{
	if (samcop_clock_enable(clk, 0))
		printk(KERN_ERR "Error while disabling clock %s", clk->name);
}

static int samcop_gclk(void *ptr)
{
	struct samcop_data *samcop = ptr;
	int half_clk;
	unsigned int memory_clock = get_memclk_frequency_10khz() * 10000;

	half_clk = (samcop_read_register(samcop, SAMCOP_CPM_ClockSleep) & SAMCOP_CPM_CLKSLEEP_HALF_CLK);

	return half_clk ? memory_clock : memory_clock / 2;
}

void samcop_set_gpio_a(struct device *dev, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_GPA_DAT) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_GPA_DAT, val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}
EXPORT_SYMBOL(samcop_set_gpio_a);

static void samcop_set_gpio_a_pullup(struct device *dev, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_GPA_PUP) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_GPA_PUP, val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}

static void samcop_set_gpio_a_con(struct device *dev, unsigned int idx, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags;
	u32 val;

	if (idx > 1) {
		printk(KERN_WARNING "%s idx %u is invalid, must be {0-1}\n",
		       __FUNCTION__, idx);
		return;
	}

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_GPA_CON(idx)) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_GPA_CON(idx), val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}

u32 samcop_get_gpio_a(struct device *dev)
{
	struct samcop_data *samcop = dev->driver_data;

	return samcop_read_register(samcop, SAMCOP_GPIO_GPA_DAT);
}
EXPORT_SYMBOL(samcop_get_gpio_a);

void samcop_set_gpio_b(struct device *dev, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_GPB_DAT) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_GPB_DAT, val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}
EXPORT_SYMBOL(samcop_set_gpio_b);

u32 samcop_get_gpio_b(struct device *dev)
{
	struct samcop_data *samcop = dev->driver_data;

	return samcop_read_register(samcop, SAMCOP_GPIO_GPB_DAT);
}
EXPORT_SYMBOL(samcop_get_gpio_b);

void samcop_set_gpio_int(struct device *dev, unsigned int idx, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	if (idx > 2) {
		printk(KERN_WARNING "%s idx %u is invalid, must be {0-2}\n",
		       __FUNCTION__, idx);
		return;
	}

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_INT(idx)) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_INT(idx), val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}

void samcop_set_gpio_filter_config(struct device *dev, unsigned int idx, u32 mask,
			       u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	if (idx > 6) {
		printk(KERN_WARNING "%s idx %u is invalid, must be {0-6}\n",
		       __FUNCTION__, idx);
		return;
	}

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_FLTCONFIG(idx)) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_FLTCONFIG(idx), val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}

void samcop_set_gpio_int_enable(struct device *dev, unsigned int idx, u32 mask,
			       u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags, val;

	if (idx > 1) {
		printk(KERN_WARNING "%s idx %u is invalid, must be {0,1}\n",
		       __FUNCTION__, idx);
		return;
	}

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_ENINT(idx)) & ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT(idx), val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}

void samcop_reset_fcd(struct device *dev)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	samcop_write_register(samcop, SAMCOP_GPIO_GPD_CON, SAMCOP_GPIO_GPD_CON_RESET);
	samcop_write_register(samcop, SAMCOP_GPIO_GPE_CON, SAMCOP_GPIO_GPE_CON_RESET);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}
EXPORT_SYMBOL(samcop_reset_fcd);

void samcop_set_spcr(struct device *dev, u32 mask, u32 bits)
{
	struct samcop_data *samcop = dev->driver_data;
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&samcop->gpio_lock, flags);
	val = samcop_read_register(samcop, SAMCOP_GPIO_SPCR);
	val &= ~mask;
	val |= bits;
	samcop_write_register(samcop, SAMCOP_GPIO_SPCR, val);
	spin_unlock_irqrestore(&samcop->gpio_lock, flags);
}
EXPORT_SYMBOL(samcop_set_spcr);


/*************************************************************
 SAMCOP clocks
 *************************************************************/

/* base clocks */

static struct clk clk_g = {
	.name		= "gclk",
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0
};

/* clock definitions */

static struct clk samcop_clocks[] = {
	{ .name    = "uclk",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  /* note that the sense of this uclk bit is reversed */
	  .ctrlbit = SAMCOP_CPM_CLKCON_UCLK_EN
	},
	{ .name    = "usb",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_USBHOST_CLKEN
	},
	{ .name    = "dma",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_DMAC_CLKEN
	},
	{ .name    = "gpio",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_GPIO_CLKEN
	},
	{ .name    = "fsi",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_FCD_CLKEN
	},
	{ .name    = "sdi",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_SD_CLKEN
	},
	{ .name    = "adc",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_ADC_CLKEN
	},
	{ .name    = "uart2",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_UART2_CLKEN
	},
	{ .name    = "uart1",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_UART1_CLKEN
	},
	{ .name    = "led",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_LED_CLKEN
	},
	{ .name    = "misc",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_MISC_CLKEN
	},
	{ .name    = "ds1wm",
	  .parent  = &clk_g,
	  .enable  = samcop_clock_en,
	  .disable = samcop_clock_dis,
	  .ctrlbit = SAMCOP_CPM_CLKCON_1WIRE_CLKEN
	},
};

/*************************************************************/

static struct ds1wm_platform_data samcop_w1_pdata = {
	.bus_shift = 2,
};

static struct tsadc_platform_data tsadc_pdata = {
	.x_pin = "samcop adc:x",
	.y_pin = "samcop adc:y",
	.z1_pin = "samcop adc:z1",
	.z2_pin = "samcop adc:z2",
	.max_jitter = 10,
};

struct samcop_block
{
	platform_device_id id;
	char *name;
	unsigned long start, end;
	unsigned long irq;
	void *platform_data;
};

static struct samcop_block samcop_blocks[] = {
	{
		.id = { -1 },
		.name = "samcop adc",
		.start = _SAMCOP_ADC_Base,
		.end = _SAMCOP_ADC_Base + 0x1f,
		.irq = _IRQ_SAMCOP_ADC,
	},
	{
		.id = { -1 },
		.name = "ts-adc",
		.platform_data = &tsadc_pdata,
		.irq = _IRQ_SAMCOP_ADCTS,
	},
	{
		.id = { -1 },
		.name = "ds1wm",
		.start = _SAMCOP_OWM_Base,
		.end = _SAMCOP_OWM_Base + 0x1f, /* 0x47 */
		.irq = _IRQ_SAMCOP_ONEWIRE,
		.platform_data = &samcop_w1_pdata,
	},
	{
		.id = { -1 },
		.name = "samcop fsi",
		.start = _SAMCOP_FSI_Base,
		.end = _SAMCOP_FSI_Base + 0x1f,
		.irq = _IRQ_SAMCOP_FCD,
	},
	{
		.id = { -1 },
		.name = "samcop sleeve",
		.start = _SAMCOP_PCMCIA_Base,
		.end = _SAMCOP_PCMCIA_Base + 0x1f,
		.irq = SAMCOP_EPS_IRQ_START,
	},
	{
		.id = { -1 },
		.name = "samcop dma",
		.start = _SAMCOP_DMAC_Base,
		.end = _SAMCOP_DMAC_Base + 0x3f,
		.irq = _IRQ_SAMCOP_DMA0,
		.platform_data = &samcop_dma_plat_data,
	},
	{
		.id = { -1 },
		.name = "samcop sdi",
		.start = _SAMCOP_SDI_Base,
		.end = _SAMCOP_SDI_Base + 0x43,
		.irq = _IRQ_SAMCOP_SD,
		.platform_data = &samcop_sdi_data,
	},
	{
		.id = { -1 },
		.name = "samcop usb host",
		.start = _SAMCOP_USBHOST_Base,
		.end = _SAMCOP_USBHOST_Base + 0xffff,
		.irq = _IRQ_SAMCOP_USBH,
	},
};

static void samcop_release(struct device *dev)
{
	struct platform_device *sdev = to_platform_device(dev);
	kfree(sdev->resource);
	kfree(sdev);
}

static int samcop_probe(struct platform_device *pdev)
{
	int i, rc;
	struct samcop_platform_data *pdata = pdev->dev.platform_data;
	struct samcop_data *samcop;

	samcop = kmalloc(sizeof(struct samcop_data), GFP_KERNEL);
	if (!samcop)
		goto enomem3;
	memset(samcop, 0, sizeof (*samcop));
	platform_set_drvdata(pdev, samcop);

	samcop->dev = &pdev->dev;
	samcop->irq_nr = platform_get_irq(pdev, 0);
	samcop->irq_base = pdata->irq_base;
	if (samcop->irq_base == 0) {
		printk("samcop: uninitialized irq_base!\n");
		goto enomem2;
	}

	samcop->mapping = ioremap((unsigned long)pdev->resource[0].start, SAMCOP_MAP_SIZE);
	if (!samcop->mapping) {
		printk("samcop: couldn't ioremap\n");
		goto enomem1;
	}

	if (dma_declare_coherent_memory(&pdev->dev,
					 pdev->resource[0].start + _SAMCOP_SRAM_Base,
					 _SAMCOP_SRAM_Base, SAMCOP_SRAM_SIZE,
					 DMA_MEMORY_MAP | DMA_MEMORY_INCLUDES_CHILDREN |
					 DMA_MEMORY_EXCLUSIVE) != DMA_MEMORY_MAP) {
		printk("samcop: couldn't declare coherent dma memory\n");
		goto enomem0;
	}

	printk("%s: using irq %d-%d on irq %d\n", pdev->name, samcop->irq_base,
	       samcop->irq_base + SAMCOP_NR_IRQS - 1, samcop->irq_nr);

	samcop_write_register(samcop, SAMCOP_CPM_ClockControl, SAMCOP_CPM_CLKCON_GPIO_CLKEN);
	samcop_write_register(samcop, SAMCOP_CPM_ClockSleep, pdata->clocksleep);
	samcop_write_register(samcop, SAMCOP_CPM_PllControl, pdata->pllcontrol);

	samcop_irq_init(samcop);

	/* Register SAMCOP's clocks. */
	clk_g.rate = samcop_gclk(samcop);

	if (clk_register(&clk_g) < 0)
		printk(KERN_ERR "failed to register SAMCOP gclk\n");

	samcop_device_data = samcop;
	for (i = 0; i < ARRAY_SIZE(samcop_clocks); i++) {
		/* ((struct samcop_clk_priv*)samcop_clocks[i].priv)->samcop = samcop; */
		rc = clk_register(&samcop_clocks[i]);
		if (rc < 0)
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       samcop_clocks[i].name, rc);
	}

	/* Register SAMCOP's platform devices. */
	samcop->ndevices = ARRAY_SIZE(samcop_blocks);
	samcop->devices = kmalloc(samcop->ndevices * sizeof(struct platform_device *), GFP_KERNEL);
	if (unlikely(!samcop->devices))
		goto enomem;
	memset(samcop->devices, 0, samcop->ndevices * sizeof(struct platform_device *));

	for (i = 0; i < samcop->ndevices; i++) {
		struct platform_device *sdev;
		struct samcop_block *blk = &samcop_blocks[i];
		struct resource *res;

		if (blk->start == _SAMCOP_ADC_Base)
			blk->platform_data = &pdata->samcop_adc_pdata;

		sdev = kmalloc(sizeof (*sdev), GFP_KERNEL);
		if (unlikely(!sdev))
			goto enomem;
		memset(sdev, 0, sizeof (*sdev));
		sdev->id = samcop_blocks[i].id.id;
		sdev->dev.parent = &pdev->dev;
		sdev->dev.platform_data = samcop_blocks[i].platform_data;
		sdev->dev.release = samcop_release;
		sdev->num_resources = (blk->irq == -1) ? 1 : 2;
		sdev->name = blk->name;
		res = kmalloc(sdev->num_resources * sizeof (struct resource), GFP_KERNEL);
		if (unlikely (!res))
			goto enomem;
		sdev->resource = res;
		memset(res, 0, sdev->num_resources * sizeof (struct resource));
		res[0].start = blk->start + pdev->resource[0].start;
		res[0].end = blk->end + pdev->resource[0].start;
		res[0].flags = IORESOURCE_MEM;
		res[0].parent = &pdev->resource[0];
		if (blk->irq != -1) {
			res[1].start = blk->irq + samcop->irq_base;
			res[1].end = res[1].start;
			res[1].flags = IORESOURCE_IRQ;
		}
		sdev->dev.dma_mem = pdev->dev.dma_mem;
		rc = platform_device_register(sdev);
		if (unlikely(rc != 0)) {
			printk("samcop: could not register %s\n", blk->name);
			kfree(sdev->resource);
			kfree(sdev);
			goto error;
		}
		samcop->devices[i] = sdev;
	}

	/* initialize GPIOs from platform_data */
	samcop_set_gpio_a_con(&pdev->dev, 0, ~0, pdata->gpio_pdata.gpacon1);
	samcop_set_gpio_a_con(&pdev->dev, 1, ~0, pdata->gpio_pdata.gpacon2);
	samcop_set_gpio_a(&pdev->dev, ~0, pdata->gpio_pdata.gpadat);
	samcop_set_gpio_a_pullup(&pdev->dev, ~0, pdata->gpio_pdata.gpaup);

	for (i = 0; i < 3; i++)
		samcop_set_gpio_int(&pdev->dev, i, ~0, pdata->gpio_pdata.gpioint[i]);

	for (i = 0; i < 7; i++)
		samcop_set_gpio_filter_config(&pdev->dev, i, ~0, pdata->gpio_pdata.gpioflt[i]);

	for (i = 0; i < 2; i++)
		samcop_set_gpio_int_enable(&pdev->dev, i, ~0, pdata->gpio_pdata.gpioenint[i]);


	/* initialize gpiodev_ops */
	pdata->gpiodev_ops.get = samcop_get_gpio_bit;
	pdata->gpiodev_ops.set = samcop_set_gpio_bit;
	pdata->gpiodev_ops.to_irq = samcop_gpio_to_irq;

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
	samcop_remove(pdev);
	return rc;

 enomem0:
	iounmap(samcop->mapping);
 enomem1:
 enomem2:
	kfree(samcop);
 enomem3:
	return -ENOMEM;
}

static int samcop_remove(struct platform_device *pdev)
{
	int i;
	struct samcop_platform_data *pdata = pdev->dev.platform_data;
	struct samcop_data *samcop = platform_get_drvdata(pdev);

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			platform_device_unregister(pdata->child_devs[i]);
		}
	}

	samcop_write_register(samcop, SAMCOP_PCMCIA_IC, 0);	/* nothing enabled */
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT1, 0);
	samcop_write_register(samcop, SAMCOP_GPIO_ENINT2, 0);
	samcop_write_register(samcop, SAMCOP_IC_INTMSK, 0xffffffff);

	for (i = 0; i < SAMCOP_NR_IRQS; i++) {
		int irq = i + samcop->irq_base;
		set_irq_flags(irq, 0);
		set_irq_handler (irq, NULL);
		set_irq_chip(irq, NULL);
		set_irq_chip_data(irq, NULL);
	}

	set_irq_chained_handler(samcop->irq_nr, NULL);

	if (samcop->devices) {
		for (i = 0; i < samcop->ndevices; i++) {
			if (samcop->devices[i])
				platform_device_unregister(samcop->devices[i]);
		}
		kfree(samcop->devices);
	}

	for (i = 0; i < ARRAY_SIZE(samcop_clocks); i++)
		clk_unregister(&samcop_clocks[i]);
	clk_unregister(&clk_g);

	samcop_write_register(samcop, SAMCOP_CPM_ClockControl, 0);

	dma_release_declared_memory(&pdev->dev);
	iounmap(samcop->mapping);

	kfree(samcop);

	return 0;
}

static void samcop_shutdown(struct platform_device *pdev)
{
}

static int samcop_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct samcop_data *samcop;

	samcop = platform_get_drvdata(pdev);

	samcop->irqmask = samcop_read_register(samcop, SAMCOP_IC_INTMSK);
	if (samcop->irqmask != 0xffffffff) {
		samcop_write_register(samcop, SAMCOP_IC_INTMSK, 0xffffffff);
		printk(KERN_WARNING "irqs %08lx still enabled\n", ~samcop->irqmask);
	}

	return 0;
}

static int samcop_resume(struct platform_device *pdev)
{
	struct samcop_data *samcop;

	samcop = platform_get_drvdata(pdev);

	if (samcop->irqmask != 0xffffffff)
		samcop_write_register(samcop, SAMCOP_IC_INTMSK, samcop->irqmask);

	return 0;
}

static struct platform_driver samcop_device_driver = {
	.driver = {
		.name		= "samcop",
	},
	.probe		= samcop_probe,
	.remove		= samcop_remove,
	.suspend	= samcop_suspend,
	.resume		= samcop_resume,
	.shutdown	= samcop_shutdown,
};

static int __init samcop_base_init(void)
{
	int retval = 0;
	retval = platform_driver_register(&samcop_device_driver);
	return retval;
}

static void __exit samcop_base_exit(void)
{
	platform_driver_unregister(&samcop_device_driver);
}

#ifdef MODULE
module_init(samcop_base_init);
#else
subsys_initcall(samcop_base_init);
#endif
module_exit(samcop_base_exit);

MODULE_AUTHOR("Jamey Hicks <jamey@handhelds.org>");
MODULE_DESCRIPTION("Base platform_device driver for the SAMCOP chip");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE("samcop");
