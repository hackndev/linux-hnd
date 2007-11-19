/*
 *  HP iPAQ h1910/h1915 IRDA driver
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
#include <asm/arch/h1900-gpio.h>
#include <asm/arch/irda.h>

static void h1900_irda_transceiver_mode(struct device *dev, int mode) {}

static struct pxaficp_platform_data h1900_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE,
	.transceiver_mode = h1900_irda_transceiver_mode,
};

static int h1900_irda_probe(struct platform_device *pdev)
{
	pxa_set_ficp_info(&h1900_ficp_platform_data);

	return 0;
}

static struct platform_driver h1900_irda_driver = {
	.driver   = {
		.name     = "h1900-irda",
	},
	.probe    = h1900_irda_probe,
};

static int __init h1900_irda_init(void)
{
	return platform_driver_register(&h1900_irda_driver);
}

module_init(h1900_irda_init);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("HP iPAQ h1910/h1915 IRDA glue driver");
MODULE_LICENSE("GPL");
