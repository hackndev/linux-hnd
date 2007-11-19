/*
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
 * Copyright 2003, 2004, 2005 Phil Blundell
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
 * 2003-05-14	Joshua Wise        Adapted for the HP iPAQ H1900
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
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/mfd/asic2_base.h>
#include <linux/mfd/asic3_base.h>
#include <linux/mfd/tmio_mmc.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/ipaq-asic2.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch/h3900.h>
#include <asm/arch/h3900-gpio.h>
#include <asm/arch/h3900-init.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>
#include <asm/types.h>

#include "../generic.h"

void h3900_ll_pm_init(void);

#ifdef DeadCode

static void h3900_control_egpio( enum ipaq_egpio_type x, int setp )
{
	switch (x) {
	case IPAQ_EGPIO_IR_ON:
		CLEAR_ASIC3( GPIO3_IR_ON_N );
		break;
	case IPAQ_EGPIO_IR_FSEL:
		break;
	case IPAQ_EGPIO_RS232_ON:
		SET_ASIC3( GPIO3_RS232_ON );
		break;
	case IPAQ_EGPIO_BLUETOOTH_ON:
		SET_ASIC3( GPIO3_BT_PWR_ON );
		break;
	default:
		printk("%s: unhandled egpio=%d\n", __FUNCTION__, x);
	}
}

static void h3900_set_led (enum led_color color, int duty_time, int cycle_time)
{
	if (duty_time) {
		IPAQ_ASIC2_LED_TimeBase(color)   = LED_EN | LED_AUTOSTOP | LED_ALWAYS | 1;
		IPAQ_ASIC2_LED_PeriodTime(color) = cycle_time;
		IPAQ_ASIC2_LED_DutyTime(color)   = duty_time;
	} else {
		IPAQ_ASIC2_LED_TimeBase(color) = 0;
	}
}
#endif

#if 0
/*
 * Common map_io initialization
 */
static short ipaq_gpio_modes[] = {
	GPIO1_RTS_MD,
	GPIO18_RDY_MD,
	GPIO15_nCS_1_MD,
	GPIO33_nCS_5_MD,
	GPIO48_nPOE_MD,
	GPIO49_nPWE_MD,
	GPIO50_nPIOR_MD,
	GPIO51_nPIOW_MD,
	GPIO52_nPCE_1_MD,
	GPIO53_nPCE_2_MD,
	GPIO54_pSKTSEL_MD,
	GPIO55_nPREG_MD,
	GPIO56_nPWAIT_MD,
	GPIO57_nIOIS16_MD,
	GPIO78_nCS_2_MD,
	GPIO79_nCS_3_MD,
	GPIO80_nCS_4_MD,
};
#endif

static void __init h3900_map_io(void)
{
#if 0
        /* redundant? */
	int i;
#endif

	pxa_map_io();

	/* Configure power management stuff. */
	PWER = PWER_GPIO0 | PWER_RTC;
	PFER = PWER_GPIO0 | PWER_RTC;
	PRER = 0;
	PCFR = PCFR_OPDE;
	CKEN = CKEN6_FFUART;

	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

#if 0
	/* redundant? */
	for (i = 0; i < ARRAY_SIZE(ipaq_gpio_modes); i++) {
		int mode = ipaq_gpio_modes[i];
		if (0)
			printk("ipaq gpio_mode: gpio_nr=%d dir=%d fn=%d\n",
			       mode&GPIO_MD_MASK_NR, mode&GPIO_MD_MASK_DIR, mode&GPIO_MD_MASK_FN);
		pxa_gpio_mode(mode);
	}
#endif

	/* Set up GPIO direction and alternate function registers */
	GAFR0_L = GAFR0x_InitValue;
	GAFR0_U = GAFR1x_InitValue;
	GAFR1_L = GAFR0y_InitValue;
	GAFR1_U = GAFR1y_InitValue;
	GAFR2_L = GAFR0z_InitValue;
	GAFR2_U = GAFR1z_InitValue;
	
	GPDR0 = GPDRx_InitValue;
	GPDR1 = GPDRy_InitValue;
	GPDR2 = GPDRz_InitValue;

	GPCR0 = 0x0fffffff;       /* All outputs are set low by default */

	/* Add wakeup on AC plug/unplug */
	PWER  |= PWER_GPIO8;
	PFER  |= PWER_GPIO8;
	PRER  |= PWER_GPIO8;

	/* Select VLIO for ASIC3 */
	MSC2 = (MSC2 & 0x0000ffff) | 0x74a40000; 

	pxa_gpio_mode(GPIO33_nCS_5_MD);

#if 0	
	/* Turn off all LEDs */
	ipaq_set_led (GREEN_LED, 0, 0);
	ipaq_set_led (BLUE_LED, 0, 0);
	ipaq_set_led (YELLOW_LED, 0, 0);
#endif
}

