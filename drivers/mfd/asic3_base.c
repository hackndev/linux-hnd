/*
 * Driver interface to HTC "ASIC3"
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2004-2005 Phil Blundell
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
 * Author:  Andrew Christian
 *          <Andrew.Christian@compaq.com>
 *          October 2001
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/ds1wm.h>
#include <linux/gpiodev2.h>
#include <asm/arch/clock.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include <linux/mfd/tmio_mmc.h>
#include "soc-core.h"


struct asic3_data {
	void *mapping;
	unsigned int bus_shift;
	int irq_base;
	int irq_nr;

	u16 irq_bothedge[4];
	struct device *dev;

	struct platform_device *mmc_dev;
};

static DEFINE_SPINLOCK(asic3_gpio_lock);

static int asic3_remove(struct platform_device *dev);

static inline unsigned long asic3_address(struct device *dev,
					  unsigned int reg)
{
	struct asic3_data *adata;

	adata = (struct asic3_data *)dev->driver_data;

	return (unsigned long)adata->mapping + (reg >> (2 - adata->bus_shift));
}

void asic3_write_register(struct device *dev, unsigned int reg, u32 value)
{
	__raw_writew(value, asic3_address(dev, reg));
}
EXPORT_SYMBOL(asic3_write_register);

u32 asic3_read_register(struct device *dev, unsigned int reg)
{
	return __raw_readw(asic3_address(dev, reg));
}
EXPORT_SYMBOL(asic3_read_register);

static inline void __asic3_write_register(struct asic3_data *asic,
					  unsigned int reg, u32 value)
{
	__raw_writew(value, (unsigned long)asic->mapping
			    + (reg >> (2 - asic->bus_shift)));
}

static inline u32 __asic3_read_register(struct asic3_data *asic,
					unsigned int reg)
{
	return __raw_readw((unsigned long)asic->mapping
			   + (reg >> (2 - asic->bus_shift)));
}

#define ASIC3_GPIO_FN(get_fn_name, set_fn_name, REG)			\
u32 get_fn_name(struct device *dev)					\
{                                                                       \
	return asic3_read_register(dev, REG);				\
}                                                                       \
EXPORT_SYMBOL(get_fn_name);						\
									\
void set_fn_name(struct device *dev, u32 bits, u32 val)			\
{                                                                       \
	unsigned long flags;						\
									\
	spin_lock_irqsave(&asic3_gpio_lock, flags);			\
	val |= (asic3_read_register(dev, REG) & ~bits);			\
	asic3_write_register(dev, REG, val);				\
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);		\
}                                                                       \
EXPORT_SYMBOL(set_fn_name);

#define ASIC3_GPIO_REGISTER(ACTION, action, fn, FN)			\
	ASIC3_GPIO_FN(asic3_get_gpio_ ## action ## _ ## fn ,		\
		       asic3_set_gpio_ ## action ## _ ## fn ,		\
		       _IPAQ_ASIC3_GPIO_ ## FN ## _Base 		\
		       + _IPAQ_ASIC3_GPIO_ ## ACTION )

#define ASIC3_GPIO_FUNCTIONS(fn, FN)					\
	ASIC3_GPIO_REGISTER(Direction, dir, fn, FN)			\
	ASIC3_GPIO_REGISTER(Out, out, fn, FN)				\
	ASIC3_GPIO_REGISTER(SleepMask, sleepmask, fn, FN)		\
	ASIC3_GPIO_REGISTER(SleepOut, sleepout, fn, FN)		\
	ASIC3_GPIO_REGISTER(BattFaultOut, battfaultout, fn, FN)	\
	ASIC3_GPIO_REGISTER(AltFunction, alt_fn, fn, FN)		\
	ASIC3_GPIO_REGISTER(SleepConf, sleepconf, fn, FN)		\
	ASIC3_GPIO_REGISTER(Status, status, fn, FN)

#if 0
	ASIC3_GPIO_REGISTER(Mask, mask, fn, FN)
	ASIC3_GPIO_REGISTER(TriggerType, trigtype, fn, FN)
	ASIC3_GPIO_REGISTER(EdgeTrigger, rising, fn, FN)
	ASIC3_GPIO_REGISTER(LevelTrigger, triglevel, fn, FN)
	ASIC3_GPIO_REGISTER(IntStatus, intstatus, fn, FN)
#endif

ASIC3_GPIO_FUNCTIONS(a, A)
ASIC3_GPIO_FUNCTIONS(b, B)
ASIC3_GPIO_FUNCTIONS(c, C)
ASIC3_GPIO_FUNCTIONS(d, D)

int asic3_gpio_get_value(struct device *dev, unsigned gpio)
{
	u32 mask = ASIC3_GPIO_bit(gpio);
	printk("%s(%d)\n", __FUNCTION__, gpio);
	switch ((gpio & GPIO_BASE_MASK) >> 4) {
	case _IPAQ_ASIC3_GPIO_BANK_A:
		return asic3_get_gpio_status_a(dev) & mask;
	case _IPAQ_ASIC3_GPIO_BANK_B:
		return asic3_get_gpio_status_b(dev) & mask;
	case _IPAQ_ASIC3_GPIO_BANK_C:
		return asic3_get_gpio_status_c(dev) & mask;
	case _IPAQ_ASIC3_GPIO_BANK_D:
		return asic3_get_gpio_status_d(dev) & mask;
	}

	printk(KERN_ERR "%s: invalid GPIO value 0x%x", __FUNCTION__, gpio);
	return 0;
}
EXPORT_SYMBOL(asic3_gpio_get_value);

void asic3_gpio_set_value(struct device *dev, unsigned gpio, int val)
{
	u32 mask = ASIC3_GPIO_bit(gpio);
	u32 bitval = 0;
	if (val)  bitval = mask;
	printk("%s(%d, %d)\n", __FUNCTION__, gpio, val);

	switch ((gpio & GPIO_BASE_MASK) >> 4) {
	case _IPAQ_ASIC3_GPIO_BANK_A:
		asic3_set_gpio_out_a(dev, mask, bitval);
		return;
	case _IPAQ_ASIC3_GPIO_BANK_B:
		asic3_set_gpio_out_b(dev, mask, bitval);
		return;
	case _IPAQ_ASIC3_GPIO_BANK_C:
		asic3_set_gpio_out_c(dev, mask, bitval);
		return;
	case _IPAQ_ASIC3_GPIO_BANK_D:
		asic3_set_gpio_out_d(dev, mask, bitval);
		return;
	}

	printk(KERN_ERR "%s: invalid GPIO value 0x%x", __FUNCTION__, gpio);
}
EXPORT_SYMBOL(asic3_gpio_set_value);

int asic3_irq_base(struct device *dev)
{
	struct asic3_data *asic = dev->driver_data;

	return asic->irq_base;
}
EXPORT_SYMBOL(asic3_irq_base);

static int asic3_gpio_to_irq(struct device *dev, unsigned gpio)
{
	struct asic3_data *asic = dev->driver_data;
	printk("%s(%d)\n", __FUNCTION__, gpio);

	return asic->irq_base + (gpio & GPIO_BASE_MASK);
}

void asic3_set_led(struct device *dev, int led_num, int duty_time,
		   int cycle_time, int timebase)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned int led_base;

	/* it's a macro thing: see #define _IPAQ_ASIC_LED_0_Base for why you
	 * can't substitute led_num in the macros below...
	 */

	switch (led_num) {
	case 0:
		led_base = _IPAQ_ASIC3_LED_0_Base;
		break;
	case 1:
		led_base = _IPAQ_ASIC3_LED_1_Base;
		break;
	case 2:
		led_base = _IPAQ_ASIC3_LED_2_Base;
		break;
	default:
		printk(KERN_ERR "%s: invalid led number %d", __FUNCTION__,
		       led_num);
		return;
	}

	__asic3_write_register(asic, led_base + _IPAQ_ASIC3_LED_TimeBase,
			       timebase | LED_EN);
	__asic3_write_register(asic, led_base + _IPAQ_ASIC3_LED_PeriodTime,
			       cycle_time);
	__asic3_write_register(asic, led_base + _IPAQ_ASIC3_LED_DutyTime,
			       0);
	udelay(20);     /* asic voodoo - possibly need a whole duty cycle? */
	__asic3_write_register(asic, led_base + _IPAQ_ASIC3_LED_DutyTime,
			       duty_time);
}
EXPORT_SYMBOL(asic3_set_led);

