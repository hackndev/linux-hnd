/*
 *  Asus MyPal A716 Bluetooth glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/serial.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>

extern int a716_wireless_switch;
extern int a716_wifi_pcmcia_detect;

static void a716_btuart_configure(int state)
{
	switch (state) {
		case PXA_UART_CFG_PRE_STARTUP:
			break;
			
		case PXA_UART_CFG_POST_STARTUP:
			a716_wifi_pcmcia_detect = 0;
			pxa_gpio_mode(GPIO42_BTRXD_MD);
			pxa_gpio_mode(GPIO43_BTTXD_MD);
			pxa_gpio_mode(GPIO44_BTCTS_MD);
			pxa_gpio_mode(GPIO45_BTRTS_MD);
			a716_gpo_set(GPO_A716_BLUETOOTH_POWER);
			mdelay(5);
			a716_gpo_set(GPO_A716_BLUETOOTH_RESET);
			mdelay(5);
			a716_gpo_clear(GPO_A716_BLUETOOTH_RESET);
			// turn on blue LED
			GPSR(GPIO_NR_A716_BLUE_LED_ENABLE) = GPIO_bit(GPIO_NR_A716_BLUE_LED_ENABLE);
			a716_wireless_switch = 1;
			break;
		case PXA_UART_CFG_PRE_SHUTDOWN:
			a716_gpo_clear(GPO_A716_BLUETOOTH_POWER);
			// turn off blue LED
			GPCR(GPIO_NR_A716_BLUE_LED_ENABLE) = GPIO_bit(GPIO_NR_A716_BLUE_LED_ENABLE);
			a716_wireless_switch = 0;
			break;

		case PXA_UART_CFG_POST_SHUTDOWN:
			break;

		default:
			break;
	}
}

static struct platform_pxa_serial_funcs a716_btuart_funcs = {
	.configure = a716_btuart_configure,
};

static int __devinit a716_bt_probe(struct platform_device *pdev)
{
	pxa_set_btuart_info(&a716_btuart_funcs);

	return 0;
}

static int a716_bt_resume(struct platform_device *pdev)
{
	if (a716_wireless_switch != 0)
		GPSR(GPIO_NR_A716_BLUE_LED_ENABLE) = GPIO_bit(GPIO_NR_A716_BLUE_LED_ENABLE);

	return 0;
}

static struct platform_driver a716_bt_driver = {
	.driver   = {
		.name     = "a716-bt",
	},
	.probe    = a716_bt_probe,
	.resume   = a716_bt_resume,
};

static int __init a716_bt_init(void)
{
	return platform_driver_register(&a716_bt_driver);
}

module_init(a716_bt_init);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 Bluetooth glue driver");
MODULE_LICENSE("GPL");
