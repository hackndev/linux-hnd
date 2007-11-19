/*
 *
 * Definitions for H3600 Handheld Computer
 *
 * Copyright 2000 Compaq Computer Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 * 2005-04-02   Holger Hans Peter Freyther migrate to own file for the 2.6 port
 *
 */

#ifndef _INCLUDE_IPAQSA_H_
#define _INCLUDE_IPAQSA_H_

/* Physical memory regions corresponding to chip selects */
#define IPAQSA_EGPIO_PHYS     0x49000000
#define IPAQSA_BANK_2_PHYS    0x10000000
#define IPAQSA_BANK_4_PHYS    0x40000000


/* Virtual memory regions corresponding to chip selects 2 & 4 (used on sleeves) */
#define IPAQSA_EGPIO_VIRT     0xf0000000
#define IPAQSA_BANK_2_VIRT    0xf1000000
#define IPAQSA_BANK_4_VIRT    0xf3800000

/*
   Machine-independent GPIO definitions
   --- these are common across SA iPAQs
*/

#define GPIO_NR_H3600_NPOWER_BUTTON	0   /* Also known as the "off button"  */

#define GPIO_NR_H3600_PCMCIA_CD1	10
#define GPIO_NR_H3600_PCMCIA_IRQ1	11

/* UDA1341 L3 Interface */
#define GPIO_H3600_L3_DATA		GPIO_GPIO (14)
#define GPIO_H3600_L3_MODE		GPIO_GPIO (15)
#define GPIO_H3600_L3_CLOCK		GPIO_GPIO (16)

#define GPIO_NR_H3600_PCMCIA_CD0	17
#define GPIO_NR_H3600_SYS_CLK		19
#define GPIO_NR_H3600_PCMCIA_IRQ0	21

#define GPIO_NR_H3600_COM_DCD		23
#define GPIO_H3600_OPT_IRQ		GPIO_GPIO (24)
#define GPIO_NR_H3600_COM_CTS		25
#define GPIO_NR_H3600_COM_RTS		26

#define IRQ_GPIO_H3600_NPOWER_BUTTON	IRQ_GPIO0
#define IRQ_GPIO_H3600_PCMCIA_CD1	IRQ_GPIO10
#define IRQ_GPIO_H3600_PCMCIA_IRQ1	IRQ_GPIO11
#define IRQ_GPIO_H3600_PCMCIA_CD0	IRQ_GPIO17
#define IRQ_GPIO_H3600_PCMCIA_IRQ0	IRQ_GPIO21
#define IRQ_GPIO_H3600_COM_DCD		IRQ_GPIO23
#define IRQ_GPIO_H3600_OPT_IRQ		IRQ_GPIO24
#define IRQ_GPIO_H3600_COM_CTS		IRQ_GPIO25


#include <asm/hardware/ipaq-ops.h>

#endif
