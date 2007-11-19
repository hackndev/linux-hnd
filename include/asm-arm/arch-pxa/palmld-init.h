/*
 * palmld-init.h
 * 
 * Init values for PalmOne LifeDrive Handheld Computer
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

#ifndef _INCLUDE_PALMLD_INIT_H_ 

#define _INCLUDE_PALMLD_INIT_H_

/* BACKLIGHT */

#define PALMLD_MAX_INTENSITY		0xFE
#define PALMLD_DEFAULT_INTENSITY	0x7E
#define PALMLD_LIMIT_MASK		0x7F
#define PALMLD_PRESCALER		0x3F
#define PALMLD_PERIOD			0x12C

/* BATTERY */

#define PALMLD_BAT_MAX_VOLTAGE		4000	/* 4.00V current voltage at max charge as from PalmOS */
#define PALMLD_BAT_MIN_VOLTAGE		3550	/* 3.55V critical voltage */
#define PALMLD_BAT_MAX_CURRENT		0	/* unknokn */
#define PALMLD_BAT_MIN_CURRENT		0	/* unknown */
#define PALMLD_BAT_MAX_CHARGE		1	/* unknown */
#define PALMLD_BAT_MIN_CHARGE		1	/* unknown */
#define PALMLD_BAT_MEASURE_DELAY (HZ * 1)
#define PALMLD_MAX_LIFE_MINS		240	/* my LifeDrive on-life in minutes */


#endif
