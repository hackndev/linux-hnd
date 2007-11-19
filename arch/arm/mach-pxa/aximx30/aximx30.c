/*
 * 
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2004 Hewlett-Packard Company.
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
 * History:
 *
 * 2004-11-2004	Michael Opdenacker	Preliminary version
 * 2004-12-16   Todd Blumer
 * 2004-12-22   Michael Opdenacker	Added USB management
 * 2005-01-30   Michael Opdenacker	Improved Asic3 settings and initialization
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/aximx30-gpio.h>
#include <asm/arch/aximx30-asic.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/audio.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#include "../generic.h"
#include "aximx30_core.h"

/* Physical address space information */

/* TI WLAN, EGPIO, External UART */
#define X30_EGPIO_WLAN_PHYS	PXA_CS5_PHYS

/* Initialization code */

static void __init x30_map_io(void)
{
	pxa_map_io();
}

static void __init x30_init_irq(void)
{
	/* int irq; */

	pxa_init_irq();
}


/* ASIC3 */

static struct asic3_platform_data x30_asic3_platform_data = {

   /* Setting ASIC3 GPIO registers to the below initialization states
    * x4700 asic3 information: http://handhelds.org/moin/moin.cgi/HpIpaqHx4700Hardware
    *
    * dir:	Direction of the GPIO pin. 0: input, 1: output.
    *      	If unknown, set as output to avoid power consuming floating input nodes
    * init:	Initial state of the GPIO bits
    *
    * These registers are configured as they are on Wince, and are configured
    * this way on bootldr.
    */
        .gpio_a = {
	//	.mask           = 0xffff,
		.dir            = 0xffff, // Unknown, set as outputs so far
		.init           = 0x0000,
	//	.trigger_type   = 0x0000,
	//	.edge_trigger   = 0x0000,
	//	.leveltri       = 0x0000,
	//	.sleep_mask     = 0xffff,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
	//	.int_status     = 0x0000,
		.alt_function   = 0xffff,
		.sleep_conf     = 0x000c,
        },
        .gpio_b = {
	//	.mask           = 0xffff,
		.dir            = 0xffff, // Unknown, set as outputs so far
		.init           = 0x0000,
	//	.trigger_type   = 0x0000,
	//	.edge_trigger   = 0x0000,
	//	.leveltri       = 0x0000,
	//	.sleep_mask     = 0xffff,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
	//	.int_status     = 0x0000,
		.alt_function   = 0xffff,
                .sleep_conf     = 0x000c,
        },
        .gpio_c = {
	//	.mask           = 0xffff,
                .dir            = 0x6067,
	// GPIOC_SD_CS_N | GPIOC_CIOW_N | GPIOC_CIOR_N  | GPIOC_PWAIT_N | GPIOC_PIOS16_N,
                .init           = 0x0000,
	//	.trigger_type   = 0x0000,
	//	.edge_trigger   = 0x0000,
	//	.leveltri       = 0x0000,
	//	.sleep_mask     = 0xffff,
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
	//	.int_status     = 0x0000,
		.alt_function   = 0xfff7, // GPIOC_LED_RED | GPIOC_LED_GREEN | GPIOC_LED_BLUE,
                .sleep_conf     = 0x000c,
        },
        .gpio_d = {
	//	.mask           = 0xffff,
		.dir            = 0x0000, // Only inputs
		.init           = 0x0000,
	//	.trigger_type   = 0x67ff,
	//	.edge_trigger   = 0x0000,
	//	.leveltri       = 0x0000,
	//	.sleep_mask     = 0x9800,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
	//	.int_status     = 0x0000,
		.alt_function   = 0x9800,
		.sleep_conf     = 0x000c,
        },
	.bus_shift = 1,
};

