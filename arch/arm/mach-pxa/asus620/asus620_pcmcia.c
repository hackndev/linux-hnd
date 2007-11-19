/*
 *
 * Asus 620/620BT PCMCIA/CF support.
 *
 * Copyright(C)
 *   Nicolas Pouillon, 2004, <nipo@ssji.net>
 *   Vitaliy Sardyko, 2004
 *   Vincent Benony, 2006
 *      Adaptation to A620
 *
 *   Konstantine A. Beklemishev <konstantine@r66.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
//#include <linux/soc-device.h>

#include <asm/hardware.h>
#include <asm/irq.h>

// #include <asm/arch/pcmcia.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus620-gpio.h>

#include <pcmcia/ss.h>

#include "soc_common.h"

#define DEBUG 0

#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

static struct pcmcia_irqs irqs[] = {
        { 0, A620_IRQ(CF_DETECT_N), "PCMCIA CF" },
};
/*
static void asus620_pcmcia_socket_power_on(void)
{
	SET_A620_GPIO(CF_POWER, 1);

//	asus620_gpo_set(GPO_A620_CF_RESET);
//	mdelay(10);
//	asus620_gpo_clear(GPO_A620_CF_RESET);
}

static void asus620_pcmcia_socket_power_off(void)
{
	SET_A620_GPIO(CF_POWER, 0);
}

static void asus620_pcmcia_hw_rst (int state)
{
	state = !(!state);
	DPRINTK("Hardware reset (%d)\n", state);
	SET_A620_GPIO(CF_RESET, state);
//	asus620_gpo_set(GPO_A620_CF_RESET);
//	mdelay(10);
//	asus620_gpo_clear(GPO_A620_CF_RESET);
//	mdelay(1000);
}
*/
static int asus620_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
//	unsigned long flags;
	int ret;

	DPRINTK("A620 PCMCIA hw_init\n");

	/* This does not work ! But how to enable IRQ ? */
	// set_irq_type (IRQ_GPIO(GPIO_NR_A620_CF_IRQ), IRQT_BOTHEDGE);

	skt->irq = IRQ_GPIO(GPIO_NR_A620_CF_IRQ);

	ret = soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));
	set_irq_type (IRQ_GPIO(GPIO_NR_A620_CF_DETECT_N), IRQT_BOTHEDGE);
/*
	local_irq_save (flags);
	SET_A620_GPIO(CF_RESET, 1);
	SET_A620_GPIO(CF_ENABLE, 0);
	SET_A620_GPIO(CF_POWER, 0);
	pxa_gpio_mode (GPIO_NR_A620_CF_RESET    | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_A620_CF_ENABLE   | GPIO_OUT);
	pxa_gpio_mode (GPIO_NR_A620_CF_POWER    | GPIO_OUT);
//	pxa_gpio_mode (GPIO_NR_A620_CF_BVD1     | GPIO_IN);
	pxa_gpio_mode (GPIO_NR_A620_CF_DETECT_N | GPIO_IN);
	pxa_gpio_mode (GPIO_NR_A620_CF_IRQ      | GPIO_IN);
	pxa_gpio_mode (GPIO_NR_A620_CF_READY_N  | GPIO_IN);
	pxa_gpio_mode (GPIO_NR_A620_CF_VS1_N    | GPIO_IN);
	pxa_gpio_mode (GPIO_NR_A620_CF_VS2_N    | GPIO_IN);

	// This one must re-disabled after pxa2xx inits
	pxa_gpio_mode (GPIO_NR_A620_LED_ENABLE  | GPIO_OUT);
	local_irq_restore (flags);
*/	
	DPRINTK("HW_init returns %d\n", ret);
	
	return ret;
}


/*
 * Release all resources.
 */
static void asus620_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
//	SET_A620_GPIO(CF_POWER, 0);
	/* Disable cards */
}

