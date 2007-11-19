/*
 * Support for the EGPIO chip present on several HTC phones.  This
 * is believed to be related to the CPLD chip present on the board.
 *
 * (c) Copyright 2007 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/errno.h> /* ENODEV */
#include <linux/interrupt.h> /* enable_irq_wake */
#include <linux/spinlock.h> /* spinlock_t */
#include <linux/module.h>
#include <linux/gpiodev2.h>
#include <linux/mfd/htc-egpio.h>

#include <asm/irq.h> /* IRQT_RISING */
#include <asm/io.h> /* ioremap_nocache */
#include <asm/mach/irq.h> /* struct irq_chip */

struct egpio_info {
	spinlock_t lock;
	u16 *addrBase;
	int bus_shift;

	/* irq info */
	int ackRegister;
	int ackWrite;
	u16 irqsEnabled;
	uint irqStart;
	uint chainedirq;

	/* output info */
	int maxRegs;
	u16 cached_values[MAX_EGPIO_REGS];
};


/****************************************************************
 * Irqs
 ****************************************************************/

static inline void ackirqs(struct egpio_info *ei)
{
	writew(ei->ackWrite, &ei->addrBase[ei->ackRegister << ei->bus_shift]);
	/* printk("EGPIO ack\n"); */
}

/* There does not appear to be a way to proactively mask interrupts
 * on the egpio chip itself.  So, we simply ignore interrupts that
 * aren't desired. */
static void egpio_mask(unsigned int irq)
{
	struct egpio_info *ei = get_irq_chip_data(irq);
	ei->irqsEnabled &= ~(1 << (irq - ei->irqStart));
	/* printk("EGPIO mask %d %04x\n", irq, ei->irqsEnabled); */
}
static void egpio_unmask(unsigned int irq)
{
	struct egpio_info *ei = get_irq_chip_data(irq);
	ei->irqsEnabled |= 1 << (irq - ei->irqStart);
	/* printk("EGPIO unmask %d %04x\n", irq, ei->irqsEnabled); */
}

static struct irq_chip egpio_muxed_chip = {
	.name   = "htc-egpio",
	.mask   = egpio_mask,
	.unmask = egpio_unmask,
};

static void egpio_handler(unsigned int irq, struct irq_desc *desc)
{
	struct egpio_info *ei = get_irq_data(irq);
	int irqpin;

	/* Read current pins. */
	u16 readval = readw(&ei->addrBase[ei->ackRegister << ei->bus_shift]);
	/* Ack/unmask interrupts. */
	ackirqs(ei);
	/* Process all set pins. */
	for (irqpin=0; irqpin<MAX_EGPIO_IRQS; irqpin++) {
		if (!(readval & ei->irqsEnabled & (1<<irqpin)))
			continue;
		/* Run irq handler */
		irq = ei->irqStart + irqpin;
		desc = &irq_desc[irq];
		desc->handle_irq(irq, desc);
	}
}

int htc_egpio_get_wakeup_irq(struct device *dev)
{
	struct egpio_info *ei = dev_get_drvdata(dev);
	int irqpin;

	/* Read current pins. */
	u16 readval = readw(&ei->addrBase[ei->ackRegister << ei->bus_shift]);
	/* Ack/unmask interrupts. */
	ackirqs(ei);
	/* Process all set pins. */
	for (irqpin=0; irqpin<MAX_EGPIO_IRQS; irqpin++) {
		if (!(readval & ei->irqsEnabled & (1<<irqpin)))
			continue;
		/* Return first irq */
		return ei->irqStart + irqpin;
	}
	return 0;
}
EXPORT_SYMBOL(htc_egpio_get_wakeup_irq);


/****************************************************************
 * Input pins
 ****************************************************************/

static inline int u16pos(int bit) {
	return bit/16;
}
static inline int u16bit(int bit) {
	return 1<<(bit & (16-1));
}

