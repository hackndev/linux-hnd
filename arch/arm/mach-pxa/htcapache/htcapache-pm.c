/*
 * Resume from HTC bootloader
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

static u32 save_a0040000[4];

static void pxa_ll_pm_suspend(unsigned long resume_addr)
{
	memcpy(save_a0040000, addr_a0040000, sizeof(save_a0040000));

	/* jump to PSPR */
	addr_a0040000[0] = 0xe3a00101; // mov r0, #0x40000000
	addr_a0040000[1] = 0xe380060f; // orr r0, r0, #0x0f000000
	addr_a0040000[2] = 0xe3800008; // orr r0, r0, #8
	addr_a0040000[3] = 0xe590f000; // ldr pc, [r0]
}

static void pxa_ll_pm_resume(void)
{
	memcpy(addr_a0040000, save_a0040000, sizeof(save_a0040000));
}

static struct pxa_ll_pm_ops ll_pm_ops = {
	.suspend = pxa_ll_pm_suspend,
	.resume  = pxa_ll_pm_resume,
};

void htcapache_ll_pm_init(void)
{
	addr_a0040000 = phys_to_virt(0xa0040000);
	pxa_pm_set_ll_ops(&ll_pm_ops);
}
#endif /* CONFIG_PM */
