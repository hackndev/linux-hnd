/*
 * linux/arch/arm/mach-pxa/htcbeetles/htcbeetles.c
 *
 * Support for the HTC Beetles (aka ipaq hw651x).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>

#include <asm/arch/htcbeetles.h>
#include <asm/arch/htcbeetles-gpio.h>
#include <asm/arch/htcbeetles-asic.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#include "../generic.h"

static struct pxafb_mode_info htcbeetles_lcd_modes[] = {
{
	.pixclock		= 480769,
	.xres			= 240,
	.yres			= 240,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 2,
	.left_margin		= 12,
	.right_margin		= 8,
	.upper_margin		= 3,
	.lower_margin		= 3,

//	.sync			= FB_SYNC_HOR_LOW_ACT|FB_SYNC_VERT_LOW_ACT,
},
};

static struct pxafb_mach_info htcbeetles_lcd = {
        .modes		= htcbeetles_lcd_modes,
        .num_modes	= ARRAY_SIZE(htcbeetles_lcd_modes), 

	/* fixme: this is a hack, use constants instead. */
	.lccr0			= 0x042000b1,
	.lccr3			= 0x04700019,
//	.lccr4			= 0x80000000,

};
#if 0
static struct tsc2046_mach_info htcbeetles_ts_platform_data = {
       .port     = 2,
       .clock    = CKEN3_SSP2,
       .pwrbit_X = 3,
       .pwrbit_Y = 3,
       .irq	 = HTCBEETLES_IRQ(TOUCHPANEL_IRQ_N)
};

static struct platform_device htcbeetles_ts        = {
       .name = "tsc2046_ts",
       .dev  = {
              .platform_data = &htcbeetles_ts_platform_data,
       },
};
#endif

static struct platform_device htcbeetles_udc       = { .name = "htcbeetles_udc", };

static struct platform_device *htcbeetles_asic3_devices[] __initdata = {
//	&htcbeetles_ts,
//	&htcbeetles_lcd,
	&htcbeetles_udc,
};

static struct asic3_platform_data htcbeetles_asic3_platform_data = {

   /*
    * These registers are configured as they are on Wince.
    */
        .gpio_a = {
		.dir            = 0xbfff, 
		.init           = 0xc0a5,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x6000, //
		.sleep_conf     = 0x000c,
        },
        .gpio_b = {
		.dir            = 0xe008,
		.init           = 0xd347,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000, //
                .sleep_conf     = 0x000c,
        },
        .gpio_c = {
                .dir            = 0xfff7,
                .init           = 0xb640,
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
		.alt_function   = 0x003b, // GPIOC_LED_RED | GPIOC_LED_GREEN | GPIOC_LED_BLUE
                .sleep_conf     = 0x000c,
        },
        .gpio_d = {
		.dir            = 0xffff,
		.init           = 0x2330,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000, //
		.sleep_conf     = 0x0008,
        },
	.bus_shift = 1,
	.irq_base = HTCBEETLES_ASIC3_IRQ_BASE,

	.child_devs     = htcbeetles_asic3_devices,
	.num_child_devs = ARRAY_SIZE(htcbeetles_asic3_devices),
};

static struct resource htcbeetles_asic3_resources[] = {
	[0] = {
		.start	= HTCBEETLES_ASIC3_GPIO_PHYS,
		.end	= HTCBEETLES_ASIC3_GPIO_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= HTCBEETLES_IRQ(ASIC3_EXT_INT),
		.end	= HTCBEETLES_IRQ(ASIC3_EXT_INT),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start  = HTCBEETLES_ASIC3_MMC_PHYS,
		.end    = HTCBEETLES_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = HTCBEETLES_IRQ(ASIC3_SDIO_INT_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device htcbeetles_asic3 = {
	.name           = "asic3",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(htcbeetles_asic3_resources),
	.resource       = htcbeetles_asic3_resources,
	.dev = { .platform_data = &htcbeetles_asic3_platform_data, },
};
EXPORT_SYMBOL(htcbeetles_asic3);

static struct platform_device *devices[] __initdata = {
	&htcbeetles_asic3,
};

static void __init htcbeetles_init(void)
{
	set_pxa_fb_info( &htcbeetles_lcd );

	platform_device_register(&htcbeetles_asic3);
//	platform_add_devices( devices, ARRAY_SIZE(devices) );
}

MACHINE_START(HTCBEETLES, "HTC Beetles")
	.phys_io	= 0x40000000,
	.io_pg_offst    = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io 	= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcbeetles_init,
MACHINE_END

