/*
 * Hardware definitions for HP iPAQ H31xx Handheld Computers
 *
 * Copyright 2000-2002 Compaq Computer Corporation.
 * Copyright 2002-2003 Hewlett-Packard Company.
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
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 *                                 and abstracted EGPIO interface.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <../drivers/video/sa1100fb.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include <linux/serial_core.h>

#include <asm/arch/h3100.h>

#include "generic.h"


/************************* H3100 *************************/

#define H3100_EGPIO	(*(volatile unsigned int *)IPAQSA_EGPIO_VIRT)
static unsigned int h3100_egpio = 0;

static void h3100_control_egpio( enum ipaq_egpio_type x, int setp )
{
	unsigned int egpio = 0;
	long         gpio = 0;
	unsigned long flags;

	switch (x) {
	case IPAQ_EGPIO_CODEC_NRESET:
		egpio |= EGPIO_H3600_CODEC_NRESET;
		break;
	case IPAQ_EGPIO_AUDIO_ON:
		gpio |= GPIO_H3100_AUD_PWR_ON
			| GPIO_H3100_AUD_ON;
		break;
	case IPAQ_EGPIO_QMUTE:
		gpio |= GPIO_H3100_QMUTE;
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
		gpio |= GPIO_H3100_IR_ON;
		break;
	case IPAQ_EGPIO_IR_FSEL:
		gpio |= GPIO_H3100_IR_FSEL;
		break;
	case IPAQ_EGPIO_RS232_ON:
		egpio |= EGPIO_H3600_RS232_ON;
		break;
	case IPAQ_EGPIO_VPP_ON:
		egpio |= EGPIO_H3600_VPP_ON;
		break;
	default:
		printk("%s: unhandled egpio=%d\n", __FUNCTION__, x);
	}

	if ( egpio || gpio ) {
		local_irq_save(flags);
		if ( setp ) {
			h3100_egpio |= egpio;
			GPSR = gpio;
		} else {
			h3100_egpio &= ~egpio;
			GPCR = gpio;
		}
		H3100_EGPIO = h3100_egpio;
		local_irq_restore(flags);
	}
}

static struct ipaq_model_ops h3100_model_ops __initdata = {
	.generic_name = "3100",
	.control      = h3100_control_egpio,
};


#if 0
static struct sa1100fb_mach_info h3100_sa1100fb_mach_info __initdata = {
	.pixclock	= 406977, 	.bpp		= 4,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 26,		.vsync_len	= 41,
	.left_margin	= 4,		.upper_margin	= 0,
	.right_margin	= 4,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 1,
	.cmap_inverse	= 1,

	.lccr0		= LCCR0_Mono | LCCR0_4PixMono | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};

static void *h3100_get_mach_info(struct lcd_device *lm)
{
	return (void *)&h3100_sa1100fb_mach_info;
}

struct lcd_device h3100_lcd_device = {
	.get_mach_info = h3100_get_mach_info,
	.set_power = h3100_lcd_set_power,
	.get_power = h3100_lcd_get_power
};
#endif


#define H3100_DIRECT_EGPIO (GPIO_H3100_BT_ON      \
                          | GPIO_H3100_GPIO3      \
                          | GPIO_H3100_QMUTE      \
                          | GPIO_H3100_LCD_3V_ON  \
	                  | GPIO_H3100_AUD_ON     \
		          | GPIO_H3100_AUD_PWR_ON \
			  | GPIO_H3100_IR_ON      \
			  | GPIO_H3100_IR_FSEL)

static void __init h3100_map_io(void)
{
	ipaqsa_map_io();
	/* sa1100_register_uart(1, 1);  */ /* Microcontroller */

	/* Initialize h3100-specific values here */
	GPCR = 0x0fffffff;       /* All outputs are set low by default */
	GPDR = GPIO_GPIO(GPIO_NR_H3600_COM_RTS) | GPIO_H3600_L3_CLOCK |
	       GPIO_H3600_L3_MODE  | GPIO_H3600_L3_DATA  |
	       GPIO_H3600_CLK_SET1 | GPIO_H3600_CLK_SET0 |
	       H3100_DIRECT_EGPIO;

	/* Older bootldrs put GPIO2-9 in alternate mode on the
	   assumption that they are used for video */
	GAFR &= ~H3100_DIRECT_EGPIO;

	H3100_EGPIO = h3100_egpio;
	ipaq_model_ops = h3100_model_ops;
}

static void __init h3100_mach_init(void)
{
	ipaqsa_mach_init();
}

MACHINE_START(H3100, "HP iPAQ H3100")
//*	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	        .phys_io        = 0xf8000000, 
		.io_pg_offst    = ( io_p2v(0x80000000) >> 18) & 0xfffc,
		.boot_params    = 0xc0000100,
		.map_io         = h3100_map_io,
		.init_irq       = sa1100_init_irq,
		.timer          = &sa1100_timer,
		.init_machine   = h3100_mach_init,
MACHINE_END

