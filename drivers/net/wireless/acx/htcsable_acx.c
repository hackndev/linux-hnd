/*
 * WLAN (TI TNETW1100B) support in the HTC Sable
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
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>
#include <asm/io.h>

#include "acx_hw.h"

#define WLAN_BASE	PXA_CS2_PHYS

/*
off: b15 c8 d3
on: d3 c8 b5 b5-
*/

#define GPIO_NR_HTCSABLE_ACX111 111

static int
htcsable_wlan_stop( void );

static int
htcsable_wlan_start( void )
{
	printk( "htcsable_wlan_start\n" );

  /*asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_ACX_RESET, 0);*/
  asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_ACX_PWR_3, 1<<GPIOC_ACX_PWR_3); /* related to acx */
  SET_HTCSABLE_GPIO(ACX111, 1);
	asic3_set_gpio_out_b(&htcsable_asic3.dev, 1<<GPIOB_ACX_PWR_1, 1<<GPIOB_ACX_PWR_1);
	asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_ACX_PWR_2, 1<<GPIOD_ACX_PWR_2);
  mdelay(260);
  asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_ACX_RESET, 1<<GPIOC_ACX_RESET);

	return 0;
}

static int
htcsable_wlan_stop( void )
{
	printk( "htcsable_wlan_stop\n" );
	asic3_set_gpio_out_b(&htcsable_asic3.dev, 1<<GPIOB_ACX_PWR_1, 0);
	asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_ACX_RESET, 0);
	asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_ACX_PWR_2, 0);
  SET_HTCSABLE_GPIO(ACX111, 0); /* not necessary to power down this one? */
	asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_ACX_PWR_3, 0); /* not necessary to power down this one? */

	return 0;
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= WLAN_BASE,
		.end	= WLAN_BASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
//		.start	= asic3_irq_base(&htcsable_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N,
//		.end	= asic3_irq_base(&htcsable_asic3.dev) + ASIC3_GPIOC_IRQ_BASE+GPIOC_WIFI_IRQ_N,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct acx_hardware_data acx_data = {
	.start_hw	= htcsable_wlan_start,
	.stop_hw	= htcsable_wlan_stop,
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
htcsable_wlan_init( void )
{
	printk( "htcsable_wlan_init: acx-mem platform_device_register\n" );
	acx_device.resource[1].start = asic3_irq_base(&htcsable_asic3.dev) + ASIC3_GPIOB_IRQ_BASE+GPIOB_ACX_IRQ_N;
	acx_device.resource[1].end = asic3_irq_base(&htcsable_asic3.dev) + ASIC3_GPIOB_IRQ_BASE+GPIOB_ACX_IRQ_N;
	return platform_device_register( &acx_device );
}


static void __exit
htcsable_wlan_exit( void )
{
	platform_device_unregister( &acx_device );
}

module_init( htcsable_wlan_init );
module_exit( htcsable_wlan_exit );

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "WLAN driver for HTC Sable" );
MODULE_LICENSE( "GPL" );

