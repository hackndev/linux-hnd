/*
 * palmte2-init.h
 * 
 * Init values for PalmOne Tungsten E2 Handheld Computer
 *
 * Copyright (C) 2007 Marek Vasut <marek.vasut@gmail.com>
 * 	    
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *    
 */

#ifndef _INCLUDE_PALMTE2_INIT_H_

#define _INCLUDE_PALMTE2_INIT_H_

/* BACKLIGHT */

#define PALMTE2_MAX_INTENSITY      	0xFE
#define PALMTE2_DEFAULT_INTENSITY  	0x7E
#define PALMTE2_LIMIT_MASK	  	0x7F
#define PALMTE2_PRESCALER		0x3F
#define PALMTE2_PERIOD			0x112

#endif /* _INCLUDE_PALMTE2_INIT_H_ */