void asic3_set_clock_sel(struct device *dev, u32 bits, u32 val)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	v = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, SEL));
	v = (v & ~bits) | val;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, SEL), v);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}
EXPORT_SYMBOL(asic3_set_clock_sel);

void asic3_set_clock_cdex(struct device *dev, u32 bits, u32 val)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	v = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX));
	v = (v & ~bits) | val;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX), v);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}
EXPORT_SYMBOL(asic3_set_clock_cdex);

static void asic3_clock_cdex_enable(struct clk *clk)
{
	struct asic3_data *asic = (struct asic3_data *)clk->parent->ctrlbit;
	unsigned long flags, val;

	local_irq_save(flags);

	val = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX));
	val |= clk->ctrlbit;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX), val);

	local_irq_restore(flags);
}

static void asic3_clock_cdex_disable(struct clk *clk)
{
	struct asic3_data *asic = (struct asic3_data *)clk->parent->ctrlbit;
	unsigned long flags, val;

	local_irq_save(flags);

	val = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX));
	val &= ~clk->ctrlbit;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX), val);

	local_irq_restore(flags);
}

/* base clocks */

static struct clk clk_g = {
	.name		= "gclk",
	.rate		= 0,
	.parent		= NULL,
};

/* clock definitions */

static struct clk asic3_clocks[] = {
	{
		.name    = "spi",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_SPI,
	},
#ifdef CONFIG_HTC_ASIC3_DS1WM
	{
		.name    = "ds1wm",
		.id      = -1,
		.rate	 = 5000000,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_OWM,
	},
#endif
	{
		.name    = "pwm0",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_PWM0,
	},
	{
		.name    = "pwm1",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_PWM1,
	},
	{
		.name    = "led0",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_LED0,
	},
	{
		.name    = "led1",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_LED1,
	},
	{
		.name    = "led2",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_LED2,
	},
	{
		.name    = "smbus",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_SMBUS,
	},
	{
		.name    = "ex0",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_EX0,
	},
	{
		.name    = "ex1",
		.id      = -1,
		.parent  = &clk_g,
		.enable  = asic3_clock_cdex_enable,
		.disable = asic3_clock_cdex_disable,
		.ctrlbit = CLOCK_CDEX_EX1,
	},
};

