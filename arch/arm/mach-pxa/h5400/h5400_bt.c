/*
 * H5400 bluetooth functions
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
 * Copyright 2004, 2005 Phil Blundell
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
 * 2002-08-23   Jamey Hicks        GPIO and IRQ support for iPAQ H5400
 * 2007-02-24   Anton Vorontsov    Split from h5400.c
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware.h>
#include <asm/arch/h5400.h>
#include <asm/arch/h5400-gpio.h>
#include <asm/arch/h5400-asic.h>
#include <asm/arch/h5400-init.h>
#include <linux/mfd/samcop_base.h>
#include <asm/arch/serial.h>

static void h5400_bluetooth_power(int on)
{
	if (on) {
		/* apply reset. The chip requires that the reset pins go
		 * high 2ms after voltage is applied to VCC and IOVCC. So it
		 * is driven low now before we set the power pins. It
		 * is assumed that the reset pins are tied together
		 * on the h5[1,4,5]xx handhelds. */
		SET_H5400_GPIO(BT_M_RESET, !on);
		
		/* Select the 'operating environment' pins to run/normal mode.
		 * Setting these pins to ENV_0 = 0 and ENV_1 = 1 would put the
		 * In-System-Programming (ISP) mode. In theory that would
		 * allow the host computer to rewrite the firmware.
		 */
		SET_H5400_GPIO(BT_ENV_0, on);
		SET_H5400_GPIO(BT_ENV_1, on);
		/* configure power pins */
		samcop_set_gpio_b(&h5400_samcop.dev,
		                  SAMCOP_GPIO_GPB_BLUETOOTH_3V0_ON,
		                  SAMCOP_GPIO_GPB_BLUETOOTH_3V0_ON);
		SET_H5400_GPIO(BT_2V8_N, !on);
                /* A 2ms delay between voltage application and reset driven
                 * high is a requirement of the power-on cycle of the device.
                 */
		mdelay(2);
		SET_H5400_GPIO(BT_M_RESET, on);
		led_trigger_event_shared(h5400_radio_trig, LED_FULL);
	} else {
		samcop_set_gpio_b(&h5400_samcop.dev,
		                  SAMCOP_GPIO_GPB_BLUETOOTH_3V0_ON,
		                  ~SAMCOP_GPIO_GPB_BLUETOOTH_3V0_ON);
		SET_H5400_GPIO(BT_2V8_N, !on);
		led_trigger_event_shared(h5400_radio_trig, LED_OFF);
	}
}

static void h5400_btuart_configure_priv(int state)
{
	switch (state) {
	case PXA_UART_CFG_PRE_STARTUP:
		pxa_gpio_mode(GPIO42_BTRXD_MD);
		pxa_gpio_mode(GPIO43_BTTXD_MD);
		pxa_gpio_mode(GPIO44_BTCTS_MD);
		pxa_gpio_mode(GPIO45_BTRTS_MD);
		h5400_bluetooth_power(1);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		h5400_bluetooth_power(0);
		break;

	default:
		break;
	}
}

static void h5400_hwuart_configure_priv(int state)
{
	switch (state) {
	case PXA_UART_CFG_PRE_STARTUP:
		pxa_gpio_mode(GPIO42_HWRXD_MD);
		pxa_gpio_mode(GPIO43_HWTXD_MD);
		pxa_gpio_mode(GPIO44_HWCTS_MD);
		pxa_gpio_mode(GPIO45_HWRTS_MD);
		h5400_bluetooth_power(1);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		h5400_bluetooth_power(0);
		break;

	default:
		break;
	}
}

static int __init h5400_bt_init(void)
{
	h5400_btuart_configure = h5400_btuart_configure_priv;
	h5400_hwuart_configure = h5400_hwuart_configure_priv;
	return 0;
}

static void __exit h5400_bt_exit(void)
{
	h5400_btuart_configure = NULL;
	h5400_hwuart_configure = NULL;
	return;
}

module_init(h5400_bt_init);
module_exit(h5400_bt_exit);
MODULE_LICENSE("GPL");
