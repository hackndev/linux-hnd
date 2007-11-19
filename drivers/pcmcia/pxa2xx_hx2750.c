/*
 * PCMCIA Support for HP iPAQ hx2750
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * Based on pxa2xx_mainstone.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx2750.h>

#include "soc_common.h"


static struct pcmcia_irqs irqs[] = {
	{ 0, HX2750_IRQ_GPIO_CF_DETECT, "PCMCIA0 CD" },
};

static int hx2750_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	/*
	 * Setup default state of GPIO outputs
	 * before we enable them as outputs.
	 */
	GPSR(GPIO48_nPOE) =
		GPIO_bit(GPIO48_nPOE) |
		GPIO_bit(GPIO49_nPWE) |
		GPIO_bit(GPIO50_nPIOR) |
		GPIO_bit(GPIO51_nPIOW) |
		GPIO_bit(GPIO85_nPCE_1) |
		GPIO_bit(GPIO54_nPCE_2);

	pxa_gpio_mode(GPIO48_nPOE_MD);
	pxa_gpio_mode(GPIO49_nPWE_MD);
	pxa_gpio_mode(GPIO50_nPIOR_MD);
	pxa_gpio_mode(GPIO51_nPIOW_MD);
	pxa_gpio_mode(GPIO85_nPCE_1_MD);
	pxa_gpio_mode(GPIO54_nPCE_2_MD);
	pxa_gpio_mode(GPIO79_pSKTSEL_MD);
	pxa_gpio_mode(GPIO55_nPREG_MD);
	pxa_gpio_mode(GPIO56_nPWAIT_MD);
	pxa_gpio_mode(GPIO57_nIOIS16_MD);

	skt->irq = (skt->nr == 0) ? HX2750_IRQ_GPIO_CF_IRQ : HX2750_IRQ_GPIO_CF_WIFIIRQ;
	return soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void hx2750_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

#define GPLR_BIT(n) (GPLR((n)) & GPIO_bit((n)))
#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

static void hx2750_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				    struct pcmcia_state *state)
{
	if(skt->nr == 0){
		state->detect = GPLR_BIT(HX2750_GPIO_CF_DETECT) ? 0 : 1;
		state->ready  = GPLR_BIT(HX2750_GPIO_CF_IRQ) ? 1 : 0;
	}
	else{
		state->detect = 1;
		state->ready  = GPLR_BIT(HX2750_GPIO_CF_WIFIIRQ) ? 1 : 0;
	}

	state->bvd1   = 1;  /* not available */
	state->bvd2   = 1;  /* not available */
	state->vs_3v  = 1;  /* not available */
	state->vs_Xv  = 0;  /* not available */
	state->wrprot = 0;  /* not available */
}

static int hx2750_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
				       const socket_state_t *state)
{
	int ret = 0;

	if(state->flags & SS_RESET) {
		if(skt->nr == 0)
			{}//hx2750_set_egpio(HX2750_EGPIO_CF0_RESET);
		else
			hx2750_set_egpio(HX2750_EGPIO_CF1_RESET);
	} else {
		if(skt->nr == 0)
			{}//hx2750_clear_egpio(HX2750_EGPIO_CF0_RESET);
		else
			hx2750_clear_egpio(HX2750_EGPIO_CF1_RESET);
	}

	/* Apply socket voltage */
	switch (state->Vcc) {
		case 0:
			if(skt->nr == 0)
				GPCR_BIT(HX2750_GPIO_CF_PWR);
			else
				hx2750_clear_egpio(HX2750_EGPIO_WIFI_PWR);
			break;
		case 50:
		case 33:
			/* Apply power to socket */
			if(skt->nr == 0)
				GPSR_BIT(HX2750_GPIO_CF_PWR);
			else
				hx2750_set_egpio(HX2750_EGPIO_WIFI_PWR);
			break;
		default:
			printk (KERN_ERR "%s: Unsupported Vcc:%d\n", __FUNCTION__, state->Vcc);
			ret = -1;
			break;
	}
	
	return ret;
}

static void hx2750_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
}

static void hx2750_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
}

static struct pcmcia_low_level hx2750_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.hw_init		= hx2750_pcmcia_hw_init,
	.hw_shutdown		= hx2750_pcmcia_hw_shutdown,
	.socket_state		= hx2750_pcmcia_socket_state,
	.configure_socket	= hx2750_pcmcia_configure_socket,
	.socket_init		= hx2750_pcmcia_socket_init,
	.socket_suspend		= hx2750_pcmcia_socket_suspend,
	.nr			= 2,
};

static struct platform_device *hx2750_pcmcia_device;

static void hx2750_pcmcia_release(struct device *dev)
{
	kfree(hx2750_pcmcia_device);
}

static int __init hx2750_pcmcia_init(void)
{
	int ret;

	hx2750_pcmcia_device = kmalloc(sizeof(*hx2750_pcmcia_device), GFP_KERNEL);
	if (!hx2750_pcmcia_device)
		return -ENOMEM;
	memset(hx2750_pcmcia_device, 0, sizeof(*hx2750_pcmcia_device));
	hx2750_pcmcia_device->name = "pxa2xx-pcmcia";
	hx2750_pcmcia_device->dev.platform_data = &hx2750_pcmcia_ops;
	hx2750_pcmcia_device->dev.release = hx2750_pcmcia_release;

	ret = platform_device_register(hx2750_pcmcia_device);
	if (ret)
		kfree(hx2750_pcmcia_device);

	return ret;
}

static void __exit hx2750_pcmcia_exit(void)
{
	platform_device_unregister(hx2750_pcmcia_device);
}

module_init(hx2750_pcmcia_init);
module_exit(hx2750_pcmcia_exit);

MODULE_AUTHOR("Richard Purdie <richard@o-hand.com>");
MODULE_DESCRIPTION("iPAQ hx2750 PCMCIA Support");
MODULE_LICENSE("GPLv2");
