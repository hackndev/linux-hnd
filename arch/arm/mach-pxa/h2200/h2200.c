/*
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2000-2004 Hewlett-Packard Company.
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
 * See http://cvs.handhelds.org/cgi-bin/viewcvs.cgi/linux/kernel26/arch/arm/mach-pxa/h2200.c
 * for the history of changes.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/gpio_keys.h>

#include <asm/types.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>
#include <asm/arch/irda.h>
#include <asm/arch/udc.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/mfd/hamcop_base.h>
#include <asm/hardware/ipaq-hamcop.h>
#include <asm/hardware/hamcop_leds.h>
#include <asm/arch/serial.h>
#include <asm/arch/h2200.h>
#include <asm/arch/h2200-gpio.h>
#include <asm/arch/h2200-irqs.h>
#include <asm/arch/h2200-init.h>
#include <linux/pda_power.h>

#include "../generic.h"


/***************************************************************************/
/* LED hook								   */
/***************************************************************************/

void (*h2200_led_hook) (struct device *dev, int led_num, int duty_time,
			int cycle_time);
EXPORT_SYMBOL(h2200_led_hook);

void
h2200_set_led (int led_num, int duty_time, int cycle_time)
{
	if (h2200_led_hook)
	    h2200_led_hook(&h2200_hamcop.dev, led_num, duty_time, cycle_time);
}
EXPORT_SYMBOL(h2200_set_led);

/***************************************************************************/
/* LED data								   */
/***************************************************************************/

static struct hamcop_led h2200_leds[] = {
	{
		.led_cdev = {
			.name = "h2200:power-amber",
			.default_trigger = "ds2760-battery.0-charging",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,
	},
	{
		.led_cdev = {
			.name = "h2200:notify-green",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
	},
	{
		.led_cdev = {
			.name = "h2200:bluetooth-blue",
			.default_trigger = "h2200-bluetooth",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 2,
	},
};

static struct hamcop_leds_machinfo h2200_leds_info = {
	.leds = h2200_leds,
	.num_leds = 3,
	.hamcop_pdev = &h2200_hamcop,
};

void h2200_leds_release(struct device *dev)
{
}

static struct platform_device h2200_leds_pdev = {
	.name = "hamcop-leds",
	.dev = {
		.platform_data = &h2200_leds_info,
		.release = h2200_leds_release,
	},
};


/***************************************************************************/
/*      IRDA								   */
/***************************************************************************/

/* XXX What are CIR_RESET and CIR_POWER_ON for? */
static void h2200_irda_transceiver_mode(struct device *dev, int mode)
{
	unsigned long flags;

	local_irq_save(flags);

	if (mode & IR_OFF)
		SET_H2200_GPIO_N(IR_ON, 0);
	else
		SET_H2200_GPIO_N(IR_ON, 1);

	local_irq_restore(flags);
}


static struct pxaficp_platform_data h2200_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = h2200_irda_transceiver_mode,
};

/* Uncomment the following line to get serial console via SIR work from
 * the very early booting stage. This is not useful for end-user.
 */
// #define EARLY_SIR_CONSOLE

#define IR_TRANSCEIVER_ON \
	SET_H2200_GPIO_N(IR_ON, 1);

#define IR_TRANSCEIVER_OFF \
	SET_H2200_GPIO_N(IR_ON, 0);


static void
h2200_irda_configure(int state)
{
	/* Switch STUART RX/TX pins to SIR */
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
	/* make sure FIR ICP is off */
	ICCR0 = 0;

	switch (state) {

	case PXA_UART_CFG_POST_STARTUP: /* post UART enable */
		/* configure STUART to for SIR */
		STISR = STISR_XMODE | STISR_RCVEIR | STISR_RXPL;
		IR_TRANSCEIVER_ON;
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* UART disabled */
		STISR = 0;
		IR_TRANSCEIVER_OFF;
		break;

	default:
		break;
	}
}

static void
h2200_irda_set_txrx(int txrx)
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
		while (!(STLSR & LSR_TEMT)) ;
		IR_TRANSCEIVER_OFF;
		STISR = new_stisr;
		IR_TRANSCEIVER_ON;
	}
}

static int
h2200_irda_get_txrx(void)
{
	return ((STISR & STISR_XMITIR) ? PXA_SERIAL_TX : 0) |
	       ((STISR & STISR_RCVEIR) ? PXA_SERIAL_RX : 0);
}

static struct platform_pxa_serial_funcs h2200_irda_funcs = {
	.configure = h2200_irda_configure,
	.set_txrx  = h2200_irda_set_txrx,
	.get_txrx  = h2200_irda_get_txrx,
};


/***************************************************************************/
/*      Bluetooth							   */
/***************************************************************************/

