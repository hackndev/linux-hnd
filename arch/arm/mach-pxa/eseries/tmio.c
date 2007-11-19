/*
 *  arch/arm/mach-pxa/eseries/tmio.c
 *
 *  Support for the TMIO ASIC in the Toshiba e800
 *
 *  Author:	Ian Molton and Sebastian Carlier
 *  Created:	Sat 7 Aug 2004
 *  Copyright:	Ian Molton and Sebastian Carlier
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/fb.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-irq.h>
#include <asm/arch/eseries-gpio.h>

#define TMIO_B(n) (*(unsigned char *)(tmio_io_base + (n)))

static struct resource * tmio_io_res;
static void* tmio_io_base;
static int tmio_host_irq;

static void tmio_mask_irq(unsigned int irq)
{
        int tmio_irq = irq - TMIO_IRQ_BASE;
//        printk("tmio: mask %u\n", irq);
        switch (tmio_irq) {
        case 0: TMIO_B(0x52) |= 0x01; break;
        case 1: TMIO_B(0x52) |= 0x02; break;
        case 2: TMIO_B(0x52) |= 0x04; break;
        case 3: TMIO_B(0x52) |= 0x10; break;
        }
}

static void tmio_unmask_irq(unsigned int irq)
{
        int tmio_irq = irq - TMIO_IRQ_BASE;
//        printk("tmio: unmask %u\n", irq);
        switch (tmio_irq) {
        case 0: TMIO_B(0x52) &= ~0x01; break;
        case 1: TMIO_B(0x52) &= ~0x02; break;
        case 2: TMIO_B(0x52) &= ~0x04; break;
        case 3: TMIO_B(0x52) &= ~0x10; break;
        }
}

static struct irq_chip tmio_irq_chip = {
        .ack            = tmio_mask_irq,
        .mask           = tmio_mask_irq,
        .unmask         = tmio_unmask_irq,
};


static void tmio_irq_handler(unsigned int irq, struct irq_desc *desc, struct pt_regs *regs)
{
        unsigned long mask, pending;
	int count = 0;

        mask = TMIO_B(0x52);
        pending = TMIO_B(0x50) & ~mask;
//	printk("TMIO: irq! %02x %02x\n", mask, pending);
        TMIO_B(0x54) = 0xff;  // Mask TMIO IRQs  (note! winCE doesnt use this method)
	GEDR(tmio_host_irq) = GPIO_bit(tmio_host_irq);	/* XXX; clear our parent irq */
	while (pending) {
		irq = TMIO_IRQ_BASE + __ffs(pending);
//		printk("IRQ: %d %d\n", count++, irq);
		desc = irq_desc + irq;
		desc->handle(irq, desc, regs);
		pending &= ~(1<<__ffs(pending));
	}
        TMIO_B(0x54) = 0;
}

int tmio_init_irq(int h_irq, unsigned int phys_base)
{
	static int inited; //FIXME hack!
        int ret, irq;

	if(inited)
		return;
	inited = 1;
        tmio_io_res = kmalloc(sizeof(*tmio_io_res), GFP_KERNEL);
        if (!tmio_io_res) {
                ret = -ENOMEM;
                goto fail;
        }

        tmio_io_res->name = "TMIO";
        tmio_io_res->start = phys_base;
        tmio_io_res->end = phys_base + 0xff;
        tmio_io_res->flags = IORESOURCE_IO;
        ret = request_resource(&iomem_resource, tmio_io_res);
        if (ret)
                goto fail_1;

        tmio_io_base = ioremap(phys_base, 0x100);
        if (!tmio_io_base) {
                ret = -EBUSY;
		goto fail_2;
        }
	tmio_host_irq = h_irq;

        // Now initialize the hardware
        set_irq_type(tmio_host_irq, IRQT_NOEDGE);
        //   Clear pending requests
        TMIO_B(0x54) = 0xff;  //FIXME - this doesnt clear, it masks.

	/* setup extra lubbock irqs */
	for (irq = TMIO_IRQ_BASE; irq < TMIO_IRQ_BASE+4; irq++) {
		set_irq_chip(irq, &tmio_irq_chip);
		set_irq_handler(irq, do_level_IRQ);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE); // XXX probe?
	}

	set_irq_chained_handler(tmio_host_irq, tmio_irq_handler);

        printk("tmio: initialised.\n");

        //  Re-enable interrupt
        set_irq_type(tmio_host_irq, IRQT_FALLING);
        TMIO_B(0x54) = 0;

        return 0;
 fail_2:
        release_resource(tmio_io_res);
 fail_1:
        kfree(tmio_io_res);
 fail:
        return ret;
}

EXPORT_SYMBOL(tmio_init_irq);


