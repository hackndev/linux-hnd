/*
 * Dell Axim X5 PCMCIA support
 *
 * Copyright © 2004 Andrew Zabolotny <zap@homelink.ru>
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
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/soc-old.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/aximx5-gpio.h>
#include <asm/irq.h>

#include <linux/mfd/mq11xx.h>

#if 0
#  define debug_out(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug_out(s, args...)
#endif
#define debug_func(s, args...) debug_out ("%s: " s, __FUNCTION__, ##args)

/* PCMCIA interrupt number, multiplexed off the MediaQ chip */
#define IRQ_PCMCIA	(mq_base->irq_base + IRQ_MQ_GPIO_2)

static struct mediaq11xx_base *mq_base;

static struct pcmcia_irqs aximx5_cd_irqs[] = {
	{ 0, AXIMX5_IRQ (PCMCIA_DETECT_N),  "PCMCIA CD" }
};

static void aximx5_cf_rst (int state)
{
	debug_func ("%d\n", state);

	SET_AXIMX5_GPIO (PCMCIA_RESET, state);
}

static int aximx5_pcmcia_hw_init (struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");

	skt->irq = IRQ_PCMCIA;

	/* GPIOs are configured in machine initialization code */

	return soc_pcmcia_request_irqs(skt, aximx5_cd_irqs,
				       ARRAY_SIZE(aximx5_cd_irqs));
}

/*
 * Release all resources.
 */
static void aximx5_pcmcia_hw_shutdown (struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");
	soc_pcmcia_free_irqs(skt, aximx5_cd_irqs, ARRAY_SIZE(aximx5_cd_irqs));
}

static void
aximx5_pcmcia_socket_state (struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = GET_AXIMX5_GPIO (PCMCIA_DETECT_N) ? 0 : 1;
        state->ready  = mq_base->get_GPIO (mq_base, 2) ? 1 : 0;
	state->bvd1   = GET_AXIMX5_GPIO (PCMCIA_BVD1) ? 1 : 0;
	state->bvd2   = GET_AXIMX5_GPIO (PCMCIA_BVD2) ? 1 : 0;
	state->wrprot = 0;
	state->vs_3v  = mq_base->get_GPIO (mq_base, 65) ? 1 : 0;
	state->vs_Xv  = 0;

	debug_out ("detect:%d ready:%d vcc:%d bvd1:%d bvd2:%d\n",
		   state->detect, state->ready, state->vs_3v, state->bvd1, state->bvd2);
}

static int
aximx5_pcmcia_configure_socket (struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* Don't enable MediaQ power more than once as it keeps a on/off
	 * counter and this can prevent from powering the device off when
         * it is no longer in use.
	 */
	static int mq_power_state = 0;

	/* Silently ignore Vpp, output enable, speaker enable. */
	debug_func ("Reset:%d Vcc:%d\n", (state->flags & SS_RESET) ? 1 : 0,
		    state->Vcc);

	aximx5_cf_rst (state->flags & SS_RESET);

	/* Apply socket voltage */
	switch (state->Vcc) {
	case 0:
		mq_base->set_GPIO (mq_base, 65, MQ_GPIO_IN | MQ_GPIO_OUT0);
		/* Power off the SPI subdevice */
		if (mq_power_state) {
			mq_base->set_power (mq_base, MEDIAQ_11XX_SPI_DEVICE_ID, 0);
			mq_power_state = 0;
		}
		/* Disable PCMCIA address bus and buffer */
		SET_AXIMX5_GPIO_N (PCMCIA_BUFF_EN, 0);
		SET_AXIMX5_GPIO_N (PCMCIA_ADD_EN, 0);
		break;
	case 50:
	case 33:
		/* Power on the SPI subdevice as it is responsible for mqGPIO65 */
		if (!mq_power_state) {
			mq_base->set_power (mq_base, MEDIAQ_11XX_SPI_DEVICE_ID, 1);
			mq_power_state = 1;
		}
		/* Enable PCMCIA address bus and buffer */
		SET_AXIMX5_GPIO_N (PCMCIA_BUFF_EN, 1);
		SET_AXIMX5_GPIO_N (PCMCIA_ADD_EN, 1);
		/* Apply power to socket */
		mq_base->set_GPIO (mq_base, 65, MQ_GPIO_IN | MQ_GPIO_OUT1);
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
static void aximx5_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");
	soc_pcmcia_enable_irqs (skt, aximx5_cd_irqs, ARRAY_SIZE(aximx5_cd_irqs));
}

/*
 * Disable card status IRQs on suspend.
 */
static void aximx5_pcmcia_socket_suspend (struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");
	aximx5_cf_rst (1);
	soc_pcmcia_disable_irqs(skt, aximx5_cd_irqs, ARRAY_SIZE(aximx5_cd_irqs));
}

static struct pcmcia_low_level aximx5_pcmcia_ops = {
	.owner			= THIS_MODULE,

	.first			= 0,
	.nr			= 1,

	.hw_init		= aximx5_pcmcia_hw_init,
	.hw_shutdown		= aximx5_pcmcia_hw_shutdown,

	.socket_state		= aximx5_pcmcia_socket_state,
	.configure_socket	= aximx5_pcmcia_configure_socket,

	.socket_init		= aximx5_pcmcia_socket_init,
	.socket_suspend		= aximx5_pcmcia_socket_suspend,
};

static void aximx5_pcmcia_release (struct device * dev)
{
	/* No need to free the structure since it is a static variable */
}

static struct platform_device aximx5_pcmcia_device = {
	.name           = "pxa2xx-pcmcia",
	.id             = 0,
	.dev            = {
		.platform_data  = &aximx5_pcmcia_ops,
		.release	= aximx5_pcmcia_release
	}
};

static int __init
aximx5_pcmcia_init(void)
{
	debug_func ("\n");

	if(!machine_is_aximx5())
		return -ENODEV;

	if (mq_driver_get ()) {
		debug_out ("MediaQ base driver not found\n");
		return -ENODEV;
	}

	if (mq_device_enum (&mq_base, 1) <= 0) {
		mq_driver_put ();
		debug_out ("MediaQ 1132 chip not found\n");
		return -ENODEV;
	}

	return platform_device_register (&aximx5_pcmcia_device);
}

static void __exit 
aximx5_pcmcia_exit(void)
{
	platform_device_unregister (&aximx5_pcmcia_device);
	mq_driver_put ();
}

module_init(aximx5_pcmcia_init);
module_exit(aximx5_pcmcia_exit);

MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("Dell Axim X5 PCMCIA platform-specific driver");
MODULE_LICENSE("GPL");