static void h2200_bluetooth_power(int on)
{
	if (on) {
		/* Power-up and reset the Zeevo. */
		SET_H2200_GPIO(BT_POWER_ON, 0); /* Make sure it's off. */
		SET_H2200_GPIO(BT_RESET_N, 1);  /* Deassert reset. */
		mdelay(5);
		SET_H2200_GPIO(BT_POWER_ON, 1);	/* Power on. */

		/* XXX This seems too long, but anything shorter makes
		 * hcid not work. */
		msleep(2000);
		h2200_set_led(2, 1, 31);
	} else {
		/* Turn off the Zeevo. */
		/* XXX Assert reset? */
		SET_H2200_GPIO(BT_POWER_ON, 0);
		h2200_set_led(2, 0, 0);
	}
}

static void
h2200_btuart_configure(int state)
{
	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP: /* pre UART enable */
		pxa_gpio_mode(GPIO42_BTRXD_MD);
		pxa_gpio_mode(GPIO43_BTTXD_MD);
		pxa_gpio_mode(GPIO44_BTCTS_MD);
		pxa_gpio_mode(GPIO45_BTRTS_MD);
		h2200_bluetooth_power(1);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* post UART disable */
		h2200_bluetooth_power(0);
		break;
		
	default:
		break;
	}
}

static void
h2200_hwuart_configure(int state)
{
	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP: /* post UART enable */
		pxa_gpio_mode(GPIO42_HWRXD_MD);
		pxa_gpio_mode(GPIO43_HWTXD_MD);
		pxa_gpio_mode(GPIO44_HWCTS_MD);
		pxa_gpio_mode(GPIO45_HWRTS_MD);
		h2200_bluetooth_power(1);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* post UART disable */
		h2200_bluetooth_power(0);
		break;

	default:
		break;
	}
}


static struct platform_pxa_serial_funcs h2200_btuart_funcs = {
	.configure = h2200_btuart_configure,
};

static struct platform_pxa_serial_funcs h2200_hwuart_funcs = {
	.configure = h2200_hwuart_configure,
};

/* Battery door (lacks appropriate driver), taken from h2200_battery. */

#if 0
static irqreturn_t h2200_battery_door_isr (int isr, void *data)
{
	int door_open = GET_H2200_GPIO(BATT_DOOR_N) ? 0 : 1;

	if (door_open)
		printk(KERN_ERR "battery door opened!\n");
	else
		printk(KERN_ERR "battery door closed\n");

	return IRQ_HANDLED;
}

	/* Install an interrupt handler for battery door open/close. */
	set_irq_type(H2200_IRQ(BATT_DOOR_N), IRQT_BOTHEDGE);
	request_irq(H2200_IRQ(BATT_DOOR_N), h2200_battery_door_isr,
		    SA_SAMPLE_RANDOM, "battery door", NULL);

#endif

/* Power */

static int h2200_is_ac_online(void)
{
	return !GET_H2200_GPIO(AC_IN_N);
}

static void h2200_set_charge(int flags)
{
	SET_H2200_GPIO(CHG_EN, !!flags);
	return;
}

static char *h2200_supplicants[] = {
	"ds2760-battery.0"
};

static struct pda_power_pdata h2200_power_pdata = {
	.is_ac_online = h2200_is_ac_online,
	.set_charge = h2200_set_charge,
	.supplied_to = h2200_supplicants,
	.num_supplicants = ARRAY_SIZE(h2200_supplicants),
};

static struct resource h2200_power_resourses[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
		.start = H2200_IRQ(AC_IN_N),
		.end = H2200_IRQ(AC_IN_N),
	},
};

static struct platform_device h2200_power = {
	.name = "pda-power",
	.id = -1,
	.resource = h2200_power_resourses,
	.num_resources = ARRAY_SIZE(h2200_power_resourses),
	.dev = {
		.platform_data = &h2200_power_pdata,
	},
};

/***************************************************************************/
/*      Initialisation                                                     */
/***************************************************************************/

static void check_serial_cable (void)
{
#warning check_serial_cable really needs a userland API, as opposed to being handled by the kernel.
// XXX: Should this be handled better?
	int connected = 1; //GET_H2200_GPIO(RS232_DCD);
	/* Toggle rs232 transceiver power according to connected status */
	SET_H2200_GPIO(RS232_ON, connected);
	/* Toggle rs232 vs CIR IC connected to FFUART. XXX: Should be connected == 0, but forcing on to prevent apps using the serial port from screwing up. */
	/* Should we make apps explicitly request CIR? */
	SET_H2200_GPIO_N(RS232_CIR, 0);
}

