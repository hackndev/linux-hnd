/*
 * iPAQ hx4700 PCMCIA support
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * Based on code from iPAQ h2200
 *   Copyright (c) 2004 Koen Kooi <koen@handhelds.org>
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>
#include <asm/arch/hx4700-core.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#include "../../../../drivers/pcmcia/soc_common.h"

extern struct platform_device hx4700_asic3;

/*
 * CF resources used on the HX4700:
 *   from hx4700-asic.h
 *     GPIOC_CF_CD_N  		   	4	// Input
 *     GPIOC_CIOW_N			5	// Output, to CF socket
 *     GPIOC_CIOR_N			6	// Output, to CF socket
 *     GPIOC_CF_PWE_N 		   	10       // Input
 *     GPIOD_CF_CD_N  		   	4	// Input, from CF socket
 *     GPIOD_CIOIS16_N		   	11	// Input, from CF socket
 *     GPIOD_CWAIT_N  		   	12	// Input, from CF socket
 *     GPIOD_CF_RNB			13	// Input (read, not busy?)
 *   from hx4700-gpio.h / hx4700.c
 *     pxa_gpio_mode(GPIO_NR_HX4700_POE_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PWE_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PIOR_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PIOW_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PCE1_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PCE2_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PREG_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PWAIT_N_MD);
 *     pxa_gpio_mode(GPIO_NR_HX4700_PIOIS16_N_MD);
 *     GPIO_NR_HX4700_CF_RNB		60	// Input
 *     GPIO_NR_HX4700_CF_RESET		114	// Output
 *   EGPIOs
 *     EGPIO4_CF_3V3_ON			// Output only
 */

#if 0
#define _debug(s, args...) printk (KERN_INFO s, ##args)
#define _debug_func(s, args...) _debug ("%s: " s, __FUNCTION__, ##args)
#else
#define _debug(s, args...)
#define _debug_func(s, args...) _debug ("%s: " s, __FUNCTION__, ##args)
#endif

static struct pcmcia_irqs hx4700_cd_irq[] = {
	{ 0, 0,  "PCMCIA CD" }	/* fill in IRQ below */
};

static int power_en=0;

static void
hx4700_cf_reset( int state )
{
	_debug_func ("%d\n", state);
	SET_HX4700_GPIO( CF_RESET, state?1:0 );
}

static int
hx4700_pcmcia_hw_init( struct soc_pcmcia_socket *skt )
{
	int statusd;
	int irq;

	_debug_func ("\n");

	/* Turn on sleep mode, I think */
	asic3_set_extcf_select(&hx4700_asic3.dev,
		ASIC3_EXTCF_CF0_SLEEP_MODE | ASIC3_EXTCF_CF1_SLEEP_MODE |
		ASIC3_EXTCF_CF0_BUF_EN     | ASIC3_EXTCF_CF1_BUF_EN     |
		ASIC3_EXTCF_CF0_PWAIT_EN   | ASIC3_EXTCF_CF1_PWAIT_EN,

		ASIC3_EXTCF_CF0_SLEEP_MODE | ASIC3_EXTCF_CF1_SLEEP_MODE |
		ASIC3_EXTCF_CF0_BUF_EN     | 0                          |
		ASIC3_EXTCF_CF0_PWAIT_EN   | 0
	);
	mdelay(50);

	statusd = asic3_get_gpio_status_d( &hx4700_asic3.dev );
	power_en = (statusd & (1<<GPIOD_CF_CD_N)) ? 0 : 1;

	irq = hx4700_cd_irq[0].irq = asic3_irq_base( &hx4700_asic3.dev )
			+ ASIC3_GPIOD_IRQ_BASE + GPIOD_CF_CD_N;
	skt->irq = HX4700_IRQ( CF_RNB );

	if (power_en) {
		hx4700_egpio_enable( EGPIO4_CF_3V3_ON );
		set_irq_type( irq, IRQT_RISING );
	}
	else {
		hx4700_egpio_disable( EGPIO4_CF_3V3_ON );
		set_irq_type( irq, IRQT_FALLING );
	}

	pxa_gpio_mode(GPIO_NR_HX4700_POE_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PWE_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PIOR_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PIOW_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PCE1_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PCE2_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PREG_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PWAIT_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PIOIS16_N_MD | GPIO_DFLT_HIGH);
	pxa_gpio_mode(GPIO_NR_HX4700_PSKTSEL_MD);

	return soc_pcmcia_request_irqs(skt, hx4700_cd_irq, ARRAY_SIZE(hx4700_cd_irq));
}

/*
 * Release all resources.
 */
static void
hx4700_pcmcia_hw_shutdown( struct soc_pcmcia_socket *skt )
{
	_debug_func ("\n");
	soc_pcmcia_free_irqs( skt, hx4700_cd_irq, ARRAY_SIZE(hx4700_cd_irq) );
}

static void
hx4700_pcmcia_socket_state( struct soc_pcmcia_socket *skt,
    struct pcmcia_state *state )
{
	int statusd;