static struct resource asic3_resources[] = {
	[0] = {
		.start	= X30_ASIC3_PHYS,
		.end	= X30_ASIC3_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= X30_IRQ(ASIC3_EXT_INT),
		.end	= X30_IRQ(ASIC3_EXT_INT),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device x30_asic3 = {
	.name		= "asic3",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(asic3_resources),
	.resource	= asic3_resources,
	.dev = {
		.platform_data = &x30_asic3_platform_data,
	},
};
EXPORT_SYMBOL(x30_asic3);

/* Core Hardware Functions */

static struct resource pxa_cs5_resources[] = {
	[0] = {
		.start = X30_EGPIO_WLAN_PHYS,		/* EGPIOs: +0x400000 */
		.end = X30_EGPIO_WLAN_PHYS + 0x1000000, 	/* WLAN: +0x800000 */
		.flags = IORESOURCE_MEM,
	},
};

static struct x30_core_funcs core_funcs;

struct platform_device x30_core = {
	.name		= "x30-core",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(pxa_cs5_resources),
	.resource	= pxa_cs5_resources,
	.dev = {
		.platform_data = &core_funcs,
	},
};

/* USB Device Controller */

static int
udc_detect(void)
{
	if (core_funcs.udc_detect != NULL)
		return core_funcs.udc_detect();
	else
		return 0;
}

static void
udc_enable(int cmd) 
{
	switch (cmd)
	{
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk (KERN_NOTICE "USB cmd disconnect\n");
			SET_X30_GPIO(USB_PUEN, 0);
			break;

		case PXA2XX_UDC_CMD_CONNECT:
			printk (KERN_NOTICE "USB cmd connect\n");
			SET_X30_GPIO(USB_PUEN, 1);
			break;
	}
}

static struct pxa2xx_udc_mach_info x30_udc_mach_info = {
	.udc_is_connected = udc_detect,
	.udc_command      = udc_enable,
};

/* PXA2xx Keys */

static struct gpio_keys_button x30_button_table[] = {
	{ KEY_SUSPEND, GPIO_NR_X30_KEY_ON, 1 },
	{ KEY_F11 /* mail */, GPIO_NR_X30_KEY_AP3, 1 },
	{ KEY_F10 /* contacts */, GPIO_NR_X30_KEY_AP1, 1 },
};

static struct gpio_keys_platform_data x30_pxa_keys_data = {
	.buttons = x30_button_table,
	.nbuttons = ARRAY_SIZE(x30_button_table),
};

static struct platform_device x30_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &x30_pxa_keys_data,
	},
};

static struct platform_device *devices[] __initdata = {
	&x30_asic3,
	&x30_core,
	&x30_pxa_keys,
};

static void __init x30_init( void )
{
#if 0	// keep for reference, from bootldr
	GPSR0 = 0x0935ede7;
	GPSR1 = 0xffdf40f7;
	GPSR2 = 0x0173c9f6;
	GPSR3 = 0x01f1e342;
	GPCR0 = ~0x0935ede7;
	GPCR1 = ~0xffdf40f7;
	GPCR2 = ~0x0173c9f6;
	GPCR3 = ~0x01f1e342;
	GPDR0 = 0xda7a841c;
	GPDR1 = 0x68efbf83;
	GPDR2 = 0xbfbff7db;
	GPDR3 = 0x007ffff5;
	GAFR0_L = 0x80115554;
	GAFR0_U = 0x591a8558;
	GAFR1_L = 0x600a9558;
	GAFR1_U = 0x0005a0aa;
	GAFR2_L = 0xa0000000;
	GAFR2_U = 0x00035402;
	GAFR3_L = 0x00010000;
	GAFR3_U = 0x00001404;
	MSC0 = 0x25e225e2;
	MSC1 = 0x12cc2364;
	MSC2 = 0x16dc7ffc;
#endif

	SET_X30_GPIO( ASIC3_RESET_N, 0 );
	mdelay(50);
	SET_X30_GPIO( ASIC3_RESET_N, 1 );
	mdelay(50);

	platform_add_devices( devices, ARRAY_SIZE(devices) );
	pxa_set_udc_info( &x30_udc_mach_info );
}


MACHINE_START(X30, "Dell Axim X30")
	/* Maintainer Giuseppe Zompatori, <giuseppe_zompatori@yahoo.it> */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= x30_map_io,
	.init_irq	= x30_init_irq,
	.init_machine	= x30_init,
	.timer		= &pxa_timer,
MACHINE_END
