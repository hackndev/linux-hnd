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
#include <linux/fs.h>
#include <linux/leds.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include <asm/arch/hx4700.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-core.h>


static void
hx4700_bt_configure( int state )
{
	int tries;

	// printk( KERN_NOTICE "hx4700 configure bluetooth: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		hx4700_egpio_enable( EGPIO5_BT_3V3_ON );
		SET_HX4700_GPIO( CPU_BT_RESET_N, 0 );
		mdelay(1);
		SET_HX4700_GPIO( CPU_BT_RESET_N, 1 );

		/*
		 * BRF6150's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			mdelay(10);
		} while ((BTMSR & MSR_CTS) == 0 && tries++ < 50);
		led_trigger_event_shared(hx4700_radio_trig, LED_FULL);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		hx4700_egpio_disable( EGPIO5_BT_3V3_ON );
		mdelay(1);
		led_trigger_event_shared(hx4700_radio_trig, LED_OFF);
		break;

	default:
		break;
	}
}


static int
hx4700_bt_probe( struct platform_device *pdev )
{
	struct hx4700_bt_funcs *funcs = pdev->dev.platform_data;

	/* configure bluetooth UART */
	pxa_gpio_mode( GPIO_NR_HX4700_BT_RXD_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_BT_TXD_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_BT_UART_CTS_MD );
	pxa_gpio_mode( GPIO_NR_HX4700_BT_UART_RTS_MD );

	funcs->configure = hx4700_bt_configure;

	return 0;
}

static int
hx4700_bt_remove( struct platform_device *pdev )
{
	struct hx4700_bt_funcs *funcs = pdev->dev.platform_data;

	funcs->configure = NULL;

	return 0;
}

static struct platform_driver bt_driver = {
	.driver = {
		.name     = "hx4700-bt",
	},
	.probe    = hx4700_bt_probe,
	.remove   = hx4700_bt_remove,
};

static int __init
hx4700_bt_init( void )
{
	printk(KERN_NOTICE "hx4700 Bluetooth Driver\n");
	return platform_driver_register( &bt_driver );
}

static void __exit
hx4700_bt_exit( void )
{
	platform_driver_unregister( &bt_driver );
}

module_init( hx4700_bt_init );
module_exit( hx4700_bt_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("hx4700 Bluetooth Support Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

