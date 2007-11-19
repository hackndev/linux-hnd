/* 
 * Bluetooth driver for HP iPAQ h4000
 *
 * Copyright 2006 Anton Babushkin
 * Copyright 2006 Paul Sokolovsky
 *
 * Based on h2200.c, hx4700_bt.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2007-03-09 Eugeny Boger	Added state indication LED
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/dpm.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#include "../generic.h"

extern struct platform_device h4000_asic3;

#define BTUART_IDX 0
#define HWUART_IDX 1

DEFINE_LED_TRIGGER(bt_trig);

static void h4000_bluetooth_power(int uart, int on)
{
        int tries;

	if (on) {
		DPM_DEBUG("h4000_bt: Turning on\n");
		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_BLUETOOTH_3V3_ON, GPIOC_BLUETOOTH_3V3_ON);
		mdelay(5);

		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_BLUETOOTH_RESET_N, GPIOC_BLUETOOTH_RESET_N);
		asic3_set_gpio_out_b(&h4000_asic3.dev, GPIOB_BT_WAKE_UP, GPIOB_BT_WAKE_UP);

		/*
		 * BRF6xxx's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			mdelay(10);
		} while (((uart ? HWMSR : BTMSR) & MSR_CTS) == 0 && ++tries < 50);
		printk("BRF initialization delay: %d0ms\n", tries);
		led_trigger_event(bt_trig, LED_FULL);

	} else {
		DPM_DEBUG("h4000_bt: Turning off\n");
		asic3_set_gpio_out_b(&h4000_asic3.dev, GPIOB_BT_WAKE_UP, 0);
		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_BLUETOOTH_RESET_N, 0);
		udelay(100);
		asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_BLUETOOTH_3V3_ON, 0);
		// Just in case we'll activated immediately again, we should delay here
		mdelay(10);
		led_trigger_event(bt_trig, LED_OFF);
	}
}

static void
h4000_btuart_configure(int state)
{
        printk("%s: %d\n", __FUNCTION__, state);

	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP: /* pre UART enable */
	        pxa_gpio_mode(GPIO12_32KHz_MD);
		pxa_gpio_mode(GPIO42_BTRXD_MD);
		pxa_gpio_mode(GPIO43_BTTXD_MD);
		pxa_gpio_mode(GPIO44_BTCTS_MD);
		pxa_gpio_mode(GPIO45_BTRTS_MD);
		h4000_bluetooth_power(BTUART_IDX, 1);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* post UART disable */
		h4000_bluetooth_power(BTUART_IDX, 0);
		break;
		
	default:
		break;
	}
}

static void
h4000_hwuart_configure(int state)
{
        printk("%s: %d\n", __FUNCTION__, state);

	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP: /* post UART enable */
	        pxa_gpio_mode(GPIO12_32KHz_MD);
		pxa_gpio_mode(GPIO42_HWRXD_MD);
		pxa_gpio_mode(GPIO43_HWTXD_MD);
		pxa_gpio_mode(GPIO44_HWCTS_MD);
		pxa_gpio_mode(GPIO45_HWRTS_MD);
		h4000_bluetooth_power(HWUART_IDX, 1);
		break;

	case PXA_UART_CFG_POST_SHUTDOWN: /* post UART disable */
		h4000_bluetooth_power(HWUART_IDX, 0);
		break;

	default:
		break;
	}
}


static struct platform_pxa_serial_funcs h4000_btuart_funcs = {
	.configure = h4000_btuart_configure,
};

static struct platform_pxa_serial_funcs h4000_hwuart_funcs = {
	.configure = h4000_hwuart_configure,
};

static int __devinit h4000_bluetooth_probe(struct platform_device *pdev)
{
	pxa_set_btuart_info(&h4000_btuart_funcs);
	pxa_set_hwuart_info(&h4000_hwuart_funcs);
	led_trigger_register_timer("bluetooth", &bt_trig);
	led_trigger_event(bt_trig, LED_OFF);
	return 0;
}

static int __devexit h4000_bluetooth_remove(struct platform_device *pdev)
{
	h4000_bluetooth_power(-1, 0);
	led_trigger_unregister_timer(bt_trig);
	return 0;
}

static struct platform_driver bluetooth_driver = {
	.driver		= {	
		.name	= "h4000-bt",
	},
	.probe		= h4000_bluetooth_probe,
	.remove		= __devexit_p(h4000_bluetooth_remove),
};

static int __init h4000_bluetooth_init(void)
{
	printk(KERN_INFO "h4000 Bluetooth Driver\n");
	platform_driver_register(&bluetooth_driver);
	
	return 0;
}

static void __exit h4000_bluetooth_exit(void)
{
        platform_driver_unregister(&bluetooth_driver);
}

module_init(h4000_bluetooth_init);
module_exit(h4000_bluetooth_exit);

MODULE_AUTHOR("Anton Babushkin <uyamba@gmail.com>, Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("HP iPAQ h4000 Bluetooth Support Driver");
MODULE_LICENSE("GPL");
