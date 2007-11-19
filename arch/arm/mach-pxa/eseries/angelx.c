/*
 *  arch/arm/mach-pxa/eseries/angelx.c
 *
 *  Support for the angelX ASIC in the Toshiba e800
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

/*
common:
  0x004:
     1: interrupt enable
     9: must be set (?)
  0x010:
    12: slot 0 service request
    13: slot 1 service request
  0x018:
    write 0x3030 to ack interrupt

pcmcia slot 0 (add 0x100 for slot 1): 
  0x30c: Socket status  (possibly not accessed by CE tho)
     0:  seems to correlate with event1 IRQs.
     2:  goes low on insert (later than 3: though) (used as CD)
     3:  goes low on insert
     4:  acts like RDY - goes low if card present AND powered
  0x314:  event source
     0: EVENT0 service request
     1: EVENT1 service request
     7: EVENT7 service request
  0x334:  write to ack event
     0: write with bit set to ack EVENT0
     1: write with bit set to ack EVENT1
     7: write with bit set to ack EVENT7
  0x31C: event enable 
     0: EVENT0 enabled if set
     1: EVENT1 enabled if set
     7: EVENT7 enabled if set
  0x32C: event enable 2 
     0: enable EVENT0      1: enable EVENT1 and EVENT7

 EVENT0 is unknown EVENT1 is unknown
 EVENT7 is card insert/eject
*/

#define ANGELX_B(n) (*(unsigned char *)(angelx_base + (n)))
#define ANGELX_H(n) (*(unsigned short *)(angelx_base + (n)))

#define PCMCIA_SLOT_0 (1<<12)
#define PCMCIA_SLOT_1 (1<<13)
#define BIT_0 1
#define BIT_1 2
#define BIT_7 0x80

//FIXME - this ought to be dynamic like the rest...
static struct resource angelx_res = {
        .name = "AngelX",
        .start  = PXA_CS5_PHYS,
        .end    = PXA_CS5_PHYS + 0xfff,
        .flags  = IORESOURCE_IO,
};

static void* angelx_base;
static int angelx_host_irq;

unsigned char angelx_get_state(int skt) {
	return ANGELX_B(0x30c+(skt == 0 ? 0 : 0x100));
}
EXPORT_SYMBOL(angelx_get_state);

static void angelx_ack_irq(unsigned int irq)
{
	int angelx_irq = (irq - ANGELX_IRQ_BASE);
//        printk("angelx: ack %d\n", angelx_irq);
        switch(angelx_irq){
                case 0: ANGELX_H(0x334) = BIT_0 ; break;
                case 1: ANGELX_H(0x334) = BIT_1 ; break;
                case 2: ANGELX_H(0x334) = BIT_7 ; break;

                case 3: ANGELX_H(0x434) = BIT_0 ; break;
                case 4: ANGELX_H(0x434) = BIT_1 ; break;
                case 5: ANGELX_H(0x434) = BIT_7 ; break;
        }
}

static void angelx_mask_irq(unsigned int irq)
{
	int angelx_irq = (irq - ANGELX_IRQ_BASE);
//	printk("angelx: mask %d\n", angelx_irq);

	switch(angelx_irq){
		case 0: ANGELX_H(0x32c) &= ~BIT_0 ;
                        ANGELX_H(0x31c) &= ~BIT_0 ; break;
                case 1: ANGELX_H(0x32c) &= ~BIT_1 ;
                        ANGELX_H(0x31c) &= ~BIT_1 ; break;
                case 2: ANGELX_H(0x32c) &= ~BIT_1 ;
                        ANGELX_H(0x31c) &= ~BIT_7 ; break;

                case 3: ANGELX_H(0x42c) &= ~BIT_0 ;
                        ANGELX_H(0x41c) &= ~BIT_0 ; break;
                case 4: ANGELX_H(0x42c) &= ~BIT_1 ;
                        ANGELX_H(0x41c) &= ~BIT_1 ; break;
                case 5: ANGELX_H(0x42c) &= ~BIT_1 ;
                        ANGELX_H(0x41c) &= ~BIT_7 ; break;
	}
}

static void angelx_unmask_irq(unsigned int irq)
{
	int angelx_irq = (irq - ANGELX_IRQ_BASE);
//	printk("angelx: unmask %d\n", angelx_irq);
	
	switch(angelx_irq){
                case 0: ANGELX_H(0x32c) |= BIT_0 ; 
			ANGELX_H(0x31c) |= BIT_0 ; break;
                case 1: ANGELX_H(0x32c) |= BIT_1 ;
			ANGELX_H(0x31c) |= BIT_1 ; break;
                case 2: ANGELX_H(0x32c) |= BIT_1 ;
			ANGELX_H(0x31c) |= BIT_7 ; break;

                case 3: ANGELX_H(0x42c) |= BIT_0 ;
			ANGELX_H(0x41c) |= BIT_0 ; break;
                case 4: ANGELX_H(0x42c) |= BIT_1 ;
			ANGELX_H(0x41c) |= BIT_1 ; break;
                case 5: ANGELX_H(0x42c) |= BIT_1 ;
			ANGELX_H(0x41c) |= BIT_7 ; break;
        }
}

