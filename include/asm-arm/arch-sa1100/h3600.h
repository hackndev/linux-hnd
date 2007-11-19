/*
 *
 * Definitions for H3600 Handheld Computer
 *
 * Copyright 2000-2002 Compaq Computer Corporation.
 * Copyright 2003 Hewlett-Packard Company.
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
 * 2001-10-??	Andrew Christian   Added support for iPAQ H3800
 *
 */

#ifndef _INCLUDE_H3600_H_
#define _INCLUDE_H3600_H_

#include <linux/interrupt.h>

#include <asm/arch/ipaqsa.h>

/* 
 * GPIO lines that are common across ALL SA1100 iPAQ models are in "asm/arch-sa1100/ipaqsa.h" 
 * This file contains machine-specific definitions
 */

#define GPIO_H3600_MICROCONTROLLER	GPIO_GPIO (1)   /* From ASIC2 on H3800 */

/* for H3600, audio sample rate clock generator */
#define GPIO_H3600_CLK_SET0		GPIO_GPIO (12)
#define GPIO_H3600_CLK_SET1		GPIO_GPIO (13)

#define GPIO_H3600_ACTION_BUTTON	GPIO_GPIO (18)
#define GPIO_H3600_SOFT_RESET           GPIO_GPIO (20)   /* Also known as BATT_FAULT */
#define GPIO_H3600_OPT_LOCK		GPIO_GPIO (22)
#define GPIO_H3600_OPT_DET		GPIO_GPIO (27)

/****************************************************/

#define GPIO_NR_H3600_ACTION_BUTTON	(18)

#define IRQ_GPIO_H3600_MICROCONTROLLER  IRQ_GPIO1
#define IRQ_GPIO_H3600_ACTION_BUTTON    IRQ_GPIO18
#define IRQ_GPIO_H3600_OPT_DET		IRQ_GPIO27

/* H3100 / 3600 EGPIO pins */
#define EGPIO_H3600_VPP_ON		(1 << 0)
#define EGPIO_H3600_CARD_RESET		(1 << 1)   /* reset the attached pcmcia/compactflash card.  active high. */
#define EGPIO_H3600_OPT_RESET		(1 << 2)   /* reset the attached option pack.  active high. */
#define EGPIO_H3600_CODEC_NRESET	(1 << 3)   /* reset the onboard UDA1341.  active low. */
#define EGPIO_H3600_OPT_NVRAM_ON	(1 << 4)   /* apply power to optionpack nvram, active high. */
#define EGPIO_H3600_OPT_ON		(1 << 5)   /* full power to option pack.  active high. */
#define EGPIO_H3600_LCD_ON		(1 << 6)   /* enable 3.3V to LCD.  active high. */
#define EGPIO_H3600_RS232_ON		(1 << 7)   /* UART3 transceiver force on.  Active high. */

/* H3600 only EGPIO pins */
#define EGPIO_H3600_LCD_PCI		(1 << 8)   /* LCD control IC enable.  active high. */
#define EGPIO_H3600_IR_ON		(1 << 9)   /* apply power to IR module.  active high. */
#define EGPIO_H3600_AUD_AMP_ON		(1 << 10)  /* apply power to audio power amp.  active high. */
#define EGPIO_H3600_AUD_PWR_ON		(1 << 11)  /* apply power to reset of audio circuit.  active high. */
#define EGPIO_H3600_QMUTE		(1 << 12)  /* mute control for onboard UDA1341.  active high. */
#define EGPIO_H3600_IR_FSEL		(1 << 13)  /* IR speed select: 1->fast, 0->slow */
#define EGPIO_H3600_LCD_5V_ON		(1 << 14)  /* enable 5V to LCD. active high. */
#define EGPIO_H3600_LVDD_ON		(1 << 15)  /* enable 9V and -6.5V to LCD. */

#endif /* _INCLUDE_H3600_H_ */
