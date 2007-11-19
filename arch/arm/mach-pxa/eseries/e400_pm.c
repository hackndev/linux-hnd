/*
 * e-series wince-bootloader support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/pxa-regs.h>

extern void pxa_cpu_resume(void);

static u32 saved_value[4];
static u32 *save_base;

static unsigned long calc_csum(u32 *buf) {
	unsigned long val = 0x5a72;
	int i;

	for(i = 0 ; i < 0x5e ; i++) {
		val += *buf;
		val = ((val<<1)&0xfffffffe)|((val>>31)&0x1);
		buf++;
	}
	return val;
}

static void eseries_pxa_ll_pm_suspend(unsigned long resume_addr) {
	saved_value[0] = *save_base;
	saved_value[1] = *(save_base+(36>>2));

//	asm ("mrc       p15, 0, %0, cr3, cr0;\n" :"=r"(rp) );
//	*(save_base+(0x9c>>2)) = 1; // FIXME - flush cache

	*save_base           = 0; // FIXME - flush cache
	*(save_base+(36>>2)) = resume_addr; // FIXME - flush cache

	PSPR = calc_csum(save_base);
	return;
}

static void eseries_pxa_ll_pm_resume(void) {
	*(save_base)         = saved_value[0]; // FIXME - flush cache
	*(save_base+(36>>2)) = saved_value[1]; // FIXME - flush cache
}

static struct pxa_ll_pm_ops eseries_ll_pm_ops = {
	.suspend = eseries_pxa_ll_pm_suspend,
	.resume = eseries_pxa_ll_pm_resume,
};

static int __init eseries_ll_pm_init(void)
{
	save_base = 0;

	if(machine_is_e400() || machine_is_e750())
		save_base = phys_to_virt(0xa3f7a000);
	if(machine_is_e800())
		save_base = phys_to_virt(0xa00b8000);

	if(save_base) {
		printk("Initialising wince bootloader suspend workaround\n");
		pxa_pm_set_ll_ops(&eseries_ll_pm_ops);
	}
	
	return 0;
}

module_init(eseries_ll_pm_init);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("implement e-series pm using the wince loader");
MODULE_LICENSE("GPLv2");
