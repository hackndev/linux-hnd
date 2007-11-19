/*
 *
 * Definitions for HP iPAQ Handheld Computer
 *
 * Copyright 2002 Compaq Computer Corporation.
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
 */

#ifndef _H3900_GPIO_H_
#define _H3900_GPIO_H_

#define GPIO_NR_H3900_POWER_BUTTON_N	(0)   /* Also known as the "off button"  */
// #define GPIO_NR_H3900_AC_IN		(1)   /* not connected ? */
#define GPIO_NR_H3900_CIR_RST		(2)
#define GPIO_NR_H3900_PCMCIA_CD0_N	(3)
#define GPIO_NR_H3900_MMC_INT_N		(4)
#define GPIO_NR_H3900_OPT_IND_N		(5)
// #define GPIO_NR_H3900_SD_IRQ		(6)  /* is this connected ? */
#define GPIO_NR_H3900_PCMCIA_IRQ0_N	(7)
#define GPIO_NR_H3900_AC_IN_N		(8)
#define GPIO_NR_H3900_OPT_INT		(9)
#define GPIO_NR_H3900_ASIC2_INT		(10)
#define GPIO_NR_H3900_CLK_OUT		(11)
#define GPIO_NR_H3900_PCMCIA_CD1_N	(12)
#define GPIO_NR_H3900_PCMCIA_IRQ1_N	(13)
#define GPIO_NR_H3900_COM_DCD		(14)
// 15 is CS1# (need to set GAFR )
#define GPIO_NR_H3900_FLASH_VPEN	(16)
#define GPIO_NR_H3900_SD_IRQ		(17) /* is this one connected? */
// 18 is RDY (need to set GAFR)  
#define GPIO_NR_H3900_OPT_BAT_FAULT	(19)
#define GPIO_NR_H3900_USBP_PULLUP	(20)
// #define GPIO_NR_H3900_MMC_INT_N		(21) also connected to gpio4 ?  
// 22 unused	
#define GPIO_NR_H3900_SSP_CLK		(23) /* not used? */
#define GPIO_NR_H3900_SSP_CS_N		(24) /* not used? */ 
#define GPIO_NR_H3900_SSP_TXD           (25) /* not used? */
#define GPIO_NR_H3900_SSP_RXD           (26) /* not used? */
#define GPIO_NR_H3900_ASIC2_SYS_CLK	(27) /* not used? */

/* IIS to UDA1380 */
#define GPIO_NR_H3900_AC97_BITCLK       (28)
#define GPIO_NR_H3900_AC97_SDATA_IN     (29)
#define GPIO_NR_H3900_AC97_SDATA_OUT    (30)
#define GPIO_NR_H3900_AC97_SYNC         (31)

#define GPIO_NR_H3900_SYS_CLK		(32)
// 33 is CS5#, (need to set GAFR)

#define GPIO_NR_H3900_FFUART_RXD        (34)
#define GPIO_NR_H3900_FFUART_CTS        (35)
#define GPIO_NR_H3900_FFUART_DCD        (36)
#define GPIO_NR_H3900_FFUART_DSR        (37)
#define GPIO_NR_H3900_SD_WP_N		(38) /* should be FFUART ring indicator */
#define GPIO_NR_H3900_FFUART_TXD        (39)
#define GPIO_NR_H3900_FFUART_DTR        (40)
#define GPIO_NR_H3900_FFUART_RTS        (41)

#define GPIO_NR_H3900_BTUART_RXD        (42)
#define GPIO_NR_H3900_BTUART_TXD        (43)
#define GPIO_NR_H3900_BTUART_CTS        (44)
#define GPIO_NR_H3900_BTUART_RTS        (45)

#define GPIO_NR_H3900_IR_RXD            (46)
#define GPIO_NR_H3900_IR_TXD            (47)

// 48 is POE# (need to set GAFR)
// 49 is PWE# (need to set GAFR)
// 50 is PIORD# (need to set GAFR)
// 51 is PIOWR# (need to set GAFR)
// 52 is PCE1# (need to set GAFR)
// 53 is PCE2# (need to set GAFR)
// 54 is PSKTSEL (need to set GAFR)
// 55 is PREG# (need to set GAFR)
// 56 is PWAIT# (need to set GAFR)
// 57 is IOIS16# (need to set GAFR)

// 58 through 77 is LCD signals (need to set GAFR)

// 78 is CS2# (need to set GAFR)
// 79 is CS3# (need to set GAFR)
// 80 is CS4# (need to set GAFR)

#define GPIO_H3900_POWER_BUTTON_N	GPIO_bit (GPIO_NR_H3900_POWER_BUTTON_N)   /* Also known as the "off button"  */
// #define GPIO_H3900_AC_IN		GPIO_bit (GPIO_NR_H3900_AC_IN)   /* not connected ? */
#define GPIO_H3900_CIR_RST		GPIO_bit (GPIO_NR_H3900_CIR_RST)
#define GPIO_H3900_PCMCIA_CD0		GPIO_bit (GPIO_NR_H3900_PCMCIA_CD0_N)
#define GPIO_H3900_MMC_INT_N		GPIO_bit (GPIO_NR_H3900_MMC_INT_N)
#define GPIO_H3900_OPT_IND_N		GPIO_bit (GPIO_NR_H3900_OPT_IND_N)

