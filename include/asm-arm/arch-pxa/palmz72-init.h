/*
 * palmz72-init.h
 * 
 * Init values for PalmOne Zire 72 Handheld Computer
 *
 * Author: Jan Herman <2hp@seznam.cz>
 * 	    
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *    
 */

#ifndef _INCLUDE_PALMZ72_INIT_H_ 

#define _INCLUDE_PALMZ72_INIT_H_

// BACKLIGHT

#define PALMZ72_MAX_INTENSITY      	0xFE
#define PALMZ72_DEFAULT_INTENSITY  	0x7E
#define PALMZ72_LIMIT_MASK	  	0x7F
#define PALMZ72_PRESCALER		0x3F
#define PALMZ72_PERIOD			0x112


// BATTERY 

#define PALMZ72_BAT_MAX_VOLTAGE 	4100	// 4.09V current voltage at max charge as from PalmOS
#define PALMZ72_BAT_MIN_VOLTAGE  	3670	// 3.67V critical voltage as from Zlauncher
#define PALMZ72_BAT_MAX_CURRENT     	0	// unknokn
#define PALMZ72_BAT_MIN_CURRENT     	0	// unknown
#define PALMZ72_BAT_MAX_CHARGE      	1	// unknown
#define PALMZ72_BAT_MIN_CHARGE      	1	// unknown
#define PALMZ72_BAT_MEASURE_DELAY (HZ * 1)
#define PALMZ72_MAX_LIFE_MINS		240     // my Zire72 on-life in minutes


#endif
