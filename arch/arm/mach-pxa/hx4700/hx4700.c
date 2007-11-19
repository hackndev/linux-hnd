/*
 * 
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 * Copyright 2005 SDG Systems, LLC
 *
 * Based on code:
 *    Copyright 2004 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * History:
 *
 * 2004-11-2004	Michael Opdenacker	Preliminary version
 * 2004-12-16   Todd Blumer
 * 2004-12-22   Michael Opdenacker	Added USB management
 * 2005-01-30   Michael Opdenacker	Improved Asic3 settings and initialization
 * 2005-06	Todd Blumer		Added serial functions
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>
#include <linux/dpm.h>
#include <linux/rs232_serial.h>
#include <asm/gpio.h>
#include <asm/arch/pxa2xx_udc_gpio.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/serial.h>
#include <asm/arch/hx4700.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>
#include <asm/arch/hx4700-core.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/asic3_keys.h>
#include <asm/hardware/asic3_leds.h>
#include <asm/arch/udc.h>
#include <asm/arch/audio.h>
#include <asm/arch/irda.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include <linux/ads7846.h>
#include <linux/touchscreen-adc.h>
#include <linux/adc_battery.h>

#include "../generic.h"

DEFINE_LED_TRIGGER_SHARED_GLOBAL(hx4700_radio_trig);
EXPORT_LED_TRIGGER_SHARED(hx4700_radio_trig);

/* Physical address space information */

/* TI WLAN, EGPIO, External UART */
#define HX4700_EGPIO_WLAN_PHYS	PXA_CS5_PHYS

/*
 * Bluetooth - Relies on other loadable modules, like ASIC3 and Core,
 * so make the calls indirectly through pointers. Requires that the
 * hx4700 bluetooth module be loaded before any attempt to use
 * bluetooth (obviously).
 */

static struct hx4700_bt_funcs bt_funcs;

static void
hx4700_bt_configure( int state )
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure( state );
}

static struct platform_pxa_serial_funcs hx4700_pxa_bt_funcs = {
	.configure = hx4700_bt_configure,
};


/*
 * IRDA
 */

/* for pxaficp_ir */
static void hx4700_irda_transceiver_mode(struct device *dev, int mode)
{
	unsigned long flags;

	local_irq_save(flags);
	if (mode & IR_OFF)
		SET_HX4700_GPIO_N(IR_ON, 0);
	else
		SET_HX4700_GPIO_N(IR_ON, 1);

	local_irq_restore(flags);
}

static struct pxaficp_platform_data hx4700_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = hx4700_irda_transceiver_mode,
};


/* Uncomment the following line to get serial console via SIR work from
 * the very early booting stage. This is not useful for end-user.
 */
/* #define EARLY_SIR_CONSOLE */

#define IR_TRANSCEIVER_ON \
	SET_HX4700_GPIO_N(IR_ON, 1)

#define IR_TRANSCEIVER_OFF \
	SET_HX4700_GPIO_N(IR_ON, 0)


static void
hx4700_irda_configure(int state)
{
	/* Switch STUART RX/TX pins to SIR */
	pxa_gpio_mode(GPIO_NR_HX4700_STD_RXD_MD);
	pxa_gpio_mode(GPIO_NR_HX4700_STD_TXD_MD);

	/* make sure FIR ICP is off */
	ICCR0 = 0;

	switch (state) {

	case PXA_UART_CFG_POST_STARTUP:
		/* configure STUART for SIR */
		STISR = STISR_XMODE | STISR_RCVEIR | STISR_RXPL;
		IR_TRANSCEIVER_ON;
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		STISR = 0;
		IR_TRANSCEIVER_OFF;
		break;
	}
}

static void
hx4700_irda_set_txrx(int txrx)
{
	unsigned old_stisr = STISR;
	unsigned new_stisr = old_stisr;

	if (txrx & PXA_SERIAL_TX) {
		/* Ignore RX if TX is set */
		txrx &= PXA_SERIAL_TX;
		new_stisr |= STISR_XMITIR;
	} else
		new_stisr &= ~STISR_XMITIR;

	if (txrx & PXA_SERIAL_RX)
		new_stisr |= STISR_RCVEIR;
	else
		new_stisr &= ~STISR_RCVEIR;

	if (new_stisr != old_stisr) {
		while (!(STLSR & LSR_TEMT))
			;
		IR_TRANSCEIVER_OFF;
		STISR = new_stisr;
		IR_TRANSCEIVER_ON;
	}
}