#define GPIO_H3900_PCMCIA_IRQ0		GPIO_bit (GPIO_NR_H3900_PCMCIA_IRQ0_N)
#define GPIO_H3900_AC_IN_N		GPIO_bit (GPIO_NR_H3900_AC_IN_N)
#define GPIO_H3900_OPT_INT		GPIO_bit (GPIO_NR_H3900_OPT_INT)
#define GPIO_H3900_ASIC2_INT		GPIO_bit (GPIO_NR_H3900_ASIC2_INT)
#define GPIO_H3900_CLK_OUT		GPIO_bit (GPIO_NR_H3900_CLK_OUT)
#define GPIO_H3900_PCMCIA_CD1		GPIO_bit (GPIO_NR_H3900_PCMCIA_CD1_N)
#define GPIO_H3900_PCMCIA_IRQ1		GPIO_bit (GPIO_NR_H3900_PCMCIA_IRQ1_N)
#define GPIO_H3900_COM_DCD		GPIO_bit (GPIO_NR_H3900_COM_DCD)

#define GPIO_H3900_FLASH_VPEN		GPIO_bit(GPIO_NR_H3900_FLASH_VPEN)
#define GPIO_H3900_SD_IRQ		GPIO_bit(GPIO_NR_H3900_SD_IRQ) /* is this one connected? */
	    
#define GPIO_H3900_OPT_BAT_FAULT	GPIO_bit(GPIO_NR_H3900_OPT_BAT_FAULT)
#define GPIO_H3900_USBP_PULLUP		GPIO_bit(GPIO_NR_H3900_USBP_PULLUP)
// #define GPIO_H3900_MMC_INT_N		GPIO_bit(GPIO_NR_H3900_MMC_INT_N) // also connected to gpio4 ?
// 22 unused	
#define GPIO_H3900_SSP_CLK		GPIO_bit(GPIO_NR_H3900_SSP_CLK)
#define GPIO_H3900_SSP_CS_N		GPIO_bit(GPIO_NR_H3900_SSP_CS_N)
	    
#define GPIO_H3900_ASIC2_SYS_CLK	GPIO_bit(GPIO_NR_H3900_ASIC2_SYS_CLK)
	    
#define GPIO_H3900_SYS_CLK		GPIO_bit(GPIO_NR_H3900_SYS_CLK)
	    
#define GPIO_H3900_SD_WP_N		GPIO_bit(GPIO_NR_H3900_SD_WP_N)

#define IRQ_GPIO_H3900_POWER_BUTTON_N	IRQ_GPIO (GPIO_NR_H3900_POWER_BUTTON_N)   /* Also known as the "off button"  */
// #define IRQ_GPIO_H3900_AC_IN		IRQ_GPIO (GPIO_NR_H3900_AC_IN)   /* not connected ? */
#define IRQ_GPIO_H3900_CIR_RST		IRQ_GPIO (GPIO_NR_H3900_CIR_RST)
#define IRQ_GPIO_H3900_PCMCIA_CD0	IRQ_GPIO (GPIO_NR_H3900_PCMCIA_CD0_N)
// #define IRQ_GPIO_H3900_MMC_INT_N	IRQ_GPIO (GPIO_NR_H3900_MMC_INT_N) // also connected to gpio4
#define IRQ_GPIO_H3900_OPT_IND_N	IRQ_GPIO (GPIO_NR_H3900_OPT_IND_N)
#define IRQ_GPIO_H3900_SYS_CLK		IRQ_GPIO (GPIO_NR_H3900_SYS_CLK)
#define IRQ_GPIO_H3900_PCMCIA_IRQ0	IRQ_GPIO (GPIO_NR_H3900_PCMCIA_IRQ0_N)
#define IRQ_GPIO_H3900_AC_IN_N		IRQ_GPIO (GPIO_NR_H3900_AC_IN_N)
#define IRQ_GPIO_H3900_OPT_INT		IRQ_GPIO (GPIO_NR_H3900_OPT_INT)
#define IRQ_GPIO_H3900_ASIC2_INT	IRQ_GPIO (GPIO_NR_H3900_ASIC2_INT)
#define IRQ_GPIO_H3900_CLK_OUT		IRQ_GPIO (GPIO_NR_H3900_CLK_OUT)
#define IRQ_GPIO_H3900_PCMCIA_CD1	IRQ_GPIO (GPIO_NR_H3900_PCMCIA_CD1_N)
#define IRQ_GPIO_H3900_PCMCIA_IRQ1	IRQ_GPIO (GPIO_NR_H3900_PCMCIA_IRQ1_N)
#define IRQ_GPIO_H3900_COM_DCD		IRQ_GPIO (GPIO_NR_H3900_COM_DCD)

#endif /* _H3900_GPIO_H_ */
