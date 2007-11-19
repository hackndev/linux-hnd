 /*
 * Driver for the HP iPAQ Mercury Backpaq camera
 * Video4Linux interface
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
 * Author: Andrew Christian
 *         <andyc@handhelds.org>
 *         4 May 2001
 * Changed: Wim de Haan
 *          Added items for i2c control of Philips imagers
 *          02/25/02
 *
 * Driver for Mercury BackPAQ camera
 *
 * Issues to be addressed:
 *    1. Writing to the FPGA when we need to do a functionality change
 *    2. Sampling the pixels correctly and building a pixel array
 *    3. Handling different pixel formats correctly
 *    4. Changing the contrast, brightness, white balance, and so forth.
 *    5. Specifying a subregion (i.e., setting "top, left" and SUBCAPTURE)
 */

#ifndef _H3600_BACKPAQ_CAMERA_H
#define _H3600_BACKPAQ_CAMERA_H

#include <linux/videodev.h>

/* SMaL camera defaults */
#define H3600_BACKPAQ_CAMERA_DEFAULT_CLOCK_DIVISOR   0x100  /* 5 fps */
#define H3600_BACKPAQ_CAMERA_DEFAULT_INTERRUPT_FIFO  0x20   /* 32 deep */

#define H3600_BACKPAQ_CAMERA_DEFAULT_SPECIAL_MODES   3 /* 0x13 */
#define H3600_BACKPAQ_CAMERA_DEFAULT_AUTOBRIGHT      4 /* 0x64 */
#define H3600_BACKPAQ_CAMERA_DEFAULT_GAIN_FORMAT     0 /* 0x70 -  8-bit, no gain */
#define H3600_BACKPAQ_CAMERA_DEFAULT_POWER_SETTING   4 /* 0x84 */
#define H3600_BACKPAQ_CAMERA_DEFAULT_POWER_MGMT    0xf /* 0x9f */

/* Philips camera defaults */
#define H3600_BACKPAQ_CAMERA_PHILIPS_CLOCK_DIVISOR      3     /* 15 fps */
#define H3600_BACKPAQ_CAMERA_PHILIPS_INTERRUPT_FIFO     0x20  /* 32 deep */
#define H3600_BACKPAQ_CAMERA_PHILIPS_ELECTRONIC_SHUTTER 255  
#define H3600_BACKPAQ_CAMERA_PHILIPS_SUBROW             0
#define H3600_BACKPAQ_CAMERA_PHILIPS_PROG_GAIN_AMP      128
#define H3600_BACKPAQ_CAMERA_PHILIPS_X1			24
#define H3600_BACKPAQ_CAMERA_PHILIPS_Y1			14
#define H3600_BACKPAQ_CAMERA_PHILIPS_WIDTH		644  
#define H3600_BACKPAQ_CAMERA_PHILIPS_HEIGHT		484
#define H3600_BACKPAQ_CAMERA_PHILIPS_BLACK_OFFSET	0x14
#define H3600_BACKPAQ_CAMERA_PHILIPS_GOOB		0x20
#define H3600_BACKPAQ_CAMERA_PHILIPS_GEOB		0x20
#define H3600_BACKPAQ_CAMERA_PHILIPS_GOEB		0x20
#define H3600_BACKPAQ_CAMERA_PHILIPS_GEEB		0x20



/* Philips camera register meanings */
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_REFEQ		(1<<6)
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_EQGAIN		(1<<5)
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_HANDV		(1<<4)  
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_MULTISEN	(1<<3)
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_RST		(1<<2)
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_PSAVE		(1<<1)
#define H3600_BACKPAQ_CAMERA_PHILIPS_CR1_PDOWN		(1<<0)  




struct h3600_backpaq_camera_params {
        /* FPGA settings */
        /*unsigned short integration_time;  */ /* Mapped to "brightness" */
	unsigned short clock_divisor;      /* 0x100 = 5 fps */
	unsigned short interrupt_fifo;

    

    
        /* Imager settings */
	unsigned char  power_setting;      /* Normally "4" */
	unsigned char  gain_format;        /* 0x8 = 12-bit mode [not allowed]
					      0x0 = 8-bit mode  (normal)
					      0x1 = 8-bit gain of 2
					      0x2 = 8-bit gain of 4
					      0x3 = 8-bit gain of 8
					      0x4 = 8-bit gain of 16 */
	unsigned char  power_mgmt;         /* See docs : uses bits 0 through 3 */
	unsigned char  special_modes;      /* See docs : uses bits 0 through 2 */
	unsigned char  autobright;         /* See docs */