static struct irq_chip angelx_irq_chip = {
	.ack		= angelx_ack_irq,
	.mask		= angelx_mask_irq,
	.unmask		= angelx_unmask_irq,
};

unsigned long angelx_get_pending(void){
	unsigned long pending = 0;
	unsigned short slot;

	slot = ANGELX_H(0x010);

	if (slot & PCMCIA_SLOT_0) {
		unsigned short event = ANGELX_H(0x314);
		if(event & BIT_0)
			pending |= 0x1;
		else if (event & BIT_1)
			pending |= 0x2;
		else if (event & BIT_7)
			pending |= 0x4;
	}
	else if(slot & PCMCIA_SLOT_1) {
		unsigned short event = ANGELX_H(0x414);
		if(event & BIT_0)
			pending |= 0x8;
		else if (event & BIT_1)
			pending |= 0x10;
		else if (event & BIT_7)
			pending |= 0x20;
	}
	else
		printk("unknown IRQ: slot = %04x\n", slot);

	return pending;
}

static void angelx_irq_handler(unsigned int irq, struct irq_desc *desc) {
	unsigned long pending;

	ANGELX_H(0x0004) = 0x0200;  // Disable AngelX IRQ
	pending = angelx_get_pending(); // FIXME - take account of the mask
	while(pending) {
		GEDR(angelx_host_irq) = GPIO_bit(angelx_host_irq);	/* clear our parent irq */
		if (likely(pending)) {
			irq = ANGELX_IRQ_BASE + __ffs(pending);
			if(irq == 113 || irq == 116 || irq == 112 || irq==115)
				printk("IRQ: %d %02x %02x\n", irq, angelx_get_state(0), angelx_get_state(1));
			desc = irq_desc + irq;
			desc->handle_irq(irq, desc);
		}
		pending &= ~(1<<__ffs(pending));
	}
	ANGELX_H(0x018) = 0x3030;
	ANGELX_H(0x0004) = 0x2222;  // Enable AngelX IRQ
}

#if 0
static irqreturn_t e800_asic2_interrupt(int irq, void *dev, struct pt_regs *regs) {
	unsigned short slot;
	ANGELX_H(0x004) = 0x0200;
	slot = ANGELX_H(0x010);
	if (slot & PCMCIA_SLOT_0) {
		unsigned short event = ANGELX_H(0x314);
		if (event & BIT_0) {
			ANGELX_H(0x334) = BIT_0;
			ANGELX_H(0x32C) |= BIT_0;
			ANGELX_H(0x31C) |= BIT_0;
			printk("pcmcia[0]: event 0\n");
		} else if (event & BIT_1) {
			ANGELX_H(0x334) = BIT_1;
			ANGELX_H(0x32C) |= BIT_1;
			ANGELX_H(0x31C) |= BIT_1;
			printk("pcmcia[0]: event 1\n");
		} else if (event & BIT_7) {
			ANGELX_H(0x334) = BIT_7;
			ANGELX_H(0x32C) |= BIT_1;
			ANGELX_H(0x31C) |= BIT_7;
			printk("pcmcia[0]: status changed\n");
		}
		ANGELX_H(0x018) = 0x3030;
	} else if (slot & PCMCIA_SLOT_1) {
		unsigned short event = ANGELX_H(0x414);
		if (event & BIT_0) {
			ANGELX_H(0x434) = BIT_0;
			ANGELX_H(0x42C) |= BIT_0;
			ANGELX_H(0x41C) |= BIT_0;
			printk("pcmcia[1]: event 0\n");
		} else if (event & BIT_1) {
			ANGELX_H(0x434) = BIT_1;
			ANGELX_H(0x42C) |= BIT_1;
			ANGELX_H(0x41C) |= BIT_1;
			printk("pcmcia[1]: event 1\n");
		} else if (event & BIT_7) {
			ANGELX_H(0x434) = BIT_7;
			ANGELX_H(0x42C) |= BIT_1;
			ANGELX_H(0x41C) |= BIT_7;
			printk("pcmcia[1]: event 2\n");
		}
	}
	ANGELX_H(0x004) = 0x0202;
	return IRQ_HANDLED;
}
#endif

#define ASIC2_H ANGELX_H

void angelx_voodoo_b(void) {
	ASIC2_H(0x004) = 0x0101;
        ASIC2_H(0x008) = 0x1212;
        ASIC2_H(0x300) = ((ASIC2_H(0x000) & 0x8000) ? 0x0401 : 0x0501);
        ASIC2_H(0x338) = 0x0000;
        ASIC2_H(0x33C) = 0x0000;
        ASIC2_H(0x340) = 0x0700;
        ASIC2_H(0x334) = 0x0080;
//      while (BIT_7 & ASIC2_H(0x334));
        ASIC2_H(0x31C) |= 0x0080; 

	ASIC2_H(0x004) = 0x2020;
        ASIC2_H(0x004) = 0x4000;
        ASIC2_H(0x008) = 0x1212;

	ASIC2_H(0x004) = 0x0101;
        ASIC2_H(0x008) = 0x2222;
        ASIC2_H(0x400) = (ASIC2_H(0x000) & 0x8000) ? 0x0401 : 0x0501;
        ASIC2_H(0x438) = 0x0000;
        ASIC2_H(0x43C) = 0x0000;
        ASIC2_H(0x440) = 0x0300;
        ASIC2_H(0x434) = 0x0080;
//      while (BIT_7 & ASIC2_H(0x434));
        ASIC2_H(0x41C) |= 0x0080;

	ASIC2_H(0x004) = 0x2020;
        ASIC2_H(0x004) = 0x4000;
        ASIC2_H(0x008) = 0x1212;
	
	ASIC2_H(0x008) = 0x2020;
}