/* Check an input pin to see if it is active. */
static int egpio_get(struct device *dev, unsigned gpio)
{
	unsigned bit = gpio & GPIO_BASE_MASK;
	struct egpio_info *ei = dev_get_drvdata(dev);
	u16 readval = readw(&ei->addrBase[u16pos(bit) << ei->bus_shift]);
	return readval & u16bit(bit);
}

static int egpio_to_irq(struct device *dev, unsigned gpio_no)
{
	struct egpio_info *ei = dev_get_drvdata(dev);
	struct htc_egpio_platform_data *pdata = dev->platform_data;
	int i;
	for (i=0; i<pdata->nr_pins; i++) {
		struct htc_egpio_pinInfo *pi = &pdata->pins[i];
		if (pi->type == HTC_EGPIO_TYPE_INPUT
		    && pi->pin_nr == (gpio_no & GPIO_BASE_MASK)
		    && pi->input_irq >= 0)
			return pi->input_irq + ei->irqStart;
		if (pi->type == HTC_EGPIO_TYPE_IRQ
		    && pi->pin_nr == (gpio_no & GPIO_BASE_MASK))
			return pi->pin_nr + ei->irqStart;
	}
	return -ENODEV;
}


/****************************************************************
 * Output pins
 ****************************************************************/

static void egpio_set(struct device *dev, unsigned gpio, int val)
{
	unsigned bit = gpio & GPIO_BASE_MASK;
	struct egpio_info *ei = dev_get_drvdata(dev);
	int pos = u16pos(bit);
	unsigned long flag;

	spin_lock_irqsave(&ei->lock, flag);
	if (val) {
		ei->cached_values[pos] |= u16bit(bit);
		printk("egpio set: reg %d = 0x%04x\n"
		       , pos, ei->cached_values[pos]);
	} else {
		ei->cached_values[pos] &= ~u16bit(bit);
		printk("egpio clear: reg %d = 0x%04x\n"
		       , pos, ei->cached_values[pos]);
	}
	writew(ei->cached_values[pos], &ei->addrBase[pos << ei->bus_shift]);
	spin_unlock_irqrestore(&ei->lock, flag);
}


/****************************************************************
 * Setup
 ****************************************************************/

static void setup_pin(struct egpio_info *ei, struct htc_egpio_pinInfo *pi)
{
	struct htc_egpio_pinInfo dummy;

	if (pi->pin_nr < 0 || pi->pin_nr > 16*MAX_EGPIO_REGS) {
		printk(KERN_ERR "EGPIO invalid pin %d\n", pi->pin_nr);
		return;
	}

	switch (pi->type) {
	case HTC_EGPIO_TYPE_INPUT:
		if (pi->input_irq < 0)
			break;
		dummy.pin_nr = pi->input_irq;
		pi = &dummy;
		/* NO BREAK */
	case HTC_EGPIO_TYPE_IRQ:
		if (ei->ackRegister != u16pos(pi->pin_nr)) {
			printk(KERN_ERR "EGPIO irq conflict %d vs %d\n"
			       , ei->ackRegister, u16pos(pi->pin_nr));
			return;
		}
		break;
	case HTC_EGPIO_TYPE_OUTPUT: {
		int pin = pi->pin_nr;
		if (ei->maxRegs < u16pos(pin))
			ei->maxRegs = u16pos(pin);
		if (pi->output_initial)
			ei->cached_values[u16pos(pin)] |= u16bit(pin);
		break;
	}
	default:
		printk(KERN_ERR "EGPIO unknown type %d\n", pi->type);
	}
}

