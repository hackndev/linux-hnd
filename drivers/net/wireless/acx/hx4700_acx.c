/*
 * WLAN (TI TNETW1100B) support in the hx470x.
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
#include <linux/leds.h>

#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-core.h>
#include <asm/io.h>

#include "acx_hw.h"

#define WLAN_OFFSET	0x1000000
#define WLAN_BASE	(PXA_CS5_PHYS+WLAN_OFFSET)


static int
hx4700_wlan_start( void )
{
	SET_HX4700_GPIO( WLAN_RESET_N, 0 );
	mdelay(5);
	hx4700_egpio_enable( EGPIO0_VCC_3V3_EN );
	mdelay(100);
	hx4700_egpio_enable( EGPIO7_VCC_3V3_WL_EN );
	mdelay(150);
	hx4700_egpio_enable( EGPIO1_WL_VREG_EN | EGPIO2_VCC_2V1_WL_EN |
                             EGPIO6_WL1V8_EN );
	mdelay(10);
	SET_HX4700_GPIO( WLAN_RESET_N, 1 );
	mdelay(50);
	led_trigger_event_shared(hx4700_radio_trig, LED_FULL);
	return 0;
}

static int
hx4700_wlan_stop( void )
{
	hx4700_egpio_disable( EGPIO0_VCC_3V3_EN | EGPIO1_WL_VREG_EN |
                              EGPIO7_VCC_3V3_WL_EN | EGPIO2_VCC_2V1_WL_EN |
                              EGPIO6_WL1V8_EN );
	SET_HX4700_GPIO( WLAN_RESET_N, 0 );
	led_trigger_event_shared(hx4700_radio_trig, LED_OFF);
	return 0;
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= WLAN_BASE,
		.end	= WLAN_BASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= HX4700_IRQ(WLAN_IRQ_N),
		.end	= HX4700_IRQ(WLAN_IRQ_N),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct acx_hardware_data acx_data = {
	.start_hw	= hx4700_wlan_start,
	.stop_hw	= hx4700_wlan_stop,
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
hx4700_wlan_init( void )
{
	printk( "hx4700_wlan_init: acx-mem platform_device_register\n" );
	return platform_device_register( &acx_device );
}


static void __exit
hx4700_wlan_exit( void )
{
	platform_device_unregister( &acx_device );
}

module_init( hx4700_wlan_init );
module_exit( hx4700_wlan_exit );

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "WLAN driver for iPAQ hx4700" );
MODULE_LICENSE( "GPL" );