static void
asus620_pcmcia_socket_state(
	struct soc_pcmcia_socket *skt,
	struct pcmcia_state *state
	)
{
//	state->detect = GET_A620_GPIO(CF_DETECT_N) ? 0 : 1;
	state->detect = GET_A620_GPIO(CF_VS1_N) ? 0 : 1;
	state->ready  = GET_A620_GPIO(CF_READY_N) ? 0 : 1;
//	state->ready = 1;

	state->bvd1   = GET_A620_GPIO(CF_BVD1) ? 0 : 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = GET_A620_GPIO(CF_VS1_N) ? 0 : 1;
	state->vs_Xv  = GET_A620_GPIO(CF_VS2_N) ? 0 : 1;
	DPRINTK ( "detect:%d ready:%d 3X:%d%d\n",
			  state->detect, state->ready,
			  state->vs_3v,
			  state->vs_Xv);
}

static int
asus620_pcmcia_configure_socket(
	struct soc_pcmcia_socket *skt,
	const socket_state_t *state
	)
{
	/* Silently ignore Vpp, output enable, speaker enable. */
	DPRINTK ("Reset:%d Output:%d Vcc:%d\n", (state->flags & SS_RESET) ? 1 : 0,
		    !(!(state->flags & SS_OUTPUT_ENA)), state->Vcc);

//	asus620_pcmcia_hw_rst (state->flags & SS_RESET);

//	SET_A620_GPIO(CF_ENABLE, (!(state->flags & SS_OUTPUT_ENA)));
	
	switch(state->Vcc) {
	case 0:
//		SET_A620_GPIO(CF_POWER, 0);
		break;
	case 33:
	case 50:
//		SET_A620_GPIO(CF_POWER, 1);
		break;
	default:
		printk("Unhandled voltage set on PCMCIA: %d\n", state->Vcc);
	}

	DPRINTK("done !\n");
	return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static void asus620_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_enable_irqs(skt, irqs, ARRAY_SIZE(irqs));
//	SET_A620_GPIO(CF_POWER, 1);
//	asus620_pcmcia_socket_power_on();
	DPRINTK ("Socket init\n");
}

/*
 * Disable card status IRQs on suspend.
 */
static void asus620_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_disable_irqs(skt, irqs, ARRAY_SIZE(irqs));
//	SET_A620_GPIO(CF_POWER, 0);
	DPRINTK ("Socket suspend\n");
//	asus620_pcmcia_socket_power_off();
//	asus620_pcmcia_hw_rst(1);
}

static struct pcmcia_low_level asus620_pcmcia_ops = { 
	.owner			= THIS_MODULE,
	
	.first			= 0,
	.nr			= 1,

	.hw_init		= asus620_pcmcia_hw_init,
	.hw_shutdown		= asus620_pcmcia_hw_shutdown,

	.socket_state		= asus620_pcmcia_socket_state,
	.configure_socket	= asus620_pcmcia_configure_socket,

	.socket_init		= asus620_pcmcia_socket_init,
	.socket_suspend		= asus620_pcmcia_socket_suspend,
};

static struct platform_device asus620_pcmcia_device = {
        .name           = "pxa2xx-pcmcia",
        .id             = 0,
        .dev            = {
                .platform_data  = &asus620_pcmcia_ops
        }
};

static int __init asus620_pcmcia_init(void)
{
	printk(KERN_INFO "Asus A620 PCMCIA driver initialization\n");
	return platform_device_register(&asus620_pcmcia_device);
}

static void __exit asus620_pcmcia_exit(void)
{
	platform_device_unregister (&asus620_pcmcia_device);
}

module_init(asus620_pcmcia_init);
module_exit(asus620_pcmcia_exit);

MODULE_AUTHOR("Nicolas Pouillon, Vitaliy Sardyko, Vincent Benony");
MODULE_DESCRIPTION("Asus A620/A620BT PCMCIA driver");
MODULE_LICENSE("GPL");
