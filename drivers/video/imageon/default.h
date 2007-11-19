/*
 * linux/drivers/video/imageon/default.h
 *
 * Frame Buffer Device for ATI Imageon
 *
 * Copyright (C) 2004, Damian Bentley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on:
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */

/*
 * Default settings for various hardware setups. This should probably be 
 * moved into the arch directories, for now we just use the ARCH_MACH_???? 
 * configuration variables. If you need to support a new hardware setup,
 * this is the place to do it. You MUST define everything as in every
 * section.
 */

#ifndef _imageon_default_h_
#define _imageon_default_h_

#ifdef CONFIG_ARCH_E7XX
	#define W100FB_BASE	0x0c000000
	#define W1030FB_BASE	0x00000000
#else
#error "Imageon Frame Buffer has not been ported to this machine yet"
#endif

#endif /* _imageon_default_h_ */
