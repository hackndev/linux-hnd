 /*
  *   h4000_pcmcia.c
  *
  *   Created Apr 2, 2005, by Shawn Anderson
  *
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License version 2 as
  *  published by the Free Software Foundation.
  *
  * * * */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include "../../../../drivers/pcmcia/soc_common.h"
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>

extern struct platform_device h4000_asic3;

static int power_state = 0;

static struct pcmcia_irqs socket_state_irqs[] = {
//	{0, IRQ_GPIO(GPIO_NR_H4000_WLAN_MAC_IRQ_N), "PCMCIA0"},
};

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

static int h4000_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	GPSR(GPIO48_nPOE) = GPIO_bit(GPIO48_nPOE) | GPIO_bit(GPIO49_nPWE) |
	    GPIO_bit(GPIO52_nPCE_1_MD) | GPIO_bit(GPIO53_nPCE_2_MD) |
	    GPIO_bit(GPIO55_nPREG_MD) | GPIO_bit(GPIO56_nPWAIT_MD);

	pxa_gpio_mode(GPIO48_nPOE_MD);
	pxa_gpio_mode(GPIO49_nPWE_MD);
	pxa_gpio_mode(GPIO52_nPCE_1_MD);
	pxa_gpio_mode(GPIO55_nPREG_MD);
	pxa_gpio_mode(GPIO56_nPWAIT_MD);

/*	asic3_set_clock_cdex(&h4000_asic3.dev, CLOCK_CDEX_EX1, CLOCK_CDEX_EX1);
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET,
				  GPIOD_WLAN_MAC_RESET);
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_WLAN_MAC_RESET, 0);*/

	skt->irq = IRQ_GPIO(GPIO_NR_H4000_WLAN_MAC_IRQ_N);

	return soc_pcmcia_request_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void h4000_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	soc_pcmcia_free_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void h4000_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				      struct pcmcia_state *state)
{
	//This function is polled at regular intervals
	//printk("%s\n", __FUNCTION__);
	state->detect = 1;	// always attached
	state->ready = GET_H4000_GPIO(WLAN_MAC_IRQ_N) ? 1 : 0;
	state->bvd1 = 1;
	state->bvd2 = 1;
	state->wrprot = 0;
	state->vs_3v = asic3_get_gpio_status_c(&h4000_asic3.dev)
	    & GPIOC_WLAN_POWER_ON;
	state->vs_Xv = 0;
};

static int h4000_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
					 const socket_state_t * state)
{
	printk("%s: Flags:%x Reset:%d Output En: %d Vcc:%d\n", __FUNCTION__,
	    state->flags, !!(state->flags & SS_RESET), !!(state->flags & SS_OUTPUT_ENA), state->Vcc);

	/* Silently ignore Vpp, output enable, speaker enable. */
	switch (state->Vcc) {
	case 0:
		if (power_state) {
			h4000_wlan_stop();
			power_state = 0;
			return 0;
		}
		break;
	case 50:
	case 33:
		if (!power_state) {
			h4000_wlan_start();
			power_state = 1;
			return 0;
		}
		break;
	default:
		printk(KERN_ERR "%s: Unsupported Vcc:%d\n", __FUNCTION__,
		       state->Vcc);
	}
	return 0;

}

static void h4000_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	soc_pcmcia_enable_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void h4000_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	soc_pcmcia_disable_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static struct pcmcia_low_level h4000_pcmcia_ops = {
	.owner            = THIS_MODULE,
	.first            = 0,
	.nr               = 1,
	.hw_init          = h4000_pcmcia_hw_init,
	.hw_shutdown      = h4000_pcmcia_hw_shutdown,
	.socket_state     = h4000_pcmcia_socket_state,
	.configure_socket = h4000_pcmcia_configure_socket,
	.socket_init      = h4000_pcmcia_socket_init,
	.socket_suspend   = h4000_pcmcia_socket_suspend,
};

static struct platform_device h4000_pcmcia = {
	.name = "pxa2xx-pcmcia",
	.dev = {.platform_data = &h4000_pcmcia_ops},
};

static int __init h4000_pcmcia_probe(struct platform_device *pdev)
{
	return platform_device_register(&h4000_pcmcia);
}

static struct platform_driver h4000_pcmcia_driver = {
	.driver = {
	    .name     = "h4000-pcmcia",
	},
        .probe    = h4000_pcmcia_probe,
};

static int __init h4000_pcmcia_init(void)
{  
	return platform_driver_register(&h4000_pcmcia_driver);
}

static void __exit h4000_pcmcia_exit(void)
{
	platform_device_unregister(&h4000_pcmcia);
	platform_driver_unregister(&h4000_pcmcia_driver);
}

module_init(h4000_pcmcia_init);
module_exit(h4000_pcmcia_exit);

MODULE_LICENSE("GPL");
