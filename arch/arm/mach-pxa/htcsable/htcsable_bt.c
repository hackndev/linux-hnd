/* Bluetooth interface driver for TI BRF6150 on HX4700
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-04-21   Todd Blumer             Created.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>

#include "htcsable_bt.h"

static uint use_led=1;

static void
htcsable_bt_configure( int state )
{
	int tries;

	printk( KERN_NOTICE "htcsable configure bluetooth: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		/*htcsable_egpio_enable(EGPIO6_BT_3V3_ON);*/
		mdelay(50);
		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_BT_PWR_ON, 1<<GPIOD_BT_PWR_ON);
		mdelay(10);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_BT_RESET, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_BT_RESET, 1<<GPIOC_BT_RESET);
		mdelay(10);

		/*
		 * BRF6150's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			mdelay(10);
		} while ((FFMSR & MSR_CTS) == 0 && tries++ < 50);
		if (use_led) {
//			htcsable_set_led(2, 16, 16);
		}
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		/*htcsable_egpio_disable(EGPIO6_BT_3V3_ON );*/
		mdelay(50);
//		htcsable_clear_led(2);
		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_BT_PWR_ON, 0);
		mdelay(10);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_BT_RESET, 0);
		break;

	default:
		break;
	}
}


static int
htcsable_bt_probe( struct platform_device *dev )
{
	struct htcsable_bt_funcs *funcs = (struct htcsable_bt_funcs *) dev->dev.platform_data;

	/* configure bluetooth UART */
	pxa_gpio_mode( GPIO_NR_HTCSABLE_BT_RXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_BT_TXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_BT_UART_CTS_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_BT_UART_RTS_MD );

	funcs->configure = htcsable_bt_configure;

	/* Make sure the LED is off */
//	htcsable_clear_led(2);

	return 0;
}

static int
htcsable_bt_remove( struct platform_device *dev )
{
	struct htcsable_bt_funcs *funcs = (struct htcsable_bt_funcs *) dev->dev.platform_data;

	funcs->configure = NULL;

	/* Make sure the LED is off */
//	htcsable_clear_led(2);

	return 0;
}

static struct platform_driver bt_driver = {
	.driver = {
		.name     = "htcsable_bt",
	},
	.probe    = htcsable_bt_probe,
	.remove   = htcsable_bt_remove,
};

module_param(use_led, uint, 0);

static int __init
htcsable_bt_init( void )
{
	printk(KERN_NOTICE "htcsable Bluetooth Driver\n");
	return platform_driver_register( &bt_driver );
}

static void __exit
htcsable_bt_exit( void )
{
	platform_driver_unregister( &bt_driver );
}

module_init( htcsable_bt_init );
module_exit( htcsable_bt_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Sable Bluetooth Support Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

