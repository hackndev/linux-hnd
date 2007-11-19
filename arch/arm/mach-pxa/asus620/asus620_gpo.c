/*
 *  Asus MyPal 620 GPO register driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright 2005 (C) Pawel Kolodziejski
 *  Copyright 2006 (C) Vincent Benony
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/asus620-gpio.h>

#include "../generic.h"

static volatile u_int16_t *gpo;
static u_int16_t gpo_local;
static spinlock_t gpo_lock;

void asus620_gpo_set(unsigned long bits)
{
	unsigned long flags;

	spin_lock_irqsave(&gpo_lock, flags);
	gpo_local |= bits;
	*gpo = gpo_local;
	spin_unlock_irqrestore(&gpo_lock, flags);
}
EXPORT_SYMBOL(asus620_gpo_set);

void asus620_gpo_clear(unsigned long bits)
{
	unsigned long flags;

	spin_lock_irqsave(&gpo_lock, flags);
	gpo_local &= ~bits;
	*gpo = gpo_local;
	spin_unlock_irqrestore(&gpo_lock, flags);
}
EXPORT_SYMBOL(asus620_gpo_clear);

void asus620_gpo_resume(void)
{
	unsigned long flags;

	spin_lock_irqsave(&gpo_lock, flags);
	*gpo = gpo_local;
	spin_unlock_irqrestore(&gpo_lock, flags);
}

void asus620_gpo_suspend(void)
{
	unsigned long flags;

	spin_lock_irqsave(&gpo_lock, flags);
	*gpo = gpo_local;
	spin_unlock_irqrestore(&gpo_lock, flags);
}

void asus620_gpo_init(void)
{
	printk("A620 GPO init\n");
	spin_lock_init(&gpo_lock);
	gpo = (volatile u_int16_t *)ioremap(0x10000000, sizeof *gpo);
	if (!gpo) return;

	gpo_local = GPO_A620_LCD_POWER1 | GPO_A620_LCD_POWER2 | GPO_A620_LCD_POWER3 | GPO_A620_BACKLIGHT | GPO_A620_SOUND | GPO_A620_UNK_2 | GPO_A620_IRDA_FIR_MODE;
	*gpo = gpo_local;
}
EXPORT_SYMBOL(asus620_gpo_init);

