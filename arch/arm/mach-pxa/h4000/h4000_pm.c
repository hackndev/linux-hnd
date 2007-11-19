/*
 * h4000 power management support for the original IPL in DoC G2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2005 Pawel Kolodziejski
 *
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pm.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>

#ifdef CONFIG_PM

static u32 *addr_a0100000;
static u32 *addr_a0100004;
static u32 *addr_a0100008;
static u32 *addr_a010000c;
static u32 save_a0100000;
static u32 save_a0100004;
static u32 save_a0100008;
static u32 save_a010000c;

static void h4000_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	save_a0100000 = *addr_a0100000;
	save_a0100004 = *addr_a0100004;
	save_a0100008 = *addr_a0100008;
	save_a010000c = *addr_a010000c;

	*addr_a0100000 = 0xe3a00101; // mov r0,     #0x40000000
	*addr_a0100004 = 0xe380060f; // orr r0, r0, #0x0f000000
	*addr_a0100008 = 0xe3800008; // orr r0, r0, #0x00000008
	*addr_a010000c = 0xe590f000; // ldr pc, [r0]

	return;
}

static void h4000_pxa_ll_pm_resume(void)
{
	/* 
	   Memory/peripherals configuration, etc. should be actually
	   handled by bootloader. It is not preserved by the kernel 
	   as generally you can't reach RAM unless you have setup it. 
	   In case of h4000 though, we are lucky that reset defaults 
	   work at least for SDRAM. We need correct values to access 
	   ASIC3 and other controllers though.
         */
	MSC0 = 0x7ff018b8;
	MSC1 = 0x7ff42358;
	MSC2 = 0x7ff47ff4;

	*addr_a0100000 = save_a0100000;
	*addr_a0100004 = save_a0100004;
	*addr_a0100008 = save_a0100008;
	*addr_a010000c = save_a010000c;
}

static struct pxa_ll_pm_ops h4000_ll_pm_ops = {
	.suspend = h4000_pxa_ll_pm_suspend,
	.resume  = h4000_pxa_ll_pm_resume,
};

void h4000_ll_pm_init(void) {
	addr_a0100000 = phys_to_virt(0xa0100000);
	addr_a0100004 = phys_to_virt(0xa0100004);
	addr_a0100008 = phys_to_virt(0xa0100008);
	addr_a010000c = phys_to_virt(0xa010000c);

	pxa_pm_set_ll_ops(&h4000_ll_pm_ops);
}

#endif /* CONFIG_PM */
