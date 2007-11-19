/*
 * Asus MyPal A716 PCMCIA/CF and internal WIFI glue driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 * Copyright (C) 2004 Nicolas Pouillon, Vitaliy Sardyko
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>

#include <pcmcia/ss.h>

#include <../drivers/pcmcia/soc_common.h>

#define DEBUG 0

#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

static int socket0_ready, socket1_ready;
int a716_wifi_pcmcia_detect;

static struct pcmcia_irqs irqs[] = {
	{ 0, A716_IRQ(CF_CARD_DETECT_N), "PCMCIA CD" },
};

static void a716_pcmcia_socket_enable_state(struct soc_pcmcia_socket *skt, int state)
{
	if (state == 1) {
		a716_gpo_set(GPO_A716_CF_SLOTS_ENABLE);
	} else {
		if ((socket0_ready == 0) && (socket1_ready == 0))
			a716_gpo_clear(GPO_A716_CF_SLOTS_ENABLE);
	}
}

static void a716_pcmcia_socket_update_ready(struct soc_pcmcia_socket *skt, int state)
{
	if (skt->nr == 0) {
		socket0_ready = state;
	} else {
		socket1_ready = state;
	}
}

static int a716_pcmcia_socket_wait_ready(struct soc_pcmcia_socket *skt)
{
	int count = 4000;

	if (skt->nr == 0) {
		while (GET_A716_GPIO(WIFI_READY) == 0 || count != 0) {
			mdelay(50);
			count--;
			break;
		}
	} else {
		while (GET_A716_GPIO(CF_READY) == 0 || count != 0) {
			mdelay(50);
			count--;
			break;
		}
	}

	if (count == 0) {
		printk("PCMCIA Socket %d Ready Timeout!\n", skt->nr);
	}

	return 0;
}

static int a716_pcmcia_reset_socket(struct soc_pcmcia_socket *skt)
{
	a716_pcmcia_socket_update_ready(skt, 0);

	if (skt->nr == 0) {
		a716_gpo_set(GPO_A716_CF_SLOT1_RESET);
    		mdelay(10);
		a716_gpo_clear(GPO_A716_CF_SLOT1_RESET);
    		mdelay(10);
	} else {
		a716_gpo_set(GPO_A716_CF_SLOT2_RESET);
    		mdelay(20);
		a716_gpo_clear(GPO_A716_CF_SLOT2_RESET);
    		mdelay(25);
	}

	a716_pcmcia_socket_wait_ready(skt);

	a716_pcmcia_socket_update_ready(skt, 1);

	return 0;
}

static int a716_pcmcia_socket_power_on(struct soc_pcmcia_socket *skt)
{
	a716_pcmcia_socket_update_ready(skt, 1);

	if (skt->nr == 0) {
		a716_pcmcia_socket_enable_state(skt, 1);

		a716_gpo_clear(GPO_A716_CF_SLOT1_POWER1_N);
		a716_gpo_clear(GPO_A716_CF_SLOT1_POWER2_N);

		a716_gpo_clear(GPO_A716_CF_SLOT1_POWER3_N);
	} else {
		// power off bluetooth module
		a716_gpo_clear(GPO_A716_BLUETOOTH_POWER);

		a716_gpo_clear(GPO_A716_CF_SLOT2_POWER1_N);
    		a716_gpo_set(GPO_A716_CF_SLOT2_POWER2);

		a716_pcmcia_socket_enable_state(skt, 1);

		a716_gpo_set(GPO_A716_WIFI_1p8V);
		mdelay(100);
		a716_gpo_set(GPO_A716_WIFI_3p3V);
		mdelay(50);

		a716_gpo_clear(GPO_A716_CF_SLOT2_POWER3_N);

		GPSR(GPIO_NR_A716_BLUE_LED_ENABLE) = GPIO_bit(GPIO_NR_A716_BLUE_LED_ENABLE);
	}

	a716_pcmcia_reset_socket(skt);

	return 0;
}

static int a716_pcmcia_socket_power_off(struct soc_pcmcia_socket *skt)
{
	a716_pcmcia_socket_update_ready(skt, 0);

	if (skt->nr == 0) {
		a716_gpo_set(GPO_A716_CF_SLOT1_POWER2_N);
		a716_gpo_set(GPO_A716_CF_SLOT1_POWER1_N);

		a716_gpo_set(GPO_A716_CF_SLOT1_POWER3_N);
	} else {
		GPCR(GPIO_NR_A716_BLUE_LED_ENABLE) = GPIO_bit(GPIO_NR_A716_BLUE_LED_ENABLE);

		a716_gpo_set(GPO_A716_CF_SLOT2_POWER1_N);
		a716_gpo_clear(GPO_A716_CF_SLOT2_POWER2);

		a716_gpo_clear(GPO_A716_WIFI_3p3V);
		mdelay(40);
		a716_gpo_clear(GPO_A716_WIFI_1p8V);
		mdelay(200);

		a716_gpo_set(GPO_A716_CF_SLOT2_POWER3_N);
	}

	a716_pcmcia_socket_enable_state(skt, 0);

	return 0;
}

static int a716_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	DPRINTK("skt: %d\n", skt->nr);

	if (skt->nr == 0) {
		skt->irq = IRQ_GPIO(GPIO_NR_A716_CF_READY);
	} else {
		skt->irq = IRQ_GPIO(GPIO_NR_A716_WIFI_READY);
	}

	soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));

	return 0;
}

static void a716_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	DPRINTK("skt: %d\n", skt->nr);

	soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void a716_pcmcia_socket_state(struct soc_pcmcia_socket *skt, struct pcmcia_state *state)
{
	if (skt->nr == 0) {
		state->detect = GET_A716_GPIO(CF_CARD_DETECT_N) ? 0 : 1;
		state->ready  = socket0_ready;
	} else {
		state->detect = a716_wifi_pcmcia_detect;
		state->ready  = socket1_ready;
	}

	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;

	DPRINTK("skt:%d, detect:%d ready:%d power:%d\n", skt->nr, state->detect, state->ready, state->vs_3v);
}

static int a716_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	DPRINTK("skt: %d, vcc:%d, flags:%d\n", skt->nr, state->Vcc, state->flags);

	//if (state->flags & SS_RESET) {
		//a716_pcmcia_reset_socket(skt);
	//}

	if (state->Vcc == 0) {
		a716_pcmcia_socket_power_off(skt);
	} else if (state->Vcc == 33) {
		a716_pcmcia_socket_power_on(skt);
	}

	return 0;
}

static void a716_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	DPRINTK("skt: %d\n", skt->nr);

	soc_pcmcia_enable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void a716_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	DPRINTK("skt: %d\n", skt->nr);

	soc_pcmcia_disable_irqs(skt, irqs, ARRAY_SIZE(irqs));

	a716_pcmcia_socket_power_off(skt);
}

static struct pcmcia_low_level a716_pcmcia_ops = { 
	.owner			= THIS_MODULE,
	.first			= 0,
	.nr			= 2,
	.hw_init		= a716_pcmcia_hw_init,
	.hw_shutdown		= a716_pcmcia_hw_shutdown,
	.socket_state		= a716_pcmcia_socket_state,
	.configure_socket	= a716_pcmcia_configure_socket,
	.socket_init		= a716_pcmcia_socket_init,
	.socket_suspend		= a716_pcmcia_socket_suspend,
};

static struct platform_device a716_pcmcia_device = {
	.name           = "pxa2xx-pcmcia",
	.id             = 0,
	.dev            = {
		.platform_data = &a716_pcmcia_ops
	}
};

static int a716_pcmcia_probe(struct platform_device *pdev)
{
	socket0_ready = 0;
	socket1_ready = 0;
	a716_wifi_pcmcia_detect = 0;

	return platform_device_register(&a716_pcmcia_device);
}

static struct platform_driver a716_pcmcia_driver = {
	.driver = {
		.name     = "a716-pcmcia",
	},
	.probe    = a716_pcmcia_probe,
};

static int __init a716_pcmcia_init(void)
{
	if (!machine_is_a716())
		return -ENODEV;

	return platform_driver_register(&a716_pcmcia_driver);
}

static void __exit a716_pcmcia_exit(void)
{
	platform_device_unregister(&a716_pcmcia_device);
	platform_driver_unregister(&a716_pcmcia_driver);
}

module_init(a716_pcmcia_init);
module_exit(a716_pcmcia_exit);

MODULE_AUTHOR("Nicolas Pouillon, Vitaliy Sardyko, Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 PCMCIA/CF and internal WIFI glue driver");
MODULE_LICENSE("GPL");
