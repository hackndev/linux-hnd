/*
 * Driver interface to HTC "ASIC2"
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2003, 2004, 2005 Phil Blundell
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
#include <linux/device.h>
#include <linux/soc-old.h>
#include <linux/platform_device.h>

#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/hardware/ipaq-asic2.h>
#include <linux/mfd/asic2_base.h>
#include "soc-core.h"

#define ASIC2_NR_IRQS	16

static int asic2_remove (struct platform_device *pdev);

struct asic2_data
{
	int irq_base;
	int irq_nr;
	void *mapping;
	struct platform_device *devices;
	int ndevices;

	spinlock_t	gpio_lock;
	u32		gpio_state;	// preserving gpio across suspend
	u32		kpio_int_shadow;

	spinlock_t      clock_lock;
	int             clock_ex1;    /* 24.765 MHz crystal */
	int             clock_ex2;    /* 33.8688 MHz crystal */
};

static inline unsigned long asic2_address(struct device *dev, unsigned int reg)
{
	struct asic2_data *asic = dev->driver_data;

	return (unsigned long)asic->mapping + reg;
}

void asic2_write_register(struct device *dev, unsigned int reg, unsigned long value)
{
	__raw_writel (value, asic2_address (dev, reg));
}
EXPORT_SYMBOL(asic2_write_register);

unsigned long asic2_read_register(struct device *dev, unsigned int reg)
{
	return __raw_readl (asic2_address (dev, reg));
}
EXPORT_SYMBOL(asic2_read_register);

static inline void __asic2_write_register(struct asic2_data *asic, unsigned int reg, unsigned long value)
{
	__raw_writel (value, (unsigned long)asic->mapping + reg);
}

static inline unsigned long __asic2_read_register(struct asic2_data *asic, unsigned int reg)
{
	return __raw_readl ((unsigned long)asic->mapping + reg);
}

/***********************************************************************************
 *      Shared resources
 *
 *      EX1 on Clock      24.576 MHz crystal (OWM,ADC,PCM,SPI,PWM,UART,SD,Audio)
 *      EX2               33.8688 MHz crystal (SD Controller and Audio)
 ***********************************************************************************/

void __asic2_shared_add(struct asic2_data *asic, unsigned long *s, enum ASIC_SHARED v)
{
	unsigned long flags, val;

	if ( (*s & v) == v )    /* Already set */
		return;

	spin_lock_irqsave (&asic->clock_lock, flags);

	if ( v & ASIC_SHARED_CLOCK_EX1 && !(*s & ASIC_SHARED_CLOCK_EX1)) {
		if ( !asic->clock_ex1++ ) {
			val = __asic2_read_register (asic, IPAQ_ASIC2_CLOCK_Enable);
			val |= ASIC2_CLOCK_EX1;
			__asic2_write_register (asic, IPAQ_ASIC2_CLOCK_Enable, val);
		}
	}

	if ( v & ASIC_SHARED_CLOCK_EX2 && !(*s & ASIC_SHARED_CLOCK_EX2)) {
		if ( !asic->clock_ex2++ ) {
			val = __asic2_read_register (asic, IPAQ_ASIC2_CLOCK_Enable);
			val |= ASIC2_CLOCK_EX2;
			__asic2_write_register (asic, IPAQ_ASIC2_CLOCK_Enable, val);
		}
	}

	*s |= v;
	spin_unlock_irqrestore (&asic->clock_lock, flags);
}
EXPORT_SYMBOL(__asic2_shared_add);

