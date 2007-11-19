/*
 *  Asus MyPal A716 GPO Register driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
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
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/asus716-gpio.h>

#include "../generic.h"

static volatile u_int32_t *gpo;
static u_int32_t gpo_local;
static spinlock_t gpo_lock;

#define GPO_DEFAULT 	0x62808000 | GPO_A716_SD_POWER_N | GPO_A716_IRDA_POWER_N | \
			GPO_A716_CPU_MODE_SEL0 | GPO_A716_CPU_MODE_SEL1;

void a716_gpo_set(unsigned long bits)
{
	unsigned long flags;

	if (machine_is_a716()) {
		spin_lock_irqsave(&gpo_lock, flags);
		gpo_local |= bits;
		*gpo = gpo_local;
		spin_unlock_irqrestore(&gpo_lock, flags);
	}
}
EXPORT_SYMBOL(a716_gpo_set);

void a716_gpo_clear(unsigned long bits)
{
	unsigned long flags;

	if (machine_is_a716()) {
		spin_lock_irqsave(&gpo_lock, flags);
		gpo_local &= ~bits;
		*gpo = gpo_local;
		spin_unlock_irqrestore(&gpo_lock, flags);
	}
}
EXPORT_SYMBOL(a716_gpo_clear);

unsigned long a716_gpo_get(void)
{
	unsigned long flags, ret = 0;

	if (machine_is_a716()) {
		spin_lock_irqsave(&gpo_lock, flags);
		ret = gpo_local;
		spin_unlock_irqrestore(&gpo_lock, flags);
	}

	return ret;
}
EXPORT_SYMBOL(a716_gpo_get);

static int a716_gpo_resume(struct platform_device *pdev)
{
	unsigned long flags;

	spin_lock_irqsave(&gpo_lock, flags);
	gpo_local = GPO_DEFAULT;
	*gpo = gpo_local;
	spin_unlock_irqrestore(&gpo_lock, flags);

	return 0;
}

static int a716_gpo_probe(struct platform_device *pdev)
{
	if (!machine_is_a716())
		return -ENODEV;

	spin_lock_init(&gpo_lock);
	gpo = (volatile u_int32_t *)ioremap(0x10000000, sizeof *gpo);
	if (!gpo)
		return -ENOMEM;
	*gpo = gpo_local;

	return 0;
}

static int a716_gpo_remove(struct platform_device *pdev)
{
	iounmap((void __iomem *)gpo);
	
	return 0;
}

static struct platform_driver a716_gpo_driver = {
	.driver		= {
		.name           = "a716-gpo",
	},
	.probe		= a716_gpo_probe,
	.remove		= a716_gpo_remove,
	.resume		= a716_gpo_resume,
};

static int a716_gpo_init(void)
{
	if (!machine_is_a716())
		return -ENODEV;

	return platform_driver_register(&a716_gpo_driver);
}

static void a716_gpo_exit(void)
{
	platform_driver_unregister(&a716_gpo_driver);
}

module_init(a716_gpo_init);
module_exit(a716_gpo_exit);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 GPO Register driver");
MODULE_LICENSE("GPL");
