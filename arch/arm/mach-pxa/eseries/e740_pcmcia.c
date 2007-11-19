/*
 * arch/arm/mach-pxa/e740_pcmcia.c
 *
 * Toshiba E-740 PCMCIA specific routines.
 *
 * Created:	20040704
 * Author:	Ian Molton
 * Copyright:	Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <pcmcia/ss.h>
#include <linux/platform_device.h>


#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-gpio.h>

#include "soc_common.h"

static struct pcmcia_irqs cd_irqs[] = {
        { 0, IRQ_GPIO(GPIO_E740_PCMCIA_CD0), "CF card detect" },
        { 1, IRQ_GPIO(GPIO_E740_PCMCIA_CD1), "Wifi switch" },
};

static int e740_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	skt->irq = skt->nr == 0 ? IRQ_GPIO(GPIO_E740_PCMCIA_RDY0):
	                          IRQ_GPIO(GPIO_E740_PCMCIA_RDY1);

        return soc_pcmcia_request_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Release all resources.
 */
static void e740_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

#define GPLR_BIT(n) (GPLR((n)) & GPIO_bit((n)))

static void e740_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
//      unsigned long gplr0 = GPLR0;   FIXME - this really ought to be atomic
//      unsigned long gplr1 = GPLR1;

	if(skt->nr == 0){
		state->detect = GPLR_BIT(GPIO_E740_PCMCIA_CD0) ? 0 : 1;
		state->ready  = GPLR_BIT(GPIO_E740_PCMCIA_RDY0) ? 1 : 0;
	}
	else{
		state->detect = GPLR_BIT(GPIO_E740_PCMCIA_CD1) ? 0 : 1;
		state->ready  = GPLR_BIT(GPIO_E740_PCMCIA_RDY1) ? 1 : 0;
	}

	state->vs_3v  = 1; //FIXME - is it right?

        state->bvd1   = 1;
        state->bvd2   = 1;
        state->wrprot = 0;
        state->vs_Xv  = 0;
}

#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

static int e740_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* reset it? */
	if(state->flags & SS_RESET) {
		if(skt->nr == 0)
			GPSR_BIT(GPIO_E740_PCMCIA_RST0);
		else
			GPSR_BIT(GPIO_E740_PCMCIA_RST1);
	} else {
		if(skt->nr == 0)
			GPCR_BIT(GPIO_E740_PCMCIA_RST0);
		else
			GPCR_BIT(GPIO_E740_PCMCIA_RST1);
	}

	/* Apply socket voltage */
	switch (state->Vcc) {
		case 0:
			if(skt->nr == 0)
				GPCR_BIT(GPIO_E740_PCMCIA_PWR0);
			else
				GPSR_BIT(GPIO_E740_PCMCIA_PWR1);
			break;
		case 50:
		case 33:
			/* Apply power to socket */
			if(skt->nr == 0)
				GPSR_BIT(GPIO_E740_PCMCIA_PWR0);
			else
				GPCR_BIT(GPIO_E740_PCMCIA_PWR1);
			break;
		default:
		printk (KERN_ERR "%s: Unsupported Vcc:%d\n", __FUNCTION__, state->Vcc);
	}

        return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static void e740_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_enable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Disable card status IRQs on suspend.
 */
static void e740_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_disable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

static struct pcmcia_low_level e740_pcmcia_ops = {
	.owner            = THIS_MODULE,
	.hw_init          = e740_pcmcia_hw_init,
	.hw_shutdown      = e740_pcmcia_hw_shutdown,
	.socket_state     = e740_pcmcia_socket_state,
	.configure_socket = e740_pcmcia_configure_socket,
	.socket_init      = e740_pcmcia_socket_init,
	.socket_suspend   = e740_pcmcia_socket_suspend,
	.nr               = 2,
};

static struct platform_device e740_pcmcia_device = {
	.name               = "pxa2xx-pcmcia",
	.dev = {
		.platform_data = &e740_pcmcia_ops,
	},
};

static int __init e740_pcmcia_init(void)
{
	int ret = 0;

	if(!machine_is_e740())
		return -ENODEV;

	printk("pcmcia: e740 detected.\n");

	ret = platform_device_register(&e740_pcmcia_device);

	return ret;
}

static void __exit e740_pcmcia_exit(void)
{
	/*
	 * This call is supposed to free our e740_pcmcia_device.
	 * Unfortunately platform_device don't have a free method, and
	 * we can't assume it's free of any reference at this point so we
	 * can't free it either.
	 */
	//platform_device_unregister(&e740_pcmcia_device);
}

module_init(e740_pcmcia_init);
module_exit(e740_pcmcia_exit);

MODULE_LICENSE("GPL");