void asic3_set_extcf_select(struct device *dev, u32 bits, u32 val)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	v = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(EXTCF, Select));
	v = (v & ~bits) | val;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(EXTCF, Select), v);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}
EXPORT_SYMBOL(asic3_set_extcf_select);

void asic3_set_extcf_reset(struct device *dev, u32 bits, u32 val)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	v = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(EXTCF, Reset));
	v = (v & ~bits) | val;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(EXTCF, Reset), v);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}
EXPORT_SYMBOL(asic3_set_extcf_reset);

void asic3_set_sdhwctrl(struct device *dev, u32 bits, u32 val)
{
	struct asic3_data *asic = dev->driver_data;
	unsigned long flags;
	u32 v;

	spin_lock_irqsave (&asic3_gpio_lock, flags);
	v = __asic3_read_register(asic, IPAQ_ASIC3_OFFSET(SDHWCTRL, SDConf));
	v = (v & ~bits) | val;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(SDHWCTRL, SDConf), v);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}
EXPORT_SYMBOL(asic3_set_sdhwctrl);


#define MAX_ASIC_ISR_LOOPS    20
#define _IPAQ_ASIC3_GPIO_Base_INCR \
	(_IPAQ_ASIC3_GPIO_B_Base - _IPAQ_ASIC3_GPIO_A_Base)

static inline void asic3_irq_flip_edge(struct asic3_data *asic,
				       u32 base, int bit)
{
	u16 edge = __asic3_read_register(asic,
		base + _IPAQ_ASIC3_GPIO_EdgeTrigger);
	edge ^= bit;
	__asic3_write_register(asic,
		base + _IPAQ_ASIC3_GPIO_EdgeTrigger, edge);
}

static void asic3_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	int iter;
	struct asic3_data *asic;

	/* Acknowledge the parrent (i.e. CPU's) IRQ */
	desc->chip->ack(irq);

	asic = desc->handler_data;

	/* printk( KERN_NOTICE "asic3_irq_demux: irq=%d\n", irq ); */
	for (iter = 0 ; iter < MAX_ASIC_ISR_LOOPS; iter++) {
		u32 status;
		int bank;

		status = __asic3_read_register(asic,
			IPAQ_ASIC3_OFFSET(INTR, PIntStat));
		/* Check all ten register bits */
		if ((status & 0x3ff) == 0)
			break;

		/* Handle GPIO IRQs */
		for (bank = 0; bank < 4; bank++) {
			if (status & (1 << bank)) {
				unsigned long base, i, istat;

				base = _IPAQ_ASIC3_GPIO_A_Base
				       + bank * _IPAQ_ASIC3_GPIO_Base_INCR;
				istat = __asic3_read_register(asic,
					base + _IPAQ_ASIC3_GPIO_IntStatus);
				/* IntStatus is write 0 to clear */
				/* XXX could miss interrupts! */
				__asic3_write_register(asic,
					base + _IPAQ_ASIC3_GPIO_IntStatus, 0);

				for (i = 0; i < 16; i++) {
					int bit = (1 << i);
					unsigned int irqnr;
					if (!(istat & bit))
						continue;

					irqnr = asic->irq_base 
						+ (16 * bank) + i;
					desc = irq_desc + irqnr;
					desc->handle_irq(irqnr, desc);
					if (asic->irq_bothedge[bank] & bit) {
						asic3_irq_flip_edge(asic, base,
								    bit);
					}
				}
			}
		}

		/* Handle remaining IRQs in the status register */
		{
			int i;

			for (i = ASIC3_LED0_IRQ; i <= ASIC3_OWM_IRQ; i++) {
				/* They start at bit 4 and go up */
				if (status & (1 << (i - ASIC3_LED0_IRQ + 4))) {
					desc = irq_desc + asic->irq_base + i;
					desc->handle_irq(asic->irq_base + i,
							 desc);
				}
			}
		}

	}

	if (iter >= MAX_ASIC_ISR_LOOPS)
		printk(KERN_ERR "%s: interrupt processing overrun\n",
		       __FUNCTION__);
}

