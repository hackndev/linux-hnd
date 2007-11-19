/*
 *  linux/arch/arm/mach-pxa/ra_alpha.c
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2003 by M&N Logistik-Lösungen Online GmbH
 *  Copyright (c) 2003 Stefan Eletzhofer <stefan.eletzhofer@inquant.de>
 *
 *  written by Stefan Eletzhofer, based on Holger Schurigs ramses code.
 * 
 */
#include <linux/init.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/root_dev.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/bitfield.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/pxapcmcia.h>

#include "generic.h"

/******************************************************************/
/*  Misc hardware                                                */
/******************************************************************/

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= RA_ALPHA_ETH_PHYS,
		.end	= RA_ALPHA_ETH_PHYS + RA_ALPHA_ETH_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= RA_ALPHA_IRQ_ETH,
		.end	= RA_ALPHA_IRQ_ETH,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

/******************************************************************/
/*  PCMCIA                                                        */
/******************************************************************/

static void ra_alpha_cf_rst( int flag)
{
	unsigned long flags;

	_DBG( "flag=%d", flag );

	local_irq_save(flags);
	if ( flag ) {
		GPSR(RA_ALPHA_GPIO_CF_RST) |= RA_ALPHA_GPIO_CF_RST;
	} else {
		GPCR(RA_ALPHA_GPIO_CF_RST) |= RA_ALPHA_GPIO_CF_RST;
	}
	local_irq_restore(flags);
}

static int ra_alpha_pcmcia_hw_init(struct pxa2xx_pcmcia_socket *skt)
{
	unsigned long flags;

	skt->irq = RA_ALPHA_IRQ_CF;

	local_irq_save(flags);
	pxa_gpio_mode( RA_ALPHA_GPIO_IRQ_CF | GPIO_IN );
	pxa_gpio_mode( RA_ALPHA_GPIO_CF_RST | GPIO_OUT );
	local_irq_restore(flags);

	_DBG( "socket nr=%d, irq=%d", skt->nr, skt->irq );

	return 0;
}

/*
 * Release all resources.
 */
static void ra_alpha_pcmcia_hw_shutdown(struct pxa2xx_pcmcia_socket *skt)
{
	_DBG( "skt=%p, nr=%d, irq=%d", skt, skt->nr, skt->irq );
	return;
}

static void
ra_alpha_pcmcia_socket_state(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_state *state)
{
	state->detect = 1; /* RotAlign CF is non-hozplugged and always inserted */
	state->ready  = (GPLR(RA_ALPHA_GPIO_IRQ_CF) & RA_ALPHA_GPIO_IRQ_CF) ? 1 : 0;
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1; /* Can only apply 3.3V */
	state->vs_Xv  = 0;
}

static int
ra_alpha_pcmcia_configure_socket(struct pxa2xx_pcmcia_socket *skt, const socket_state_t *state)
{
	/* Silently ignore Vpp, output enable, speaker enable. */
	_DBG( "socket nr=%d, irq=%d", skt->nr, skt->irq );

	ra_alpha_cf_rst( ( state->flags & SS_RESET ) == SS_RESET );

	return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static void ra_alpha_pcmcia_socket_init(struct pxa2xx_pcmcia_socket *skt)
{
	/* FIXME: Do a reset of the CF card here? */
	_DBG( "skt=%p, nr=%d, irq=%d", skt, skt->nr, skt->irq );
	return;
}

/*
 * Disable card status IRQs on suspend.
 */
static void ra_alpha_pcmcia_socket_suspend(struct pxa2xx_pcmcia_socket *skt)
{
	_DBG( "skt=%p, nr=%d, irq=%d", skt, skt->nr, skt->irq );
	ra_alpha_cf_rst( 1 );
}

static struct pcmcia_low_level ra_alpha_pcmcia_ops = { 
	.owner			= THIS_MODULE,

	/* only have one socket and I want to start at 1, not 0 */
	.first			= 1,
	.nr			= 1,

	.hw_init		= ra_alpha_pcmcia_hw_init,
	.hw_shutdown		= ra_alpha_pcmcia_hw_shutdown,

	.socket_state		= ra_alpha_pcmcia_socket_state,
	.configure_socket	= ra_alpha_pcmcia_configure_socket,

	.socket_init		= ra_alpha_pcmcia_socket_init,
	.socket_suspend		= ra_alpha_pcmcia_socket_suspend,
};

static struct platform_device pcmcia_device = {
        .name           = "pxa2xx-pcmcia",
        .id             = 0,
	.dev            = {
		.platform_data  = &ra_alpha_pcmcia_ops
	}
};

/******************************************************************/
/*  Initialisation                                                */
/******************************************************************/

static struct platform_device *devices[] __initdata = {
	&smc91x_device,
	&pcmcia_device,
};


static void __init ra_alpha_init(void)
{
	int ret;
	printk("ra_alpha_init()\n");

 	ret = platform_add_devices(devices, ARRAY_SIZE(devices));
 	_DBG( "platform_add_devices=%d", ret );
}

static void __init ra_alpha_init_irq(void)
{
	pxa_init_irq();
	set_irq_type(RA_ALPHA_IRQ_ETH, RA_ALPHA_IRQT_ETH);
}

static void __init
fixup_ra_alpha(struct machine_desc *desc, struct tag *tags, char **cmdline, struct meminfo *mi)
{
	// TODO: should be done in the bootloader!
	mi->nr_banks = 1;
	SET_BANK (0, 0xa0000000, 64*1024*1024);
}

static struct map_desc ra_alpha_io_desc[] __initdata = {
 /* virtual               physical              length                type */
  { RA_ALPHA_ETH_BASE,      RA_ALPHA_ETH_PHYS,      RA_ALPHA_ETH_SIZE,      MT_DEVICE },
  { RA_ALPHA_CPLD_VIRT,     RA_ALPHA_CPLD_PHYS,     RA_ALPHA_CPLD_SIZE,     MT_DEVICE },
};

static void __init ra_alpha_map_io(void)
{
	pxa_map_io();

  	/* input GPIOs */
  	pxa_gpio_mode(RA_ALPHA_GPIO_IRQ_ETH|GPIO_IN);

 	/* output GPIOs */
 	pxa_gpio_mode(RA_ALPHA_GPIO_n3V3_ON|GPIO_OUT);
 	pxa_gpio_mode(RA_ALPHA_GPIO_5V4_ON|GPIO_OUT);
 	pxa_gpio_mode(RA_ALPHA_GPIO_nUSBH_ON|GPIO_OUT);
 	pxa_gpio_mode(RA_ALPHA_GPIO_CORE_FAST1|GPIO_OUT);
 	pxa_gpio_mode(RA_ALPHA_GPIO_CORE_FAST2|GPIO_OUT);
 	pxa_gpio_mode(RA_ALPHA_GPIO_nMSP_RST|GPIO_OUT);

	iotable_init(ra_alpha_io_desc, ARRAY_SIZE(ra_alpha_io_desc));
}

MACHINE_START(RA_ALPHA, "PT RotAlign Alpha")
	MAINTAINER("Stefan Eletzhofer <stefan.eletzhofer@inquant.de>")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
	BOOT_PARAMS(0xa0000100)
	FIXUP(fixup_ra_alpha)
	MAPIO(ra_alpha_map_io)
	INITIRQ(ra_alpha_init_irq)
	INIT_MACHINE(ra_alpha_init)
MACHINE_END
