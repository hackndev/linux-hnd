/* 
    pxa_camera - PXA camera driver header file

    Copyright (C) 2003, Intel Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __LINUX_PXA_CAMERA_H_
#define __LINUX_PXA_CAMERA_H_

#define WCAM_VIDIOCSCAMREG  	_IOW('v', 211, int)
#define WCAM_VIDIOCGCAMREG   	_IOR('v', 212, int)
#define WCAM_VIDIOCSCIREG    	_IOW('v', 213, int)
#define WCAM_VIDIOCGCIREG    	_IOR('v', 214, int)
#define WCAM_VIDIOCSINFOR	_IOW('v', 215, int)
#define WCAM_VIDIOCGINFOR	_IOR('v', 216, int)

/*
Image format definition
*/
#define CAMERA_IMAGE_FORMAT_RAW8                0
#define CAMERA_IMAGE_FORMAT_RAW9                1
#define CAMERA_IMAGE_FORMAT_RAW10               2
                                                                                                                             
#define CAMERA_IMAGE_FORMAT_RGB444              3
#define CAMERA_IMAGE_FORMAT_RGB555              4
#define CAMERA_IMAGE_FORMAT_RGB565              5
#define CAMERA_IMAGE_FORMAT_RGB666_PACKED       6
#define CAMERA_IMAGE_FORMAT_RGB666_PLANAR       7
#define CAMERA_IMAGE_FORMAT_RGB888_PACKED       8
#define CAMERA_IMAGE_FORMAT_RGB888_PLANAR       9
#define CAMERA_IMAGE_FORMAT_RGBT555_0          10  //RGB+Transparent bit 0
#define CAMERA_IMAGE_FORMAT_RGBT888_0          11
#define CAMERA_IMAGE_FORMAT_RGBT555_1          12  //RGB+Transparent bit 1
#define CAMERA_IMAGE_FORMAT_RGBT888_1          13
                                                                                                                             
#define CAMERA_IMAGE_FORMAT_YCBCR400           14
#define CAMERA_IMAGE_FORMAT_YCBCR422_PACKED    15
#define CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR    16
#define CAMERA_IMAGE_FORMAT_YCBCR444_PACKED    17
#define CAMERA_IMAGE_FORMAT_YCBCR444_PLANAR    18

/*
Bpp definition
*/

#define YUV422_BPP				16
#define RGB565_BPP				16
#define RGB666_UNPACKED_BPP			32
#define RGB666_PACKED_BPP			24

/*
VIDIOCCAPTURE Arguments
*/
#define STILL_IMAGE				1
#define VIDEO_START				0
#define VIDEO_STOP				-1


#endif /* __LINUX_PXA_CAMERA_H_ */