void __asic2_shared_release(struct asic2_data *asic, unsigned long *s, enum ASIC_SHARED v)
{
	unsigned long flags, val;

	spin_lock_irqsave (&asic->clock_lock, flags);

	if ( v & ASIC_SHARED_CLOCK_EX1 && !(~*s & ASIC_SHARED_CLOCK_EX1)) {
		if ( !--asic->clock_ex1 ) {
			val = __asic2_read_register (asic, IPAQ_ASIC2_CLOCK_Enable);
			val &= ~ASIC2_CLOCK_EX1;
			__asic2_write_register (asic, IPAQ_ASIC2_CLOCK_Enable, val);
		}
	}

	if ( v & ASIC_SHARED_CLOCK_EX2 && !(~*s & ASIC_SHARED_CLOCK_EX2)) {
		if ( !--asic->clock_ex2 ) {
			val = __asic2_read_register (asic, IPAQ_ASIC2_CLOCK_Enable);
			val &= ~ASIC2_CLOCK_EX2;
			__asic2_write_register (asic, IPAQ_ASIC2_CLOCK_Enable, val);
		}
	}

	*s &= ~v;
	spin_unlock_irqrestore (&asic->clock_lock, flags);
}
EXPORT_SYMBOL(__asic2_shared_release);

void asic2_shared_add(struct device *dev, unsigned long *s, enum ASIC_SHARED v)
{
	struct asic2_data *asic = dev->driver_data;

	__asic2_shared_add (asic, s, v);
}
EXPORT_SYMBOL(asic2_shared_add);

void asic2_shared_release(struct device *dev, unsigned long *s, enum ASIC_SHARED v)
{
	struct asic2_data *asic = dev->driver_data;

	__asic2_shared_release (asic, s, v);
}
EXPORT_SYMBOL(asic2_shared_release);

void asic2_set_gpiod(struct device *dev, unsigned long mask, unsigned long bits)
{
	struct asic2_data *asic = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave (&asic->gpio_lock, flags);
	bits |= (__asic2_read_register (asic, IPAQ_ASIC2_GPIOPIOD) & ~mask);
	__asic2_write_register (asic, IPAQ_ASIC2_GPIOPIOD, bits);
	spin_unlock_irqrestore (&asic->gpio_lock, flags);
}
EXPORT_SYMBOL(asic2_set_gpiod);

unsigned long asic2_read_gpiod(struct device *dev)
{
	struct asic2_data *asic = dev->driver_data;

	return __asic2_read_register (asic, IPAQ_ASIC2_GPIOPIOD);
}
EXPORT_SYMBOL(asic2_read_gpiod);

void asic2_set_gpint(struct device *dev, unsigned long bit, unsigned int type, unsigned int sense)
{
	struct asic2_data *asic = dev->driver_data;
	unsigned long flags, val;

	spin_lock_irqsave (&asic->gpio_lock, flags);
	val = __asic2_read_register (asic, IPAQ_ASIC2_GPIINTTYPE);
	if (type)
		val |=  bit;  // Edge interrupt
	else
		val &=  ~bit; // Level interrupt?
	__asic2_write_register (asic, IPAQ_ASIC2_GPIINTTYPE, val);

	val = __asic2_read_register (asic, IPAQ_ASIC2_GPIINTESEL);
	if (sense)
		val |=  bit;  // Rising edge?
	else
		val &= ~bit;  // Falling edge
	__asic2_write_register (asic, IPAQ_ASIC2_GPIINTESEL, val);

	spin_unlock_irqrestore (&asic->gpio_lock, flags);
}
EXPORT_SYMBOL(asic2_set_gpint);

void __asic2_clock_enable(struct asic2_data *asic, unsigned long bit, int on)
{
	unsigned long flags, val;

	spin_lock_irqsave (&asic->clock_lock, flags);

	val = __asic2_read_register (asic, IPAQ_ASIC2_CLOCK_Enable);

	if (on)
		val |= bit;
	else
		val &= ~bit;

	__asic2_write_register (asic, IPAQ_ASIC2_CLOCK_Enable, val);

	spin_unlock_irqrestore (&asic->clock_lock, flags);
}

void asic2_clock_enable(struct device *dev, unsigned long bit, int on)
{
	struct asic2_data *asic = dev->driver_data;
	__asic2_clock_enable (asic, bit, on);
}
EXPORT_SYMBOL(asic2_clock_enable);

unsigned int asic2_irq_base(struct device *dev)
{
	struct asic2_data *asic = dev->driver_data;

	return asic->irq_base;
}
EXPORT_SYMBOL(asic2_irq_base);

