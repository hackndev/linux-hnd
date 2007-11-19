/*
 * Hardware definitions for HP iPAQ H3xxx Handheld Computers
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
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/setup.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include <linux/serial_core.h>

#include <asm/arch/h3800.h>

#include "generic.h"

/************************* H3800 *************************/



#if 0
/*
  On screen enable, we get

     h3800_video_power_on(1)
     LCD controller starts
     h3800_video_lcd_enable(1)

  On screen disable, we get

     h3800_video_lcd_enable(0)
     LCD controller stops
     h3800_video_power_on(0)
*/
static int h3800_lcd_set_power( struct lcd_device *lm, int setp )
{
	if ( setp ) {
		H3800_ASIC1_GPIO_OUT |= GPIO1_LCD_ON;
		mdelay(30);
		H3800_ASIC1_GPIO_OUT |= GPIO1_VGL_ON;
		mdelay(5);
		H3800_ASIC1_GPIO_OUT |= GPIO1_VGH_ON;
		mdelay(50);
		H3800_ASIC1_GPIO_OUT |= GPIO1_LCD_5V_ON;
		mdelay(5);
	}
	else {
		mdelay(5);
		H3800_ASIC1_GPIO_OUT &= ~GPIO1_LCD_5V_ON;
		mdelay(50);
		H3800_ASIC1_GPIO_OUT &= ~GPIO1_VGL_ON;
		mdelay(5);
		H3800_ASIC1_GPIO_OUT &= ~GPIO1_VGH_ON;
		mdelay(100);
		H3800_ASIC1_GPIO_OUT &= ~GPIO1_LCD_ON;
	}
	return 0;
}

static int h3800_lcd_set_enable( struct lcd_device *lm, int setp )
{
	if ( setp ) {
		mdelay(17);     // Wait one from before turning on
		H3800_ASIC1_GPIO_OUT |= GPIO1_LCD_PCI;
	}
	else {
		H3800_ASIC1_GPIO_OUT &= ~GPIO1_LCD_PCI;
		mdelay(30);     // Wait before turning off
	}
	return 0;
}
#endif

#if 0
static struct sa1100fb_mach_info h3800_sa1100fb_mach_info __initdata = {
	.pixclock	= 174757, 	.bpp		= 16,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 3,		.vsync_len	= 3,
	.left_margin	= 12,		.upper_margin	= 10,
	.right_margin	= 17,		.lower_margin	= 1,

	.cmap_static	= 1,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};

static void *h3800_get_mach_info(struct lcd_device *lm)
{
	return (void *)&h3800_sa1100fb_mach_info;
}

struct lcd_device h3800_lcd_device = {
	.get_mach_info = h3800_get_mach_info,
	.get_power     = h3800_lcd_set_power,
	.set_enable    = h3800_lcd_set_enable,
};
#endif

static void h3800_control_egpio( enum ipaq_egpio_type x, int setp )
{
#if 0
	switch (x) {
	case IPAQ_EGPIO_LCD_POWER:
		h3800_lcd_power_on( NULL, setp );
		break;
	case IPAQ_EGPIO_LCD_ENABLE:
		h3800_lcd_enable( NULL, setp );
		break;
	case IPAQ_EGPIO_CODEC_NRESET:
	case IPAQ_EGPIO_AUDIO_ON:
	case IPAQ_EGPIO_QMUTE:
		printk("%s: error - should not be called\n", __FUNCTION__);
		break;
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		SET_ASIC2( GPIO2_OPT_ON_NVRAM );
		break;
	case IPAQ_EGPIO_OPT_ON:
		SET_ASIC2( GPIO2_OPT_ON );
		break;
	case IPAQ_EGPIO_CARD_RESET:
		SET_ASIC2( GPIO2_OPT_PCM_RESET );
		break;
	case IPAQ_EGPIO_OPT_RESET:
		SET_ASIC2( GPIO2_OPT_RESET );
		break;
	case IPAQ_EGPIO_IR_ON:
		CLEAR_ASIC1( GPIO1_IR_ON_N );
		break;
	case IPAQ_EGPIO_IR_FSEL:
		break;
	case IPAQ_EGPIO_RS232_ON:
		SET_ASIC1( GPIO1_RS232_ON );
		break;
	case IPAQ_EGPIO_VPP_ON:
		IPAQ_ASIC2_FlashWP_VPP_ON = setp;
		break;
	default:
		printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, x);
	}
#endif
}

static unsigned long h3800_read_egpio( enum ipaq_egpio_type x )
{
	switch (x) {
	default:
		return ipaqsa_common_read_egpio(x);
	}
}


static struct ipaq_model_ops h3800_model_ops __initdata = {
	.generic_name = "3800",
	.control      = h3800_control_egpio,
};

static void __init h3800_init_irq( void )
{
	int i;

	/* Initialize standard IRQs */
	sa1100_init_irq();


	/* Don't start up the ADC IRQ automatically */
	set_irq_flags(IRQ_H3800_ADC, IRQF_VALID | IRQF_PROBE | IRQF_NOAUTOEN);
	set_irq_type( IRQ_GPIO_H3800_ASIC, IRQT_RISING );
}


static void __init h3800_map_io(void)
{
	ipaqsa_map_io();

	/* Add wakeup on AC plug/unplug */
	PWER  |= PWER_GPIO12;

	/* Initialize h3800-specific values here */
	GPCR = 0x0fffffff;       /* All outputs are set low by default */
	GAFR =  GPIO_H3800_CLK_OUT |
		GPIO_LDD15 | GPIO_LDD14 | GPIO_LDD13 | GPIO_LDD12 |
		GPIO_LDD11 | GPIO_LDD10 | GPIO_LDD9  | GPIO_LDD8;
	GPDR =  GPIO_H3800_CLK_OUT |
		GPIO_GPIO(GPIO_NR_H3600_COM_RTS) | GPIO_H3600_L3_CLOCK |
		GPIO_H3600_L3_MODE  | GPIO_H3600_L3_DATA  |
		GPIO_LDD15 | GPIO_LDD14 | GPIO_LDD13 | GPIO_LDD12 |
		GPIO_LDD11 | GPIO_LDD10 | GPIO_LDD9  | GPIO_LDD8;
	TUCR =  TUCR_3_6864MHz;   /* Seems to be used only for the Bluetooth UART */

	/* Serial port 1 is used as GPIO */
	Ser1UTCR3 = 0;       /* Older bootloaders may set this */
	PPDR  = (PPDR & ~PPC_H3800_NSD_WP) | PPC_H3800_COM_DTR;
	PPSR &= ~PPC_H3800_COM_DTR;        /* Drive it low, which will give +5v to the port */

	/* Fix the memory bus */
	MSC2 = (MSC2 & 0x0000ffff) | 0xE4510000;

	ipaq_model_ops = h3800_model_ops;
}

static void __init h3800_mach_init(void)
{
	ipaqsa_mach_init();
}

MACHINE_START(H3800, "HP iPAQ H3800")
        .phys_io	= 0x80000000,
	.io_pg_offst	= (io_p2v(0x80000000) >> 18) & 0xfffc,
	.boot_params	= 0xc0000100,
	.map_io		= h3800_map_io,
	.init_irq	= sa1100_init_irq,
	.timer		= &sa1100_timer,
	.init_machine   = &h3800_mach_init,
MACHINE_END

