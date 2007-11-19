/*
 * arch/arm/mach-pxa/e800_pcmcia.c
 *
 * Toshiba E-800 PCMCIA specific routines.
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
#include <asm/arch/eseries-irq.h>
#include <asm/arch/eseries-gpio.h>

#include "soc_common.h"

static struct pcmcia_irqs cd_irqs[] = {
        { 0, ANGELX_CD0_IRQ, "CF card detect" },
        { 1, ANGELX_CD1_IRQ, "Wifi switch" },
	{ 0, ANGELX_ST0_IRQ, "ST0" },
	{ 1, ANGELX_ST1_IRQ, "ST1" },
};

static int e800_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	skt->irq = skt->nr == 0 ? ANGELX_RDY0_IRQ : ANGELX_RDY1_IRQ;

	return soc_pcmcia_request_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Release all resources.
 */
static void e800_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

#define GPLR_BIT(n) (GPLR((n)) & GPIO_bit((n)))

extern unsigned char angelx_get_state(int skt);
static void e800_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	unsigned char socket_state = angelx_get_state(skt->nr);
	unsigned long gplr0 = GPLR0; //  FIXME - this really ought to be atomic
	unsigned long gplr2 = GPLR2;


	if(skt->nr == 0){
	printk("pcmaia state: %d %02x\n", skt->nr, socket_state);
		state->vs_3v  = gplr0 & GPIO_bit(20) ? 1 : 0;
	}
	else{
		state->vs_3v  = gplr2 & GPIO_bit(73) ? 1 : 0;
	}

	state->vs_3v  = 1; //FIXME - is it right?

	state->detect = socket_state & 0x04 ? 0 : 1;
	state->ready  = socket_state & 0x10 ? 0 : 1;
        state->bvd1   = 1;
        state->bvd2   = 1;
        state->wrprot = 0;
        state->vs_Xv  = 0;
}

#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

static int e800_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* reset it? */
	printk("pcmcia reset state %d %s\n", skt->nr, state->flags & SS_RESET ? "asserted" : "deasserted");
#if 0
	if(state->flags & SS_RESET) {
		if(skt->nr == 0)
			GPSR_BIT(GPIO_E800_PCMCIA_RST0);
		else
			GPSR_BIT(GPIO_E800_PCMCIA_RST1);
	} else {
		if(skt->nr == 0)
			GPCR_BIT(GPIO_E800_PCMCIA_RST0);
		else
			GPSR_BIT(GPIO_E800_PCMCIA_RST1);
	}
#endif

	/* Apply socket voltage */
	switch (state->Vcc) {
		case 0:
			if(skt->nr == 0)
				GPCR_BIT(GPIO_E800_PCMCIA_PWR0);
			else
				GPCR_BIT(GPIO_E800_PCMCIA_PWR1);
			break;
		case 50:
		case 33:
			/* Apply power to socket */
			if(skt->nr == 0)
				GPSR_BIT(GPIO_E800_PCMCIA_PWR0);
			else
				GPSR_BIT(GPIO_E800_PCMCIA_PWR1);
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
static void e800_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_enable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Disable card status IRQs on suspend.
 */
static void e800_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_disable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

static struct pcmcia_low_level e800_pcmcia_ops = {
	.owner            = THIS_MODULE,
	.hw_init          = e800_pcmcia_hw_init,
	.hw_shutdown      = e800_pcmcia_hw_shutdown,
	.socket_state     = e800_pcmcia_socket_state,
	.configure_socket = e800_pcmcia_configure_socket,
	.socket_init      = e800_pcmcia_socket_init,
	.socket_suspend   = e800_pcmcia_socket_suspend,
	.nr               = 2,
};

static struct platform_device e800_pcmcia_device = {
	.name               = "pxa2xx-pcmcia",
	.dev = {
		.platform_data = &e800_pcmcia_ops,
	},
};

static int __init e800_pcmcia_init(void)
{
	int ret = 0;

	if(!machine_is_e800())
		return -ENODEV;

	angelx_init_irq(IRQ_GPIO(GPIO_E800_ANGELX_IRQ), PXA_CS5_PHYS);

	//return -ENODEV;
	printk("pcmcia: e800 detected.\n");

	ret = platform_device_register(&e800_pcmcia_device);

	return ret;
}

static void __exit e800_pcmcia_exit(void)
{
	/*
	 * This call is supposed to free our e800_pcmcia_device.
	 * Unfortunately platform_device don't have a free method, and
	 * we can't assume it's free of any reference at this point so we
	 * can't free it either.
	 */
	//platform_device_unregister(&e800_pcmcia_device);
}

module_init(e800_pcmcia_init);
module_exit(e800_pcmcia_exit);

MODULE_LICENSE("GPL");