static int egpio_probe(struct platform_device *pdev)
{
	struct htc_egpio_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct egpio_info *ei;
	int irq, i, ret;

	/* Initialize ei data structure. */
	ei = kzalloc(sizeof(*ei), GFP_KERNEL);
	if (!ei)
		return -ENOMEM;

	spin_lock_init(&ei->lock);

	/* Find chained irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		goto fail;
	ei->chainedirq = res->start;

	/* Map egpio chip into virtual address space. */
	ret = -EINVAL;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto fail;
	ei->addrBase = (u16 *)ioremap_nocache(res->start, res->end - res->start);
	if (!ei->addrBase)
		goto fail;
	printk(KERN_NOTICE "EGPIO phys=%08x virt=%p\n"
	       , res->start, ei->addrBase);
	ei->bus_shift = pdata->bus_shift;

	pdata->ops.get = egpio_get;
	pdata->ops.set = egpio_set;
	pdata->ops.to_irq = egpio_to_irq;
	platform_set_drvdata(pdev, ei);

	{
	struct gpio_ops ops;
	ops.get_value = egpio_get;
	ops.set_value = egpio_set;
	ops.to_irq = egpio_to_irq;
	gpiodev_register(pdata->gpio_base, &pdev->dev, &ops);
	}

	/* Go through list of pins. */
	ei->irqStart = pdata->irq_base;
	ei->maxRegs = pdata->nrRegs - 1;
	ei->ackRegister = pdata->ackRegister;
	for (i = 0; i < pdata->nr_pins; i++)
		setup_pin(ei, &pdata->pins[i]);

	/* Setup irq handlers */
	ei->ackWrite = 0xFFFF;
	if (pdata->invertAcks)
		ei->ackWrite = 0;
	for (irq = ei->irqStart; irq < ei->irqStart+MAX_EGPIO_IRQS; irq++) {
		set_irq_chip(irq, &egpio_muxed_chip);
		set_irq_chip_data(irq, ei);
		set_irq_handler(irq, handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	set_irq_type(ei->chainedirq, IRQT_RISING);
	set_irq_data(ei->chainedirq, ei);
	set_irq_chained_handler(ei->chainedirq, egpio_handler);
	ackirqs(ei);

	/* Setup initial output pin values. */
	for (i = 0; i<=ei->maxRegs; i++)
		if (i != ei->ackRegister)
			writew(ei->cached_values[i], &ei->addrBase[i << ei->bus_shift]);

	device_init_wakeup(&pdev->dev, 1);

	return 0;
fail:
	printk(KERN_NOTICE "EGPIO failed to setup\n");
	kfree(ei);
	return ret;
}

static int egpio_remove(struct platform_device *pdev)
{
	struct egpio_info *ei = platform_get_drvdata(pdev);
	unsigned int irq;
	for (irq = ei->irqStart; irq < ei->irqStart+MAX_EGPIO_IRQS; irq++) {
		set_irq_chip(irq, NULL);
		set_irq_handler(irq, NULL);
		set_irq_flags(irq, 0);
	}
	set_irq_chained_handler(ei->chainedirq, NULL);

	iounmap(ei->addrBase);
	kfree(ei);

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

#ifdef CONFIG_PM
static int egpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct egpio_info *ei = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev))
		enable_irq_wake(ei->chainedirq);
	return 0;
}

static int egpio_resume(struct platform_device *pdev)
{
	struct egpio_info *ei = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev))
		disable_irq_wake(ei->chainedirq);
	return 0;
}
#else
#define egpio_suspend NULL
#define egpio_resume NULL
#endif


static struct platform_driver egpio_driver = {
	.driver = {
		.name           = "htc-egpio",
	},
	.probe          = egpio_probe,
	.remove         = egpio_remove,
	.suspend	= egpio_suspend,
	.resume		= egpio_resume,
};

static int __init egpio_init(void)
{
	return platform_driver_register(&egpio_driver);
}

static void __exit egpio_exit(void)
{
	platform_driver_unregister(&egpio_driver);
}

#ifdef MODULE
module_init(egpio_init);
#else   /* start early for dependencies */
subsys_initcall(egpio_init);
#endif
module_exit(egpio_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin O'Connor <kevin@koconnor.net>");
