/*
 * linux/drivers/video/imageon/w100_idct.h
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


#ifndef _w100_idct_h_
#define _w100_idct_h_

#define mmIDCT_RUNS		0x0C00
#define mmIDCT_LEVELS		0x0C04
#define mmIDCT_CONTROL		0x0C3C
#define mmIDCT_AUTH_CONTROL	0x0C08
#define mmIDCT_AUTH		0x0C0C

typedef union {
	unsigned long val				: 32;
	struct _idct_runs_t {
		unsigned long idct_runs_3		: 8;
		unsigned long idct_runs_2		: 8;
		unsigned long idct_runs_1		: 8;
		unsigned long idct_runs_0		: 8;
	} f;
} idct_runs_u;

typedef union {
	unsigned long val				: 32;
	struct _idct_levels_t {
		unsigned long idct_level_hi		: 16;
		unsigned long idct_level_lo		: 16;
	} f;
} idct_levels_u;

typedef union {
	unsigned long val				: 32;
	struct _idct_control_t {
		unsigned long idct_ctl_luma_rd_format	: 2;
		unsigned long idct_ctl_chroma_rd_format	: 2;
		unsigned long idct_ctl_scan_pattern	: 1;
		unsigned long idct_ctl_intra		: 1;
		unsigned long idct_ctl_flush		: 1;
		unsigned long idct_ctl_passthru		: 1;
		unsigned long idct_ctl_sw_reset		: 1;
		unsigned long idct_ctl_constreq		: 1;
		unsigned long idct_ctl_scramble		: 1;
		unsigned long idct_ctl_alt_scan		: 1;
		unsigned long				: 20;
	} f;
} idct_control_u;

typedef union {
	unsigned long val				: 32;
	struct _idct_auth_control_t {
		unsigned long control_bits		: 32;
	} f;
} idct_auth_control_u;

typedef union {
	unsigned long val				: 32;
	struct _idct_auth_t {
		unsigned long auth			: 32;
	} f;
} idct_auth_u;


#endif /* _imageon_idct_h_ */