static inline int asic3_irq_to_bank(struct asic3_data *asic, int irq)
{
	int n;

	n = (irq - asic->irq_base) >> 4;

	return (n * (_IPAQ_ASIC3_GPIO_B_Base - _IPAQ_ASIC3_GPIO_A_Base));
}

static inline int asic3_irq_to_index(struct asic3_data *asic, int irq)
{
	return (irq - asic->irq_base) & 15;
}

static void asic3_mask_gpio_irq(unsigned int irq)
{
	struct asic3_data *asic = get_irq_chip_data(irq);
	u32 val, bank, index;
	unsigned long flags;

	bank = asic3_irq_to_bank(asic, irq);
	index = asic3_irq_to_index(asic, irq);

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	val = __asic3_read_register(asic, bank + _IPAQ_ASIC3_GPIO_Mask);
	val |= 1 << index;
	__asic3_write_register(asic, bank + _IPAQ_ASIC3_GPIO_Mask, val);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}

static void asic3_mask_irq(unsigned int irq)
{
	struct asic3_data *asic = get_irq_chip_data(irq);
	int regval;

	if (irq < ASIC3_NR_GPIO_IRQS) {
		printk(KERN_ERR "asic3_base: gpio mask attempt, irq %d\n",
		       irq);
		return;
	}

	regval = __asic3_read_register(asic,
		_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask);

	switch (irq - asic->irq_base) {
	case ASIC3_LED0_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK0);
		break;
	case ASIC3_LED1_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK1);
		break;
	case ASIC3_LED2_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK2);
		break;
	case ASIC3_SPI_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK3);
		break;
	case ASIC3_SMBUS_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK4);
		break;
	case ASIC3_OWM_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval & ~ASIC3_INTMASK_MASK5);
		break;
	default:
		printk(KERN_ERR "asic3_base: bad non-gpio irq %d\n", irq);
		break;
	}
}

static void asic3_unmask_gpio_irq(unsigned int irq)
{
	struct asic3_data *asic = get_irq_chip_data(irq);
	u32 val, bank, index;
	unsigned long flags;

	bank = asic3_irq_to_bank(asic, irq);
	index = asic3_irq_to_index(asic, irq);

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	val = __asic3_read_register(asic, bank + _IPAQ_ASIC3_GPIO_Mask);
	val &= ~(1 << index);
	__asic3_write_register(asic, bank + _IPAQ_ASIC3_GPIO_Mask, val);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
}

static void asic3_unmask_irq(unsigned int irq)
{
	struct asic3_data *asic = get_irq_chip_data(irq);
	int regval;

	if (irq < ASIC3_NR_GPIO_IRQS) {
		printk(KERN_ERR "asic3_base: gpio unmask attempt, irq %d\n",
		       irq);
		return;
	}

	regval = __asic3_read_register(asic,
		_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask);

	switch (irq - asic->irq_base) {
	case ASIC3_LED0_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK0);
		break;
	case ASIC3_LED1_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK1);
		break;
	case ASIC3_LED2_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK2);
		break;
	case ASIC3_SPI_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK3);
		break;
	case ASIC3_SMBUS_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK4);
		break;
	case ASIC3_OWM_IRQ:
		__asic3_write_register(asic,
			_IPAQ_ASIC3_INTR_Base + _IPAQ_ASIC3_INTR_IntMask,
			regval | ASIC3_INTMASK_MASK5);
		break;
	default:
		printk(KERN_ERR "asic3_base: bad non-gpio irq %d\n", irq);
		break;
	}
}

