/*
 * linux/drivers/video/imageon/w100/w100.h
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
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */

#ifndef _imageon_w100_h_
#define _imageon_w100_h_

#include "w100/accel.h"
#include "w100/cif.h"
#include "w100/display.h"
#include "w100/idct.h"
#include "w100/power.h"
#include "w100/cfg.h"
#include "w100/cp.h"
#include "w100/hwcursor.h"
#include "w100/memory.h"
#include "w100/rbbm.h"


int __init w100fb_init(void);
void __exit w100fb_cleanup(void);

#define W100FB_SIZE	0x01000000

/* Remapping info */
#define CFG_OFFSET	0x000000
#define REGS_OFFSET	0x010000
#define FB_OFFSET	0x100000
#define CFG_BASE	(W100FB_BASE + CFG_OFFSET)
#define REGS_BASE	(W100FB_BASE + REGS_OFFSET)
#define FB_BASE		(W100FB_BASE + FB_OFFSET)
#define CFG_SIZE	0x000010
#define REGS_SIZE	0x002000
#define FB_SIZE		0x15FF00

#endif /* _imageon_w100_h_ */
