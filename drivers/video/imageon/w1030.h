/*
 * linux/drivers/video/imageon/w1030/w1030.h
 *
 * Frame Buffer Device for ATI Imageon 1030
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

#ifndef _imageon_w1030_h_
#define _imageon_w1030_h_

int __init w1030fb_init(void);
void __exit w1030fb_cleanup(void);

#endif /* _imageon_w1030_h_ */