extern struct platform_device h3900_bl;
static struct platform_device h3900_lcd       = { .name = "h3900-lcd", };

static struct platform_device *h3900_asic2_devices[] __initdata = {
        &h3900_lcd,
	&h3900_bl,
};

static struct asic2_platform_data h3900_asic2_platform_data = {
	.irq_base = H3900_ASIC2_IRQ_BASE,
        .child_devs     = h3900_asic2_devices,
        .num_child_devs = ARRAY_SIZE(h3900_asic2_devices),
};

static struct resource asic2_resources[] = {
	[0] = {
		.start	= H3900_ASIC2_PHYS,
		.end	= H3900_ASIC2_PHYS + 0x7fffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO_H3900_ASIC2_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device h3900_asic2 = {
	.name		= "asic2",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(asic2_resources),
	.resource	= asic2_resources,
        .dev = { .platform_data = &h3900_asic2_platform_data, },
};
EXPORT_SYMBOL(h3900_asic2);

/*
 *	SD/MMC Controller
 */

static unsigned long shared;
static int clock_enabled;

#define asic2 &h3900_asic2.dev

static void h3900_set_mmc_clock(struct platform_device *pdev, int status)
{
	switch (status) {
	case MMC_CLOCK_ENABLED:
		if (!clock_enabled) {
			/* Enable clock from asic2 */
			asic2_shared_add (asic2, &shared, ASIC_SHARED_CLOCK_EX1);
			asic2_clock_enable (asic2, ASIC2_CLOCK_SD_2, 1);
			mdelay (1);

			clock_enabled = 1;
			printk("h3900_mmc: clock enabled\n");
		} else {
			printk(KERN_ERR "h3900_mmc: clock was already enabled\n");
		}
		break;

	default:
		if (clock_enabled) {
			asic2_clock_enable (asic2, ASIC2_CLOCK_SD_2, 0);
			asic2_shared_release (asic2, &shared, ASIC_SHARED_CLOCK_EX1);
			clock_enabled = 0;
			printk("h3900_mmc: clock disabled\n");
		} else {
			printk(KERN_ERR "h3900_mmc: clock was already disabled\n");
		}
		break;
	}
}


static struct tmio_mmc_hwconfig h3900_mmc_hwconfig = {
	.set_mmc_clock = h3900_set_mmc_clock,
};

static struct asic3_platform_data asic3_platform_data = {
	.irq_base = H3900_ASIC3_IRQ_BASE,
	.gpio_a = {
		.dir = ASIC3GPIO_INIT_DIR,            /* All outputs */
	},
	.gpio_b = {
		.dir = ASIC3GPIO_INIT_DIR,            /* All outputs */
		.init = GPIO3_IR_ON_N | GPIO3_RS232_ON | GPIO3_TEST_POINT_123,
		.sleep_out = ASIC3GPIO_SLEEP_OUT,
		.batt_fault_out = ASIC3GPIO_BATFALT_OUT,
	},
	.gpio_c = {
		.dir = ASIC3GPIO_INIT_DIR,            /* All outputs */
	},
	.gpio_d = {
		.dir = ASIC3GPIO_INIT_DIR,            /* All outputs */
	},
	.tmio_mmc_hwconfig = &h3900_mmc_hwconfig,
};

static struct resource asic3_resources[] = {
	/* GPIO part */
	[0] = {
		.start	= H3900_ASIC3_PHYS,
		.end	= H3900_ASIC3_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		/* No IRQ, ASIC3 used only for output */
		.start  = -1,
		.flags  = IORESOURCE_IRQ,
	},
	/* SD part */
	[2] = {
		.start  = H3900_ASIC3_SD_PHYS,
		.end    = H3900_ASIC3_SD_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_H3900_MMC_INT_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device h3900_asic3 = {
	.name		= "asic3",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(asic3_resources),
	.resource	= asic3_resources,
	.dev = {
		.platform_data = &asic3_platform_data,
	},
};
EXPORT_SYMBOL(h3900_asic3);

static struct gpio_keys_button h3900_button_table[] = {
	{ KEY_POWER, GPIO_NR_H3900_POWER_BUTTON_N, 1 },
};

static struct gpio_keys_platform_data h3900_pxa_keys_data = {
	.buttons = h3900_button_table,
	.nbuttons = ARRAY_SIZE(h3900_button_table),
};

static struct platform_device h3900_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &h3900_pxa_keys_data,
	},
};

static struct platform_device *devices[] __initdata = {
	&h3900_asic2,
	&h3900_asic3,
	&h3900_pxa_keys,
};

static int h3900_udc_is_connected(void) 
{
	return 1; //FIXME - at least this could mimic the state set below...
}

static void h3900_udc_command (int cmd) 
{
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
		GPDR0 &= ~GPIO_H3900_USBP_PULLUP;
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		GPDR0 |= GPIO_H3900_USBP_PULLUP;
		GPSR0 |= GPIO_H3900_USBP_PULLUP;
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static struct pxa2xx_udc_mach_info h3900_udc_mach_info = {
	.udc_is_connected = h3900_udc_is_connected,
	.udc_command      = h3900_udc_command,
};

/* read LCCR0: 0x00000081 */
/* read LCCR1: 0x0b10093f -- BLW=0x0b, ELW=0x10, HSW=2, PPL=0x13f */
/* read LCCR2: 0x051008ef -- BFW=0x05, EFW=0x10, VSW=2, LPP=0xef */
/* read LCCR3: 0x0430000a */

static struct pxafb_mode_info h3900_lcd_modes[] = {
{
	.pixclock =	221039,	/* clock period in ps */
	.bpp =		16,
	.xres =		320,
	.yres =		240,
	.hsync_len =	3,
	.vsync_len =	3,
	.left_margin =	12,
	.upper_margin =	4,
	.right_margin =	17,
	.lower_margin =	17,
	.sync =		0, /* both horiz and vert active low sync */
}
};

static struct pxafb_mach_info h3900_fb_info = {
	.modes		= h3900_lcd_modes,
	.num_modes	= ARRAY_SIZE(h3900_lcd_modes),

	.lccr0 =	(LCCR0_PAS),
};

static void __init h3900_init (void)
{
	platform_add_devices (devices, ARRAY_SIZE (devices));
	pxa_set_udc_info (&h3900_udc_mach_info);
	set_pxa_fb_info (&h3900_fb_info);
#ifdef CONFIG_PM
        h3900_ll_pm_init();
#endif
}

MACHINE_START(H3900, "HP iPAQ H3900")
	/* Maintainer HP Labs, Cambridge Research Labs */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= h3900_map_io,
	.init_irq	= pxa_init_irq,
        .timer = &pxa_timer,
        .init_machine = h3900_init,
MACHINE_END
