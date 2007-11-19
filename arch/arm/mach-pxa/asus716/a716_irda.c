/*
 *  Asus MyPal A716 IRDA glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>

#include <asm/arch/serial.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/arch/irda.h>

static void a716_irda_transceiver_mode(struct device *dev, int mode)
{
	unsigned long flags;

	local_irq_save(flags);

	if (mode & IR_SIRMODE) {
		a716_gpo_clear(GPO_A716_FIR_MODE);
	} else if (mode & IR_FIRMODE) {
		a716_gpo_set(GPO_A716_FIR_MODE);
	}

	if (mode & IR_OFF) {
		a716_gpo_set(GPO_A716_IRDA_POWER_N);
	} else {
		a716_gpo_clear(GPO_A716_IRDA_POWER_N);
	}

	local_irq_restore(flags);
}

static struct pxaficp_platform_data a716_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_FIRMODE | IR_OFF,
	.transceiver_mode = a716_irda_transceiver_mode,
};

static int a716_irda_probe(struct platform_device *pdev)
{
	pxa_set_ficp_info(&a716_ficp_platform_data);

	return 0;
}

static struct platform_driver a716_irda_driver = {
	.driver   = {
		.name     = "a716-irda",
	},
	.probe    = a716_irda_probe,
};

static int __init a716_irda_init(void)
{
	return platform_driver_register(&a716_irda_driver);
}

module_init(a716_irda_init);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 IRDA glue driver");
MODULE_LICENSE("GPL");
