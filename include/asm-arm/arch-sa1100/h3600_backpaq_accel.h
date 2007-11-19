/*
*
* Driver for the HP iPAQ Mercury Backpaq accelerometer
*
* Copyright 2001 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*                   
*       Acceleration directions are as follows:
*
*             < 0 >          <=== Camera
*     +Y <-  /-----\
*            |     |         <=== iPAQ
*            |     |
*            |     |
*            |     |
*            \-----/
* 
*             | +X
*             V
*
* Author: Andrew Christian
*         <andrew.christian@compaq.com>
 */

#ifndef _H3600_BACKPAQ_ACCEL_H
#define _H3600_BACKPAQ_ACCEL_H

#include <linux/ioctl.h>

/* 
   Return scaled 16 bit values in gravities:
   8 bits of integer, 8 bits of fraction

   This structure is returned for each read of the device
*/

typedef short          s16_scaled;    /* 8 bits of integer, 8 bits of fraction */
typedef unsigned short u16_fraction;    /* 16 bits of fraction */

/* Format of data returned from a read() request */
struct h3600_backpaq_accel_data {
 	s16_scaled x_acceleration;
	s16_scaled y_acceleration;
};

/* Format of data returned from a read() request on accel device with odd minor number (e.g., camaccelxyz) */
struct h3600_backpaq_accel_data_xyz {
 	s16_scaled x_acceleration;
	s16_scaled y_acceleration;
	s16_scaled z_acceleration;
};

/* Raw timing data, returned from an ioctl() */
struct h3600_backpaq_accel_raw_data {
	unsigned short xt1;
	unsigned short xt2;
	unsigned short yt1;
	unsigned short yt2;
};

/* Acceleration parameters, set using the EEPROM interface */
struct h3600_backpaq_accel_params {
	u16_fraction x_offset;   /* 16-bit fraction (i.e., 0x8000 = 0.5) */
	u16_fraction x_scale;
	u16_fraction y_offset;
	u16_fraction y_scale;
};

/* Raw timing data, returned from an ioctl() */
struct h3600_backpaq_accel_raw_data_xyz {
	unsigned short xt1;
	unsigned short xt2;
	unsigned short yt1;
	unsigned short yt2;
	unsigned short zt1;
	unsigned short zt2;
};

/* Acceleration parameters, set using the EEPROM interface */
struct h3600_backpaq_accel_params_xyz {
	u16_fraction x_offset;   /* 16-bit fraction (i.e., 0x8000 = 0.5) */
	u16_fraction x_scale;
	u16_fraction y_offset;
	u16_fraction y_scale;
	u16_fraction z_offset;
	u16_fraction z_scale;
};

/*
  Ioctl interface
 */

#define H3600_ACCEL_MAGIC    0xD0      /* Picked at random - see Documentation/ioctl-number.txt */

#define H3600_ACCEL_G_PARAMS     _IOR( H3600_ACCEL_MAGIC, 0, struct h3600_backpaq_accel_params)
#define H3600_ACCEL_G_RAW        _IOR( H3600_ACCEL_MAGIC, 2, struct h3600_backpaq_accel_raw_data)
#define H3600_ACCEL_G_PARAMS_XYZ _IOR( H3600_ACCEL_MAGIC, 4, struct h3600_backpaq_accel_params_xyz)
#define H3600_ACCEL_G_RAW_XYZ    _IOR( H3600_ACCEL_MAGIC, 6, struct h3600_backpaq_accel_raw_data_xyz)

#endif /*  _H3600_BACKPAQ_ACCEL_H */

