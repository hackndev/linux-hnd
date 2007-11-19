/*
 * linux/include/asm-arm/hardware/tc6387xb.h
 *
 * This file contains the definitions for the TC6393XB
 *
 * (C) Copyright 2005 Ian Molton <spyro@f2s.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#ifndef _ASM_ARCH_T7L66XB_SOC
#define _ASM_ARCH_T7L66XB_SOC

#include <linux/platform_device.h>

// FIXME - this needs to be a common struct to all TMIO based SoCs.
struct tmio_hwconfig {
	void (*hwinit)(struct platform_device *sdev);
	void (*suspend)(struct platform_device *sdev);
	void (*resume)(struct platform_device *sdev);
};

struct tc6387xb_platform_data
{
	/* Standard MFD properties */
	int irq_base;
	struct platform_device **child_devs;
	int num_child_devs;

	void (* hw_init) (void);
	void (* suspend) (void);
	void (* resume)  (void);
};

#define TC6387XB_MMC_CNF_BASE    (0x000200)
#define TC6387XB_MMC_CTL_BASE    (0x000800)
#define TC6387XB_MMC_IRQ         (0)

#endif