        /* Driver image processing settings */
	unsigned char  read_polling_mode;  /* Force "read" to use polling mode */
	unsigned char  flip;               /* Set to TRUE to invert image */
};

struct h3600_backpaq_camera_philips {
	unsigned short clock_divisor;      /* 9 = 5 fps */
	unsigned short interrupt_fifo;
	unsigned char  read_polling_mode;  /* Force "read" to use polling mode */
	unsigned char  flip;               /* Set to TRUE to invert image */

	unsigned short electronic_shutter;
	unsigned char  subrow;
};


struct h3600_backpaq_camera_philips_winsize {
    unsigned short x1;      /* 1st row */
    unsigned short y1;      /* 1st column */
    unsigned short  width;  
    unsigned short height;  
};

struct h3600_backpaq_camera_philips_gains {
    unsigned short  goob;
    unsigned short  geob;
    unsigned short  goeb;  
    unsigned short  geeb;  
};


enum h3600_camera_type {
	H3600_SMAL,
	H3600_PHILIPS
};

struct h3600_backpaq_camera_type {
	unsigned char  model;              /* See "backpaq.h" for a list of the camera types */
	unsigned char  orientation;        /* 0 = portrait, 1 = landscape */
	enum h3600_camera_type type;       /* General type */
};

/*
   Private IOCTL to control camera parameters and image flipping
 */

#define H3600CAM_G_TYPE         _IOR ('v', BASE_VIDIOCPRIVATE+6, struct h3600_backpaq_camera_type)
#define H3600CAM_RESET		_IO  ('v', BASE_VIDIOCPRIVATE+2 )


/* Ioctl's specific to the SMaL imager */
#define H3600CAM_G_PARAMS	_IOR ('v', BASE_VIDIOCPRIVATE+0, struct h3600_backpaq_camera_params)
#define H3600CAM_S_PARAMS	_IOWR('v', BASE_VIDIOCPRIVATE+1, struct h3600_backpaq_camera_params)

/* Ioctl's specific to the Philips imager */
#define H3600CAM_PH_G_PARAMS    _IOR ('v', BASE_VIDIOCPRIVATE+7, struct h3600_backpaq_camera_philips)
#define H3600CAM_PH_S_PARAMS    _IOW ('v', BASE_VIDIOCPRIVATE+8, struct h3600_backpaq_camera_philips)
#define H3600CAM_PH_SET_ESHUT	_IOW ('v', BASE_VIDIOCPRIVATE+3, int )
#define H3600CAM_PH_SET_SUBROW	_IOW ('v', BASE_VIDIOCPRIVATE+4, int )
#define H3600CAM_PH_SET_PGA	_IOW ('v', BASE_VIDIOCPRIVATE+5, int )

#define H3600CAM_PH_G_WINSIZE   _IOR ('v', BASE_VIDIOCPRIVATE+11, struct h3600_backpaq_camera_philips_winsize)
#define H3600CAM_PH_S_WINSIZE   _IOW ('v', BASE_VIDIOCPRIVATE+12, struct h3600_backpaq_camera_philips_winsize)

#define H3600CAM_PH_G_GAINS   _IOR ('v', BASE_VIDIOCPRIVATE+13, struct h3600_backpaq_camera_philips_gains)
#define H3600CAM_PH_S_GAINS   _IOW ('v', BASE_VIDIOCPRIVATE+14, struct h3600_backpaq_camera_philips_gains)

#define H3600CAM_PH_G_BLACK_OFFSET   _IOR ('v', BASE_VIDIOCPRIVATE+15, int)
#define H3600CAM_PH_S_BLACK_OFFSET   _IOW ('v', BASE_VIDIOCPRIVATE+16, int)


/* Ioctl's for debugging  these set/get the value of a 32 bit register on the  */
/* FPGA.  this can be very useful for timing if you bring some of those signals */
/* out to a scope :) */
#define H3600CAM_GET_TEST32	_IOR ('v', BASE_VIDIOCPRIVATE+9, int )
#define H3600CAM_SET_TEST32	_IOW ('v', BASE_VIDIOCPRIVATE+10, int )

#endif /*  _H3600_BACKPAQ_CAMERA_H */