void asic2_set_pwm(struct device *dev, unsigned long pwm,
	       unsigned long dutytime, unsigned long periodtime,
	       unsigned long timebase)
{
	struct asic2_data *asic = dev->driver_data;
	void *base = asic->mapping + pwm;

	__raw_writel(dutytime, base + _IPAQ_ASIC2_PWM_DutyTime);
	__raw_writel(periodtime, base + _IPAQ_ASIC2_PWM_PeriodTime);
	__raw_writel(timebase, base + _IPAQ_ASIC2_PWM_TimeBase);
}
EXPORT_SYMBOL(asic2_set_pwm);

/*************************************************************/

#define MAX_ASIC_ISR_LOOPS    20

/* The order of these is important - see #include <asm/arch/irqs.h> */
static u32 kpio_irq_mask[] = {
	KPIO_KEY_ALL,
	KPIO_SPI_INT,
	KPIO_OWM_INT,
	KPIO_ADC_INT,
	KPIO_UART_0_INT,
	KPIO_UART_1_INT,
	KPIO_TIMER_0_INT,
	KPIO_TIMER_1_INT,
	KPIO_TIMER_2_INT
};

static u32 gpio_irq_mask[] = {
	GPIO2_PEN_IRQ,
	GPIO2_SD_DETECT,
	GPIO2_EAR_IN_N,
	GPIO2_USB_DETECT_N,
	GPIO2_SD_CON_SLT,
};

static void asic2_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	int i;
	struct asic2_data *asic;

	asic = desc->handler_data;

	if (0) printk("%s: interrupt received\n", __FUNCTION__);

	for ( i = 0 ; i < MAX_ASIC_ISR_LOOPS; i++ ) {
		u32 girq, kirq;
		int j;

		/* KPIO */
		kirq = __asic2_read_register (asic, IPAQ_ASIC2_KPIINTFLAG);
		if (0) printk("%s: KPIO 0x%08X\n", __FUNCTION__, kirq );
		for ( j = 0 ; j < IPAQ_ASIC2_KPIO_IRQ_COUNT ; j++ )
			if (kirq & kpio_irq_mask[j]) {
				int kpio_irq = j + asic->irq_base + IPAQ_ASIC2_KPIO_IRQ_START;
				desc = irq_desc + kpio_irq;
				desc->handle_irq(kpio_irq, desc);
			}

		/* GPIO2 */
		girq = __asic2_read_register (asic, IPAQ_ASIC2_GPIINTFLAG);
		if (0) printk("%s: GPIO 0x%08X\n", __FUNCTION__, girq );
		for ( j = 0 ; j < IPAQ_ASIC2_GPIO_IRQ_COUNT ; j++ )
			if (girq & gpio_irq_mask[j]) {
				int gpio_irq = j + asic->irq_base + IPAQ_ASIC2_GPIO_IRQ_START;
				desc = irq_desc + gpio_irq;
				desc->handle_irq(gpio_irq, desc);
			}

		if ((girq | kirq) == 0)
			break;
	}

	if ( i >= MAX_ASIC_ISR_LOOPS )
		printk("%s: interrupt processing overrun\n", __FUNCTION__);
}

/* mask_ack <- IRQ is first serviced.
       mask <- IRQ is disabled.
     unmask <- IRQ is enabled

     The INTCLR registers are poorly documented.  I believe that writing
     a "1" to the register clears the specific interrupt, but the documentation
     indicates writing a "0" clears the interrupt.  In any case, they shouldn't
     be read (that's the INTFLAG register)
*/

static void asic2_mask_ack_kpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 mask;

	mask = kpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_KPIO_IRQ_START)];
	asic->kpio_int_shadow &= ~mask;
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTSTAT, asic->kpio_int_shadow);
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTCLR, mask);
}

static void asic2_mask_kpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 mask;

	mask = kpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_KPIO_IRQ_START)];
	asic->kpio_int_shadow &= ~mask;
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTSTAT, asic->kpio_int_shadow);
}

static void asic2_unmask_kpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 mask;

	mask = kpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_KPIO_IRQ_START)];
	asic->kpio_int_shadow |= mask;
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTSTAT, asic->kpio_int_shadow);
}

