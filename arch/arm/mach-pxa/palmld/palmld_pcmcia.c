/*
 *
 * Driver for Palm LifeDrive PCMCIA
 *
 * Copyright (C) 2007 Marek Vasut <marek.vasut@gmail.com>
 * Copyright (C) 2006 Alex Osborne <bobofdoom@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/palmld-gpio.h>
#include <asm/irq.h>

#define DRV_NAME "palmld_pcmcia"

/* Debuging macro */
#define PALMLD_PCMCIA_DEBUG

#ifdef PALMLD_PCMCIA_DEBUG
#define palmld_pcmcia_dbg(format, args...) \
        printk(KERN_INFO DRV_NAME": " format, ## args)
#else
#define palmld_pcmcia_dbg(format, args...) do {} while (0)
#endif

static struct pcmcia_irqs palmld_socket_state_irqs[] = {
};

static int palmld_pcmcia_hw_init (struct soc_pcmcia_socket *skt)
{
        GPSR(GPIO48_nPOE_MD) =	GPIO_bit(GPIO48_nPOE_MD) |
				GPIO_bit(GPIO49_nPWE_MD) |
				GPIO_bit(GPIO50_nPIOR_MD) |
				GPIO_bit(GPIO51_nPIOW_MD) |
				GPIO_bit(GPIO85_nPCE_1_MD) |
				GPIO_bit(GPIO54_nPCE_2_MD) |
				GPIO_bit(GPIO79_pSKTSEL_MD) |
				GPIO_bit(GPIO55_nPREG_MD) |
				GPIO_bit(GPIO56_nPWAIT_MD) |
				GPIO_bit(GPIO57_nIOIS16_MD);

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

	skt->irq = IRQ_GPIO(GPIO_NR_PALMLD_PCMCIA_READY);

	palmld_pcmcia_dbg("%s:%i, Socket:%d\n", __FUNCTION__, __LINE__, skt->nr);
        return soc_pcmcia_request_irqs(skt, palmld_socket_state_irqs,
				       ARRAY_SIZE(palmld_socket_state_irqs));
}

static void palmld_pcmcia_hw_shutdown (struct soc_pcmcia_socket *skt)
{
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);
}

static void
palmld_pcmcia_socket_state (struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = 1; /* always inserted */
	state->ready  = GET_PALMLD_GPIO(PCMCIA_READY) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;
}

static int
palmld_pcmcia_configure_socket (struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	palmld_pcmcia_dbg("%s:%i Reset:%d Vcc:%d\n", __FUNCTION__, __LINE__,
			  (state->flags & SS_RESET) ? 1 : 0, state->Vcc);
	
	SET_PALMLD_GPIO(PCMCIA_POWER, 1);
	SET_PALMLD_GPIO(PCMCIA_RESET, (state->flags & SS_RESET) ? 1 : 0);
	
	return 0;
}

static void palmld_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);
}

static void palmld_pcmcia_socket_suspend (struct soc_pcmcia_socket *skt)
{ 
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);
}

static struct pcmcia_low_level palmld_pcmcia_ops = {
	.owner			= THIS_MODULE,

	/* Setting it this way makes pcmcia-cs scream about memory-cs
	   (because of HDD/CF memory in socket 0), but it's much nicer
	   than make some weird changes in pxa pcmcia subsystem */
	.first			= 0,
	.nr			= 2,

	.hw_init		= palmld_pcmcia_hw_init,
	.hw_shutdown		= palmld_pcmcia_hw_shutdown,

	.socket_state		= palmld_pcmcia_socket_state,
	.configure_socket	= palmld_pcmcia_configure_socket,

	.socket_init		= palmld_pcmcia_socket_init,
	.socket_suspend		= palmld_pcmcia_socket_suspend,
};


static void palmld_pcmcia_release (struct device * dev)
{
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);
}

static struct platform_device palmld_pcmcia_device = {
	.name           = "pxa2xx-pcmcia",
	.id             = 0,
	.dev            = {
		.platform_data  = &palmld_pcmcia_ops,
		.release	= palmld_pcmcia_release
	}
};

static int __init palmld_pcmcia_init(void)
{
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);

	if(!machine_is_xscale_palmld())
	    return -ENODEV;

	return platform_device_register (&palmld_pcmcia_device);
}

static void __exit palmld_pcmcia_exit(void)
{
	palmld_pcmcia_dbg("%s:%i\n", __FUNCTION__, __LINE__);

	platform_device_unregister (&palmld_pcmcia_device);
}

module_init(palmld_pcmcia_init);
module_exit(palmld_pcmcia_exit);

MODULE_AUTHOR ("Alex Osborne <bobofdoom@gmail.com>, Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION ("PCMCIA support for Palm LifeDrive");
MODULE_LICENSE ("GPL");
