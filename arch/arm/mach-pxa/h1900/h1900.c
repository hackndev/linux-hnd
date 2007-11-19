/*
 * Platform code for the iPAQ h1910/h1915
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 * Copyright (C) 2000-2003 Hewlett-Packard Company.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * History (deprecated):
 *
 * 2003-08-28   Joshua Wise        Ported to kernel 2.6
 * 2003-05-14   Joshua Wise        Adapted for the HP iPAQ H1900
 * 2002-08-23   Jamey Hicks        Adapted for use with PXA250-based iPAQs
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 *                                 and abstracted EGPIO interface.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/mfd/asic3_base.h>
#include <linux/platform_device.h>
#include <linux/apm-emulation.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <asm/arch/pxa-regs.h>
#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>
#include <asm/arch/h1910.h>
#include <asm/arch/h1900-asic.h>
#include <asm/arch/h1900-gpio.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/irda.h>

#include <asm/arch/irq.h>
#include <asm/types.h>

#include "../generic.h"

extern struct platform_device h1900_bl;
static struct platform_device h1900_lcd = { .name = "h1900-lcd", };
static struct platform_device h1900_buttons = { .name = "h1900-buttons", };
static struct platform_device h1900_ssp = { .name = "h1900-ssp", };
static struct platform_device h1900_power = { .name = "h1900-power", };
static struct platform_device h1900_irda = { .name = "h1900-irda", };
static struct platform_device h1900_udc = { .name = "h1900-udc", };

extern struct platform_device h1900_asic3;

static struct platform_device *child_devices[] __initdata = {
	&h1900_lcd,
	&h1900_bl,
	&h1900_buttons,
	&h1900_power,
	&h1900_udc,
};

static struct asic3_platform_data h1900_asic3_platform_data = {
	.gpio_a = {
		.dir		= 0xffff,
		.init		= 0x0000,
		.sleep_mask	= 0xffff,
		.sleep_out	= 0x0000,
		.batt_fault_out	= 0x0000,
		.alt_function	= 0x0000,
		.sleep_conf	= 0x000c,
	},
	.gpio_b = {
		.dir		= 0xffff,
		.init		= 0x0000,
		.sleep_mask	= 0xffff,
		.sleep_out	= 0x0000,
		.batt_fault_out	= 0x0000,
		.alt_function	= 0x0000,
		.sleep_conf	= 0x000c,
	},
	.gpio_c = {
		.dir		= 0xffff,
		.init		= 0x0170,
		.sleep_mask	= 0xffff,
		.sleep_out	= 0x0000,
		.batt_fault_out	= 0x0000,
		.alt_function	= 0x0003,
		.sleep_conf	= 0x000c,
	},
	.gpio_d = {
		.dir		= 0xff83,
		.init		= 0x0000,
		.sleep_mask	= 0xff83,
		.sleep_out	= 0x0000,
		.batt_fault_out	= 0x0100,
		.alt_function	= 0x0000,
		.sleep_conf	= 0x000c,
	},
	.irq_base = H1910_ASIC3_IRQ_BASE,

	.child_devs = child_devices,
	.num_child_devs = ARRAY_SIZE(child_devices),
};

static struct platform_device *core_devices[] __initdata = {
	&h1900_asic3,
	&h1900_ssp,
	&h1900_irda,
};

static struct resource h1900_asic3_resources[] = {
	[0] = {
		.start	= 0x0c000000,
		.end	= 0x0c000000 + IPAQ_ASIC3_MAP_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIO(GPIO_NR_H1900_ASIC_IRQ_1_N),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start  = 0x10000000,
		.end    = 0x10000000 + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_H1900_SD_IRQ_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device h1900_asic3 = {
	.name		= "asic3",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(h1900_asic3_resources),
	.resource	= h1900_asic3_resources,
	.dev = { .platform_data = &h1900_asic3_platform_data, },
};
EXPORT_SYMBOL(h1900_asic3);

#define asic3 &h1900_asic3.dev

static void h1900_init_pxa_gpio(void)
{
	PGSR0 = 0x01020000;
	PGSR1 = 0x00420102;
	PGSR2 = 0x0001C000;

	GPSR0 = 0x01030000;
	GPSR1 = 0x00430106;
	GPSR2 = 0x0001C000;

	GPCR0 = 0xD2D09000;
	GPCR1 = 0xFC3484F9;
	GPCR2 = 0x00003FFF;

	GAFR0_L = 0x00000000;
	GAFR0_U = 0x5A1A8012;
	GAFR1_L = 0x60000009;
	GAFR1_U = 0xAAA10008;
	GAFR2_L = 0xAAAAAAAA;
	GAFR2_U = 0x00000002;

	GRER0 = 0x00000000;
	GRER1 = 0x00000000;
	GRER2 = 0x00000000;
	GFER0 = 0x00000000;
	GFER1 = 0x00000000;
	GFER2 = 0x00000000;
	GEDR0 = 0x00000000;
	GEDR1 = 0x00000000;
	GEDR2 = 0x00000000;

	GPDR0 = 0xD3D39000;
	GPDR1 = 0xFC7785FF;
	GPDR2 = 0x0001FFFF;

	PWER = PWER_RTC | PWER_GPIO0 | PWER_GPIO3 | PWER_GPIO4;
	PFER = PWER_GPIO0 | PWER_GPIO4;
	PRER = PWER_GPIO4;
	PEDR = 0;
	PCFR = PCFR_OPDE;
}

void h1900_set_led(int color, int duty_time, int cycle_time)
{
	if (color == H1900_RED_LED) {
		asic3_set_led(asic3, 0, duty_time, cycle_time, 6);
		asic3_set_led(asic3, 1, 0, cycle_time, 6);
	}

	if (color == H1900_GREEN_LED) {
		asic3_set_led(asic3, 1, duty_time, cycle_time, 6);
		asic3_set_led(asic3, 0, 0, cycle_time, 6);
	}

	if (color == H1900_YELLOW_LED) {
		asic3_set_led(asic3, 1, duty_time, cycle_time, 6);
		asic3_set_led(asic3, 0, duty_time, cycle_time, 6);
	}
}

static void __init h1900_init(void)
{
	h1900_init_pxa_gpio();

	platform_add_devices(core_devices, ARRAY_SIZE(core_devices));
}

MACHINE_START(H1900, "HP iPAQ H1910")
	/* Maintainer: Pawel Kolodziejski */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= pxa_map_io,
	.init_irq       = pxa_init_irq,
	.timer          = &pxa_timer,
	.init_machine   = h1900_init,
MACHINE_END
