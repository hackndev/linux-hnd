/*
 * Grayhill GHI270 PCMCIA support
 * 
 * Copyright 2007 SDG Systems LLC
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
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/soc-old.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/irq.h>
#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/ghi270.h>

#include "soc_common.h"


static struct pcmcia_irqs ghi270_cd_irq[] = {
	{ 0, gpio_to_irq(GHI270_GPIO14_PCMCIA_CD1),  "PCMCIA CD" }
};

static int ghi270_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{

	/* Set state of GPIO outputs before we enable them as outputs. */
	gpio_set_value(GPIO48_nPOE, 1);
	gpio_set_value(GPIO49_nPWE, 1);
	gpio_set_value(GPIO50_nPIOR, 1);
	gpio_set_value(GPIO51_nPIOW, 1);
	gpio_set_value(GPIO86_nPCE_1, 1);
	gpio_set_value(GPIO78_nPCE_2, 1);
	gpio_set_value(GHI270_GPIO19_PCMCIA_PRESET1, 0);
	gpio_set_value(GHI270_GPIO20_PCMCIA_PRESET2, 0);
	gpio_set_value(GHI270_GPIO37_PCMCIA_nPWR1, 1);

	pxa_gpio_mode(GPIO48_nPOE_MD);
	pxa_gpio_mode(GPIO49_nPWE_MD);
	pxa_gpio_mode(GPIO50_nPIOR_MD);
	pxa_gpio_mode(GPIO51_nPIOW_MD);
	pxa_gpio_mode(GPIO55_nPREG_MD);
	pxa_gpio_mode(GPIO56_nPWAIT_MD);
	pxa_gpio_mode(GPIO57_nIOIS16_MD);
	pxa_gpio_mode(GPIO78_nPCE_2_MD);
	pxa_gpio_mode(GPIO86_nPCE_1_MD);
	pxa_gpio_mode(GPIO79_pSKTSEL_MD);

	gpio_direction_output(GHI270_GPIO19_PCMCIA_PRESET1);
	gpio_direction_output(GHI270_GPIO20_PCMCIA_PRESET2);
	gpio_direction_output(GHI270_GPIO37_PCMCIA_nPWR1);
	gpio_direction_input (GHI270_GPIO21_PCMCIA_RDY_IRQ1);
	gpio_direction_input (GHI270_GPIO22_PCMCIA_RDY_IRQ2);
	gpio_direction_input (GHI270_GPIO98_VS0_1);
	gpio_direction_input (GHI270_GPIO99_VS1_1);
	gpio_direction_input (GHI270_GPIO14_PCMCIA_CD1);

	skt->irq = (skt->nr == 0) ? gpio_to_irq(GHI270_GPIO21_PCMCIA_RDY_IRQ1) :
				    gpio_to_irq(GHI270_GPIO22_PCMCIA_RDY_IRQ2);

	return soc_pcmcia_request_irqs(skt, ghi270_cd_irq,
				       ARRAY_SIZE(ghi270_cd_irq));
}

/*
 * Release all resources.
 */
static void ghi270_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, ghi270_cd_irq, ARRAY_SIZE(ghi270_cd_irq));
}

static int wificd;

static void ghi270_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				       struct pcmcia_state *state)
{
	if (skt->nr == 0) {
		/* internal CF */
		state->detect = gpio_get_value(GHI270_GPIO14_PCMCIA_CD1) ? 0 : 1;
		state->ready  = gpio_get_value(GHI270_GPIO21_PCMCIA_RDY_IRQ1) ? 1 : 0;
		state->vs_3v  = gpio_get_value(GHI270_GPIO98_VS0_1) ? 0 : 1;
		state->vs_Xv  = gpio_get_value(GHI270_GPIO99_VS1_1) ? 0 : 1;
	} else {
		/* wifi/bt */
		state->detect = wificd;
		if(wificd) {
			state->ready  =
			  gpio_get_value(GHI270_GPIO22_PCMCIA_RDY_IRQ2) ? 1 : 0;
			state->vs_3v  = 1;
			state->vs_Xv  = 0;
		}
	}
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
}

