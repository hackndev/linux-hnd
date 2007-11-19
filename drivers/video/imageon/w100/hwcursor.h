/*
 * linux/drivers/video/imageon/w100_hwcursor.h
 *
 * Frame Buffer Device for ATI Imageon 100
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
 *  ATI's w100fb.h Wallaby code
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */


#ifndef _w100_hwcursor_h_
#define _w100_hwcursor_h_

/* We did define all registers before, however this is simpler: */
#define CURSOR1_BASE			0x0460
#define CURSOR2_BASE			0x0474

#define CURSOR_OFFSET			0x0000
#define CURSOR_H_POS			0x0004
#define CURSOR_V_POS			0x0008
#define CURSOR_COLOR0			0x000C
#define CURSOR_COLOR1			0x0010


typedef union {
	unsigned long val			: 32;
	struct _cursor_offset_t {
		unsigned long offset		: 24;
		unsigned long x_offset		: 4;
		unsigned long y_offset		: 4;
	} f;
} cursor_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _cursor_h_pos_t {
		unsigned long start		: 10;
		unsigned long			: 6;
		unsigned long end		: 10;
		unsigned long			: 5;
		unsigned long en		: 1;
	} f;
} cursor_h_pos_u;


typedef union {
	unsigned long val			: 32;
	struct _cursor_v_pos_t {
		unsigned long start		: 10;
		unsigned long			: 6;
		unsigned long end		: 10;
		unsigned long			: 6;
	} f;
} cursor_v_pos_u;


typedef union {
	unsigned long val			: 32;
	struct _cursor_color_t {
		unsigned long r			: 8;
		unsigned long g			: 8;
		unsigned long b			: 8;
		unsigned long			: 8;
	} f;
} cursor_color_u;


#endif /* _w100_hwcursor_h_ */