static void asic2_mask_ack_gpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 mask, val;

	mask = gpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_GPIO_IRQ_START)];
	val = __asic2_read_register(asic, IPAQ_ASIC2_GPIINTSTAT);
	val &= ~mask;
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTSTAT, val);
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTCLR, mask);
}

static void asic2_mask_gpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 val;

	val = __asic2_read_register(asic, IPAQ_ASIC2_GPIINTSTAT);
	val &= ~gpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_GPIO_IRQ_START)];
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTSTAT, val);
}

static void asic2_unmask_gpio_irq(unsigned int irq)
{
	struct asic2_data *asic = get_irq_chip_data(irq);
	u32 val;

	val = __asic2_read_register(asic, IPAQ_ASIC2_GPIINTSTAT);
	val |= gpio_irq_mask[irq - (asic->irq_base + IPAQ_ASIC2_GPIO_IRQ_START)];
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTSTAT, val);
}

static struct irq_chip asic2_kpio_irq_chip = {
	.name		= "ASIC2-KPIO",
	.ack		= asic2_mask_ack_kpio_irq,
	.mask		= asic2_mask_kpio_irq,
	.unmask		= asic2_unmask_kpio_irq,
};

static struct irq_chip asic2_gpio_irq_chip = {
	.name		= "ASIC2-GPIO",
	.ack		= asic2_mask_ack_gpio_irq,
	.mask		= asic2_mask_gpio_irq,
	.unmask		= asic2_unmask_gpio_irq,
};

