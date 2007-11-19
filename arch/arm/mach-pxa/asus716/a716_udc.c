/*
 *  Asus MyPal A716 UDC glue driver
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

#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/arch/udc.h>

static int a716_udc_is_connected(void)
{
	int ret = GET_A716_GPIO(USB_CABLE_DETECT_N);

	return ret;
}

static void a716_udc_command(int cmd)
{
	switch (cmd) {
		case PXA2XX_UDC_CMD_DISCONNECT:
			a716_gpo_clear(GPO_A716_USB);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			a716_gpo_set(GPO_A716_USB);
			break;
		default:
			break;
	}
}

static struct pxa2xx_udc_mach_info a716_udc_mach_info __initdata = {
	.udc_is_connected = a716_udc_is_connected,
	.udc_command      = a716_udc_command,
};

static int a716_udc_probe(struct platform_device *pdev)
{
	pxa_set_udc_info(&a716_udc_mach_info);

	return 0;
}

static struct platform_driver a716_udc_driver = {
	.driver   = {
		.name     = "a716-udc",
	},
	.probe    = a716_udc_probe,
};

static int __init a716_udc_init(void)
{
	return platform_driver_register(&a716_udc_driver);
}

#ifdef MODULE
module_init(a716_udc_init);
#else   /* start early for dependencies */
fs_initcall(a716_udc_init);
#endif

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 UDC glue driver");
MODULE_LICENSE("GPL");
