/*
 * Hardware definitions for the Toshiba eseries PDAs
 *
 * Copyright (c) Ian Molton 2003
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>
#include <asm/arch/pxa-dmabounce.h>

#include "../generic.h"

#include <asm/arch/eseries-irq.h>

#define DEBUG

static void __init eseries_init_irq( void )
{
	/* Initialize standard IRQs */
	pxa_init_irq();
//	if(machine_is_e800())
//		angelx_init_irq(PXA_CS5_PHYS);
}

#ifdef DEBUG
/* A debug mapping giving access to the screen RAM */
static struct map_desc eseries_io_desc[] __initdata = {
 /* virtual            physical           length      domain                 */
  { 0xf0000000,  0x0c100000,  0x02000000, DOMAIN_IO}
};
#endif
                                                                                
/* for dmabounce */
static int eseries_dma_needs_bounce(struct device *dev, dma_addr_t addr, size_t size) {
	return (addr >= 0x10000000 + 0x10000);
}

static void __init eseries_map_io(void)
{
	/* Initialize standard IO maps */
        pxa_map_io();

#ifdef DEBUG
        iotable_init(eseries_io_desc, ARRAY_SIZE(eseries_io_desc));
#endif

	pxa_set_dma_needs_bounce(eseries_dma_needs_bounce);
}

static void __init eseries_fixup(struct machine_desc *desc,
                struct tag *tags, char **cmdline, struct meminfo *mi)
{
        mi->nr_banks=1;
        mi->bank[0].start = 0xa0000000;
        mi->bank[0].node = 0;
        if (machine_is_e800())
                mi->bank[0].size = (128*1024*1024);
        else
                mi->bank[0].size = (64*1024*1024);
}

#ifdef CONFIG_MACH_E330
MACHINE_START(E330, "Toshiba e330")
        /* Maintainer: Ian Molton (spyro@f2s.com) */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= eseries_map_io,
	.init_irq	= eseries_init_irq,
	.fixup          = eseries_fixup,
        .timer = &pxa_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_E740
MACHINE_START(E740, "Toshiba e740")
        /* Maintainer: Ian Molton (spyro@f2s.com) */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= eseries_map_io,
	.init_irq	= eseries_init_irq,
	.fixup          = eseries_fixup,
        .timer = &pxa_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_E750
MACHINE_START(E750, "Toshiba e750")
        /* Maintainer: Ian Molton (spyro@f2s.com) */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= eseries_map_io,
	.init_irq	= eseries_init_irq,
	.fixup          = eseries_fixup,
        .timer = &pxa_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_E400
MACHINE_START(E400, "Toshiba e400")
        /* Maintainer: Ian Molton (spyro@f2s.com) */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= eseries_map_io,
	.init_irq	= eseries_init_irq,
	.fixup          = eseries_fixup,
	.timer = &pxa_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_E800
MACHINE_START(E800, "Toshiba e800")
        /* Maintainer: Ian Molton (spyro@f2s.com) */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= eseries_map_io,
	.init_irq	= eseries_init_irq,
	.fixup          = eseries_fixup,
        .timer = &pxa_timer,
MACHINE_END
#endif