static int asic3_gpio_irq_type(unsigned int irq, unsigned int type)
{
	struct asic3_data *asic = get_irq_chip_data(irq);
	u32 bank, index;
	unsigned long flags;
	u16 trigger, level, edge, bit;

	bank = asic3_irq_to_bank(asic, irq);
	index = asic3_irq_to_index(asic, irq);
	bit = 1<<index;

	spin_lock_irqsave(&asic3_gpio_lock, flags);
	level = __asic3_read_register(asic,
		bank + _IPAQ_ASIC3_GPIO_LevelTrigger);
	edge = __asic3_read_register(asic,
		bank + _IPAQ_ASIC3_GPIO_EdgeTrigger);
	trigger = __asic3_read_register(asic,
		bank + _IPAQ_ASIC3_GPIO_TriggerType);
	asic->irq_bothedge[(irq - asic->irq_base) >> 4] &= ~bit;

	if (type == IRQT_RISING) {
		trigger |= bit;
		edge |= bit;
	} else if (type == IRQT_FALLING) {
		trigger |= bit;
		edge &= ~bit;
	} else if (type == IRQT_BOTHEDGE) {
		trigger |= bit;
		if (asic3_gpio_get_value(asic->dev, irq - asic->irq_base))
			edge &= ~bit;
		else
			edge |= bit;
		asic->irq_bothedge[(irq - asic->irq_base) >> 4] |= bit;
	} else if (type == IRQT_LOW) {
		trigger &= ~bit;
		level &= ~bit;
	} else if (type == IRQT_HIGH) {
		trigger &= ~bit;
		level |= bit;
	} else {
		/*
		 * if type == IRQT_NOEDGE, we should mask interrupts, but
		 * be careful to not unmask them if mask was also called.
		 * Probably need internal state for mask.
		 */
		printk(KERN_NOTICE "asic3: irq type not changed.\n");
	}
	__asic3_write_register(asic, bank + _IPAQ_ASIC3_GPIO_LevelTrigger,
			       level);
	__asic3_write_register(asic, bank + _IPAQ_ASIC3_GPIO_EdgeTrigger,
			       edge);
	__asic3_write_register(asic, bank + _IPAQ_ASIC3_GPIO_TriggerType,
			       trigger);
	spin_unlock_irqrestore(&asic3_gpio_lock, flags);
	return 0;
}

static struct irq_chip asic3_gpio_irq_chip = {
	.name		= "ASIC3-GPIO",
	.ack		= asic3_mask_gpio_irq,
	.mask		= asic3_mask_gpio_irq,
	.unmask		= asic3_unmask_gpio_irq,
	.set_type	= asic3_gpio_irq_type,
};

static struct irq_chip asic3_irq_chip = {
	.name		= "ASIC3",
	.ack		= asic3_mask_irq,
	.mask		= asic3_mask_irq,
	.unmask		= asic3_unmask_irq,
};

static void asic3_release(struct device *dev)
{
	struct platform_device *sdev = to_platform_device(dev);

	kfree(sdev->resource);
	kfree(sdev);
}

int asic3_register_mmc(struct device *dev)
{
	struct platform_device *sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	struct tmio_mmc_hwconfig *mmc_config = kmalloc(sizeof(*mmc_config),
						       GFP_KERNEL);
	struct platform_device *pdev = to_platform_device(dev);
	struct asic3_data *asic = dev->driver_data;
	struct asic3_platform_data *asic3_pdata = dev->platform_data;
	struct resource *res;
	int rc;

	if (sdev == NULL || mmc_config == NULL)
		return -ENOMEM;

	if (asic3_pdata->tmio_mmc_hwconfig) {
		memcpy(mmc_config, asic3_pdata->tmio_mmc_hwconfig,
		       sizeof(*mmc_config));
	} else {
		memset(mmc_config, 0, sizeof(*mmc_config));
	}
	mmc_config->address_shift = asic->bus_shift;

	sdev->id = -1;
	sdev->name = "asic3_mmc";
	sdev->dev.parent = dev;
	sdev->num_resources = 2;
	sdev->dev.platform_data = mmc_config;
	sdev->dev.release = asic3_release;

	res = kzalloc(sdev->num_resources * sizeof(struct resource),
		      GFP_KERNEL);
	if (res == NULL) {
		kfree(sdev);
		kfree(mmc_config);
		return -ENOMEM;
	}
	sdev->resource = res;

	res[0].start = pdev->resource[2].start;
	res[0].end   = pdev->resource[2].end;
	res[0].flags = IORESOURCE_MEM;
	res[1].start = res[1].end = pdev->resource[3].start;
	res[1].flags = IORESOURCE_IRQ;

	rc = platform_device_register(sdev);
	if (rc) {
		printk(KERN_ERR "asic3_base: "
		       "Could not register asic3_mmc device\n");
		kfree(res);
		kfree(sdev);
		return rc;
	}

	asic->mmc_dev = sdev;

	return 0;
}
EXPORT_SYMBOL(asic3_register_mmc);

int asic3_unregister_mmc(struct device *dev)
{
	struct asic3_data *asic = dev->driver_data;
	platform_device_unregister(asic->mmc_dev);
	asic->mmc_dev = 0;

	return 0;
}
EXPORT_SYMBOL(asic3_unregister_mmc);

#ifdef CONFIG_HTC_ASIC3_DS1WM
/*
 * 	DS1WM subdevice
 */

