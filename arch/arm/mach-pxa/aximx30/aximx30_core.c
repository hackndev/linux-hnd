/* Core Hardware driver for x30 (ASIC3, EGPIOs)
 *
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see below:
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer             Converted  basic structure to support hx4700
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/aximx30-gpio.h>
#include <asm/arch/aximx30-asic.h>

#include <linux/mfd/asic3_base.h>
#include <asm/hardware/ipaq-asic3.h>

#include "aximx30_core.h"

#define EGPIO_OFFSET	0
#define EGPIO_BASE	(PXA_CS5_PHYS+EGPIO_OFFSET)
#define WLAN_OFFSET	0x800000
#define WLAN_BASE	(PXA_CS5_PHYS+WLAN_OFFSET)
#define PPSH_OFFSET	0x1000000

volatile u_int16_t *egpios;
u_int16_t egpio_reg;

/*
 * may make sense to put egpios elsewhere, but they're here now
 * since they share some of the same address space with the TI WLAN
 *
 * EGPIO register is write-only
 */

void
x30_egpio_enable( unsigned long bits )
{
	egpio_reg |= bits;
	*egpios = egpio_reg;
}
EXPORT_SYMBOL(x30_egpio_enable);

void
x30_egpio_disable( unsigned long bits )
{
	egpio_reg &= ~bits;
	*egpios = egpio_reg;
}
EXPORT_SYMBOL(x30_egpio_disable);

int
x30_udc_detect( void )
{
	return (asic3_get_gpio_status_d(&x30_asic3.dev)
			& (1 << GPIOD_USBC_DETECT_N)) ? 0 : 1;
}

static unsigned int serial_irq = 0xffffffff;
static unsigned int ac_irq = 0xffffffff;

static int
serial_isr(int irq, void *dev_id)
{
	unsigned int statusd;
	int connected;

	if (irq != serial_irq)
	    return IRQ_NONE;

	statusd = asic3_get_gpio_status_d( &x30_asic3.dev );
	connected = (statusd & (1<<GPIOD_COM_DCD)) != 0;
	printk( KERN_INFO "serial_isr: com_dcd=%d\n", connected );
	SET_X30_GPIO( RS232_ON, connected );
	return IRQ_HANDLED;
}

static int
ac_isr(int irq, void *dev_id)
{
	unsigned int statusd;
	int connected;

	if (irq != ac_irq)
	    return IRQ_NONE;

	statusd = asic3_get_gpio_status_d( &x30_asic3.dev );
	connected = (statusd & (1<<GPIOD_AC_IN_N)) == 0;
	printk( KERN_INFO "ac_isr: connected=%d\n", connected );
	if (connected)
	    set_irq_type( ac_irq, IRQT_RISING ); 
	else
	    set_irq_type( ac_irq, IRQT_FALLING ); 
	SET_X30_GPIO_N( CHARGE_EN, connected );
	return IRQ_HANDLED;
}

static int
x30_core_probe(struct platform_device *pdev)
{
	unsigned int statusd;
	int connected;
	struct x30_core_funcs *funcs = (struct x30_core_funcs *)pdev->dev.platform_data;
	printk( KERN_NOTICE "X30 Core Hardware Driver\n" );

	funcs->udc_detect = x30_udc_detect;

	/* UART IRQ */
	egpios = (volatile u_int16_t *)ioremap_nocache( EGPIO_BASE, sizeof *egpios );
	if (!egpios)
		return -ENODEV;

        serial_irq = asic3_irq_base( &x30_asic3.dev ) + ASIC3_GPIOD_IRQ_BASE
                + GPIOD_COM_DCD;
        if (request_irq( serial_irq, serial_isr, SA_INTERRUPT,
			    "X30 Serial", NULL ) != 0) {
		printk( KERN_ERR "Unable to configure serial port interrupt.\n" );
		return -ENODEV;
	}
	set_irq_type( serial_irq, IRQT_RISING ); 

        ac_irq = asic3_irq_base( &x30_asic3.dev ) + ASIC3_GPIOD_IRQ_BASE
                + GPIOD_AC_IN_N;

	statusd = asic3_get_gpio_status_d( &x30_asic3.dev );
	connected = (statusd & (1<<GPIOD_AC_IN_N)) == 0;
	printk( KERN_INFO "AC: connected=%d\n", connected );
	if (connected)
	    set_irq_type( ac_irq, IRQT_RISING ); 
	else
	    set_irq_type( ac_irq, IRQT_FALLING ); 

        if (request_irq( ac_irq, ac_isr, SA_INTERRUPT,
			    "X30 AC Detect", NULL ) != 0) {
		printk( KERN_ERR "Unable to configure AC detect interrupt.\n" );
		free_irq( serial_irq, NULL );
		return -ENODEV;
	}
	SET_X30_GPIO_N( CHARGE_EN, connected );
	return 0;
}

static int
x30_core_remove(struct platform_device *pdev)
{
	int irq;

        irq = asic3_irq_base( &x30_asic3.dev ) + ASIC3_GPIOD_IRQ_BASE
			+ GPIOD_COM_DCD;
	if (egpios != NULL)
		iounmap( (void *)egpios );
	if (serial_irq != 0xffffffff)
		free_irq( serial_irq, NULL );
	if (ac_irq != 0xffffffff)
		free_irq( ac_irq, NULL );
	return 0;
}

struct platform_driver x30_core_driver = {
	.driver	  = {
	    .name = "X30-core",
	},
	.probe    = x30_core_probe,
	.remove   = x30_core_remove,
};

static int __init
x30_core_init( void )
{
	return platform_driver_register(&x30_core_driver);
}


static void __exit
x30_core_exit( void )
{
	platform_driver_unregister(&x30_core_driver);
}

module_init( x30_core_init );
module_exit( x30_core_exit );

MODULE_AUTHOR("Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>");
MODULE_DESCRIPTION("Dell Axim X30 Core Hardware Driver");
MODULE_LICENSE("GPL");

