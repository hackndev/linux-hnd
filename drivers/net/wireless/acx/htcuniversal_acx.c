/*
 * WLAN (TI TNETW1100B) support in the HTC Universal
 *
 * Copyright (c) 2006 SDG Systems, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 28-March-2006          Todd Blumer <todd@sdgsystems.com>
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>
#include <asm/io.h>

#include "acx_hw.h"

#define WLAN_BASE	PXA_CS2_PHYS


static int
htcuniversal_wlan_start( void )
{
	htcuniversal_egpio_enable(1<<EGPIO6_WIFI_ON);
	asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_WIFI_PWR1_ON, 1<<GPIOC_WIFI_PWR1_ON);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_WIFI_PWR3_ON, 1<<GPIOD_WIFI_PWR3_ON);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_WIFI_PWR2_ON, 1<<GPIOD_WIFI_PWR2_ON);
	mdelay(100);

	asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_WIFI_RESET, 0);
	mdelay(100);
	asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_WIFI_RESET, 1<<GPIOC_WIFI_RESET);
	mdelay(100);
	return 0;
}

static int
htcuniversal_wlan_stop( void )
{
	asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_WIFI_RESET, 0);

	htcuniversal_egpio_disable(1<<EGPIO6_WIFI_ON);
	asic3_set_gpio_out_c(&htcuniversal_asic3.dev, 1<<GPIOC_WIFI_PWR1_ON, 0);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_WIFI_PWR2_ON, 0);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_WIFI_PWR3_ON, 0);
	return 0;
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= WLAN_BASE,
		.end	= WLAN_BASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
//		.start	= asic3_irq_base(&htcuniversal_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N,
//		.end	= asic3_irq_base(&htcuniversal_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct acx_hardware_data acx_data = {
	.start_hw	= htcuniversal_wlan_start,
	.stop_hw	= htcuniversal_wlan_stop,
};

static struct platform_device acx_device = {
	.name	= "acx-mem",
	.dev	= {
		.platform_data = &acx_data,
	},
	.num_resources	= ARRAY_SIZE( acx_resources ),
	.resource	= acx_resources,
};

static int __init
htcuniversal_wlan_init( void )
{
	printk( "htcuniversal_wlan_init: acx-mem platform_device_register\n" );
	acx_device.resource[1].start = asic3_irq_base(&htcuniversal_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N;
	acx_device.resource[1].end = asic3_irq_base(&htcuniversal_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N;
	return platform_device_register( &acx_device );
}


static void __exit
htcuniversal_wlan_exit( void )
{
	platform_device_unregister( &acx_device );
}

module_init( htcuniversal_wlan_init );
module_exit( htcuniversal_wlan_exit );

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "WLAN driver for HTC Universal" );
MODULE_LICENSE( "GPL" );

