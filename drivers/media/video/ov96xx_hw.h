/*
 *  ov96xx_hw.h - Omnivision 9640, 9650 CMOS sensor driver
 *
 *  Copyright (C) 2003, Intel Corporation
 *  Copyright (C) 2004 Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Merge of OV9640,OV9650 (C) 2007 Philipp Zabel
 *
 */

#ifndef __OV_96xx_HW_H__
#define __OV_96xx_HW_H__

/* Revision constants */
#define PID_OV96xx		0x96
#define REV_OV9640		0x48
#define REV_OV9640_v3		0x49
#define REV_OV9650_0		0x51
#define REV_OV9650		0x52

/* Return codes */
#define OV_ERR_NONE       	0x00
#define OV_ERR_TIMEOUT    	-1
#define OV_ERR_PARAMETER  	-2
#define OV_COMM_ERR		-3

/* Output Size & Format */
/*
#define OV_SIZE_NONE		0
#define OV_SIZE_QQVGA		0x01
#define OV_SIZE_QVGA		( OV_SIZE_QQVGA << 1 )
#define OV_SIZE_VGA		( OV_SIZE_QQVGA << 2 )
#define OV_SIZE_SXGA		( OV_SIZE_QQVGA << 3 )
#define OV_SIZE_QQCIF		0x10
#define OV_SIZE_QCIF		( OV_SIZE_QQCIF << 1 )
#define OV_SIZE_CIF		( OV_SIZE_QQCIF << 2 )
#define OV_FORMAT_NONE		0
#define OV_FORMAT_YUV_422	1
#define OV_FORMAT_RGB_565	2
*/
enum OV_SIZE {
	OV_SIZE_NONE=0	,
	OV_SIZE_QQVGA	,
	OV_SIZE_QVGA	,
	OV_SIZE_VGA	,
	OV_SIZE_SXGA	,
	OV_SIZE_QQCIF	,
	OV_SIZE_QCIF	,
	OV_SIZE_CIF
};
enum OV_FORMAT {
	OV_FORMAT_NONE=0 ,
	OV_FORMAT_YUV_422,
	OV_FORMAT_RGB_565,
};

/* Camera Mode */
#define VIEWFINDER_MODE     0x10
#define STILLFRAME_MODE     0x20

/* Others */
#define OV96xx_TIMEOUT    1000    // ms to timeout.