	statusd = asic3_get_gpio_status_d( &hx4700_asic3.dev );
	// printk( "hx4700_pcmcia_socket_state: statusd=0x%x\n", statusd );
	state->detect = (statusd & (1<<GPIOD_CF_CD_N)) ? 0 : 1;
	state->ready  = GET_HX4700_GPIO( CF_RNB ) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;

#if 0
	if (state->detect)
	    asic3_set_gpio_status_c( &hx4700_asic3.dev, 1<<GPIOC_CIOR_N,
		1<<GPIOC_CIOR_N );
	else
	    asic3_set_gpio_status_c( &hx4700_asic3.dev, 1<<GPIOC_CIOR_N, 0 );
#endif

	_debug( "detect:%d ready:%d vcc:%d\n",
	       state->detect, state->ready, state->vs_3v );
}

static int
hx4700_pcmcia_config_socket( struct soc_pcmcia_socket *skt,
    const socket_state_t *state )
{
	int statusc;

	statusc = asic3_get_gpio_status_c( &hx4700_asic3.dev );
	/* Silently ignore Vpp, output enable, speaker enable. */
	_debug_func( "Reset:%d Vcc:%d Astatusc=0x%x\n",
	    (state->flags & SS_RESET) ? 1 : 0, state->Vcc,
	    statusc );

	hx4700_cf_reset( state->flags & SS_RESET );
	if (state->Vcc == 0) {
		/* Turn on sleep mode, I think.  No docs for these bits. */
		asic3_set_extcf_select(&hx4700_asic3.dev,
			ASIC3_EXTCF_CF0_SLEEP_MODE | ASIC3_EXTCF_CF1_SLEEP_MODE |
			ASIC3_EXTCF_CF0_BUF_EN     | ASIC3_EXTCF_CF1_BUF_EN     |
			ASIC3_EXTCF_CF0_PWAIT_EN   | ASIC3_EXTCF_CF1_PWAIT_EN,

			ASIC3_EXTCF_CF0_SLEEP_MODE | ASIC3_EXTCF_CF1_SLEEP_MODE |
			ASIC3_EXTCF_CF0_BUF_EN     | 0                          |
			ASIC3_EXTCF_CF0_PWAIT_EN   | 0
		);
		hx4700_egpio_disable( EGPIO4_CF_3V3_ON );
		hx4700_cf_reset(1);	/* Ensure the slot is reset when powered off */
		power_en = 0;
	}
	else if (state->Vcc == 33 || state->Vcc == 50) {
		hx4700_egpio_enable( EGPIO4_CF_3V3_ON );
		mdelay(1);
		/* Turn on CF stuff without messing with SD bit 14.  */
		asic3_set_extcf_select(&hx4700_asic3.dev,
			ASIC3_EXTCF_CF0_SLEEP_MODE | ASIC3_EXTCF_CF1_SLEEP_MODE |
			ASIC3_EXTCF_CF0_BUF_EN     | ASIC3_EXTCF_CF1_BUF_EN     |
			ASIC3_EXTCF_CF0_PWAIT_EN   | ASIC3_EXTCF_CF1_PWAIT_EN,

			0                          | ASIC3_EXTCF_CF1_SLEEP_MODE |
			ASIC3_EXTCF_CF0_BUF_EN     | 0                          |
			ASIC3_EXTCF_CF0_PWAIT_EN   | 0
		);
		power_en = 1;
	}
	else {
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
static void
hx4700_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	_debug_func ("\n");
}

/*
 * Disable card status IRQs on suspend.
 */
static void
hx4700_pcmcia_socket_suspend( struct soc_pcmcia_socket *skt )
{
	_debug_func ("\n");
}

static struct pcmcia_low_level hx4700_pcmcia_ops = {
	.owner          	= THIS_MODULE,
	.nr         		= 1,
	.hw_init        	= hx4700_pcmcia_hw_init,
	.hw_shutdown		= hx4700_pcmcia_hw_shutdown,
	.socket_state		= hx4700_pcmcia_socket_state,
	.configure_socket	= hx4700_pcmcia_config_socket,
	.socket_init		= hx4700_pcmcia_socket_init,
	.socket_suspend		= hx4700_pcmcia_socket_suspend,
};

static struct platform_device hx4700_pcmcia_device = {
	.name			= "pxa2xx-pcmcia",
	.dev			= {
		.platform_data	= &hx4700_pcmcia_ops,
	}
};

static int __init
hx4700_pcmcia_init(void)
{
	if (!machine_is_h4700())
		return -ENODEV;
	_debug_func ("\n");

	return platform_device_register( &hx4700_pcmcia_device );
}

static void __exit
hx4700_pcmcia_exit(void)
{
	platform_device_unregister( &hx4700_pcmcia_device );
}

module_init(hx4700_pcmcia_init);
module_exit(hx4700_pcmcia_exit);

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("iPAQ Hx4700 PCMCIA/CF platform-specific driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

