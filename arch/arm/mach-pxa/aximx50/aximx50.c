/*
 * 
 * Hardware definitions for Dell Axim X50/51(v)
 * 
 * History:
 *
 * 2007-01-22   Pierre Gaufillet, <pierre.gaufillet@magic.fr>
 * Copyright (c) 2007 Patrick Steiner
 * Copyright (c) 2007 Matt Barclay
 * 
 * Based on the work of :
 *              Giuseppe Zompatori, <giuseppe_zompatori@yahoo.it>
 *              Andrew Zabolotny <zap@homelink.ru>
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/corgi_bl.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/aximx50-gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>

#include "../generic.h"

/*************************
 * USB Device Controller *
 *************************/

static int
udc_detect(void)
{
	printk (KERN_NOTICE "entering/leaving udc_detect\n");
	return 0;
}

static void
udc_enable(int cmd) 
{
	switch (cmd)
	{
	case PXA2XX_UDC_CMD_DISCONNECT:
		printk (KERN_NOTICE "USB cmd disconnect\n");
//		SET_X50_GPIO(USB_PUEN, 0);
		break;

	case PXA2XX_UDC_CMD_CONNECT:
		printk (KERN_NOTICE "USB cmd connect\n");
//		SET_X50_GPIO(USB_PUEN, 1);
		break;
	}
}

static struct pxa2xx_udc_mach_info aximx50_udc_mach_info = {
	.udc_is_connected = udc_detect,
	.udc_command      = udc_enable,
};

/*************
 * Backlight *
 *************/

#define AXIMX50_MAX_INTENSITY 0x3ff

static void aximx50_set_bl_intensity(int intensity)
{
	if (intensity < 7) intensity = 0;

	PWM_CTRL0 = 1;
	PWM_PERVAL0 = AXIMX50_MAX_INTENSITY;
	PWM_PWDUTY0 = intensity;

	if (intensity) {
		pxa_gpio_mode(GPIO16_PWM0_MD);
		pxa_set_cken(CKEN0_PWM0, 1);
		SET_X50_GPIO(BACKLIGHT_ON, 1);
	} else {
		pxa_set_cken(CKEN0_PWM0, 0);
		SET_X50_GPIO(BACKLIGHT_ON, 0);
	}
}

static struct corgibl_machinfo aximx50_bl_machinfo = {
	.max_intensity = AXIMX50_MAX_INTENSITY,
	.default_intensity = AXIMX50_MAX_INTENSITY,
	.limit_mask = 0xff, /* limit brightness to about 1/4 on low battery */
	.set_bl_intensity = aximx50_set_bl_intensity,
};

struct platform_device aximx50_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &aximx50_bl_machinfo,
	},
};

/***********
 * Buttons *
 ***********/

static struct platform_device aximx50_buttons           = { 
	.name = "aximx50-buttons", 
};

/***************
 * Framebuffer *
 ***************/

/* Axim X50 VGA Version */
static struct pxafb_mode_info aximx50_pxafb_modes_vga[] = {
{
	.pixclock	= 96153,
	.bpp		= 16,
	.xres		= 480,
	.yres		= 640,
	.hsync_len	= 64,
	.vsync_len	= 5,
	.left_margin	= 17,
	.upper_margin	= 1,
	.right_margin	= 87,
	.lower_margin   = 4,
},
};

static struct pxafb_mach_info aximx50_fb_info_vga = {
	.modes		= aximx50_pxafb_modes_vga,
	.num_modes	= ARRAY_SIZE(aximx50_pxafb_modes_vga),
	.lccr0		= LCCR0_ENB | LCCR0_LDM |		// 0x9
		LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_Act |	// 0xf
		LCCR0_QDM |					// 0x8
								// 0x0
								// 0x0
		LCCR0_BM  | LCCR0_OUM | LCCR0_RDSTM |		// 0xb
		LCCR0_CMDIM					// 0x1
		,						// 0x0
		//0x01b008f9,
	.lccr3		= 0x04f00001,
};
/* Axim X50 QVGA Version */
static struct pxafb_mode_info aximx50_pxafb_modes_qvga[] = {
{
	.pixclock	= 96153,
	.bpp		= 16,
	.xres		= 240,
	.yres		= 320,
	.hsync_len	= 20,
	.vsync_len	= 4,
	.left_margin	= 59,
	.upper_margin	= 4,
	.right_margin	= 16,
	.lower_margin	= 0,
},
};


static struct pxafb_mach_info aximx50_fb_info_qvga = {
	.modes		= aximx50_pxafb_modes_qvga,
	.num_modes	= ARRAY_SIZE(aximx50_pxafb_modes_qvga),
	.lccr0		= LCCR0_ENB | LCCR0_LDM |		// 0x9
		LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_Act |	// 0xf
		LCCR0_QDM |					// 0x8
								// 0x0
								// 0x0
		LCCR0_BM  | LCCR0_OUM				// 0x3
								// 0x0
		,						// 0x0
		//0x003008f9,
	.lccr3		= 0x04900008,
};                                  

static int lcd_vga = 0;

static int aximx50_lcd_probe(void)
{
	//TODO: select the correct device automatic
	#ifdef CONFIG_X50_VGA
	lcd_vga = 1;
	#endif

	if (lcd_vga == 1) {
		printk(KERN_NOTICE "DELL AXIMX50 VGA LCD driver\n");
		set_pxa_fb_info(&aximx50_fb_info_vga);
	} else {
		printk(KERN_NOTICE "DELL AXIMX50 QVGA LCD driver\n");
		set_pxa_fb_info(&aximx50_fb_info_qvga);
	}

	return 0;
}

static struct platform_device aximx50_lcd = {
	.name = "aximx50-lcd",
	.id = -1,
};

/***************
 * Touchscreen *
 ***************/

static struct platform_device aximx50_ts                = { 
	.name = "aximx50-ts", 
};

/****************
 * Init Machine *
 ****************/

static struct platform_device *devices[] __initdata = {
	&aximx50_buttons,
	&aximx50_ts,
	&aximx50_bl,
	//&aximx50_lcd,
};

static void __init aximx50_map_io(void)
{
	pxa_map_io();
}

static void __init aximx50_init_irq(void)
{
	pxa_init_irq();
}

static void __init aximx50_init( void )
{
#if 0    // keep for reference, from bootldr
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
	GAFR2_L = 0xa8000000;
	GAFR2_U = 0x00035402;
	GAFR3_L = 0x00010000;
	GAFR3_U = 0x00001404;
	MSC0 = 0x25e225e2;
	MSC1 = 0x12cc2364;
	MSC2 = 0x16dc7ffc;
#endif

	aximx50_lcd_probe();
	platform_add_devices(devices, ARRAY_SIZE(devices));
	pxa_set_udc_info(&aximx50_udc_mach_info);
}


MACHINE_START(X50, "Dell Axim X50/X51(v)")
	.phys_io = 0x40000000,
	.io_pg_offst = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params = 0xa8000100,
	.map_io = aximx50_map_io,
	.init_irq = aximx50_init_irq,
	.timer = &pxa_timer,
	.init_machine = aximx50_init,
MACHINE_END

/* ex: set ts=8: */ 
