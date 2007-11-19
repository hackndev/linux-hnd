 /*
  *  blueangel_pcmcia.c
  *
  *  Based on h4000_pcmcia.c by Shawn Anderson
  *  htcbluangel support by Fabrice Crohas
  *
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License version 2 as
  *  published by the Free Software Foundation.
  *
  * * * */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include "../../../../drivers/pcmcia/soc_common.h"
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>

extern struct platform_device blueangel_asic3;

static struct pcmcia_irqs socket_state_irqs[] = {
};

static int blueangel_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	GPSR(GPIO48_nPOE) = GPIO_bit(GPIO48_nPOE) | GPIO_bit(GPIO49_nPWE) |
	    GPIO_bit(GPIO52_nPCE_1_MD) | GPIO_bit(GPIO53_nPCE_2_MD) |
	    GPIO_bit(GPIO55_nPREG_MD) | GPIO_bit(GPIO56_nPWAIT_MD);

	pxa_gpio_mode(GPIO48_nPOE_MD);
	pxa_gpio_mode(GPIO49_nPWE_MD);
	pxa_gpio_mode(GPIO52_nPCE_1_MD);
	pxa_gpio_mode(GPIO55_nPREG_MD);
	pxa_gpio_mode(GPIO56_nPWAIT_MD);

	skt->irq = BLUEANGEL_IRQ(WLAN_IRQ_N);

	return soc_pcmcia_request_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void blueangel_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void blueangel_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				      struct pcmcia_state *state)
{
	state->detect = 1; 
	state->ready = 1;
	state->bvd1 = 1;
	state->bvd2 = 1;
	state->wrprot = 0;
	state->vs_3v = asic3_get_gpio_status_b(&blueangel_asic3.dev)
	    & GPIOB_WLAN_PWR3;
	state->vs_Xv = 0;
};

static int blueangel_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
					 const socket_state_t * state)
{
	printk("%s\n", __FUNCTION__);
	/* Silently ignore Vpp, output enable, speaker enable. */
	printk( "Reset:%d ,Vcc:%d ,Flags:%x\n",
	    (state->flags & SS_RESET) ? 1 : 0, state->Vcc, state->flags);

	switch (state->Vcc) {
	case 0:
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_PWR1, 0 );
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_PWR2, 0 );
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1 << GPIOB_WLAN_PWR3, 0 );
		break;
	case 50:
		if ( (state->flags & SS_OUTPUT_ENA) ? 1 : 0 ) {
			asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_PWR1, 1 << GPIOA_WLAN_PWR1 );
			asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_PWR2, 1 << GPIOA_WLAN_PWR2 );
			asic3_set_gpio_out_b(&blueangel_asic3.dev, 1 << GPIOB_WLAN_PWR3, 1 << GPIOB_WLAN_PWR3 );
		}
		if ( (state->flags & SS_RESET) ? 1 : 0 )
			asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_RESET,  1 << GPIOA_WLAN_RESET);
		else
			asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_WLAN_RESET, 0);
		break;
	case 33:
		break;
	default:
		printk(KERN_ERR "%s: Unsupported Vcc:%d\n", __FUNCTION__,
		       state->Vcc);
	}
	return 0;

}

static void blueangel_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	soc_pcmcia_enable_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static void blueangel_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	printk("%s\n", __FUNCTION__);
	soc_pcmcia_disable_irqs(skt, socket_state_irqs, ARRAY_SIZE(socket_state_irqs));
}

static struct pcmcia_low_level blueangel_pcmcia_ops = {
	.owner            = THIS_MODULE,
	.first            = 0,
	.nr               = 1,
	.hw_init          = blueangel_pcmcia_hw_init,
	.hw_shutdown      = blueangel_pcmcia_hw_shutdown,
	.socket_state     = blueangel_pcmcia_socket_state,
	.configure_socket = blueangel_pcmcia_configure_socket,
	.socket_init      = blueangel_pcmcia_socket_init,
	.socket_suspend   = blueangel_pcmcia_socket_suspend,
};

static struct platform_device blueangel_pcmcia = {
	.name = "pxa2xx-pcmcia",
	.dev = {.platform_data = &blueangel_pcmcia_ops},
};

static int __init blueangel_pcmcia_probe(struct platform_device *pdev)
{
	return platform_device_register(&blueangel_pcmcia);
}

static struct platform_driver blueangel_pcmcia_driver = {
	.driver = {
	    .name     = "blueangel-pcmcia",
	},
        .probe    = blueangel_pcmcia_probe,
};

static int __init blueangel_pcmcia_init(void)
{  
	return platform_driver_register(&blueangel_pcmcia_driver);
}

static void __exit blueangel_pcmcia_exit(void)
{
	platform_device_unregister(&blueangel_pcmcia);
	platform_driver_unregister(&blueangel_pcmcia_driver);
}

module_init(blueangel_pcmcia_init);
module_exit(blueangel_pcmcia_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabrice Crohas <fcrohas gmail com>");
