/*
 * linux/include/asm-arm/hardware/t7l66xb.h
 *
 * This file contains the definitions for the T7L66XB
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

struct tmio_ohci_hwconfig {
	void (*start)(struct platform_device *dev);
};

struct t7l66xb_platform_data
{
	/* Standard MFD properties */
	int irq_base;
	struct platform_device **child_devs;
	int num_child_devs;

	void (* hw_init) (void);
	void (* suspend) (void);
	void (* resume)  (void);
};


#define T7L66XB_NAND_CNF_BASE  (0x000100)
#define T7L66XB_NAND_CTL_BASE  (0x001000)

#define T7L66XB_MMC_CNF_BASE   (0x000200)
#define T7L66XB_MMC_CTL_BASE   (0x000800)
#define T7L66XB_MMC_IRQ        (1)

#define T7L66XB_USB_CNF_BASE   (0x000300)
#define T7L66XB_USB_CTL_BASE   (0x002000)
#define T7L66XB_OHCI_IRQ       (0)

/* System Configuration register */
#define T7L66XB_SYS_RIDR    0x008        // Revision ID
#define T7L66XB_SYS_ISR     0x0e1        // Interrupt Status
#define T7L66XB_SYS_IMR     0x042        // Interrupt Mask
//#define T7L66XB_SYS_IRR     0x054        // Interrupt Routing

#define T7L66XB_NR_IRQS	8

#endif
