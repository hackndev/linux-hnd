/*
 * arch/arm/mach-pxa/e750_pcmcia.c
 *
 * Toshiba E-750 PCMCIA specific routines.
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
#include <linux/platform_device.h>

#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-gpio.h>

#include "soc_common.h"

static struct pcmcia_irqs cd_irqs[] = {
        { 0, IRQ_GPIO(GPIO_E750_PCMCIA_CD0), "CF card detect" },
};

static int e750_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	skt->irq = IRQ_GPIO(GPIO_E750_PCMCIA_RDY0);

        return soc_pcmcia_request_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Release all resources.
 */
static void e750_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

#define GPLR_BIT(n) (GPLR((n)) & GPIO_bit((n)))

static void e750_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
//      unsigned long gplr0 = GPLR0;   FIXME - this really ought to be atomic
//      unsigned long gplr1 = GPLR1;

	state->detect = GPLR_BIT(GPIO_E740_PCMCIA_CD0) ? 0 : 1;
	state->ready  = GPLR_BIT(GPIO_E740_PCMCIA_RDY0) ? 1 : 0;

	state->vs_3v  = 1; //FIXME - is it right?

        state->bvd1   = 1;
        state->bvd2   = 1;
        state->wrprot = 0;
        state->vs_Xv  = 0;
}

#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

static int e750_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* reset it? */
	if(state->flags & SS_RESET)
		GPSR_BIT(GPIO_E740_PCMCIA_RST0);
	else 
		GPCR_BIT(GPIO_E740_PCMCIA_RST0);
	

	/* Apply socket voltage */
	switch (state->Vcc) {
		case 0:
			GPCR_BIT(GPIO_E740_PCMCIA_PWR0);
			break;
		case 50:
		case 33:
			/* Apply power to socket */
			GPSR_BIT(GPIO_E740_PCMCIA_PWR0);
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
static void e750_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_enable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Disable card status IRQs on suspend.
 */
static void e750_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_disable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

static struct pcmcia_low_level e750_pcmcia_ops = {
	.owner            = THIS_MODULE,
	.hw_init          = e750_pcmcia_hw_init,
	.hw_shutdown      = e750_pcmcia_hw_shutdown,
	.socket_state     = e750_pcmcia_socket_state,
	.configure_socket = e750_pcmcia_configure_socket,
	.socket_init      = e750_pcmcia_socket_init,
	.socket_suspend   = e750_pcmcia_socket_suspend,
	.nr               = 1,
};

static struct platform_device e750_pcmcia_device = {
	.name               = "pxa2xx-pcmcia",
	.dev = {
		.platform_data = &e750_pcmcia_ops,
	},
};

static int __init e750_pcmcia_init(void)
{
	int ret = 0;

	if(!machine_is_e750())
		return -ENODEV;

	printk("pcmcia: e750 detected.\n");

	ret = platform_device_register(&e750_pcmcia_device);

	return ret;
}

static void __exit e750_pcmcia_exit(void)
{
	/*
	 * This call is supposed to free our e750_pcmcia_device.
	 * Unfortunately platform_device don't have a free method, and
	 * we can't assume it's free of any reference at this point so we
	 * can't free it either.
	 */
	//platform_device_unregister(&e750_pcmcia_device);
}

module_init(e750_pcmcia_init);
module_exit(e750_pcmcia_exit);

MODULE_LICENSE("GPL");
