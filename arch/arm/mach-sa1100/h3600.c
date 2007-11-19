/*
 * Hardware definitions for Compaq iPAQ H3xxx Handheld Computers
 *
 * Copyright 2000,1 Compaq Computer Corporation.
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
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2001-10-??	Andrew Christian   Added support for iPAQ H3800
 *				   and abstracted EGPIO interface.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/serial_core.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/setup.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include <asm/arch/h3600.h>
#include <linux/gpio_keys.h>

#include "generic.h"

static struct mtd_partition h3xxx_partitions[] = {
	{
		.name		= "H3XXX boot firmware",
		.size		= 0x00040000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,  /* force read-only */
	}, {
#ifdef CONFIG_MTD_2PARTS_IPAQ
		.name		= "H3XXX root jffs2",
		.size		= MTDPART_SIZ_FULL,
		.offset		= 0x00040000,
#else
		.name		= "H3XXX kernel",
		.size		= 0x00080000,
		.offset		= 0x00040000,
	}, {
		.name		= "H3XXX params",
		.size		= 0x00040000,
		.offset		= 0x000C0000,
	}, {
#ifdef CONFIG_JFFS2_FS
		.name		= "H3XXX root jffs2",
		.size		= MTDPART_SIZ_FULL,
		.offset		= 0x00100000,
#else
		.name		= "H3XXX initrd",
		.size		= 0x00100000,
		.offset		= 0x00100000,
	}, {
		.name		= "H3XXX root cramfs",
		.size		= 0x00300000,
		.offset		= 0x00200000,
	}, {
		.name		= "H3XXX usr cramfs",
		.size		= 0x00800000,
		.offset		= 0x00500000,
	}, {
		.name		= "H3XXX usr local",
		.size		= MTDPART_SIZ_FULL,
		.offset		= 0x00d00000,
#endif
#endif
	}
};



static struct flash_platform_data h3xxx_flash_data = {
	.map_name	= "cfi_probe",
	.set_vpp	= ipaqsa_mtd_set_vpp,
	.parts		= h3xxx_partitions,
	.nr_parts	= ARRAY_SIZE(h3xxx_partitions),
};

static struct resource h3xxx_flash_resource = {
	.start		= SA1100_CS0_PHYS,
	.end		= SA1100_CS0_PHYS + SZ_32M - 1,
	.flags		= IORESOURCE_MEM,
};

static struct gpio_keys_button h3600_button_table[] = {
	{ KEY_POWER, GPIO_NR_H3600_NPOWER_BUTTON, 1 },
	{ KEY_ENTER, GPIO_NR_H3600_ACTION_BUTTON, 1 },
};

static struct gpio_keys_platform_data h3600_keys_data = {
	.buttons = h3600_button_table,
	.nbuttons = ARRAY_SIZE(h3600_button_table),
};

static struct platform_device h3600_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &h3600_keys_data,
	},
};

/************************* H3600 *************************/

#ifdef CONFIG_SA1100_H3600

#define H3600_EGPIO	(*(volatile unsigned int *)IPAQSA_EGPIO_VIRT)
static unsigned int h3600_egpio = EGPIO_H3600_RS232_ON;

