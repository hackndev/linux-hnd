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

static int palmtc_pcmcia_hw_init (struct soc_pcmcia_socket *skt)
{
	set_irq_type(13, IRQT_RISING);
	skt->irq = IRQ_GPIO(13);
	set_irq_type(14, IRQT_RISING);
	skt->irq = IRQ_GPIO(14);

	printk("pcmcia_hw_init %d\n", skt->nr);
	return 0;
}

static void palmtc_pcmcia_hw_shutdown (struct soc_pcmcia_socket *skt)
{

}


static void
palmtc_pcmcia_socket_state (struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = 1; /* always inserted */
	state->ready  = ((GET_GPIO(13) ? 1 : 0) || (GET_GPIO(14) ? 1 : 0));
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;
}



static int
palmtc_pcmcia_configure_socket (struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	if (state->Vcc == 0) {
	    SET_GPIO(15, 0);
	    SET_GPIO(37, 0);
	    SET_GPIO(33, 0);
	printk("V0 PWR-%i SCK1-%i SCK2-%i RDY1-%i RDY2-%i\n",((GET_GPIO(33)) ? 1 : 0),
	((GET_GPIO(15)) ? 1 : 0),((GET_GPIO(37)) ? 1 : 0),
	((GET_GPIO(13)) ? 1 : 0),((GET_GPIO(14)) ? 1 : 0));
	} else if (state->Vcc == 33) {
	    SET_GPIO(15, 1);
	    SET_GPIO(37, 1);
	    SET_GPIO(33, 1);
	    SET_GPIO(37, 0);
	printk("V3 PWR-%i SCK1-%i SCK2-%i RDY1-%i RDY2-%i\n",((GET_GPIO(33)) ? 1 : 0),
	((GET_GPIO(15)) ? 1 : 0),((GET_GPIO(37)) ? 1 : 0),
	((GET_GPIO(13)) ? 1 : 0),((GET_GPIO(14)) ? 1 : 0));
	}
//	printk("skt: %d, vcc:%d, flags:%d\n", skt->nr, state->Vcc, state->flags);
	return 0;
}

static void palmtc_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	printk("palmtc_pcmcia_socket_init\n");

}


static void palmtc_pcmcia_socket_suspend (struct soc_pcmcia_socket *skt)
{ 
	printk("palmtc_pcmcia_socket_suspend\n");
}

static struct pcmcia_low_level palmtc_pcmcia_ops = {
	.owner			= THIS_MODULE,

	/* Setting it this way makes pcmcia-cs
	   scream about memory-cs (because of
	   HDD/CF memory in socket 0), but it's
	   much nicer than make some weird changes
	   in pxa pcmcia subsystem */
	.first			= 0,
	.nr			= 2,

	.hw_init		= palmtc_pcmcia_hw_init,
	.hw_shutdown		= palmtc_pcmcia_hw_shutdown,

	.socket_state		= palmtc_pcmcia_socket_state,
	.configure_socket	= palmtc_pcmcia_configure_socket,

	.socket_init		= palmtc_pcmcia_socket_init,
	.socket_suspend		= palmtc_pcmcia_socket_suspend,
};


static void palmtc_pcmcia_release (struct device * dev)
{
}


static struct platform_device palmtc_pcmcia_device = {
	.name           = "pxa2xx-pcmcia",
	.id             = 0,
	.dev            = {
		.platform_data  = &palmtc_pcmcia_ops,
		.release	= palmtc_pcmcia_release
	}
};

static int __init palmtc_pcmcia_init(void)
{
	printk ("pcmcia_init\n");

//	if(!machine_is_xscale_palmtc()) return -ENODEV;

	return platform_device_register (&palmtc_pcmcia_device);
}

static void __exit palmtc_pcmcia_exit(void)
{
	platform_device_unregister (&palmtc_pcmcia_device);
}

module_init(palmtc_pcmcia_init);
module_exit(palmtc_pcmcia_exit);

MODULE_AUTHOR ("Marek Vasut");
MODULE_DESCRIPTION ("PCMCIA support for Palm Tungsten|C");
MODULE_LICENSE ("GPL");