static void asic3_ds1wm_enable(struct platform_device *ds1wm_dev)
{
	struct device *dev = ds1wm_dev->dev.parent;

	/* Turn on external clocks and the OWM clock */
	asic3_set_clock_cdex(dev,
		CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | CLOCK_CDEX_OWM,
		CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | CLOCK_CDEX_OWM);

	mdelay(1);

	asic3_set_extcf_reset(dev, ASIC3_EXTCF_OWM_RESET,
			      ASIC3_EXTCF_OWM_RESET);
	mdelay(1);
	asic3_set_extcf_reset(dev, ASIC3_EXTCF_OWM_RESET, 0);
	mdelay(1);

	/* Clear OWM_SMB, set OWM_EN */
	asic3_set_extcf_select(dev,
		ASIC3_EXTCF_OWM_SMB | ASIC3_EXTCF_OWM_EN,
		0                   | ASIC3_EXTCF_OWM_EN);

	mdelay(1);
}

static void asic3_ds1wm_disable(struct platform_device *ds1wm_dev)
{
	struct device *dev = ds1wm_dev->dev.parent;

	asic3_set_extcf_select(dev,
		ASIC3_EXTCF_OWM_SMB | ASIC3_EXTCF_OWM_EN,
		0                   | 0);

	asic3_set_clock_cdex(dev,
		CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | CLOCK_CDEX_OWM,
		CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1 | 0);
}


static struct resource asic3_ds1wm_resources[] = {
	{
		.start = _IPAQ_ASIC3_OWM_Base,
		.end   = _IPAQ_ASIC3_OWM_Base + 0x14 - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = ASIC3_OWM_IRQ,
		.end   = ASIC3_OWM_IRQ,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
			 IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct ds1wm_platform_data ds1wm_pd = {
	.enable = asic3_ds1wm_enable,
	.disable = asic3_ds1wm_disable,
};
#endif

static struct soc_device_data asic3_blocks[] = {
#ifdef CONFIG_HTC_ASIC3_DS1WM
	{
		.name = "ds1wm",
		.res = asic3_ds1wm_resources,
		.num_resources = ARRAY_SIZE(asic3_ds1wm_resources),
		.hwconfig = &ds1wm_pd,
	},
#endif
};

static int asic3_probe(struct platform_device *pdev)
{
	struct asic3_platform_data *pdata = pdev->dev.platform_data;
	struct asic3_data *asic;
	struct device *dev = &pdev->dev;
	unsigned long clksel;
	int i, rc;

	asic = kzalloc(sizeof(struct asic3_data), GFP_KERNEL);
	if (!asic)
		return -ENOMEM;

	platform_set_drvdata(pdev, asic);
	asic->dev = &pdev->dev;

	asic->mapping = ioremap(pdev->resource[0].start, IPAQ_ASIC3_MAP_SIZE);
	if (!asic->mapping) {
		printk(KERN_ERR "asic3: couldn't ioremap ASIC3\n");
		kfree (asic);
		return -ENOMEM;
	}

	if (pdata && pdata->bus_shift)
		asic->bus_shift = pdata->bus_shift;
	else
		asic->bus_shift = 2;

	/* XXX: should get correct SD clock values from pdata struct  */
	clksel = 0;
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, SEL), clksel);

	/* Register ASIC3's clocks. */
	clk_g.ctrlbit = (int)asic;

	if (clk_register(&clk_g) < 0)
		printk(KERN_ERR "asic3: failed to register ASIC3 gclk\n");

	for (i = 0; i < ARRAY_SIZE(asic3_clocks); i++) {
		rc = clk_register(&asic3_clocks[i]);
		if (rc < 0)
			printk(KERN_ERR "asic3: "
			       "failed to register clock %s (%d)\n",
			       asic3_clocks[i].name, rc);
	}

	__asic3_write_register(asic, IPAQ_ASIC3_GPIO_OFFSET(A, Mask), 0xffff);
	__asic3_write_register(asic, IPAQ_ASIC3_GPIO_OFFSET(B, Mask), 0xffff);
	__asic3_write_register(asic, IPAQ_ASIC3_GPIO_OFFSET(C, Mask), 0xffff);
	__asic3_write_register(asic, IPAQ_ASIC3_GPIO_OFFSET(D, Mask), 0xffff);

	asic3_set_gpio_sleepmask_a(dev, 0xffff, 0xffff);
	asic3_set_gpio_sleepmask_b(dev, 0xffff, 0xffff);
	asic3_set_gpio_sleepmask_c(dev, 0xffff, 0xffff);
	asic3_set_gpio_sleepmask_d(dev, 0xffff, 0xffff);