static void h3600_control_egpio(enum ipaq_egpio_type x, int setp)
{
	unsigned int egpio = 0;
	unsigned long flags;

	switch (x) {
	case IPAQ_EGPIO_LCD_POWER:
		egpio |= EGPIO_H3600_LCD_ON |
			 EGPIO_H3600_LCD_5V_ON |
			 EGPIO_H3600_LVDD_ON;
		break;
	case IPAQ_EGPIO_LCD_ENABLE:
		egpio |= EGPIO_H3600_LCD_PCI;
		break;
	case IPAQ_EGPIO_CODEC_NRESET:
		egpio |= EGPIO_H3600_CODEC_NRESET;
		break;
	case IPAQ_EGPIO_AUDIO_ON:
		egpio |= EGPIO_H3600_AUD_AMP_ON |
			 EGPIO_H3600_AUD_PWR_ON;
		break;
	case IPAQ_EGPIO_QMUTE:
		egpio |= EGPIO_H3600_QMUTE;
		break;
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		egpio |= EGPIO_H3600_OPT_NVRAM_ON;
		break;
	case IPAQ_EGPIO_OPT_ON:
		egpio |= EGPIO_H3600_OPT_ON;
		break;
	case IPAQ_EGPIO_CARD_RESET:
		egpio |= EGPIO_H3600_CARD_RESET;
		break;
	case IPAQ_EGPIO_OPT_RESET:
		egpio |= EGPIO_H3600_OPT_RESET;
		break;
	case IPAQ_EGPIO_IR_ON:
		egpio |= EGPIO_H3600_IR_ON;
		break;
	case IPAQ_EGPIO_IR_FSEL:
		egpio |= EGPIO_H3600_IR_FSEL;
		break;
	case IPAQ_EGPIO_RS232_ON:
		egpio |= EGPIO_H3600_RS232_ON;
		break;
	case IPAQ_EGPIO_VPP_ON:
		egpio |= EGPIO_H3600_VPP_ON;
		break;

        /*
	 * unhandled EGPIOs
	 */
	case IPAQ_EGPIO_PCMCIA_CD0_N:
	case IPAQ_EGPIO_PCMCIA_CD1_N:
	case IPAQ_EGPIO_PCMCIA_IRQ0:
	case IPAQ_EGPIO_PCMCIA_IRQ1:
	case IPAQ_EGPIO_BLUETOOTH_ON:
	default:
		break;
	}

	if (egpio) {
		local_irq_save(flags);
		if (setp)
			h3600_egpio |= egpio;
		else
			h3600_egpio &= ~egpio;
		H3600_EGPIO = h3600_egpio;
		local_irq_restore(flags);
	}
}


static struct ipaq_model_ops h3600_model_ops __initdata = {
	.generic_name	= "3600",
	.control	= h3600_control_egpio,
};

static void __init h3600_map_io(void)
{
	ipaqsa_map_io();

	/* Initialize h3600-specific values here */

	GPCR = 0x0fffffff;	 /* All outputs are set low by default */
	GPDR = GPIO_GPIO(GPIO_NR_H3600_COM_RTS) | GPIO_H3600_L3_CLOCK |
	       GPIO_H3600_L3_MODE  | GPIO_H3600_L3_DATA  |
	       GPIO_H3600_CLK_SET1 | GPIO_H3600_CLK_SET0 |
	       GPIO_LDD15 | GPIO_LDD14 | GPIO_LDD13 | GPIO_LDD12 |
	       GPIO_LDD11 | GPIO_LDD10 | GPIO_LDD9  | GPIO_LDD8;

	H3600_EGPIO = h3600_egpio;	   /* Maintains across sleep? */
	ipaq_model_ops = h3600_model_ops;
}


/*--- list of devices ---*/

struct platform_device h3600micro_device = {
	.name		= "h3600-micro",
	.id		= -1,
};
EXPORT_SYMBOL(h3600micro_device);

static struct platform_device h3600lcd_device = {
	.name		= "h3600-lcd",
/*	.dev		= {                      */
/*		.parent  = &sa11x0fb_device.dev, */
/*	}, 	                                 */
	.id		= -1,
};

static struct platform_device *devices[] __initdata = {
	&h3600micro_device,
	&h3600lcd_device,
	&h3600_keys,
};


static void h3600_mach_init(void)
{
	ipaqsa_mach_init();
	sa11x0_set_flash_data(&h3xxx_flash_data, &h3xxx_flash_resource, 1);
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(H3600, "Compaq iPAQ H3600")
	.phys_io	= 0x80000000,
	.io_pg_offst	= ( io_p2v(0x80000000) >> 18) & 0xfffc,
	.boot_params	= 0xc0000100,
	.map_io		= h3600_map_io,
	.init_irq	= sa1100_init_irq,
	.timer		= &sa1100_timer,
	.init_machine	= h3600_mach_init,
MACHINE_END

#endif /* CONFIG_SA1100_H3600 */


