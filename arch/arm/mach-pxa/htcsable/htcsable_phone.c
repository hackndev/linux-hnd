
/* Phone interface driver for Qualcomm MSM6250 on HTC Sable
 *
 * Copyright (c) 2005 SDG Systems, LLC
 * Copyright (C) 2006 Luke Kenneth Casson Leighton
 *
 * 2005-04-21   Todd Blumer             Created.
 * 2006-11-16   lkcl                    Copied.  Worked out init sequence.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>
#include <linux/serial_core.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>

#include "htcsable_phone.h"

static void
htcsable_phone_configure( int state )
{
	unsigned short statusb;
	int tries;

	printk( KERN_NOTICE "htcsable configure phone: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		/* pre-serial-up hardware configuration */

		/* D4 */
		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_1, 1<<GPIOD_PHONE_1);
		mdelay(10);
		/* D8 */
		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_2, 1<<GPIOD_PHONE_2);
		mdelay(30);

		/* C12 */
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_PHONE_4, 1<<GPIOC_PHONE_4);
		mdelay(10);
		/* C13 */
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_PHONE_5, 1<<GPIOC_PHONE_5);
		mdelay(10);
		/* D5 */
		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_6, 1<<GPIOD_PHONE_6);

		tries = 0;
		do {
			mdelay(100);
			statusb = asic3_get_gpio_status_b( &htcsable_asic3.dev );
		} while ( (statusb & (1<<GPIOB_GSM_DCD)) == 0 && tries++ < 3);

		printk("GSM_DCD (?) tries=%d of 3\n",tries);
		break;

	case PXA_UART_CFG_POST_STARTUP:
		tries = 0;
		do {
			mdelay(100);
			statusb = asic3_get_gpio_status_b( &htcsable_asic3.dev );
		} while ( (statusb & (1<<GPIOB_BB_READY)) == 0 && tries++ < 10);

		printk("BB_READY (?) tries=%d of 10\n",tries);

		/* 11 */
		mdelay(20);
		SET_HTCSABLE_GPIO(PHONE_3,1); 
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:

		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_PHONE_4, 0);
		mdelay(20);
		asic3_set_gpio_out_c(&htcsable_asic3.dev, 1<<GPIOC_PHONE_5, 0);
		mdelay(20);

		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_1, 0);
		mdelay(20);

		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_6, 0);
		mdelay(20);

		asic3_set_gpio_out_d(&htcsable_asic3.dev, 1<<GPIOD_PHONE_2, 0);
		mdelay(20);
		SET_HTCSABLE_GPIO(PHONE_3,0); 

		break;

	default:
		break;
	}
}

static int
htcsable_phone_probe( struct platform_device *dev )
{
	struct htcsable_phone_funcs *funcs = (struct htcsable_phone_funcs *) dev->dev.platform_data;

	/* configure phone UART */
	pxa_gpio_mode( GPIO_NR_HTCSABLE_PHONE_RXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_PHONE_TXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_PHONE_UART_CTS_MD );
	pxa_gpio_mode( GPIO_NR_HTCSABLE_PHONE_UART_RTS_MD );

	funcs->configure = htcsable_phone_configure;

	return 0;
}

static int
htcsable_phone_remove( struct platform_device *dev )
{
	struct htcsable_phone_funcs *funcs = (struct htcsable_phone_funcs *) dev->dev.platform_data;

	funcs->configure = NULL;

	return 0;
}

static struct platform_driver phone_driver = {
	.driver = {
		.name     = "htcsable_phone",
	},
	.probe    = htcsable_phone_probe,
	.remove   = htcsable_phone_remove,
};

static int __init
htcsable_phone_init( void )
{
	printk(KERN_NOTICE "htcsable Phone Driver\n");
	return platform_driver_register( &phone_driver );
}

static void __exit
htcsable_phone_exit( void )
{
	platform_driver_unregister( &phone_driver );
}

module_init( htcsable_phone_init );
module_exit( htcsable_phone_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC; Luke Kenneth Casson Leighton");
MODULE_DESCRIPTION("HTC Sable Phone Support Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

