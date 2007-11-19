/*
 * Support for the EGPIO capabilities on the HTC Athena phone.  This
 * is believed to be related to the CPLD chip present on the board.
 *
 * (c) Copyright 2006 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/errno.h> /* ENODEV */
#include <linux/list.h> /* struct list_head */
#include <linux/spinlock.h> /* spinlock_t */
#include <linux/module.h> /* EXPORT_SYMBOL_GPL */

#include <asm/arch/pxa-regs.h> /* PXA_CS2_PHYS */
#include <asm/irq.h> /* IRQ_BOARD_START, IRQT_RISING */
#include <asm/io.h> /* ioremap_nocache */
#include <asm/mach/irq.h> /* struct irq_chip */

#include <asm/arch/htcathena-gpio.h> 
#include <asm/arch/htcathena-asic.h>


/* Base location of CPLD gpio registers. */
static volatile u16 *egpio;


/****************************************************************
 * Input pins
 ****************************************************************/

/* There does not appear to be a way to mask interrupts on the egpio
 chip itself.  It would be possible to do it in software by
 disabling the pxa gpio pin, however since no service currently
 needs enable/disable_irq there is no pressing need to implement
 this.  It isn't necessary to worry about re-entrant irq handlers,
 because the egpio interrupt is checked from the pxa interrupt which
 is itself non re-entrant. */

static void egpio_ack(unsigned int irq)
{
}
static void egpio_mask(unsigned int irq)
{
}
static void egpio_unmask(unsigned int irq)
{
}

static struct irq_chip egpio_muxed_chip = {
        .ack            = egpio_ack,
        .mask           = egpio_mask,
        .unmask         = egpio_unmask,
};

static void
egpio_handler(unsigned int irq, struct irq_desc *desc)
{
	int i;
	/* Read current pins. */
	u16 readval = egpio[0];
	/* Process all set pins. */
#if 0
	for (i=0; i<LAST_EGPIO; i++) {
		int irqpin = i+8;
		if (!(readval & (1<<irqpin)))
			continue;
		/* Ack/unmask current pin. */
		egpio[0] = ~(1<<irqpin);
		/* Handle irq */
		irq = IRQ_EGPIO(i);
		desc = &irq_desc[irq];
		desc_handle_irq(irq, desc);
	}
#else
	printk("CPLD gpio handler called: irq=%d. not handled.\n",irq);
#endif
}

/* Check an input pin to see if it is active. */
int
htcathena_cpld1_gpio_isset(int bit)
{
	u16 readval = egpio[0];
	return readval & (1<<bit);
}
EXPORT_SYMBOL_GPL(htcathena_cpld1_gpio_isset);


/****************************************************************
 * Output pins
 ****************************************************************/

/* Local copies of what is in the egpio registers */
static u16 cached_out_egpio[9];
static spinlock_t outLock;

static inline int u16bank(int bit) {
	return bit/16;
}
static inline int u16bit(int bit) {
	return 1<<(bit & (16-1));
}

void
htcathena_cpld1_gpio_set(int bit)
{
	int pos = u16bank(bit);
	unsigned long flag;

	spin_lock_irqsave(&outLock, flag);
	cached_out_egpio[pos] |= u16bit(bit);
	printk("CPLD gpio set: bank %d = 0x%04x\n", pos, cached_out_egpio[pos]);
	egpio[pos] = cached_out_egpio[pos];
	spin_unlock_irqrestore(&outLock, flag);
}
EXPORT_SYMBOL_GPL(htcathena_cpld1_gpio_set);

void
htcathena_cpld1_gpio_clr(int bit)
{
	int pos = u16bank(bit);
	unsigned long flag;

	spin_lock_irqsave(&outLock, flag);
	cached_out_egpio[pos] &= ~u16bit(bit);
	printk("CPLD gpio clear: bank %d = 0x%04x\n", pos, cached_out_egpio[pos]);
	egpio[pos] = cached_out_egpio[pos];
	spin_unlock_irqrestore(&outLock, flag);
}
EXPORT_SYMBOL_GPL(htcathena_cpld1_gpio_clr);


/****************************************************************
 * Setup
 ****************************************************************/

int
htcathena_cpld1_gpio_init(void)
{
	int irq;

	spin_lock_init(&outLock);

	/* Map egpio chip into virtual address space. */
	egpio = (volatile u16 *)ioremap_nocache(HTCATHENA_CPLD1_BASE
						, sizeof(cached_out_egpio));
	if (!egpio)
		return -ENODEV;

	printk(KERN_NOTICE "Athena CPLD1 phys=%08x virt=%p\n"
	       , HTCATHENA_CPLD1_BASE, egpio);

#if 0
	/* Setup irq handlers. */
	for (irq = IRQ_EGPIO(0); irq <= IRQ_EGPIO(LAST_EGPIO); irq++) {
		set_irq_chip(irq, &egpio_muxed_chip);
		set_irq_handler(irq, handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
#endif

	set_irq_type(HTCATHENA_IRQ(CPLD1_EXT_INT), IRQT_RISING);
        set_irq_chained_handler(HTCATHENA_IRQ(CPLD1_EXT_INT)
				, egpio_handler);

#if 0
	/* Setup initial output pin values. */
	cached_out_egpio[2] = (1<<8);
	egpio[1] = cached_out_egpio[1];
	egpio[2] = cached_out_egpio[2];

	/* Unmask all current irqs. */
	egpio[0] = 0;
#endif

	return 0;
}
