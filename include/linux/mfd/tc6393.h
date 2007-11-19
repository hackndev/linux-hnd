/*
 * linux/include/asm-arm/hardware/tc6393.h
 *
 * This file contains the definitions for the TC6393 
 *
 * (C) Copyright 2005 Dirk Opfer
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#ifndef _ASM_ARCH_TC6393XB_SOC
#define _ASM_ARCH_TC6393XB_SOC

struct tc6393xb_platform_data
{
	/* Standard MFD properties */
	int irq_base;
	struct platform_device **child_devs;
	int num_child_devs;

	u16 sys_gper;
	u16 sys_gpodsr1;
	u16 sys_gpooecr1;
	u16 sys_pll2cr;
	u16 sys_ccr;
	u16 sys_mcr;
	void (* hw_init) (void);
	void (* suspend) (void);
	void (* resume)  (void);
};

#define CK32KEN 0x1
#define USBCLK  0x2

//TC6393XB Resource Area Map (Offset)
//  System Configration Register Area         0x00000000 - 0x000000FF 
//  NAND Flash Host Controller Register Area  0x00000100 - 0x000001FF 
//  USB Host Controller Register Area         0x00000300 - 0x000003FF
//  LCD Host Controller Register Area         0x00000500 - 0x000005FF
//  NAND Flash Control Register               0x00001000 - 0x00001007
//  USB Control Register                      0x00003000 - 0x000031FF
//  LCD Control Register                      0x00005000 - 0x000051FF
//  Local Memory 0 (32KB)                     0x00010000 - 0x00017FFF
//  Local Memory 0 (32KB) alias               0x00018000 - 0x0001FFFF
//  Local Memory 1 (1MB)                      0x00100000 - 0x001FFFFF

#define TC6393_SYS_BASE		0

#define TC6393XB_NAND_CNF_BASE	(TC6393_SYS_BASE + 0x000100)
#define TC6393XB_NAND_CTL_BASE  (TC6393_SYS_BASE + 0x001000)
//#define TC6393XB_NAND_IRQ       -1

#define TC6393XB_MMC_CNF_BASE	(TC6393_SYS_BASE + 0x000200)
#define TC6393XB_MMC_CTL_BASE	(TC6393_SYS_BASE + 0x000800)
#define TC6393XB_MMC_IRQ          (1)

#define TC6393XB_USB_CNF_BASE   (TC6393_SYS_BASE + 0x000300)
#define TC6393XB_USB_CTL_BASE   (TC6393_SYS_BASE + 0x000a00)
#define TC6393XB_USB_IRQ        (0)

#define TC6393_SERIAL_CONF_BASE	(TC6393_SYS_BASE + 0x000400)
#define TC6393_GC_CONF_BASE	(TC6393_SYS_BASE + 0x000500)
#define TC6393_RAM0_BASE	(TC6393_SYS_BASE + 0x010000)
#define TC6393_RAM0_SIZE	(32*1024)
#define TC6393_RAM1_BASE	(TC6393_SYS_BASE + 0x100000)
#define TC6393_RAM1_SIZE	(64 * 1024 * 16)


/* 
 * Internal Local Memory use purpose
 *   RAM0 is used for USB
 *   RAM1 is used for GC
 */
