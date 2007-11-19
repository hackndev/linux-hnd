/*
 * Hardware definitions for SA1100-based HP iPAQ Handheld Computers
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
#include <linux/irq.h>

#include <asm/irq.h>
#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>
#include <asm/mach/serial_sa1100.h>

#include <asm/arch/ipaqsa.h>

#include <linux/serial_core.h>

#include "generic.h"

struct ipaq_model_ops ipaq_model_ops;
EXPORT_SYMBOL(ipaq_model_ops);

/*
 * low-level UART features
 */

static void ipaqsa_uart_set_mctrl(struct uart_port *port, u_int mctrl)
{
	if (port->mapbase == _Ser3UTCR0) {
		gpio_set_value(GPIO_NR_H3600_COM_RTS, !(mctrl & TIOCM_RTS));
	}
}

static u_int ipaqsa_uart_get_mctrl(struct uart_port *port)
{
	u_int ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;

	if (port->mapbase == _Ser3UTCR0) {
                /* DCD and CTS bits are inverted in GPLR by RS232 transceiver */
		if (gpio_get_value(GPIO_NR_H3600_COM_DCD))
			ret &= ~TIOCM_CD;
		if (gpio_get_value(GPIO_NR_H3600_COM_CTS))
			ret &= ~TIOCM_CTS;
	}

	return ret;
}

static void ipaqsa_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	if (port->mapbase == _Ser2UTCR0) { /* TODO: REMOVE THIS */
		assign_ipaqsa_egpio( IPAQ_EGPIO_IR_ON, !state );
	} else if (port->mapbase == _Ser3UTCR0) {
		assign_ipaqsa_egpio( IPAQ_EGPIO_RS232_ON, !state );
	}
}

/*
 * Enable/Disable wake up events for this serial port.
 * Obviously, we only support this on the normal COM port.
 */
static int ipaqsa_uart_set_wake(struct uart_port *port, u_int enable)
{
	int err = -EINVAL;

	if (port->mapbase == _Ser3UTCR0) {
		if (enable)
			PWER |= PWER_GPIO23 | PWER_GPIO25 ; /* DCD and CTS */
		else
			PWER &= ~(PWER_GPIO23 | PWER_GPIO25); /* DCD and CTS */
		err = 0;
	}
	return err;
}

static struct sa1100_port_fns ipaqsa_port_fns __initdata = {
	.set_mctrl	= ipaqsa_uart_set_mctrl,
	.get_mctrl	= ipaqsa_uart_get_mctrl,
	.pm		= ipaqsa_uart_pm,
	.set_wake	= ipaqsa_uart_set_wake,
};


struct egpio_irq_info {
	int egpio_nr;
	int gpio; /* GPIO_GPIO(n) */
	int irq;  /* IRQ_GPIOn */
};

static struct egpio_irq_info ipaqsa_egpio_irq_info[] = {
	{ IPAQ_EGPIO_PCMCIA_CD0_N, GPIO_GPIO(GPIO_NR_H3600_PCMCIA_CD0), IRQ_GPIO_H3600_PCMCIA_CD0 },
	{ IPAQ_EGPIO_PCMCIA_CD1_N, GPIO_GPIO(GPIO_NR_H3600_PCMCIA_CD1), IRQ_GPIO_H3600_PCMCIA_CD1 },
	{ IPAQ_EGPIO_PCMCIA_IRQ0, GPIO_GPIO(GPIO_NR_H3600_PCMCIA_IRQ0), IRQ_GPIO_H3600_PCMCIA_IRQ0 },
	{ IPAQ_EGPIO_PCMCIA_IRQ1, GPIO_GPIO(GPIO_NR_H3600_PCMCIA_IRQ1), IRQ_GPIO_H3600_PCMCIA_IRQ1 },
	{ 0, 0 }
};

int ipaqsa_irq_number(int egpio_nr)
{
	struct egpio_irq_info *info = ipaqsa_egpio_irq_info;
	while (info->irq != 0) {
		if (info->egpio_nr == egpio_nr) {
			if (0) printk("%s: egpio_nr=%d irq=%d\n", __FUNCTION__, egpio_nr, info->irq);
			return info->irq;
		}
		info++;
	}

	printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, egpio_nr);
	return -EINVAL;
}
EXPORT_SYMBOL(ipaqsa_irq_number);

int ipaqsa_set_irq_type(enum ipaq_egpio_type egpio_nr, unsigned int type)
{
	struct egpio_irq_info *info = ipaqsa_egpio_irq_info;
	while (info->irq != 0) {
		if (info->egpio_nr == egpio_nr) {
			if (0) printk("%s: egpio_nr=%d gpio=%x irq=%d\n", __FUNCTION__, egpio_nr, info->gpio, info->irq);
			set_irq_type( info->irq, type);
			return 0;
		}
		info++;
	}
	printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, egpio_nr);
	return -EINVAL;
}
EXPORT_SYMBOL(ipaqsa_set_irq_type);

int ipaqsa_request_irq(enum ipaq_egpio_type egpio_number,
			    irqreturn_t (*handler)(int, void *),
			    unsigned long flags, const char *name, void *data)
{
	int sa1100_irq = ipaqsa_irq_number(egpio_number);
	return request_irq( sa1100_irq, handler, flags, name, data);
}
EXPORT_SYMBOL(ipaqsa_request_irq);

