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
#include <asm/irq.h>

#define GET_GPIO(gpio) \
        (GPLR(gpio) & GPIO_bit(gpio))
#define SET_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(gpio) = GPIO_bit(gpio); \
else \
	GPCR(gpio) = GPIO_bit(gpio); \
} while (0)

static int palmld_pcmcia_hw_init (struct soc_pcmcia_socket *skt)
{
	set_irq_type(38, IRQT_FALLING);
	skt->irq = IRQ_GPIO(38);

	printk("pcmcia_hw_init %d\n", skt->nr);
	return 0;
}

static void palmld_pcmcia_hw_shutdown (struct soc_pcmcia_socket *skt)
{

}


static void
palmld_pcmcia_socket_state (struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = 1; /* always inserted */
	state->ready  = GET_GPIO(38) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 1;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;

	/*
	state->detect = GET_AXIMX5_GPIO (PCMCIA_DETECT_N) ? 0 : 1;
    state->ready  = mq_base->get_GPIO (mq_base, 2) ? 1 : 0;
	state->bvd1   = GET_AXIMX5_GPIO (PCMCIA_BVD1) ? 1 : 0;
	state->bvd2   = GET_AXIMX5_GPIO (PCMCIA_BVD2) ? 1 : 0;
	state->wrprot = 0;
	state->vs_3v  = mq_base->get_GPIO (mq_base, 65) ? 1 : 0;
	state->vs_Xv  = 0;
	*/
	/*printk ("detect:%d ready:%d vcc:%d bvd1:%d bvd2:%d\n",
		   state->detect, state->ready, state->vs_3v, state->bvd1, state->bvd2);
		   */
}



static int
palmld_pcmcia_configure_socket (struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	printk ("palmld_pcmcia_config_skt: Reset:%d Vcc:%d\n", (state->flags & SS_RESET) ? 1 : 0,
		    state->Vcc);
	
	/* GPIO 36 appears to control power to the chip */
	SET_GPIO(36, 1);

	/* GPIO 81 appears to be reset */
	SET_GPIO(81, (state->flags & SS_RESET) ? 1 : 0);
	
	return 0;
}

static void palmld_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	printk("palmld_pcmcia_socket_init\n");

}



static void palmld_pcmcia_socket_suspend (struct soc_pcmcia_socket *skt)
{ 
	printk("palmld_pcmcia_socket_suspend\n");
}

static struct pcmcia_low_level palmld_pcmcia_ops = {
	.owner			= THIS_MODULE,

	/* Setting it this way makes pcmcia-cs
	   scream about memory-cs (because of
	   HDD/CF memory in socket 0), but it's
	   much nicer than make some weird changes
	   in pxa pcmcia subsystem */
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
	printk ("pcmcia_init\n");

	if(!machine_is_xscale_palmld()) return -ENODEV;

	return platform_device_register (&palmld_pcmcia_device);
}

static void __exit palmld_pcmcia_exit(void)
{
	platform_device_unregister (&palmld_pcmcia_device);
}

module_init(palmld_pcmcia_init);
module_exit(palmld_pcmcia_exit);

MODULE_AUTHOR ("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION ("PCMCIA support for Palm LifeDrive");
MODULE_LICENSE ("GPL");