	if (pdata) {
		asic3_set_gpio_out_a(dev, 0xffff, pdata->gpio_a.init);
		asic3_set_gpio_out_b(dev, 0xffff, pdata->gpio_b.init);
		asic3_set_gpio_out_c(dev, 0xffff, pdata->gpio_c.init);
		asic3_set_gpio_out_d(dev, 0xffff, pdata->gpio_d.init);

		asic3_set_gpio_dir_a(dev, 0xffff, pdata->gpio_a.dir);
		asic3_set_gpio_dir_b(dev, 0xffff, pdata->gpio_b.dir);
		asic3_set_gpio_dir_c(dev, 0xffff, pdata->gpio_c.dir);
		asic3_set_gpio_dir_d(dev, 0xffff, pdata->gpio_d.dir);

		asic3_set_gpio_sleepmask_a(dev, 0xffff,
					   pdata->gpio_a.sleep_mask);
		asic3_set_gpio_sleepmask_b(dev, 0xffff,
					   pdata->gpio_b.sleep_mask);
		asic3_set_gpio_sleepmask_c(dev, 0xffff,
					   pdata->gpio_c.sleep_mask);
		asic3_set_gpio_sleepmask_d(dev, 0xffff,
					   pdata->gpio_d.sleep_mask);

		asic3_set_gpio_sleepout_a(dev, 0xffff,
					  pdata->gpio_a.sleep_out);
		asic3_set_gpio_sleepout_b(dev, 0xffff,
					  pdata->gpio_b.sleep_out);
		asic3_set_gpio_sleepout_c(dev, 0xffff,
					  pdata->gpio_c.sleep_out);
		asic3_set_gpio_sleepout_d(dev, 0xffff,
					  pdata->gpio_d.sleep_out);

		asic3_set_gpio_battfaultout_a(dev, 0xffff,
					      pdata->gpio_a.batt_fault_out);
		asic3_set_gpio_battfaultout_b(dev, 0xffff,
					      pdata->gpio_b.batt_fault_out);
		asic3_set_gpio_battfaultout_c(dev, 0xffff,
					      pdata->gpio_c.batt_fault_out);
		asic3_set_gpio_battfaultout_d(dev, 0xffff,
					      pdata->gpio_d.batt_fault_out);

		asic3_set_gpio_sleepconf_a(dev, 0xffff,
					   pdata->gpio_a.sleep_conf);
		asic3_set_gpio_sleepconf_b(dev, 0xffff,
					   pdata->gpio_b.sleep_conf);
		asic3_set_gpio_sleepconf_c(dev, 0xffff,
					   pdata->gpio_c.sleep_conf);
		asic3_set_gpio_sleepconf_d(dev, 0xffff,
					   pdata->gpio_d.sleep_conf);

		asic3_set_gpio_alt_fn_a(dev, 0xffff,
					pdata->gpio_a.alt_function);
		asic3_set_gpio_alt_fn_b(dev, 0xffff,
					pdata->gpio_b.alt_function);
		asic3_set_gpio_alt_fn_c(dev, 0xffff,
					pdata->gpio_c.alt_function);
		asic3_set_gpio_alt_fn_d(dev, 0xffff,
					pdata->gpio_d.alt_function);
	}

	asic->irq_nr = -1;
	asic->irq_base = -1;

	if (pdev->num_resources > 1)
		asic->irq_nr = pdev->resource[1].start;

	if (asic->irq_nr != -1) {
		unsigned int i;

		if (!pdata->irq_base) {
			printk(KERN_ERR "asic3: IRQ base not specified\n");
			asic3_remove(pdev);
			return -EINVAL;
		}

		asic->irq_base = pdata->irq_base;

		/* turn on clock to IRQ controller */
		clksel |= CLOCK_SEL_CX;
		__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, SEL),
				       clksel);

		printk(KERN_INFO "asic3: using irq %d-%d on irq %d\n",
		       asic->irq_base, asic->irq_base + ASIC3_NR_IRQS - 1,
		       asic->irq_nr);

		for (i = 0 ; i < ASIC3_NR_IRQS ; i++) {
			int irq = i + asic->irq_base;
			if (i < ASIC3_NR_GPIO_IRQS) {
				set_irq_chip(irq, &asic3_gpio_irq_chip);
				set_irq_chip_data(irq, asic);
				set_irq_handler(irq, handle_level_irq);
				set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
			} else {
				/* The remaining IRQs are not GPIO */
				set_irq_chip(irq, &asic3_irq_chip);
				set_irq_chip_data(irq, asic);
				set_irq_handler(irq, handle_level_irq);
				set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
			}
		}

		__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(INTR, IntMask),
					ASIC3_INTMASK_GINTMASK);

		set_irq_chained_handler(asic->irq_nr, asic3_irq_demux);
		set_irq_type(asic->irq_nr, IRQT_RISING);
		set_irq_data(asic->irq_nr, asic);
	}

#ifdef CONFIG_HTC_ASIC3_DS1WM
	ds1wm_pd.bus_shift = asic->bus_shift;
