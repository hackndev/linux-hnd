/*
 * MyPal 716 power management support for the original HTC IPL in DoC G3
 *
 * Use consistent with the GNU GPL is permitted, provided that this
 * copyright notice is preserved in its entirety in all copies and
 * derived works.
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

static u32 *addr_a0040000;
static u32 *addr_a0040004;
static u32 *addr_a0040008;
static u32 *addr_a004000c;

static u32 save_a0040000;
static u32 save_a0040004;
static u32 save_a0040008;
static u32 save_a004000c;

static void htcsable_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	save_a0040000 = *addr_a0040000;
	save_a0040004 = *addr_a0040004;
	save_a0040008 = *addr_a0040008;
	save_a004000c = *addr_a004000c;

	/* jump to PSPR */
	*addr_a0040000 = 0xe3a00101; // mov r0, #0x40000000
	*addr_a0040004 = 0xe380060f; // orr r0, r0, #0x0f000000
	*addr_a0040008 = 0xe3800008; // orr r0, r0, #8
	*addr_a004000c = 0xe590f000; // ldr pc, [r0]
}

static void htcsable_pxa_ll_pm_resume(void)
{
	*addr_a0040000 = save_a0040000;
	*addr_a0040004 = save_a0040004;
	*addr_a0040008 = save_a0040008;
	*addr_a004000c = save_a004000c;
}

static struct pxa_ll_pm_ops htcsable_ll_pm_ops = {
	.suspend = htcsable_pxa_ll_pm_suspend,
	.resume  = htcsable_pxa_ll_pm_resume,
};

void htcsable_ll_pm_init(void) {
	addr_a0040000 = phys_to_virt(0xa0040000);
	addr_a0040004 = phys_to_virt(0xa0040004);
	addr_a0040008 = phys_to_virt(0xa0040008);
	addr_a004000c = phys_to_virt(0xa004000c);

	pxa_pm_set_ll_ops(&htcsable_ll_pm_ops);
}
#endif /* CONFIG_PM */