/* Internal register mapping */
#define TC6393_GC_INTERNAL_REG_BASE	0x000600	/* Length 0x200 */

	
/* System Configuration register */
//#define TC6393_SYS_REG(ofst) (*(volatile unsigned short*)(TC6393_SYS_BASE+(ofst)))
#define TC6393_SYS_RIDR		0x008			// Revision ID
#define TC6393_SYS_ISR		0x050			// Interrupt Status
#define TC6393_SYS_IMR		0x052			// Interrupt Mask
#define TC6393_SYS_IRR		0x054			// Interrupt Routing
#define TC6393_SYS_GPER		0x060			// GP Enable
#define TC6393_SYS_GPAIOEN	0x061			// GP Alternative Enable
#define TC6393_SYS_GPISR1	0x064			// GPI Status 1
#define TC6393_SYS_GPISR2	0x066			// GPI Status 2
#define TC6393_SYS_GPIIMR1	0x068			// GPI INT Mask 1
#define TC6393_SYS_GPIIMR2	0x06A			// GPI INT Mask 2
#define TC6393_SYS_GPIEDER1	0x06C			// GPI Edge Detect Enable 1
#define TC6393_SYS_GPIEDER2	0x06E			// GPI Edge Detect Enable 2
#define TC6393_SYS_GPILIR1	0x070			// GPI Level Invert 1
#define TC6393_SYS_GPILIR2	0x072			// GPI Level Invert 2
#define TC6393_SYS_GPODSR1	0x078			// GPO Data set 1
#define TC6393_SYS_GPODSR2      0x07A			// GPO Data set 2
#define TC6393_SYS_GPOOECR1     0x07C			// GPO Data OE Contorol 1
#define TC6393_SYS_GPOOECR2     0x07E			// GPO Data OE Contorol 2
#define TC6393_SYS_GPIARCR1     0x080			// GP Internal Active Register Contorol 1
#define TC6393_SYS_GPIARCR2     0x082			// GP Internal Active Register Contorol 2
#define TC6393_SYS_GPIARLCR1    0x084			// GP Internal Active Register Level Contorol 1
#define TC6393_SYS_GPIARLCR2    0x086			// GP Internal Active Register Level Contorol 2

#define TC6393_SYS_GPIBCR1      0x089			// GPa Internal Activ Register Contorol 1

#define TC6393_SYS_GPIBCR2      0x08A			// GPa Internal Activ Register Contorol 2
#define TC6393_SYS_GPaIARCR     0x08C			
#define TC6393_SYS_GPaIARLCR    0x090
#define TC6393_SYS_GPaIBCR      0x094
#define TC6393_SYS_CCR          0x098			/* Clock Control Register */
#define TC6393_SYS_PLL2CR       0x09A			// PLL2 Control
#define TC6393_SYS_PLL1CR1      0x09C			// PLL1 Control 1
#define TC6393_SYS_PLL1CR2      0x09E			// PLL1 Control 2
#define TC6393_SYS_DCR          0x0A0
#define TC6393_SYS_FER          0x0E0			/* Function Enable Register */
#define TC6393_SYS_MCR          0x0E4
#define TC6393_SYS_ConfigCR     0x0FC

//#define IS_TC6393_RAM0(p)	(TC6393_RAM0_BASE <= (unsigned int)p && (unsigned int)p <= TC6393_RAM0_BASE + TC6393_RAM0_SIZE)
//#define TC6393_RAM0_VAR_TO_OFFSET(x)	((unsigned int)x - TC6393_RAM0_BASE)
//#define TC6393_RAM0_OFFSET_TO_VAR(x)	((unsigned int)x + TC6393_RAM0_BASE)

/* GPIO bit */
#define TC6393_GPIO19  ( 1 << 19 )
#define TC6393_GPIO18  ( 1 << 18 )
#define TC6393_GPIO17  ( 1 << 17 )
#define TC6393_GPIO16  ( 1 << 16 )
#define TC6393_GPIO15  ( 1 << 15 )
#define TC6393_GPIO14  ( 1 << 14 )
#define TC6393_GPIO13  ( 1 << 13 )
#define TC6393_GPIO12  ( 1 << 12 )
#define TC6393_GPIO11  ( 1 << 11 )
#define TC6393_GPIO10  ( 1 << 10 )
#define TC6393_GPIO9   ( 1 << 9 )
#define TC6393_GPIO8   ( 1 << 8 )
#define TC6393_GPIO7   ( 1 << 7 )
#define TC6393_GPIO6   ( 1 << 6 )
#define TC6393_GPIO5   ( 1 << 5 )
#define TC6393_GPIO4   ( 1 << 4 )
#define TC6393_GPIO3   ( 1 << 3 )
#define TC6393_GPIO2   ( 1 << 2 )
#define TC6393_GPIO1   ( 1 << 1 )
#define TC6393_GPIO0   ( 1 << 0 )

#define TC6393XB_NR_IRQS	8

#endif
