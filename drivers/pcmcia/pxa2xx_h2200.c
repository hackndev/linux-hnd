/*
 * iPAQ h2200 PCMCIA support
 * 
 * Copyright (c) 2004 Koen Kooi <koen@handhelds.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/soc-old.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/h2200-gpio.h>

#include "soc_common.h"


#if 0
#  define _debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define _debug(s, args...)
#endif
#define _debug_func(s, args...) _debug ("%s: " s, __FUNCTION__, ##args)

static struct pcmcia_irqs h2200_cd_irq[] = {
	{ 0, H2200_IRQ (CF_DETECT_N),  "PCMCIA CD" }
};

static void h2200_cf_rst (int state)
{
	_debug_func ("\n");

	SET_H2200_GPIO (CF_RESET, state);
}

static int h2200_pcmcia_hw_init (struct soc_pcmcia_socket *skt)
{
	unsigned long flags;

	_debug_func ("\n");

	skt->irq = H2200_IRQ (CF_INT_N);

	// This must go away as we'll fill h2200-init.h
	local_irq_save (flags);
	pxa_gpio_mode (GPIO_NR_H2200_CF_RESET | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_H2200_CF_ADD_EN_N | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_H2200_CF_POWER_EN | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_H2200_CF_BUFF_EN | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_H2200_CF_DETECT_N | GPIO_IN);
        pxa_gpio_mode (GPIO_NR_H2200_CF_INT_N | GPIO_IN);
	local_irq_restore (flags);

	return soc_pcmcia_request_irqs(skt, h2200_cd_irq, ARRAY_SIZE(h2200_cd_irq));
}

/*
 * Release all resources.
 */
static void h2200_pcmcia_hw_shutdown (struct soc_pcmcia_socket *skt)
{
	_debug_func ("\n");
	soc_pcmcia_free_irqs(skt, h2200_cd_irq, ARRAY_SIZE(h2200_cd_irq));
}

static void h2200_pcmcia_socket_state (struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = GET_H2200_GPIO (CF_DETECT_N) ? 0 : 1;
	state->ready  = GET_H2200_GPIO (CF_INT_N) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = GET_H2200_GPIO (CF_POWER_EN) ? 1 : 0;
	state->vs_Xv  = 0;

	_debug ("detect:%d ready:%d vcc:%d\n",
	       state->detect, state->ready, state->vs_3v);
}

static int h2200_pcmcia_configure_socket (struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* Silently ignore Vpp, output enable, speaker enable. */
	_debug_func ("Reset:%d Vcc:%d\n", (state->flags & SS_RESET) ? 1 : 0,
		    state->Vcc);

	h2200_cf_rst (state->flags & SS_RESET);

	/* Apply socket voltage */
	switch (state->Vcc) {
	case 0:
		SET_H2200_GPIO (CF_POWER_EN, 0);
		SET_H2200_GPIO_N (CF_ADD_EN, 0);
		SET_H2200_GPIO (CF_BUFF_EN, 0);
		break;
	case 50:
	case 33:
		/* Apply power to socket */
		SET_H2200_GPIO (CF_POWER_EN, 1);
		SET_H2200_GPIO_N (CF_ADD_EN, 1);
		SET_H2200_GPIO (CF_BUFF_EN, 1);
		break;
	default:
		printk (KERN_ERR "%s: Unsupported Vcc:%d\n",
			__FUNCTION__, state->Vcc);
	}

	return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static void h2200_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	_debug_func ("\n");
}

/*
 * Disable card status IRQs on suspend.
 */
static void h2200_pcmcia_socket_suspend (struct soc_pcmcia_socket *skt)
{
	_debug_func ("\n");
	h2200_cf_rst (1);
}

static struct pcmcia_low_level h2200_pcmcia_ops = {
	.owner          	= THIS_MODULE,

	.first          	= 0,
	.nr         		= 1,

	.hw_init        	= h2200_pcmcia_hw_init,
	.hw_shutdown		= h2200_pcmcia_hw_shutdown,

	.socket_state		= h2200_pcmcia_socket_state,
	.configure_socket	= h2200_pcmcia_configure_socket,

	.socket_init		= h2200_pcmcia_socket_init,
	.socket_suspend		= h2200_pcmcia_socket_suspend,
};

static struct platform_device h2200_pcmcia_device = {
	.name			= "pxa2xx-pcmcia",
	.dev			= {
		.platform_data	= &h2200_pcmcia_ops,
	}
};

static int __init h2200_pcmcia_init (void)
{
	_debug_func ("\n");
	return platform_device_register (&h2200_pcmcia_device);
}

static void __exit
h2200_pcmcia_exit(void)
{
	platform_device_unregister (&h2200_pcmcia_device);
}

module_init(h2200_pcmcia_init);
module_exit(h2200_pcmcia_exit);

MODULE_AUTHOR("Koen Kooi <koen@handhelds.org>");
MODULE_DESCRIPTION("HP iPAQ h2200 PCMCIA/CF platform-specific driver");
MODULE_LICENSE("GPL");
