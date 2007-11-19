/*
 * WLAN (TI TNETW1100B) support in the h4000.
 *
 * Copyright (c) 2006 SDG Systems, LLC
 * Copyright (c) 2006 Paul Sokolovsky
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Based on hx4700_acx.c
 */

#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>
#include <linux/mfd/asic3_base.h>
//#include <asm/arch/ipaq-asic3.h>
#include <asm/io.h>
//#include "acx_hw.h"

// Mem area of PCMCIA socket #0
#define WLAN_BASE	0x2c000000

extern struct platform_device h4000_asic3;

static irqreturn_t h4000_wifi_isr(int irq, void *dev_id)
{
        printk("IRQ: GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));

	return IRQ_HANDLED;
}

int h4000_wlan_start(void)
{
        int timeout;
	printk("h4000_wlan_start\n");

        printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
        asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET, GPIOD_WLAN_MAC_RESET);
//        printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
        mdelay(100);
//        printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
        
	asic3_set_gpio_dir_b(&h4000_asic3.dev, 1<<GPIOB_WLAN_SOMETHING, 0);
//	printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
        
	asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_WLAN_POWER_ON, GPIOC_WLAN_POWER_ON);
//	printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
	mdelay(30);
//	printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));
	mdelay(100);

//	MECR |= MECR_CIT;
	printk("GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N));

	mdelay(100);
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET, 0);

        int val = -1;
	for (timeout = 250; timeout; timeout--) {
		int v2 = !!gpio_get_value(GPIO_NR_H4000_WLAN_MAC_IRQ_N);

		if (v2 != val) {
        		printk("%d: GPIO_NR_H4000_WLAN_MAC_IRQ_N=%d\n", timeout, v2);
        		val = v2;
		}

		if (v2) break;
		mdelay(1);
	}

	if (!timeout) printk("Timeout waiting for WLAN\n");

	return 0;
}

int h4000_wlan_stop(void)
{
	printk("h4000_wlan_stop\n");
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET, GPIOD_WLAN_MAC_RESET);
	mdelay(100);
	asic3_set_gpio_out_c(&h4000_asic3.dev, GPIOC_WLAN_POWER_ON, 0);
	asic3_set_gpio_dir_b(&h4000_asic3.dev, 1<<GPIOB_WLAN_SOMETHING, 1);
	mdelay(100);
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET, 0);

//	MECR &= ~MECR_CIT;

	return 0;
}
