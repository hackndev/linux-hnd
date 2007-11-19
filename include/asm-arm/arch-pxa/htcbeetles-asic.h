/*
 * include/asm/arm/arch-pxa/htcbeetles-asic.h
 *
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see below:
 * 
 * include/asm/arm/arch-pxa/hx4700-asic.h
 *      Copyright (c) 2004 SDG Systems, LLC
 *
 */

#ifndef _HTCBEETLES_ASIC_H_
#define _HTCBEETLES_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/* ASIC3 */

#define HTCBEETLES_ASIC3_GPIO_PHYS	PXA_CS4_PHYS
#define HTCBEETLES_ASIC3_MMC_PHYS	PXA_CS3_PHYS+0x02000000

/* TODO: some information is missing here :) */

/* ASIC3 GPIO A bank */

#define GPIOA_USB_PUEN 			7	/* Output */

/* ASIC3 GPIO B bank */

#define GPIOB_USB_DETECT 			6	/* Input */

/* ASIC3 GPIO C bank */

/* ASIC3 GPIO D bank */

#endif /* _HTCBEETLES_ASIC_H_ */

extern struct platform_device htcbeetles_asic3;
