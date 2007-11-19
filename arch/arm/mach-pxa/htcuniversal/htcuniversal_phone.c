
/* Phone interface driver for Qualcomm MSM6250 on HTC Universal
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
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

#include "htcuniversal_phone.h"

static void phone_reset(void)
{
	asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_RESET2, 0);

	SET_HTCUNIVERSAL_GPIO(PHONE_RESET,0);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BB_RESET1, 0);
	mdelay(1);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BB_RESET1, 1<<GPIOD_BB_RESET1);
	mdelay(20);
	SET_HTCUNIVERSAL_GPIO(PHONE_RESET,1);
	mdelay(200);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BB_RESET1, 0);
}

static void phone_off(void)
{
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BB_RESET1, 1<<GPIOD_BB_RESET1);
	mdelay(2000);
	asic3_set_gpio_out_d(&htcuniversal_asic3.dev, 1<<GPIOD_BB_RESET1, 0);

	asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_RESET2, 1<<GPIOB_BB_RESET2);
	SET_HTCUNIVERSAL_GPIO(PHONE_OFF,0);
}
													
static void
htcuniversal_phone_configure( int state )
{
	int tries;
	unsigned short statusb;

	printk( KERN_NOTICE "htcuniversal configure phone: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */

		SET_HTCUNIVERSAL_GPIO(PHONE_START,0);   /* "bootloader" */
		SET_HTCUNIVERSAL_GPIO(PHONE_UNKNOWN,0); /* not used     */
		SET_HTCUNIVERSAL_GPIO(PHONE_OFF,0);     /* PHONE_OFF    */

		phone_reset();

		SET_HTCUNIVERSAL_GPIO(PHONE_START,1); /* phone */

		phone_reset();

		asic3_set_gpio_dir_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_READY, 0);
		asic3_set_gpio_dir_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_UNKNOWN3, 0);
		
		/*
		 */
		tries = 0;
		do {
			mdelay(10);
			statusb = asic3_get_gpio_status_b( &htcuniversal_asic3.dev );
		} while ( (statusb & (1<<GPIOB_UMTS_DCD)) == 0 && tries++ < 200);

		printk("UMTS_DCD tries=%d of 200\n",tries);

		tries = 0;
		do {
			SET_HTCUNIVERSAL_GPIO(PHONE_OFF,1);
			mdelay(10);
			SET_HTCUNIVERSAL_GPIO(PHONE_OFF,0);
			mdelay(20);
			statusb = asic3_get_gpio_status_b( &htcuniversal_asic3.dev );
		} while ( (statusb & (1<<GPIOB_BB_READY)) == 0 && tries++ < 200);

		printk("BB_READY tries=%d of 200\n",tries);

		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:

			phone_off();

		break;

	default:
		break;
	}
}


static int
htcuniversal_phone_probe( struct platform_device *dev )
{
	struct htcuniversal_phone_funcs *funcs = dev->dev.platform_data;

	/* configure phone UART */
	pxa_gpio_mode( GPIO_NR_HTCUNIVERSAL_PHONE_RXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCUNIVERSAL_PHONE_TXD_MD );
	pxa_gpio_mode( GPIO_NR_HTCUNIVERSAL_PHONE_UART_CTS_MD );
	pxa_gpio_mode( GPIO_NR_HTCUNIVERSAL_PHONE_UART_RTS_MD );

	funcs->configure = htcuniversal_phone_configure;

	return 0;
}

static int
htcuniversal_phone_remove( struct platform_device *dev )
{
	struct htcuniversal_phone_funcs *funcs = dev->dev.platform_data;

	funcs->configure = NULL;

	asic3_set_gpio_dir_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_READY, 1<<GPIOB_BB_READY);
	asic3_set_gpio_dir_b(&htcuniversal_asic3.dev, 1<<GPIOB_BB_UNKNOWN3, 1<<GPIOB_BB_UNKNOWN3);

	return 0;
}

static struct platform_driver phone_driver = {
	.driver   = {
		.name     = "htcuniversal_phone",
	},
	.probe    = htcuniversal_phone_probe,
	.remove   = htcuniversal_phone_remove,
};

static int __init
htcuniversal_phone_init( void )
{
	printk(KERN_NOTICE "htcuniversal Phone Driver\n");
	return platform_driver_register( &phone_driver );
}

static void __exit
htcuniversal_phone_exit( void )
{
	platform_driver_unregister( &phone_driver );
}

module_init( htcuniversal_phone_init );
module_exit( htcuniversal_phone_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Universal Phone Support Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/
