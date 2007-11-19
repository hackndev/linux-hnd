/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
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
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restrutured June 2002
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/serial.h>  /* For bluetooth */

#include <asm/irq.h>
#include <asm/uaccess.h>   /* for copy to/from user space */
#include <asm/arch/hardware.h>
#include <asm/arch-sa1100/h3600_hal.h>
#include <asm/arch-sa1100/h3600_asic.h>
#include <asm/arch-sa1100/serial_h3800.h>

#include "h3600_asic_io.h"
#include "h3600_asic_core.h"

/***********************************************************************************
 *   Bluetooth
 *
 *   Resources used:    
 *   Shared resources:  
 *
 ***********************************************************************************/

struct ipaq_asic2_bluetooth {
	int line;       // Serial port line
	unsigned long shared;
};

static struct ipaq_asic2_bluetooth g_bluedev;

static void ipaq_asic2_bluetooth_up( struct ipaq_asic2_bluetooth *dev )
{
	H3800_ASIC2_INTR_MaskAndFlag &= ~ASIC2_INTMASK_UART_0;

	H3800_ASIC2_UART_0_RSR = 1;   // Reset the UART
	H3800_ASIC2_UART_0_RSR = 0;

	H3800_ASIC2_CLOCK_Enable = 
		(H3800_ASIC2_CLOCK_Enable & ~ASIC2_CLOCK_UART_0_MASK) | ASIC2_CLOCK_SLOW_UART_0;
	ipaq_asic2_shared_add( &dev->shared, ASIC_SHARED_CLOCK_EX1 );

	// H3600.c has already taken care of sending the 3.6864 MHz clock to the UART (TUCR) 
	// but we're paranoid...
	GPDR |= GPIO_H3800_CLK_OUT;
	GAFR |= GPIO_H3800_CLK_OUT;

	TUCR = TUCR_3_6864MHzA;

	// Set up the TXD & RXD of the UART to normal operation
	H3800_ASIC2_GPIODIR &= ~GPIO2_UART0_TXD_ENABLE;
	H3800_ASIC2_GPIOPIOD |= GPIO2_UART0_TXD_ENABLE;

	// More black magic
	H3800_ASIC2_KPIODIR  &= ~KPIO_UART_MAGIC;
	H3800_ASIC2_KPIOPIOD |=  KPIO_UART_MAGIC;
}

static void ipaq_asic2_bluetooth_down( struct ipaq_asic2_bluetooth *dev )
{
	H3800_ASIC2_INTR_MaskAndFlag   |= ASIC2_INTMASK_UART_0;
	ipaq_asic2_shared_release( &dev->shared, ASIC_SHARED_CLOCK_EX1 );
}

int ipaq_asic2_bluetooth_suspend( void )
{
	ipaq_asic2_bluetooth_down( &g_bluedev );
	return 0;
}

void ipaq_asic2_bluetooth_resume( void )
{
	ipaq_asic2_bluetooth_up( &g_bluedev );
}

static void serial_fn_pm( struct uart_port *port, u_int state, u_int oldstate )
{
	if (0) printk("%s: %p %d %d\n", __FUNCTION__, port, state, oldstate);
}

static int serial_fn_open( struct uart_port *port, struct uart_info *info )
{
	if (0) printk("%s: %p %p\n", __FUNCTION__, port, info);
	MOD_INC_USE_COUNT;

	H3800_ASIC1_GPIO_OUT |= GPIO1_BT_PWR_ON;
	ipaq_asic2_led_blink(BLUE_LED,1,7,1);
	return 0;
}

static void serial_fn_close( struct uart_port *port, struct uart_info *info )
{
	if (0) printk("%s: %p %p\n", __FUNCTION__, port, info);
	ipaq_asic2_led_off(BLUE_LED);
	H3800_ASIC1_GPIO_OUT &= ~GPIO1_BT_PWR_ON;

	MOD_DEC_USE_COUNT;
}

static struct serial_h3800_fns serial_fns = {
	pm    : serial_fn_pm,
	open  : serial_fn_open,
	close : serial_fn_close
};

int ipaq_asic2_bluetooth_init( void )
{
	struct serial_struct ipaq_asic2_serial_struct;

	ipaq_asic2_bluetooth_up( &g_bluedev );

	g_bluedev.line = register_serial(&h3800_serial_req);
	if ( g_bluedev.line < 0 ) {
		printk("%s: register serial failed\n", __FUNCTION__);
		return g_bluedev.line;
	} 

	return 0;
}

void ipaq_asic2_bluetooth_cleanup( void )
{
	printk("%s: %d\n", __FUNCTION__, g_bluedev.line);
	unregister_serial_h3800( g_bluedev.line );
	ipaq_asic2_bluetooth_down( &g_bluedev );
}

extern struct device_driver ipaq_asic2_bluetooth_device_driver = {
	.name     = "asic2 bluetooth",
	.probe    = ipaq_asic2_bluetooth_init,
	.shutdown = ipaq_asic2_bluetooth_cleanup,
	.suspend  = ipaq_asic2_bluetooth_suspend,
	.resume   = ipaq_asic2_bluetooth_resume
};
EXPORT_SYMBOL(ipaq_asic2_bluetooth_device_driver);
