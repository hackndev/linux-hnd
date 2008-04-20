/*
 * palmtt3-init.h
 *
 * Init values for Palm Tungsten|T3 Handheld Computer
 *
 * Based on palmz72-init.h by Jan Herman, other palmXX-init.h and T|T3 files.
 * 
 * Authors: Radek Machacek <keddar@volny.cz>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * - initial release
 * - change lccr0 and lccr3 value from static to LCCRman's
 */

#ifndef _INCLUDE_PALMTT3_INIT_H_

#define _INCLUDE_PALMTT3_INIT_H_

// ADDRESSES

#define PALMTT3_PHYS_IO_START	0x40000000


// BACKLIGHT

#define PALMTT3_MAX_INTENSITY      	0x100
#define PALMTT3_DEFAULT_INTENSITY  	0x50
#define PALMTT3_LIMIT_MASK	  	0x7F
#define PALMTT3_PRESCALER		1
#define PALMTT3_PERIOD			0x12B


// BATTERY

#define PALMTT3_BAT_MAX_VOLTAGE 	4156	// strange, but this is my current upper limit. so it be.
#define PALMTT3_BAT_MIN_VOLTAGE 	3710	// 3.71 V, Critical Threshold set by PalmOS
#define PALMTT3_BAT_MAX_CURRENT 	0	// unknokn
#define PALMTT3_BAT_MIN_CURRENT 	0	// unknown
#define PALMTT3_BAT_MAX_CHARGE 		1	// unknown
#define PALMTT3_BAT_MIN_CHARGE 		1	// unknown

// LCD REGISTERS

/*
// static value
#define PALMTT3_INIT_LCD_LCCR0 0x003008F9
#define PALMTT3_INIT_LCD_LCCR3 0x03700002
*/

/* value converted using LCCRman */
#define PALMTT3_INIT_LCD_LCCR0  LCCR0_ENB | LCCR0_Color | LCCR0_Sngl | LCCR0_LDM \
	| LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_Act | LCCR0_4PixMono | \
	LCCR0_QDM | LCCR0_BM | LCCR0_OUM

#define PALMTT3_INIT_LCD_LCCR3  LCCR3_PixClkDiv(2) | LCCR3_HorSnchL \
	| LCCR3_VrtSnchL | LCCR3_PixFlEdg | LCCR3_OutEnH | LCCR3_Bpp(3)


#endif
