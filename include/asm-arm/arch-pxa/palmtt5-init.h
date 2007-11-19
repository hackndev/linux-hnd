/*
 * Init values for Palm Tungsten|T5 Handheld Computer
 *
 * Authors: Marek Vasut <marek.vasut@gmail.com>
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

#ifndef _INCLUDE_PALMTT5_INIT_H_ 

#define _INCLUDE_PALMTT5_INIT_H_

/* Various addresses  */

#define PALMTT5_PHYS_RAM_START	0xa0000000
#define PALMTT5_PHYS_IO_START	0x40000000

/* KEYPAD configuration */

#define KPASMKP(col)		(col/2==0 ? KPASMKP0 : KPASMKP1)
#define KPASMKPx_MKC(row, col)	(1 << (row + 16*(col%2)))

#define PALMTT5_KM_ROWS		4
#define PALMTT5_KM_COLS		3


/* TOUCHSCREEN */

#define AC97_LINK_FRAME		21


/* BATTERY */

#define PALMTT5_BAT_MAX_VOLTAGE		4000	/* 4.00v current voltage at max charge as from Filez */
#define PALMTT5_BAT_MIN_VOLTAGE		3550	/* 3.55v critical voltage as from FileZ */
#define PALMTT5_BAT_MAX_CURRENT		   0	/* unknokn */
#define PALMTT5_BAT_MIN_CURRENT		   0	/* unknown */
#define PALMTT5_BAT_MAX_CHARGE		   1	/* unknown */
#define PALMTT5_BAT_MIN_CHARGE		   1	/* unknown */
#define PALMTT5_MAX_LIFE_MINS		 360    /* on-life in minutes */

#define PALMTT5_BAT_MEASURE_DELAY (HZ * 1)

/* BACKLIGHT */

#define PALMTT5_MAX_INTENSITY            0xFE
#define PALMTT5_DEFAULT_INTENSITY        0x7E
#define PALMTT5_LIMIT_MASK               0x7F
#define PALMTT5_PRESCALER                0x3F
#define PALMTT5_PERIOD                   0x12C

#endif