/* OV96xx Register Definitions */
#define OV96xx_GAIN		0x00
#define OV96xx_BLUE		0x01
#define OV96xx_RED		0x02
#define OV96xx_VREF		0x03
#define OV96xx_COM1		0x04
#define OV96xx_BAVE		0x05				// U/B Average Level
#define OV96xx_GEAVE		0x06				// Y/Ge Average Level
#define OV96xx_GOAVE		0x07				// Y/Go Average Level
#define OV96xx_RAVE		0x08				// V/R Average level
#define OV96xx_COM2		0x09				// Common control 2
#define OV96xx_PID		0x0A				// Product ID
#define OV96xx_REV		0x0B				// Revision
#define OV96xx_COM3		0x0C
#define OV96xx_COM4		0x0D
#define OV96xx_COM5		0x0E
#define OV96xx_COM6		0x0F
#define OV96xx_AECH		0x10
#define OV96xx_CLKRC		0x11
#define OV96xx_COM7		0x12
#define OV96xx_COM8		0x13
#define OV96xx_COM9		0x14
#define OV96xx_COM10		0x15
#define OV96xx_WS		0x16
#define OV96xx_HSTART		0x17
#define OV96xx_HSTOP		0x18
#define OV96xx_VSTRT		0x19
#define OV96xx_VSTOP		0x1A
#define OV96xx_PSHFT		0x1B
#define OV96xx_MIDH		0x1C
#define OV96xx_MIDL		0x1D
#define OV9640_DLY		0x1E
#define OV9650_MVLP		0x1E
#define OV96xx_LAEC		0x1F
#define OV96xx_BOS		0x20
#define OV96xx_GBOS		0x21
#define OV96xx_GROS		0x22
#define OV96xx_ROS		0x23
#define OV96xx_AEW		0x24
#define OV96xx_AEB		0x25
#define OV96xx_VPT		0x26
#define OV96xx_BBIAS		0x27
#define OV96xx_GbBIAS		0x28
#define OV9640_GrBIAS		0x29
#define OV9650_GrCOM		0x29
#define OV96xx_EXHCH		0x2A
#define OV96xx_EXHCL		0x2B
#define OV96xx_RBIAS		0x2C
#define OV96xx_ADVFL		0x2D
#define OV96xx_ADVFH		0x2E
#define OV96xx_YAVE		0x2F
#define OV96xx_HSYST		0x30
#define OV96xx_HSYEN		0x31
#define OV96xx_HREF		0x32
#define OV96xx_CHLF		0x33
#define OV96xx_ARBLM		0x34
#define OV96xx_VRHL		0x35
#define OV96xx_VIDO		0x36
#define OV96xx_ADC		0x37
#define OV96xx_ACOM		0x38
#define OV96xx_OFON		0x39
#define OV96xx_TSLB		0x3A
#define OV96xx_COM11		0x3B
#define OV96xx_COM12		0x3C
#define OV96xx_COM13		0x3D
#define OV96xx_COM14		0x3E
#define OV96xx_EDGE		0x3F
#define OV96xx_COM15		0x40
#define OV96xx_COM16		0x41
#define OV96xx_COM17		0x42
#define OV96xx_AWBTH1		0x43
#define OV96xx_AWBTH2		0x44
#define OV96xx_AWBTH3		0x45
#define OV96xx_AWBTH4		0x46
#define OV96xx_AWBTH5		0x47
#define OV96xx_AWBTH6		0x48
#define OV96xx_MTX1		0x4F
#define OV96xx_MTX2		0x50
#define OV96xx_MTX3		0x51
#define OV96xx_MTX4		0x52
#define OV96xx_MTX5		0x53
#define OV96xx_MTX6		0x54
#define OV96xx_MTX7		0x55
#define OV96xx_MTX8		0x56
#define OV96xx_MTX9		0x57
#define OV96xx_MTXS		0x58
#define OV96xx_AWBC1		0x59
#define OV96xx_AWBC2		0x5A
#define OV96xx_AWBC3		0x5B
#define OV96xx_AWBC4		0x5C
#define OV96xx_AWBC5		0x5D
#define OV96xx_AWBC6		0x5E
#define OV96xx_AWBC7		0x5F
#define OV96xx_AWBC8		0x60
#define OV96xx_AWBC9		0x61
#define OV96xx_LCC1		0x62
#define OV96xx_LCC2		0x63
#define OV96xx_LCC3		0x64
#define OV96xx_LCC4		0x65
#define OV96xx_LCC5		0x66
#define OV96xx_MANU		0x67
#define OV96xx_MANV		0x68
#define OV96xx_HV		0x69
#define OV96xx_MBD		0x6A
#define OV96xx_DBLV		0x6B
#define OV96xx_GSP0		0x6C
#define OV96xx_GSP1		0x6D
#define OV96xx_GSP2		0x6E
#define OV96xx_GSP3		0x6F
#define OV96xx_GSP4		0x70
#define OV96xx_GSP5		0x71
#define OV96xx_GSP6		0x72
#define OV96xx_GSP7		0x73
#define OV96xx_GSP8		0x74
#define OV96xx_GSP9		0x75
#define OV96xx_GSP10		0x76
#define OV96xx_GSP11		0x77
#define OV96xx_GSP12		0x78
#define OV96xx_GSP13		0x79
#define OV96xx_GSP14		0x7A
#define OV96xx_GSP15		0x7B
#define OV96xx_GST0		0x7C
#define OV96xx_GST1		0x7D
#define OV96xx_GST2		0x7E
#define OV96xx_GST3		0x7F
#define OV96xx_GST4		0x80
#define OV96xx_GST5		0x81
#define OV96xx_GST6		0x82
#define OV96xx_GST7		0x83
#define OV96xx_GST8		0x84
#define OV96xx_GST9		0x85
#define OV96xx_GST10		0x86
#define OV96xx_GST11		0x87
#define OV96xx_GST12		0x88
#define OV96xx_GST13		0x89
#define OV96xx_GST14		0x8A

/* End of OV9640 register */
#define OV9640_REGEND		( OV96xx_GST14 + 1 )

/* OV9650 Register Definitions */
#define OV9650_COM21		0x8B
#define OV9650_COM22		0x8C
#define OV9650_COM23		0x8D
#define OV9650_COM24		0x8E
#define OV9650_DBLC1		0x8F
#define OV9650_DBLCB		0x90
#define OV9650_DBLCR		0x91
#define OV9650_DMLNL		0x92
#define OV9650_DMLNH		0x93

#define OV9650_AECHM		0xA1

/* End of OV9650 register */
#define OV9650_LASTREG		0xAA

/* End flag of register */
#define OV9650_REGEND		( 0xff )

#endif
