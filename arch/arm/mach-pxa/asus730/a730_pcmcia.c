/*
 * Asus MyPal 730 PCMCIA/CF support. Based on a716_pcmcia.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2005 Pawel Kolodziejski
 * Copyright (C) 2004 Nicolas Pouillon, Vitaliy Sardyko
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus730-gpio.h>

#include <pcmcia/ss.h>

#include <../drivers/pcmcia/soc_common.h>

#include <linux/platform_device.h>

#define DEBUG 1

static int socket0_ready = 0;
/*     GPIOC_CIOW_N			5	// Output, to CF socket
 *     GPIOC_CIOR_N			6	// Output, to CF socket
 *     GPIOC_CF_PWE_N 		   	10      // Input
 *     GPIOD_CF_CD_N  		   	4	// Input, from CF socket
 *     GPIOD_CIOIS16_N		   	11	// Input, from CF socket
 *     GPIOD_CWAIT_N  		   	12	// Input, from CF socket
 *     GPIOD_CF_RNB			13	// Input (read, not busy?)*/

//ok:
//38 - int_bothedge - card detect nCD<0>
//jak 0 -> karta jest

//cf insert:
//38i 80o 91o 14i 22o 11i 22o 57a

//14-22-80-91-22-11
//38i 0    (wait)  14i 1    22o 1    80o 0    91o 1    11i 1    22o 0
//57 1 - alt. card is 16b
//cf eject:
//38i 1    80o 1    91o 0    11i 0    14i 0
//57 0

#define GPIO_NR_A730_CF_CARD_DETECT_N	38
#define GPIO_NR_A730_CF_IN_1	11
#define GPIO_NR_A730_CF_IN_2	14
#define GPIO_NR_A730_CF_OUT_3	22
#define GPIO_NR_A730_CF_OUT_1	80
#define GPIO_NR_A730_CF_OUT_2	91
#define GPIO_NR_A730_nIOIS16 57

#define GPIO_CLR(nr) (GPCR(nr) = GPIO_bit(nr))
#define GPIO_SET(nr) (GPSR(nr) = GPIO_bit(nr))

static struct pcmcia_irqs irqs[] = {
	{ 0, A730_IRQ(CF_CARD_DETECT_N), "PCMCIA CD" },
};

static int a730_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
    int ret;

    pxa_gpio_mode(GPIO_NR_A730_CF_OUT_1 | GPIO_OUT);
    pxa_gpio_mode(GPIO_NR_A730_CF_OUT_2 | GPIO_OUT);
    pxa_gpio_mode(GPIO_NR_A730_CF_OUT_3 | GPIO_OUT);
    
    pxa_gpio_mode(GPIO_NR_A730_CF_IN_1 | GPIO_IN);
    pxa_gpio_mode(GPIO_NR_A730_CF_IN_2 | GPIO_IN);
    
    pxa_gpio_mode(GPIO_NR_A730_CF_CARD_DETECT_N | GPIO_IN);
    pxa_gpio_mode(GPIO_NR_A730_nIOIS16 | GPIO_ALT_FN_1_IN);

    if (skt->nr == 0)
    {
	skt->irq = A730_IRQ(CF_IN_1);//or _2 or something entirelly different
	set_irq_type(irqs->irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
	ret = soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));
	socket0_ready = 1;
    }
    else
    {
	skt->irq = 0;
	ret = 0;
    }
    
    return ret;
}

static void a730_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
    if (skt->nr == 0) soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void a730_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
    int in1 = (GET_A730_GPIO(CF_IN_1) >> 11) & 0x1;
    int in2 = (GET_A730_GPIO(CF_IN_2) >> 14) & 0x1;
    
    in2 = in2;
    
    if (skt->nr == 0)
    {
	state->detect = GET_A730_GPIO(CF_CARD_DETECT_N) ? 0 : 1;
	state->ready  = in1;
    }
    else
    {
	// disabled fo now
	state->detect = 0;
	state->ready  = 0;//in2;
    }

    state->bvd1   = 1;
    state->bvd2   = 1;
    state->wrprot = 0;
    state->vs_3v  = 1;
    state->vs_Xv  = 0;
    
    //printk("11=0x%x, 14=0x%x, 57=0x%x\n", in1, in2, GET_A730_GPIO(nIOIS16));
    //if (state->detect && state->ready) printk("%s: skt->nr = %d, detect = %d, ready = %d\n", __FUNCTION__, skt->nr, state->detect, state->ready);
}
static int power_en;
/*
    Three set_socket_calls
    1. RESET deasserted
    2. RESET asserted
    3. RESET deasserted
*/
static int a730_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
    if (skt->nr == 0)
    {
	switch (state->Vcc)
	{
	    case 0:
		SET_A730_GPIO(CF_OUT_1, 0);//80
		SET_A730_GPIO(CF_OUT_2, 0);
		SET_A730_GPIO(CF_POWER, 1);
		power_en = 0;
		break;
	    case 33:
		SET_A730_GPIO(CF_OUT_1, 0);//80
		SET_A730_GPIO(CF_OUT_2, 1);//91
		SET_A730_GPIO(CF_OUT_3, 1);//22
		mdelay(50);
		SET_A730_GPIO(CF_OUT_3, 0);//22
		SET_A730_GPIO(CF_POWER, 0);
		power_en = 1;
		break;
	    case 50:
		SET_A730_GPIO(CF_OUT_1, 0);//80
		SET_A730_GPIO(CF_OUT_2, 1);//91
		SET_A730_GPIO(CF_OUT_3, 1);//22
		mdelay(50);
		SET_A730_GPIO(CF_OUT_3, 0);//22
		SET_A730_GPIO(CF_POWER, 0);
		power_en = 1;
		break;
	    default:
		printk (KERN_ERR "%s: Unsupported Vcc:%d\n", __FUNCTION__, state->Vcc);
	}
    }

    return 0;
}

static void a730_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
    //reset
    if (skt->nr == 0) soc_pcmcia_enable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void a730_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
    //reset
    if (skt->nr == 0) soc_pcmcia_disable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static struct pcmcia_low_level a730_pcmcia_ops = { 
	.owner			= THIS_MODULE,
	.first			= 0,
	.nr			= 1,
	.hw_init		= a730_pcmcia_hw_init,
	.hw_shutdown		= a730_pcmcia_hw_shutdown,
	.socket_state		= a730_pcmcia_socket_state,
	.configure_socket	= a730_pcmcia_configure_socket,
	.socket_init		= a730_pcmcia_socket_init,
	.socket_suspend		= a730_pcmcia_socket_suspend,
};

static struct platform_device a730_pcmcia_device = {
	.name           = "pxa2xx-pcmcia",
	.id             = 0,
	.dev            = {
		.platform_data = &a730_pcmcia_ops
	}
};

static int __init a730_pcmcia_init(void)
{
    if (!machine_is_a730()) return -ENODEV;
    return platform_device_register(&a730_pcmcia_device);
}

static void __exit a730_pcmcia_exit(void)
{
    platform_device_unregister(&a730_pcmcia_device);
}

module_init(a730_pcmcia_init);
module_exit(a730_pcmcia_exit);

MODULE_AUTHOR("Michal Sroczynski");
MODULE_DESCRIPTION("Asus MyPal 730(W) PCMCIA driver");
MODULE_LICENSE("GPL");
