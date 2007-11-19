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

extern void pxa_cpu_resume(void);

static u32 saved_value[4];
static u32 *save_base;

static void eseries_pxa_ll_pm_suspend(unsigned long resume_addr) {

	saved_value[0] = *(save_base+(0x9c>>2));
	saved_value[1] = *(save_base+(0x98>>2));
	saved_value[2] = *(save_base+(0x94>>2));
	saved_value[3] = *(save_base+(0x3c>>2));

//	asm ("mrc       p15, 0, %0, cr3, cr0;\n" :"=r"(rp) );
	*(save_base+(0x9c>>2)) = 1; // FIXME - flush cache
 //       asm ("mrc       p15, 0, %0, cr2, cr0;\n" :"=r"(rp) );
	*(save_base+(0x98>>2)) = 0xa0000000; // FIXME - flush cache
//	asm ("mrc       p15, 0, %0, cr1, cr0;\n" :"=r"(rp) );
	*(save_base+(0x94>>2)) = 0x78; // FIXME - flush cache
	*(save_base+(0x3c>>2)) = resume_addr; // FIXME - flush cache
	return;
}

static void eseries_pxa_ll_pm_resume(void) {
	*(save_base+(0x9c>>2)) = saved_value[0]; // FIXME - flush cache
	*(save_base+(0x98>>2)) = saved_value[1]; // FIXME - flush cache
	*(save_base+(0x94>>2)) = saved_value[2]; // FIXME - flush cache
	*(save_base+(0x3c>>2)) = saved_value[3]; // FIXME - flush cache
}

static struct pxa_ll_pm_ops eseries_ll_pm_ops = {
	.suspend = eseries_pxa_ll_pm_suspend,
	.resume = eseries_pxa_ll_pm_resume,
};

static int __init eseries_ll_pm_init(void)
{
	if(machine_is_e330() || machine_is_e740()) {
		printk("Initialising wince bootloader suspend workaround\n");
		save_base = phys_to_virt(0xa0040400);
		pxa_pm_set_ll_ops(&eseries_ll_pm_ops);
	}
	
	return 0;
}

module_init(eseries_ll_pm_init);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("implement e-series pm using the wince loader");
MODULE_LICENSE("GPLv2");
