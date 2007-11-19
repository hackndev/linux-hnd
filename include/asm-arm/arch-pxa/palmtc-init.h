/*
 * palmtc-init.h
 * 
 * Init values for Palm Tungsten|C Handheld Computer
 *
 * Author: Marek Vasut <marek.vasut@gmail.com>
 * 	    
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *    
 */

#ifndef _INCLUDE_PALMTC_INIT_H_ 

#define _INCLUDE_PALMTC_INIT_H_

/* BACKLIGHT */

#define PALMTC_MAX_INTENSITY		0xFE
#define PALMTC_DEFAULT_INTENSITY	0x7E
#define PALMTC_LIMIT_MASK		0x7F
#define PALMTC_PRESCALER		0x3F
#define PALMTC_PERIOD			0x12C

/* BATTERY */

#define PALMTC_BAT_MAX_VOLTAGE		4000	/* 4.00V current voltage at max charge as from PalmOS */
#define PALMTC_BAT_MIN_VOLTAGE		3550	/* 3.55V critical voltage */
#define PALMTC_BAT_MAX_CURRENT		0	/* unknokn */
#define PALMTC_BAT_MIN_CURRENT		0	/* unknown */
#define PALMTC_BAT_MAX_CHARGE		1	/* unknown */
#define PALMTC_BAT_MIN_CHARGE		1	/* unknown */
#define PALMTC_BAT_MEASURE_DELAY (HZ * 1)
#define PALMTC_MAX_LIFE_MINS		240	/* my Tungsten|C on-life in minutes */


#endif
