
/*
 * drivers/pcmcia/sa1100_jornada56x.c
 *
 * PCMCIA implementation routines for the HP Jornada 56x
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/arch/jornada56x.h>
#include "sa1100_generic.h"

#define SOCKET0_POWER GPIO_GPIO7
static struct pcmcia_irqs irqs[] = {
	{ 1, IRQ_JORNADA_CF_REMOVE,   "Jornada56x CF Remove"   },
	{ 1, IRQ_JORNADA_CF_INSERT,   "Jornada56x CF Insert"   },
	{ 1, IRQ_JORNADA_CF_STSCHG,   "Jornada56x CF Status"   },
	{ 1, IRQ_JORNADA_CF_WAIT_ERR, "Jornada56x CF Wait err" },
};

static int jornada56x_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	JORNADA_GPDPDR |= SOCKET0_POWER;
	JORNADA_GPDPSR = SOCKET0_POWER;

	skt->irq = IRQ_JORNADA_CF;

	return soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void jornada56x_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void jornada56x_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	unsigned long status = JORNADA_CFSR;

	state->detect=(status & JORNADA_CF_VALID)?1:0;
	state->ready=(status & JORNADA_CF_READY)?1:0;
	state->bvd1=(status & JORNADA_CF_BVD1)?1:0;
	state->bvd2=(status & JORNADA_CF_BVD2)?1:0;
	state->wrprot=(status & JORNADA_CF_WP)?1:0;
	state->vs_3v=(status & JORNADA_CF_VS1)?0:1;
	state->vs_Xv=(status & JORNADA_CF_VS2)?0:2;
}

static int jornada56x_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	switch (state->Vcc) {
	case 0:
		JORNADA_GPDPSR = SOCKET0_POWER;
		break;
	case 33:
		JORNADA_GPDPCR = SOCKET0_POWER;
		JORNADA_CFCR = JORNADA_CF_PWAIT_EN | JORNADA_CF_FLT;
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__, state->Vcc);
		return -1;
	}
	if (state->flags & SS_RESET) {
		JORNADA_CFCR |= JORNADA_CF_RESET;
		}
	else {
		JORNADA_CFCR &= ~JORNADA_CF_RESET;
	}
	return 0;
}

static void jornada56x_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	JORNADA_GPDPSR = SOCKET0_POWER;
	soc_pcmcia_enable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void jornada56x_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	JORNADA_GPDPSR = SOCKET0_POWER;
	soc_pcmcia_disable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static struct pcmcia_low_level jornada56x_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.hw_init		= jornada56x_pcmcia_hw_init,
	.hw_shutdown		= jornada56x_pcmcia_hw_shutdown,
	.socket_state		= jornada56x_pcmcia_socket_state,
	.configure_socket	= jornada56x_pcmcia_configure_socket,
	.socket_init		= jornada56x_pcmcia_socket_init,
	.socket_suspend		= jornada56x_pcmcia_socket_suspend,
};

int __init pcmcia_jornada56x_init(struct device *dev)
{
	int ret = -ENODEV;

	if (machine_is_jornada56x())
		ret = sa11xx_drv_pcmcia_probe(dev, &jornada56x_pcmcia_ops, 0, 1);

	return ret;
};