static void asic2_irq_init(struct asic2_data *asic)
{
	int i;

	asic->kpio_int_shadow = 0;

	/* Disable all IRQs and set up clock */
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTSTAT, 0);		/* Disable all interrupts */
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTSTAT, 0);

	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTCLR, 0);			/* Clear all KPIO interrupts */
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTCLR, 0);			/* Clear all GPIO interrupts */

	for ( i = 0 ; i < IPAQ_ASIC2_KPIO_IRQ_COUNT ; i++ ) {
		int irq = i + asic->irq_base + IPAQ_ASIC2_KPIO_IRQ_START;
		set_irq_chip(irq, &asic2_kpio_irq_chip);
		set_irq_chip_data(irq, asic);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	for ( i = 0 ; i < IPAQ_ASIC2_GPIO_IRQ_COUNT ; i++ ) {
		int irq = i + asic->irq_base + IPAQ_ASIC2_GPIO_IRQ_START;
		set_irq_chip(irq, &asic2_gpio_irq_chip);
		set_irq_chip_data(irq, asic);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	/* Don't start up the ADC IRQ automatically */
	set_irq_flags(asic->irq_base + IRQ_IPAQ_ASIC2_ADC, IRQF_VALID | IRQF_NOAUTOEN);
}

static int
asic2_gclk (void *data)
{
	return 4096000;		// really OWM clock
}

/*************************************************************/

static struct resource asic2_gpio_resources[] = {
	{
		.start = 0,
		.end   = 0x3f,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource asic2_keys_resources[] = {
	{
		.start = 0x200,
		.end   = 0x23f,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_IPAQ_ASIC2_KEY,
		.end   = IRQ_IPAQ_ASIC2_KEY,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct resource asic2_spi_resources[] = {
	{
		.start = 0x400,
		.end   = 0x40f,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_IPAQ_ASIC2_SPI,
		.end   = IRQ_IPAQ_ASIC2_SPI,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct resource asic2_adc_resources[] = {
	{
		.start = 0x1200,
		.end   = 0x120f,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_IPAQ_ASIC2_ADC,
		.end   = IRQ_IPAQ_ASIC2_ADC,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct resource asic2_owm_resources[] = {
	{
		.start = 0x1800,
		.end   = 0x180f,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_IPAQ_ASIC2_OWM,
		.end   = IRQ_IPAQ_ASIC2_OWM,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SOC_SUBDEVICE,
	},
};

static struct soc_device_data asic2_blocks[] = {
	{
		.name = "asic2-gpio",
		.res = asic2_gpio_resources,
		.num_resources = ARRAY_SIZE(asic2_gpio_resources),
	},
	{
		.name = "asic2-keys",
		.res = asic2_keys_resources,
		.num_resources = ARRAY_SIZE(asic2_keys_resources),
	},
	{
		.name = "asic2-spi",
		.res = asic2_spi_resources,
		.num_resources = ARRAY_SIZE(asic2_spi_resources),
	},
	{
		.name = "asic2-adc",
		.res = asic2_adc_resources,
		.num_resources = ARRAY_SIZE(asic2_adc_resources),
	},
	{
		.name = "asic2-owm",
		.res = asic2_owm_resources,
		.num_resources = ARRAY_SIZE(asic2_owm_resources),
	},
};

static void asic2_release(struct device *dev)
{
	struct platform_device *sdev = to_platform_device(dev);
	kfree(sdev->resource);
	kfree(sdev);
}

static int asic2_probe(struct platform_device *pdev)
{
	int rc;
	struct asic2_platform_data *pdata = pdev->dev.platform_data;
	struct asic2_data *asic;

	asic = kmalloc(sizeof (struct asic2_data), GFP_KERNEL);
	if (!asic)
		goto enomem3;

	memset(asic, 0, sizeof (*asic));
	pdev->dev.driver_data = asic;

	asic->irq_base = pdata->irq_base;
	if (!asic->irq_base) {
		printk ("asic2: uninitialized irq_base!\n");
		goto enomem1;
	}

	asic->mapping = ioremap((unsigned long)pdev->resource[0].start, IPAQ_ASIC2_MAP_SIZE);
	if (!asic->mapping) {
		printk ("asic2: couldn't ioremap\n");
		goto enomem1;
	}
	spin_lock_init(&asic->gpio_lock);
	spin_lock_init(&asic->clock_lock);
	asic->clock_ex1 = asic->clock_ex2 = 0;
	asic2_irq_init(asic);

	asic->irq_nr = pdev->resource[1].start;

	printk ("%s: using irq %d-%d on irq %d\n", pdev->name, asic->irq_base,
		asic->irq_base + ASIC2_NR_IRQS - 1, asic->irq_nr);

	set_irq_chained_handler(asic->irq_nr, asic2_irq_demux);
	set_irq_type(asic->irq_nr, IRQT_RISING);
	set_irq_data(asic->irq_nr, asic);

	__asic2_write_register(asic, IPAQ_ASIC2_CLOCK_Enable, ASIC2_CLOCK_EX0);	/* 32 kHz crystal on */
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_ClockPrescale, ASIC2_INTCPS_SET);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_ClockPrescale,
			       ASIC2_INTCPS_CPS(0x0e) | ASIC2_INTCPS_SET);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_TimerSet, 1);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_MaskAndFlag, ASIC2_INTMASK_GLOBAL
				| ASIC2_INTMASK_UART_0 | ASIC2_INTMASK_UART_1 | ASIC2_INTMASK_TIMER);

	/* Set up GPIO */
	__asic2_write_register(asic, IPAQ_ASIC2_GPIOPIOD, GPIO2_IN_Y1_N | GPIO2_IN_X1_N);
	__asic2_write_register(asic, IPAQ_ASIC2_GPOBFSTAT, GPIO2_IN_Y1_N | GPIO2_IN_X1_N);
	__asic2_write_register(asic, IPAQ_ASIC2_GPIODIR, GPIO2_PEN_IRQ | GPIO2_SD_DETECT
				| GPIO2_EAR_IN_N | GPIO2_USB_DETECT_N | GPIO2_SD_CON_SLT);

	asic->ndevices = ARRAY_SIZE(asic2_blocks);
	asic->devices = soc_add_devices(pdev, asic2_blocks,
					ARRAY_SIZE(asic2_blocks),
					&pdev->resource[0], 0, asic->irq_base);
	if (!asic->devices)
		goto enomem;

	if (pdata && pdata->num_child_devs != 0) {
		int i;
		for (i = 0; i < pdata->num_child_devs; i++) {
			pdata->child_devs[i]->dev.parent = &pdev->dev;
			platform_device_register(pdata->child_devs[i]);
		}
	}

	return 0;

 enomem:
	rc = -ENOMEM;
 error:
	asic2_remove(pdev);
	return rc;
 enomem1:
	kfree(asic);
 enomem3:
	return -ENOMEM;
}

static int asic2_remove(struct platform_device *pdev)
{
	int i;
	struct asic2_platform_data *pdata = pdev->dev.platform_data;
	struct asic2_data *asic;

	if (pdata && pdata->num_child_devs != 0) {
		for (i = 0; i < pdata->num_child_devs; i++) {
			platform_device_unregister(pdata->child_devs[i]);
		}
	}

	asic = platform_get_drvdata(pdev);

	__asic2_write_register(asic, IPAQ_ASIC2_CLOCK_Enable, 0);

	for (i = 0 ; i < ASIC2_NR_IRQS ; i++) {
		int irq = i + asic->irq_base;
		set_irq_handler(irq, NULL);
		set_irq_chip(irq, NULL);
		set_irq_chip_data(irq, NULL);
		set_irq_flags(irq, 0);
	}

	set_irq_chained_handler(asic->irq_nr, NULL);

	if (asic->devices) {
		soc_free_devices(asic->devices, ARRAY_SIZE(asic2_blocks));
	}

	iounmap(asic->mapping);

	kfree(asic);

	return 0;
}

static void asic2_shutdown(struct platform_device *pdev)
{
}

static int asic2_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct asic2_data *asic = platform_get_drvdata(pdev);

	asic->gpio_state = __asic2_read_register(asic, IPAQ_ASIC2_GPIOPIOD);

	return 0;
}

static int asic2_resume(struct platform_device *pdev)
{
	struct asic2_data *asic = platform_get_drvdata(pdev);

	/* TODO: Check and refactor this code, see probe */
	/* Ported from 2.4 arm/mach-pxa/h3900_asic_core.c */

	/* These probably aren't necessary */
	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTSTAT, 0);		/* Disable all interrupts */
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTSTAT, 0);

	__asic2_write_register(asic, IPAQ_ASIC2_KPIINTCLR, 0xffff);			/* Clear all KPIO interrupts */
	__asic2_write_register(asic, IPAQ_ASIC2_GPIINTCLR, 0xffff);			/* Clear all GPIO interrupts */

	__asic2_write_register(asic, IPAQ_ASIC2_CLOCK_Enable, ASIC2_CLOCK_EX0);	/* 32 kHz crystal on */
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_ClockPrescale, ASIC2_INTCPS_SET);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_ClockPrescale,
			       ASIC2_INTCPS_CPS(0x0e) | ASIC2_INTCPS_SET);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_TimerSet, 1);
	__asic2_write_register(asic, IPAQ_ASIC2_INTR_MaskAndFlag, ASIC2_INTMASK_GLOBAL
				| ASIC2_INTMASK_UART_0 | ASIC2_INTMASK_UART_1 | ASIC2_INTMASK_TIMER);


	__asic2_write_register(asic, IPAQ_ASIC2_GPIOPIOD, asic->gpio_state);
	__asic2_write_register(asic, IPAQ_ASIC2_GPIODIR, GPIO2_PEN_IRQ
				| GPIO2_SD_DETECT
				| GPIO2_EAR_IN_N
				| GPIO2_USB_DETECT_N
				| GPIO2_SD_CON_SLT);

	return 0;
}

static struct platform_driver asic2_device_driver = {
	.driver	= {
	    .name	= "asic2",
	},
	.probe		= asic2_probe,
	.remove		= asic2_remove,
	.suspend	= asic2_suspend,
	.resume		= asic2_resume,
	.shutdown	= asic2_shutdown,
};

static int __init asic2_base_init(void)
{
	return platform_driver_register(&asic2_device_driver);
}

static void __exit asic2_base_exit(void)
{
	platform_driver_unregister(&asic2_device_driver);
}

#ifdef MODULE
module_init(asic2_base_init);
#else	/* start early for dependencies */
subsys_initcall(asic2_base_init);
#endif
module_exit(asic2_base_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org> and others");
MODULE_DESCRIPTION("Core driver for iPAQ ASIC2");
MODULE_SUPPORTED_DEVICE("asic2");