#endif

	pdata->gpiodev_ops.get = asic3_gpio_get_value;
	pdata->gpiodev_ops.set = asic3_gpio_set_value;
	pdata->gpiodev_ops.to_irq = asic3_gpio_to_irq;

	{
	struct gpio_ops ops;
	ops.get_value = asic3_gpio_get_value;
	ops.set_value = asic3_gpio_set_value;
	ops.to_irq = asic3_gpio_to_irq;
	gpiodev_register(pdata->gpio_base, dev, &ops);
	}

	soc_add_devices(pdev, asic3_blocks, ARRAY_SIZE(asic3_blocks),
			&pdev->resource[0],
			asic->bus_shift - ASIC3_DEFAULT_ADDR_SHIFT,
			asic->irq_base);

	if (pdev->num_resources > 2) {
		int rc;
		rc = asic3_register_mmc(dev);
		if (rc) {
			asic3_remove(pdev);
			return rc;
		}
	}

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			pdata->child_devs[i]->dev.parent = &pdev->dev;
			platform_device_register(pdata->child_devs[i]);
		}
	}

	return 0;
}

static int asic3_remove(struct platform_device *pdev)
{
	struct asic3_platform_data *pdata = pdev->dev.platform_data;
	struct asic3_data *asic = platform_get_drvdata(pdev);
	int i;

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			platform_device_unregister(pdata->child_devs[i]);
		}
	}

	if (asic->irq_nr != -1) {
		unsigned int i;

		for (i = 0 ; i < ASIC3_NR_IRQS ; i++) {
			int irq = i + asic->irq_base;
			set_irq_flags(irq, 0);
			set_irq_handler (irq, NULL);
			set_irq_chip (irq, NULL);
			set_irq_chip_data(irq, NULL);
		}

		set_irq_chained_handler(asic->irq_nr, NULL);
	}

	if (asic->mmc_dev)
		asic3_unregister_mmc(&pdev->dev);

	for (i = 0; i < ARRAY_SIZE(asic3_clocks); i++)
		clk_unregister(&asic3_clocks[i]);
	clk_unregister(&clk_g);

	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, SEL), 0);
	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(INTR, IntMask), 0);

	iounmap(asic->mapping);

	kfree(asic);

	return 0;
}

static void asic3_shutdown(struct platform_device *pdev)
{
}

#define ASIC3_SUSPEND_CDEX_MASK \
	(CLOCK_CDEX_LED0 | CLOCK_CDEX_LED1 | CLOCK_CDEX_LED2)
static unsigned short suspend_cdex;

static int asic3_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct asic3_data *asic = platform_get_drvdata(pdev);
	suspend_cdex = __asic3_read_register(asic,
		_IPAQ_ASIC3_CLOCK_Base + _IPAQ_ASIC3_CLOCK_CDEX);
	/* The LEDs are still active during suspend */
	__asic3_write_register(asic,
		_IPAQ_ASIC3_CLOCK_Base + _IPAQ_ASIC3_CLOCK_CDEX,
		suspend_cdex & ASIC3_SUSPEND_CDEX_MASK);
	return 0;
}

static int asic3_resume(struct platform_device *pdev)
{
	struct asic3_data *asic = platform_get_drvdata(pdev);
	unsigned short intmask;

	__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(CLOCK, CDEX),
			       suspend_cdex);

	if (asic->irq_nr != -1) {
		/* Toggle the interrupt mask to try to get ASIC3 to show
		 * the CPU an interrupt edge. For more details see the
		 * kernel-discuss thread around 13 June 2005 with the
		 * subject "asic3 suspend / resume". */
		intmask = __asic3_read_register(asic,
				IPAQ_ASIC3_OFFSET(INTR, IntMask));
		__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(INTR, IntMask),
				       intmask & ~ASIC3_INTMASK_GINTMASK);
		mdelay(1);
		__asic3_write_register(asic, IPAQ_ASIC3_OFFSET(INTR, IntMask),
				       intmask | ASIC3_INTMASK_GINTMASK);
	}

	return 0;
}

static struct platform_driver asic3_device_driver = {
	.driver		= {
		.name	= "asic3",
	},
	.probe		= asic3_probe,
	.remove		= asic3_remove,
	.suspend	= asic3_suspend,
	.resume		= asic3_resume,
	.shutdown	= asic3_shutdown,
};

static int __init asic3_base_init(void)
{
	int retval = 0;
	retval = platform_driver_register(&asic3_device_driver);
	return retval;
}

static void __exit asic3_base_exit(void)
{
	platform_driver_unregister(&asic3_device_driver);
}

#ifdef MODULE
module_init(asic3_base_init);
#else	/* start early for dependencies */
subsys_initcall(asic3_base_init);
#endif
module_exit(asic3_base_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Core driver for HTC ASIC3");
MODULE_SUPPORTED_DEVICE("asic3");
