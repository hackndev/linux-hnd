/*
 * Rover P5+ PCMCIA support
 * 
 * Konstantine A. Beklemishev <konstantine@r66.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/signal.h>

#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/roverp5p-gpio.h>

#if 0
#  define rover_debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define rover_debug(s, args...)
#endif
#define debug_func(s, args...) rover_debug ("%s: " s, __FUNCTION__, ##args)

#define GPIO_STATE(x)	(GPLR (x) & GPIO_bit (x))
#define GPIO_SET(x)	(GPSR (x) = GPIO_bit (x))
#define GPIO_CLR(x)	(GPCR (x) = GPIO_bit (x))

static struct pcmcia_irqs cd_irqs[] = {
        { 0, IRQ_GPIO(GPIO_NR_ROVERP5P_CF_DETECT), "PCMCIA CD" },
};
                                                                                
static void roverp5p_pcmcia_hw_rst (int state)
{
	debug_func ("\n");
	SET_ROVERP5P_GPIO (CF_RESET, state);
/*	if (state)
		GPIO_SET (GPIO_NR_ROVERP5P_CF_RESET);
	else
		GPIO_CLR (GPIO_NR_ROVERP5P_CF_RESET);*/
}

static int roverp5p_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	unsigned long flags;
	
	skt->irq = IRQ_GPIO(GPIO_NR_ROVERP5P_PXA_PRDY);
	
	local_irq_save (flags);
	pxa_gpio_mode (GPIO_NR_ROVERP5P_CF_RESET | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_ROVERP5P_CF_PWR_ON | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_ROVERP5P_CF_BUS_nON | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_ROVERP5P_CF_DETECT | GPIO_IN);
        pxa_gpio_mode (GPIO_NR_ROVERP5P_PXA_PRDY | GPIO_IN);
	local_irq_restore (flags);

	return soc_pcmcia_request_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}


/*
 * Release all resources.
 */
static void roverp5p_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

static void
roverp5p_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = GET_ROVERP5P_GPIO (CF_DETECT) ? 0 : 1;
	state->ready  = GET_ROVERP5P_GPIO (PXA_PRDY) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = GET_ROVERP5P_GPIO (CF_PWR_ON) ? 0 : 1;
	state->vs_Xv  = 0;

	rover_debug ("detect:%d ready:%d vcc:%d\n",
	       state->detect, state->ready, state->vs_3v);
}

static int
roverp5p_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	/* Silently ignore Vpp, output enable, speaker enable. */
	debug_func ("Reset:%d Vcc:%d\n", (state->flags & SS_RESET) ? 1 : 0,
		    state->Vcc);

	roverp5p_pcmcia_hw_rst (state->flags & SS_RESET);

	/* Apply socket voltage */
	switch (state->Vcc) {
	case 0:
		SET_ROVERP5P_GPIO (CF_PWR_ON, 1);
		SET_ROVERP5P_GPIO (CF_BUS_nON, 1);
		/*GPIO_CLR (GPIO_NR_ROVERP5P_CF_PWR_ON);
		GPIO_CLR (GPIO_NR_ROVERP5P_CF_BUS_nON);*/
		break;
	case 50:
		/*GPIO_SET (GPIO_NR_ROVERP5P_CF_PWR_ON);
		break;*/
	case 33:
		/* Apply power to socket */
		SET_ROVERP5P_GPIO (CF_PWR_ON, 0);
		SET_ROVERP5P_GPIO (CF_BUS_nON, 0);
		/*GPIO_SET (GPIO_NR_ROVERP5P_CF_PWR_ON);
		GPIO_SET (GPIO_NR_ROVERP5P_CF_BUS_nON);*/
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
static void roverp5p_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");
	soc_pcmcia_enable_irqs (skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

/*
 * Disable card status IRQs on suspend.
 */
static void roverp5p_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	debug_func ("\n");
	roverp5p_pcmcia_hw_rst(1);
	soc_pcmcia_disable_irqs(skt, cd_irqs, ARRAY_SIZE(cd_irqs));
}

static struct pcmcia_low_level roverp5p_pcmcia_ops = { 
	.owner			= THIS_MODULE,
	
	.first			= 0,
	.nr			= 1,

	.hw_init		= roverp5p_pcmcia_hw_init,
	.hw_shutdown		= roverp5p_pcmcia_hw_shutdown,

	.socket_state		= roverp5p_pcmcia_socket_state,
	.configure_socket	= roverp5p_pcmcia_configure_socket,

	.socket_init		= roverp5p_pcmcia_socket_init,
	.socket_suspend		= roverp5p_pcmcia_socket_suspend,
};

static struct platform_device roverp5p_pcmcia_device = {
        .name           = "pxa2xx-pcmcia",
        .id             = 0,
        .dev            = {
                .platform_data  = &roverp5p_pcmcia_ops
        }
};

static int __init roverp5p_pcmcia_init(void)
{
	return platform_device_register(&roverp5p_pcmcia_device);
}

static void __exit roverp5p_pcmcia_exit(void)
{
	platform_device_unregister (&roverp5p_pcmcia_device);
}

module_init(roverp5p_pcmcia_init);
module_exit(roverp5p_pcmcia_exit);

MODULE_AUTHOR("Konstantine A. Beklemishev");
MODULE_DESCRIPTION("Rover P5+ PCMCIA driver");
MODULE_LICENSE("GPL");