static void __init h2200_map_io(void)
{
	pxa_map_io ();

	/* Configure power management stuff. */
	PWER = PWER_GPIO0 | PWER_GPIO12 | PWER_GPIO13 | PWER_RTC;
	PFER = PWER_GPIO0 | PWER_GPIO12 | PWER_GPIO13 | PWER_RTC;
	PRER = PWER_GPIO0 | PWER_GPIO12 | PWER_GPIO13;
	PCFR = PCFR_OPDE;
	CKEN = CKEN6_FFUART;

	/* Configure power management stuff. */
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

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

	GPSR0 = GPSRx_InitValue;
	GPSR1 = GPSRy_InitValue;
	GPSR2 = GPSRz_InitValue;

	GPCR0 = ~GPSRx_InitValue;
	GPCR1 = ~GPSRy_InitValue;
	GPCR2 = ~GPSRz_InitValue;

	MSC0 = 0x246c7ffc;
	(void)MSC0;
	MSC1 = 0x7ff07ff0;
	(void)MSC1;
	MSC2 = 0x7ff07ff0;
	(void)MSC2;

	check_serial_cable ();

	pxa_set_btuart_info(&h2200_btuart_funcs);
	pxa_set_stuart_info(&h2200_irda_funcs);
	pxa_set_hwuart_info(&h2200_hwuart_funcs);

#ifdef EARLY_SIR_CONSOLE
	h2200_irda_configure (NULL, 1);
	h2200_irda_set_txrx (NULL, PXA_SERIAL_TX);
#endif
}

/* ------------------- */

static irqreturn_t h2200_serial_cable (int irq, void *dev_id)
{
	check_serial_cable ();
	return IRQ_HANDLED;
};

static int h2200_late_init (void)
{
	if (!machine_is_h2200 ())
		return 0;

	request_irq (H2200_IRQ(RS232_DCD), &h2200_serial_cable,
		     0, "Serial cable", NULL);
	set_irq_type (H2200_IRQ(RS232_DCD), IRQT_BOTHEDGE);
	return 0;
}
device_initcall (h2200_late_init);

/* ------------------- */

static int 
h2200_udc_is_connected (void)
{
	return GET_H2200_GPIO(USB_DETECT_N) ? 0 : 1;
}

static void 
h2200_udc_command (int cmd) 
{
	switch (cmd)
	{
	case PXA2XX_UDC_CMD_DISCONNECT:
		SET_H2200_GPIO_N(USB_PULL_UP, 0);
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		SET_H2200_GPIO_N(USB_PULL_UP, 1);
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static struct pxa2xx_udc_mach_info h2200_udc_mach_info = {
	.udc_is_connected = h2200_udc_is_connected,
	.udc_command      = h2200_udc_command,
};

/* Buttons */

static struct gpio_keys_button h2200_button_table[] = {
	{ KEY_POWER, GPIO_NR_H2200_POWER_ON_N, 1 },
};

static struct gpio_keys_platform_data h2200_gpio_keys_data = {
	.buttons = h2200_button_table,
	.nbuttons = ARRAY_SIZE(h2200_button_table),
};

static struct platform_device h2200_gpio_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &h2200_gpio_keys_data,
	},
};

static struct platform_device h2200_buttons = {
	.name = "h2200 buttons",
	.id   = -1,
};

/* SoC device */
static struct resource hamcop_resources[] = {
	[0] = {		
		.start	= H2200_HAMCOP_BASE,
		.end	= H2200_HAMCOP_BASE + 0x00ffffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO_H2200_ASIC_INT,
		.end	= IRQ_GPIO_H2200_ASIC_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device *h2200_hamcop_devices[] __initdata = {
	&h2200_buttons,
	&h2200_leds_pdev,
};

static struct hamcop_platform_data hamcop_platform_data = {
	.irq_base = H2200_HAMCOP_IRQ_BASE,
	.clocksleep = HAMCOP_CPM_CLKSLEEP_XTCON | HAMCOP_CPM_CLKSLEEP_UCLK_ON |
		      HAMCOP_CPM_CLKSLEEP_CLKSEL,
	.pllcontrol = 0xd15e,           /* value from wince via haret */ 

	.child_devs     = h2200_hamcop_devices,
	.num_child_devs = ARRAY_SIZE(h2200_hamcop_devices),
};

struct platform_device h2200_hamcop = {
	.name		= "hamcop",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(hamcop_resources),
	.resource	= hamcop_resources,
	.dev		= {
		.platform_data = &hamcop_platform_data,
	},
};
EXPORT_SYMBOL(h2200_hamcop);


static void __init 
h2200_init (void)
{
	platform_device_register(&h2200_hamcop);
	platform_device_register(&h2200_gpio_keys);
	platform_device_register(&h2200_power);
	pxa_set_udc_info (&h2200_udc_mach_info);
	pxa_set_ficp_info(&h2200_ficp_platform_data);
}

MACHINE_START(H2200, "HP iPAQ H2200")
        /* Maintainer: HP Labs, Cambridge Research Labs */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= h2200_map_io,
	.init_irq	= pxa_init_irq,
        .timer		= &pxa_timer,
        .init_machine	= h2200_init,
MACHINE_END