static int
hx4700_irda_get_txrx (void)
{
	return ((STISR & STISR_XMITIR) ? PXA_SERIAL_TX : 0) |
	       ((STISR & STISR_RCVEIR) ? PXA_SERIAL_RX : 0);
}
 

static struct platform_pxa_serial_funcs hx4700_pxa_irda_funcs = {
	.configure = hx4700_irda_configure,
	.set_txrx  = hx4700_irda_set_txrx,
	.get_txrx  = hx4700_irda_get_txrx,
};

/* LEDs */
static struct asic3_led hx4700_leds[] = {
	{
		.led_cdev  = {
			.name	         = "hx4700:amber",
			.default_trigger = "ds2760-battery.0-charging",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,

	},
	{
		.led_cdev  = {
			.name	         = "hx4700:green",
			.default_trigger = "ds2760-battery.0-full",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
	},
	{
		.led_cdev  = {
			.name	         = "hx4700:blue",
			.default_trigger = "hx4700-radio",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 2,
	},
};

static struct asic3_leds_machinfo hx4700_leds_machinfo = {
	.num_leds = ARRAY_SIZE(hx4700_leds),
	.leds = hx4700_leds,
	.asic3_pdev = &hx4700_asic3,
};

static struct platform_device hx4700_leds_pdev = {
	.name = "asic3-leds",
	.dev = {
		.platform_data = &hx4700_leds_machinfo,
	},
};


/* Initialization code */

static void __init hx4700_map_io(void)
{
	pxa_map_io();
#if 0
	iotable_init( hx4700_io_desc, ARRAY_SIZE(hx4700_io_desc) );
#endif
	pxa_set_stuart_info(&hx4700_pxa_irda_funcs);
#ifdef EARLY_SIR_CONSOLE
	hx4700_irda_configure(NULL, 1);
	hx4700_irda_set_txrx(NULL, PXA_SERIAL_TX);
#endif
	pxa_set_btuart_info(&hx4700_pxa_bt_funcs);
}

static void __init hx4700_init_irq(void)
{
	/* int irq; */

	pxa_init_irq();

#if 0
	/* setup extra irqs */
	for(irq = HX4700_IRQ(0); irq <= HX4700_IRQ(15); irq++) {
		set_irq_chip(irq, &hx4700_irq_chip);
		set_irq_handler(irq, do_level_IRQ);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	set_irq_flags(HX4700_IRQ(8), 0);
	set_irq_flags(HX4700_IRQ(12), 0);

	MST_INTMSKENA = 0;
	MST_INTSETCLR = 0;

	set_irq_chained_handler(IRQ_GPIO(0), hx4700_irq_handler);
	set_irq_type(IRQ_GPIO(0), IRQT_FALLING);
#endif
}

/* ASIC3 */

static struct platform_device hx4700_gpio_keys;

struct pxa2xx_udc_gpio_info hx4700_udc_info = {
	.detect_gpio = {&hx4700_asic3.dev, ASIC3_GPIOD_IRQ_BASE + GPIOD_USBC_DETECT_N},
	.detect_gpio_negative = 1,
	.power_ctrl = {
		.power_gpio = {&pxagpio_device.dev, GPIO_NR_HX4700_USB_PUEN},
	},

};

static struct platform_device hx4700_udc = { 
	.name = "pxa2xx-udc-gpio",
	.dev = {
		.platform_data = &hx4700_udc_info
	}
};


static struct rs232_serial_pdata hx4700_rs232_data = {
	.detect_gpio = {&hx4700_asic3.dev, ASIC3_GPIOD_IRQ_BASE + GPIOD_COM_DCD},
	.power_ctrl = {	
		.power_gpio = {&pxagpio_device.dev, GPIO_NR_HX4700_RS232_ON},
	},
};
static struct platform_device hx4700_serial = { 
	.name	= "rs232-serial",
	.dev	= {
		.platform_data = &hx4700_rs232_data
	},
};

static struct platform_device *hx4700_asic3_devices[] __initdata = {
	&hx4700_serial,
	&hx4700_udc,
	&hx4700_gpio_keys,
	&hx4700_leds_pdev,
};

static struct asic3_platform_data hx4700_asic3_platform_data = {

   /* Setting ASIC3 GPIO registers to the below initialization states
    * hx4700 asic3 information: http://handhelds.org/moin/moin.cgi/HpIpaqHx4700Hardware
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
	  	.sleep_mask     = 0xffff,
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
	  	.sleep_mask     = 0xffff,
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
	  	.sleep_mask     = 0xffff,
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
	  	.sleep_mask     = 0x9800,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
	//	.int_status     = 0x0000,
		.alt_function   = 0x9800,
		.sleep_conf     = 0x000c,
        },
	.bus_shift = 1,
	.irq_base = HX4700_ASIC3_IRQ_BASE,
	.gpio_base = HX4700_ASIC3_GPIO_BASE,

	.child_devs	 = hx4700_asic3_devices,
	.num_child_devs = ARRAY_SIZE(hx4700_asic3_devices),
};

static struct resource asic3_resources[] = {
        /* GPIO part */
	[0] = {
		.start	= HX4700_ASIC3_PHYS,
		.end	= HX4700_ASIC3_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= HX4700_IRQ(ASIC3_EXT_INT),
		.end	= HX4700_IRQ(ASIC3_EXT_INT),
		.flags	= IORESOURCE_IRQ,
	},
        /* SD part */
	[2] = {
		.start	= HX4700_ASIC3_SD_PHYS,
		.end	= HX4700_ASIC3_SD_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= HX4700_IRQ(ASIC3_SDIO_INT_N),
		.end	= HX4700_IRQ(ASIC3_SDIO_INT_N),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device hx4700_asic3 = {
	.name		= "asic3",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(asic3_resources),
	.resource	= asic3_resources,
	.dev = {
		.platform_data = &hx4700_asic3_platform_data,
	},
};
EXPORT_SYMBOL(hx4700_asic3);

/* Core Hardware Functions */

static struct hx4700_core_funcs core_funcs;

struct platform_device hx4700_core = {
	.name		= "hx4700-core",
	.id		= -1,
	.dev = {
		.platform_data = &core_funcs,
	},
};

/* Backup battery */

static struct battery_adc_platform_data hx4700_backup_batt_params = {
	.battery_info = {
		.name = "backup-battery",
		.voltage_max_design = 1400000,
		.voltage_min_design = 1000000,
		.charge_full_design = 100000,
	},
	.voltage_pin = "ads7846-ssp:vaux",
};

static struct platform_device hx4700_backup_batt = { 
	.name = "adc-battery",
	.id = -1,
	.dev = {
		.platform_data = &hx4700_backup_batt_params,
	}
};

/* PXA2xx Keys */

static struct gpio_keys_button hx4700_gpio_buttons[] = {
	{ KEY_POWER,	 GPIO_NR_HX4700_KEY_ON_N, 1, "Power button" },
	{ _KEY_MAIL,	 GPIO_NR_HX4700_KEY_AP3,  0, "Mail button" },
	{ _KEY_CONTACTS, GPIO_NR_HX4700_KEY_AP1,  0, "Contacts button" },
	{ _KEY_RECORD,   HX4700_ASIC3_GPIO_BASE+ASIC3_GPIOD_IRQ_BASE+GPIOD_AUD_RECORD_N, 1, "Record button" },
	{ _KEY_CALENDAR, HX4700_ASIC3_GPIO_BASE+ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_AP2_N, 1, "Calendar button" },
	{ _KEY_HOMEPAGE, HX4700_ASIC3_GPIO_BASE+ASIC3_GPIOD_IRQ_BASE+GPIOD_KEY_AP4_N, 1, "Home button" },
};

static struct gpio_keys_platform_data hx4700_gpio_keys_data = {
	.buttons = hx4700_gpio_buttons,
	.nbuttons = ARRAY_SIZE(hx4700_gpio_buttons),
};

static struct platform_device hx4700_gpio_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &hx4700_gpio_keys_data,
	},
};

/* LCD */

static struct platform_device hx4700_lcd = {
	.name = "hx4700-lcd",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

/* NavPoint */

static struct platform_device hx4700_navpt = {
	.name = "hx4700-navpoint",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

/* Backlight */
extern struct platform_device hx4700_bl;

/* Bluetooth */

static struct platform_device hx4700_bt = {
	.name = "hx4700-bt",
	.id = -1,
	.dev = {
		.platform_data = &bt_funcs,
	},
};

static struct platform_device hx4700_wlan = {
    .name = "hx4700-wlan",
    .id = -1,
};

static struct platform_device hx4700_flash = {
    .name = "hx4700-flash",
    .id = -1,
};

struct ads7846_ssp_platform_data hx4700_ssp_params = {
	.port = 2,
	.pd_bits = 1,
	.freq = 720000,
};
static struct platform_device ads7846_ssp = { 
	.name = "ads7846-ssp", 
	.id = -1,
	.dev = {
		.platform_data = &hx4700_ssp_params,
	}
};

struct tsadc_platform_data hx4700_ts_params = {
	.pen_gpio = GPIO_NR_HX4700_TOUCHPANEL_IRQ_N,
	.x_pin = "ads7846-ssp:x",
	.y_pin = "ads7846-ssp:y",
	.z1_pin = "ads7846-ssp:z1",
	.z2_pin = "ads7846-ssp:z2",
	.pressure_factor = 100000,
	.min_pressure = 2,
	.max_jitter = 8,
};
static struct resource hx4700_pen_irq = {
	.start = IRQ_GPIO(GPIO_NR_HX4700_TOUCHPANEL_IRQ_N),
	.end = IRQ_GPIO(GPIO_NR_HX4700_TOUCHPANEL_IRQ_N),
	.flags = IORESOURCE_IRQ,
};
static struct platform_device ads7846_ts = {
	.name = "ts-adc",
	.id = -1,
	.resource = &hx4700_pen_irq,
	.num_resources = 1,
	.dev = {
		.platform_data = &hx4700_ts_params,
	}
};

static struct platform_device *devices[] __initdata = {
	&ads7846_ssp,
	&ads7846_ts,
	&hx4700_asic3,
	&hx4700_core,
	&hx4700_backup_batt,
	&hx4700_lcd,
	&hx4700_navpt,
	&hx4700_bl,
	&hx4700_bt,
	&hx4700_wlan,
	&hx4700_flash,
};

static void __init hx4700_init( void )
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

	SET_HX4700_GPIO( ASIC3_RESET_N, 0 );
	mdelay(10);
	SET_HX4700_GPIO( ASIC3_RESET_N, 1 );
	mdelay(10);
	SET_HX4700_GPIO( EUART_RESET, 1 );

	/* configure serial */
	pxa_gpio_mode( GPIO_NR_HX4700_COM_RXD_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_CTS_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_DCD_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_DSR_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_RING_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_TXD_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_DTR_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_COM_RTS_MD );

	/* Enable RS232 in case we'd have boot console there. In case 
	   cable is not actually attached, it will be turned off as 
	   soon as hx4700_serial is initialized. */
 	SET_HX4700_GPIO(RS232_ON, 1);

	pxa_gpio_mode( GPIO_NR_HX4700_I2C_SCL_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_I2C_SDA_MD );

	platform_add_devices( devices, ARRAY_SIZE(devices) );
	pxa_set_ficp_info(&hx4700_ficp_platform_data);

	led_trigger_register_shared("hx4700-radio", &hx4700_radio_trig);
}


MACHINE_START(H4700, "HP iPAQ HX4700")
        /* Maintainer SDG Systems, HP Labs, Cambridge Research Labs */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
        .boot_params	= 0xa0000100,
        .map_io		= hx4700_map_io,
        .init_irq	= hx4700_init_irq,
        .timer = &pxa_timer,
        .init_machine = hx4700_init,
MACHINE_END

/* vim600: set sw=8 noexpandtab : */
