/*
 * h3900 supsend/resume support for the original bootloader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2006 Paul Sokolovsky
 *
 * Based on code from hx4700_core.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>


static u32 save[4];
static u32 save2[0xa * 4];

static void
h3900_pxa_ll_pm_suspend(unsigned long resume_addr)
{
        int i;
        u32 csum, tmp, *p;

        for (p = phys_to_virt(0xa007a000), i = 0; i < ARRAY_SIZE(save2); i++)
                save2[i] = p[i];

        for (p = phys_to_virt(0xa0000000), i = 0; i < ARRAY_SIZE(save); i++)
                save[i] = p[i];

        /* Set the first four words at 0xa0000000 to:
         * resume address; MMU control; TLB base addr; domain id */
        p[0] = resume_addr;

        asm( "mrc\tp15, 0, %0, c1, c0, 0" : "=r" (tmp) );
        p[1] = tmp & ~(0x3987);     /* mmu off */

        asm( "mrc\tp15, 0, %0, c2, c0, 0" : "=r" (tmp) );
        p[2] = tmp;     /* Shouldn't matter, since MMU will be off. */

        asm( "mrc\tp15, 0, %0, c3, c0, 0" : "=r" (tmp) );
        p[3] = tmp;     /* Shouldn't matter, since MMU will be off. */

        /* Set PSPR to the checksum the HTC bootloader wants to see. */
        for (csum = 0, i = 0; i < 0x30; i++) {
                tmp = p[i] & 0x1;
                tmp = tmp << 31;
                tmp |= tmp >> 1;
                csum += tmp;
        }

        PSPR = csum;
}

static void
h3900_pxa_ll_pm_resume(void)
{
        int i;
        u32 *p;

        for (p = phys_to_virt(0xa0000000), i = 0; i < ARRAY_SIZE(save); i++)
                p[i] = save[i];

        for (p = phys_to_virt(0xa007a000), i = 0; i < ARRAY_SIZE(save2); i++)
                p[i] = save2[i];

        /* XXX Do we need to flush the cache? */
}

struct pxa_ll_pm_ops h3900_ll_pm_ops = {
        .suspend = h3900_pxa_ll_pm_suspend,
        .resume  = h3900_pxa_ll_pm_resume,
};

void h3900_ll_pm_init(void) {
        pxa_pm_set_ll_ops(&h3900_ll_pm_ops);
}
