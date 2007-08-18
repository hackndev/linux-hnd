/*
 * Init values for Palm TX Handheld Computer
 *
 * Authors: Cristiano P. <cristianop AT users DOT sourceforge DOT net>
 * 	    Jan Herman <2hp@seznam.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * this code is in a very early stage:
 * - use it at your own risk
 * - any help is encouraged and will be highly appreciated
 *    
 */

#ifndef _INCLUDE_PALMTX_INIT_H_ 

#define _INCLUDE_PALMTX_INIT_H_

/* Various addresses */

#define PALMTX_PHYS_FLASH_START	0x00000000 /* ChipSelect 0 */
#define PALMTX_PHYS_NAND_START	0x04000000 /* ChipSelect 1 */

#define PALMTX_PHYS_RAM_START	0xa0000000
#define PALMTX_PHYS_IO_START	0x40000000

/* LCD REGISTERS */
#define PALMTX_INIT_LCD_LLC0	LCCR0_ENB | LCCR0_Color | LCCR0_Sngl | LCCR0_LDM | \
				LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_Act | \
				LCCR0_4PixMono | LCCR0_QDM | LCCR0_BM | LCCR0_OUM | \
				LCCR0_RDSTM | LCCR0_CMDIM | LCCR0_OUC | LCCR0_LDDALT

#define PALMTX_INIT_LCD_LLC3	LCCR3_PixClkDiv(2) | LCCR3_HorSnchL | LCCR3_VrtSnchL | \
				LCCR3_PixFlEdg | LCCR3_OutEnH | LCCR3_Bpp(4)


/* KEYPAD configuration */

#define KPASMKP(col)            (col/2==0 ? KPASMKP0 : KPASMKP1)
#define KPASMKPx_MKC(row, col)  (1 << (row + 16*(col%2)))

#define PALMTX_KM_ROWS	4
#define PALMTX_KM_COLS	3


/* TOUCHSCREEN */

#define AC97_LINK_FRAME	21


/* BATTERY */

#define PALMTX_BAT_MAX_VOLTAGE		4000	/* 4.00v current voltage at max charge as from ZLauncher */
#define PALMTX_BAT_MIN_VOLTAGE		3500	/* 3.60v critical voltage as from FileZ */
#define PALMTX_BAT_MAX_CURRENT		   0	/* unknokn */
#define PALMTX_BAT_MIN_CURRENT		   0	/* unknown */
#define PALMTX_BAT_MAX_CHARGE		   1	/* unknown */
#define PALMTX_BAT_MIN_CHARGE		   1	/* unknown */
#define PALMTX_MAX_LIFE_MINS		 360    /* on-life in minutes */

#define PALMTX_BAT_MEASURE_DELAY (HZ * 1)

/* BACKLIGHT */

#define PALMTX_MAX_INTENSITY		0xFE
#define PALMTX_DEFAULT_INTENSITY	0x7E
#define PALMTX_LIMIT_MASK		0x7F
#define PALMTX_PRESCALER		0x3F
#define PALMTX_PERIOD			0x12C

#endif
