/*
 * Machine initialization for Dell Axim X3
 *
 * Authors: Andrew Zabolotny <zap@homelink.ru>
 *
 * For now this is mostly a placeholder file; since I don't own an Axim X3
 * it is supposed the project to be overtaken by somebody else. The code
 * in here is *supposed* to work so that you can at least boot to the command
 * line, but there is no guarantee.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>

#include <asm/setup.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/irqs.h>

#include <asm/arch/aximx30-init.h>
#include <asm/arch/aximx30-gpio.h>

#include "../generic.h"


static void __init x30_init_irq (void)
{
	/* Initialize standard IRQs */
	pxa_init_irq();

// @@@ todo
//	set_irq_type	(IRQ_GPIO(GPIO_NR_AXIMX3_CABEL_RDY_N), IRQT_BOTHEDGE);
//	setup_irq	(IRQ_GPIO(GPIO_NR_AXIMX3_CABEL_RDY_N), &aximx3_charge_irq);
//	set_irq_type	(IRQ_GPIO(GPIO_NR_AXIMX3_BATTERY_LATCH_N), IRQT_BOTHEDGE);
//	setup_irq	(IRQ_GPIO(GPIO_NR_AXIMX3_BATTERY_LATCH_N), &aximx3_charge_irq);
//	set_irq_type	(IRQ_GPIO(GPIO_NR_AXIMX3_CHARGING_N), IRQT_BOTHEDGE);
//	setup_irq	(IRQ_GPIO(GPIO_NR_AXIMX3_CHARGING_N), &aximx3_charge_irq);
}

static void __init x30_map_io(void)
{
	pxa_map_io();

#if 0
	/* Configure power management stuff. */
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

	/* Wake up on CF/SD card insertion, Power and Record buttons,
	   AC adapter plug/unplug */
	PWER = PWER_GPIO0 | PWER_GPIO6 | PWER_GPIO7 | PWER_GPIO10
	     | PWER_RTC | PWER_GPIO4;
	PFER = PWER_GPIO0 | PWER_GPIO4 | PWER_RTC;
	PRER = PWER_GPIO4 | PWER_GPIO10;
	PCFR = PCFR_OPDE;

	/* Set up GPIO direction and alternate function registers */
	GAFR0_L = GAFR0x_InitValue;
	GAFR0_U = GAFR1x_InitValue;
	GAFR1_L = GAFR0y_InitValue;
	GAFR1_U = GAFR1y_InitValue;
	GAFR2_L = GAFR0z_InitValue;
	GAFR2_U = GAFR1z_InitValue;
	
	GPDR0 = GPDRx_InitValue;
	GPDR1 = GPDRy_InitValue;
	GPDR2 = GPDRz_InitValue;
#endif
	
//	GPCR0 = 0x0fffffff;       /* All outputs are set low by default */
}

static void __init x30_init (void)
{
	/* nothing to do here now */
}

MACHINE_START(X30, "Dell Axim X30")
	MAINTAINER("Giusepe Zompatori <giuseppe_zompatori@yahoo.it>")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
	BOOT_PARAMS(0xa0000100)
	MAPIO(x30_map_io)
	INITIRQ(x30_init_irq)
	.init_machine = x30_init,
MACHINE_END
