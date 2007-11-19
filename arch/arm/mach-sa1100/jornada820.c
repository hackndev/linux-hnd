/*
 * linux/arch/arm/mach-sa1100/jornada820.c
 * Jornada 820 fixup and initialization code.
 * 2004/01/22 George Almasi (galmasi@optonline.net)
 * Modelled after the Jornada 720 code.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/hardware/sa1101.h>
#include <asm/hardware/ssp.h>
#include <asm/delay.h>
#include "generic.h"

#include <asm/arch/jornada820.h>

#include <linux/lcd.h>
#include <linux/fb.h>
#include <../drivers/video/sa1100fb.h>

static struct resource sa1101_resources[] = {
	[0] = {
		.start  = JORNADA820_SA1101_BASE,
		.end    = JORNADA820_SA1101_BASE+0x3ffffff,
		.flags  = IORESOURCE_MEM,
	},
        [1] = {
                .start          = GPIO_JORNADA820_SA1101_CHAIN_IRQ,
                .end            = GPIO_JORNADA820_SA1101_CHAIN_IRQ,
                .flags          = IORESOURCE_IRQ,
        },
};

static struct platform_device sa1101_device = {
	.name       = "sa1101",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(sa1101_resources),
	.resource   = sa1101_resources,
};

static struct platform_device *devices[] __initdata = {
	&sa1101_device,
};

#if 0
static struct sa1100fb_mach_info jornada820_sa1100fb __initdata = {
	.pixclock	= 305000, 	.bpp		= 8,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 3,		.vsync_len	= 1,
	.left_margin	= 2,		.upper_margin	= 0,
	.right_margin	= 2,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_DPD | LCCR0_Pas,
	.lccr3		= LCCR3_ACBsDiv(512)
};

static void *jornada820_sa1100fb_get_mach_info(struct lcd_device *lm)
{
	return (void *)&jornada820_sa1100fb_mach_info;
}

struct lcd_device jornada820_sa1100fb_lcd_device = {
	.get_mach_info = jornada820_sa1100fb_get_mach_info,
//	.set_power = jornada820_sa1100fb_lcd_set_power,
//	.get_power = jornada820_sa1100fb_lcd_get_power
};
#endif

/* *********************************************************************** */
/* Initialize the Jornada 820.                                             */
/* *********************************************************************** */

static int __init jornada820_init(void)
{
  printk("In jornada820_init\n");

#if 0
  /* ------------------- */
  /* Initialize the SSP  */
  /* ------------------- */
  
  Ser4MCCR0 &= ~MCCR0_MCE;       /* disable MCP */
  PPAR |= PPAR_SSPGPIO;          /* assign alternate pins to SSP */
  GAFR |= (GPIO_GPIO10 | GPIO_GPIO11 | GPIO_GPIO12 | GPIO_GPIO13);
  GPDR |= (GPIO_GPIO10 | GPIO_GPIO12 | GPIO_GPIO13);
  GPDR &= ~GPIO_GPIO11;

  if (ssp_init()) printk("ssp_init() failed.\n");

    /* 8 bit, Motorola, enable, 460800 bit rate */
  Ser4SSCR0 = SSCR0_DataSize(8)+SSCR0_Motorola+SSCR0_SSE+SSCR0_SerClkDiv(8);
//  Ser4SSCR1 = SSCR1_RIE | SSCR1_SClkIactH | SSCR1_SClk1_2P;

  ssp_enable();

  Ser4MCCR0 |= MCCR0_MCE;       /* reenable MCP */
#endif

  /* Initialize the 1101. */
  GAFR |= GPIO_32_768kHz;
  GPDR |= GPIO_32_768kHz;

  TUCR = TUCR_3_6864MHz; /* */

  return platform_add_devices(devices, ARRAY_SIZE(devices));
}

//arch_initcall(jornada820_init);

static void __init jornada820_map_io(void)
{
  sa1100_map_io();
//  iotable_init(jornada820_io_desc, ARRAY_SIZE(jornada820_io_desc));
  
  /* rs232 out */
  sa1100_register_uart(0, 3);

  /* internal diag. */
  sa1100_register_uart(1, 1);
}

MACHINE_START(JORNADA820, "HP Jornada 820")
	.phys_io	= 0x80000000,
	.io_pg_offst	= ((0xf8000000) >> 18) & 0xfffc,
	.boot_params	= 0xc0000100,
	.map_io		= jornada820_map_io,
	.init_irq	= sa1100_init_irq,
	.timer		= &sa1100_timer,
	.init_machine 	= jornada820_init,
MACHINE_END