void ipaqsa_free_irq(unsigned int egpio_number, void *data)
{
	int sa1100_irq = ipaqsa_irq_number(egpio_number);
	free_irq( sa1100_irq, data );
}
EXPORT_SYMBOL(ipaqsa_free_irq);

void ipaqsa_set_led (enum led_color color, int duty_time, int cycle_time)
{
	if (ipaq_model_ops.set_led)
		ipaq_model_ops.set_led (color, duty_time, cycle_time);
}
EXPORT_SYMBOL(ipaqsa_set_led);


static struct map_desc ipaqsa_io_desc[] __initdata = {
  {     /* Flash bank 0         CS#0 */
    .virtual = 0xe8000000,
    .pfn     = __phys_to_pfn(0x00000000),
    .length  = 0x02000000,
    .type    = MT_DEVICE
  } , { /* EGPIO 0              CS#5 */
    .virtual = IPAQSA_EGPIO_VIRT,
    .pfn     = __phys_to_pfn(IPAQSA_EGPIO_PHYS),
    .length  = 0x01000000,
    .type    = MT_DEVICE
  } , { /* static memory bank 2 CS#2 */
    .virtual = IPAQSA_BANK_2_VIRT,
    .pfn     = __phys_to_pfn(SA1100_CS2_PHYS),
    .length  = 0x02800000,
    .type    = MT_DEVICE
  } , { /* static memory bank 4 CS#4 */
    .virtual = IPAQSA_BANK_4_VIRT,
    .pfn     = __phys_to_pfn(SA1100_CS4_PHYS),
    .length  = 0x00800000,
    .type    = MT_DEVICE
  }
};
/*static struct map_desc ipaqsa_io_desc[] __initdata = {              */
/*    virtual             physical           length      type         */
/*  { 0xe8000000,         0x00000000,        0x02000000, MT_DEVICE }, *//* Flash bank 0          CS#0 */	
/*  { IPAQSA_EGPIO_VIRT,  IPAQSA_EGPIO_PHYS, 0x01000000, MT_DEVICE }, *//* EGPIO 0               CS#5 */
/*  { IPAQSA_BANK_2_VIRT, SA1100_CS2_PHYS,   0x02800000, MT_DEVICE }, *//* static memory bank 2  CS#2 */
/*  { IPAQSA_BANK_4_VIRT, SA1100_CS4_PHYS,   0x00800000, MT_DEVICE }, *//* static memory bank 4  CS#4 */
/*};*/


/*
 * Common map_io initialization
 */
void __init ipaqsa_map_io(void)
{
	sa1100_map_io();
	iotable_init(ipaqsa_io_desc, ARRAY_SIZE(ipaqsa_io_desc));

	sa1100_register_uart_fns(&ipaqsa_port_fns);
	sa1100_register_uart(0, 3); /* Common serial port */

	/* Ensure those pins are outputs and driving low  */
	PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
	PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);

	/* Configure suspend conditions */
	PGSR = 0;
	PWER = PWER_GPIO0 | PWER_RTC;
	PCFR = PCFR_OPDE;
	PSDR = 0;

}


/*
 * This turns the IRDA power on or off on the Compaq H3600
 */
static int ipaqsa_irda_set_power(struct device *dev, unsigned int state)
{
	assign_ipaqsa_egpio( IPAQ_EGPIO_IR_ON, state );

	return 0;
}

static void ipaqsa_irda_set_speed(struct device *dev, unsigned int speed)
{
	if (speed < 4000000) {
		assign_ipaqsa_egpio(IPAQ_EGPIO_IR_FSEL, 0);
	} else {
		assign_ipaqsa_egpio(IPAQ_EGPIO_IR_FSEL, 1);
	}
}

static struct irda_platform_data ipaqsa_irda_data = {
	.set_power	= ipaqsa_irda_set_power,
	.set_speed	= ipaqsa_irda_set_speed,
};

void __init ipaqsa_mach_init(void)
{
	sa11x0_set_irda_data(&ipaqsa_irda_data);
}

void __init ipaqsa_mtd_set_vpp(int vpp)
{
	assign_ipaqsa_egpio(IPAQ_EGPIO_VPP_ON, !!vpp );
}

unsigned long ipaqsa_common_read_egpio( enum ipaq_egpio_type x)
{
	switch (x) {
	case IPAQ_EGPIO_PCMCIA_CD0_N:
		return gpio_get_value(GPIO_NR_H3600_PCMCIA_CD0);
	case IPAQ_EGPIO_PCMCIA_CD1_N:
		return gpio_get_value(GPIO_NR_H3600_PCMCIA_CD1);
	case IPAQ_EGPIO_PCMCIA_IRQ0:
		return gpio_get_value(GPIO_NR_H3600_PCMCIA_IRQ0);
	case IPAQ_EGPIO_PCMCIA_IRQ1:
		return gpio_get_value(GPIO_NR_H3600_PCMCIA_IRQ1);
	default:
		printk("%s: unhandled egpio_nr=%d\n", __FUNCTION__, x);
		return 0;
	}
}