void angelx_voodoo(void) {
	int voodoo_int;
	ASIC2_H(0x204) = 0x0000;
        ASIC2_H(0x238) = 0x0000;
        voodoo_int = ASIC2_H(0x018);
	printk("angelx: voodoo_int = %08x\n", voodoo_int);
        ASIC2_H(0x004) = 0x8000;
        ASIC2_H(0x004) = 0x4040;
        ASIC2_H(0x004) = 0x2000;
        ASIC2_H(0x008) = 0x3A00;

	ASIC2_H(0x004) = 0x0101;
        ASIC2_H(0x008) = 0x0202;
        ASIC2_H(0x200) = 0x0000;
        ASIC2_H(0x208) = 0x0000;
        ASIC2_H(0x018) = 0xFF00;
        ASIC2_H(0x004) = 0x2222;

ASIC2_H(0x004) = 0x0101;
	ASIC2_H(0x008) = 0x1212;
	ASIC2_H(0x300) = ((ASIC2_H(0x000) & 0x8000) ? 0x0401 : 0x0501);
	ASIC2_H(0x338) = 0x0000;
	ASIC2_H(0x33C) = 0x0000;
	ASIC2_H(0x340) = 0x0700;
	ASIC2_H(0x334) = 0x0080;
//	while (BIT_7 & ASIC2_H(0x334));
	ASIC2_H(0x31C) |= 0x0080;

ASIC2_H(0x004) = 0x0101;
	ASIC2_H(0x008) = 0x2222;
	ASIC2_H(0x400) = (ASIC2_H(0x000) & 0x8000) ? 0x0401 : 0x0501;
	ASIC2_H(0x438) = 0x0000;
	ASIC2_H(0x43C) = 0x0000;
	ASIC2_H(0x440) = 0x0300;
	ASIC2_H(0x434) = 0x0080;
//	while (BIT_7 & ASIC2_H(0x434));
	ASIC2_H(0x41C) |= 0x0080;

	ASIC2_H(0x204) = 0x0008;
        ASIC2_H(0x2A0) = 0x0092;
	ASIC2_H(0x018) = (voodoo_int << 8) | (voodoo_int & 0xFF);
}


void __init angelx_init_irq(int h_irq, unsigned int phys_base) {
        int ret, irq;

        ret = request_resource(&iomem_resource, &angelx_res);
        if (ret)
		return;

        angelx_base = ioremap(phys_base, 0x1000);
        if (!angelx_base)
		return;
	angelx_host_irq = h_irq;

        // Now initialize the hardware
	// Test Chip ID
	if(ANGELX_H(0) != 0x8215){
		printk("This appears to be an unknown AngelX variant. Please contact us with details of your hardware.\n");
		return;
	}
        set_irq_type(angelx_host_irq, IRQT_NOEDGE);
        //   A bit of voodoo...
//	angelx_voodoo_b();
#if 0
        ANGELX_H(0x004) = 0x0101;
        ANGELX_H(0x008) = 0x1212;
        ANGELX_H(0x018) = 0xFF00;
        ANGELX_H(0x004) = 0x2222;
#endif
	ANGELX_H(0x004) = 0x0101;
        ANGELX_H(0x008) = 0x0202;
        ANGELX_H(0x018) = 0xFF00;
        ANGELX_H(0x004) = 0x2222;

        //   Clear pending socket 0 requests
        ANGELX_H(0x334) |= BIT_0 | BIT_1 | BIT_7;
        ANGELX_H(0x31C) |= BIT_0 | BIT_1 | BIT_7;
        ANGELX_H(0x32C) |= BIT_1;
        ANGELX_H(0x434) |= BIT_0 | BIT_1 | BIT_7;
        ANGELX_H(0x41C) |= BIT_0 | BIT_1 | BIT_7;
        ANGELX_H(0x42C) |= BIT_1;
        ANGELX_H(0x018) = 0x3030;

        printk("angelx: initialised.\n");

	/* setup extra lubbock irqs */
	for (irq = ANGELX_IRQ_BASE; irq < ANGELX_IRQ_BASE+6; irq++) {
		set_irq_chip(irq, &angelx_irq_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	set_irq_chained_handler(angelx_host_irq, angelx_irq_handler);

        //  Re-enable interrupt
        set_irq_type(angelx_host_irq, IRQT_RISING);

}
EXPORT_SYMBOL(angelx_init_irq);