static int ghi270_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
					  const socket_state_t *state)
{
			
	if (skt->nr == 0) {
		gpio_set_value(GHI270_GPIO19_PCMCIA_PRESET1,
			(state->flags & SS_RESET) ? 0 : 1);

		switch (state->Vcc) {
		case 0:
			gpio_set_value(GHI270_GPIO37_PCMCIA_nPWR1, 1);
			break;
		case 33:
			gpio_set_value(GHI270_GPIO37_PCMCIA_nPWR1, 0);
			break;
		default:
			printk(KERN_ERR "%s: Unsupported Vcc:%d\n",
				__FUNCTION__, state->Vcc);
		}
	} else {
		gpio_set_value(GHI270_GPIO20_PCMCIA_PRESET2,
			state->flags & SS_RESET ? 0 : 1);
	}

	return 0;
}

static void ghi270_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
}

static void ghi270_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	gpio_set_value(skt->nr == 0 ? GHI270_GPIO19_PCMCIA_PRESET1 :
				      GHI270_GPIO20_PCMCIA_PRESET2, 0);
	if(skt->nr == 0) {
		gpio_set_value(GHI270_GPIO37_PCMCIA_nPWR1, 1);
	}
}

static struct pcmcia_low_level ghi270_pcmcia_ops = {
	.owner          	= THIS_MODULE,
	.hw_init        	= ghi270_pcmcia_hw_init,
	.hw_shutdown		= ghi270_pcmcia_hw_shutdown,
	.socket_state		= ghi270_pcmcia_socket_state,
	.configure_socket	= ghi270_pcmcia_configure_socket,
	.socket_init		= ghi270_pcmcia_socket_init,
	.socket_suspend		= ghi270_pcmcia_socket_suspend,
	.nr         		= 2,	/* 0 = internal slot, 1 = wifi/bt */
};

static struct platform_device ghi270_pcmcia_device = {
	.name			= "pxa2xx-pcmcia",
	.dev			= {
		.platform_data	= &ghi270_pcmcia_ops,
	}
};


static ssize_t
wificd_read (struct file *file, char __user *buffer, size_t bytes, loff_t *off)
{
	if(bytes && (*off == 0)) {
		*buffer = wificd ? '1' : '0';
		*off += 1;
		return 1;
	}
	return 0;
}

static ssize_t
wificd_write(struct file *file, const char __user *buffer, size_t bytes, loff_t *off)
{
	switch(*buffer) {
		case '1':
			wificd = 1;
			break;
		case '0':
			wificd = 0;
			break;
		default:
			return -EINVAL;
			break;
	}
	return bytes;
}

static struct file_operations wificd_fops = {
	.owner              = THIS_MODULE,
	.read               = wificd_read,
	.write              = wificd_write,
};

static struct miscdevice wificd_dev = {
	.minor              = MISC_DYNAMIC_MINOR,
	.name               = "wificd",
	.fops               = &wificd_fops,
};


static int __init ghi270_pcmcia_init(void)
{
	if(misc_register(&wificd_dev)) {
		printk("Could not register wifi insertion device.\n");
		/* It's not fatal. */
	}

	return platform_device_register(&ghi270_pcmcia_device);
}

static void __exit
ghi270_pcmcia_exit(void)
{
	misc_deregister(&wificd_dev);
	platform_device_unregister(&ghi270_pcmcia_device);
}

module_init(ghi270_pcmcia_init);
module_exit(ghi270_pcmcia_exit);

MODULE_AUTHOR("Matt Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("Grayhill GHI270 PCMCIA/CF driver");
MODULE_LICENSE("GPL");
/* vim600: set noexpandtab sw=8 : */
