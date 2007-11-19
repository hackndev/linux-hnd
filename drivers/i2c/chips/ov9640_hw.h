/* 
    ov9640_hw - Omnivision 9640 CMOS sensor driver 

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

#ifndef _OV_9640_HW_H_
#define _OV_9640_HW_H_

/***********************************************************************
 *
 * Constants & Structures
 *
 ***********************************************************************/
// Revision constants
#define PID_OV			0x96
#define PID_9640		0x48

#define PID_OV_v3		0x96
#define PID_9640_v3		0x49

// Return codes
#define OV_ERR_NONE       	0x00
#define OV_ERR_TIMEOUT    	-1
#define OV_ERR_PARAMETER  	-2  
#define OV_COMM_ERR		-3

#define CIBR0_PHY	(0x50000000 + 0x28)
#define CIBR1_PHY	(0x50000000 + 0x30)
#define CIBR2_PHY	(0x50000000 + 0x38)
#define DPRINTK(fmt,args...)	do { if (DEBUG) printk("in function %s "fmt,__FUNCTION__,##args);} while(0)

// Output Size & Format
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

// Camera Mode
#define VIEWFINDER_MODE     0x10
#define STILLFRAME_MODE     0x20

// Others
#define OV9640_TIMEOUT    1000    // ms to timeout.

// OV9640 Register Definitions
#define OV9640_GAIN		0x0000
#define OV9640_BLUE		0x0001
#define OV9640_RED		0x0002
#define OV9640_VREF		0x0003
#define OV9640_COM1		0x0004
#define OV9640_BAVE		0x0005				// U/B Average Level
#define OV9640_GEAVE		0x0006				// Y/Ge Average Level
#define OV9640_GOAVE		0x0007				// Y/Go Average Level
#define OV9640_RAVE		0x0008				// V/R Average level
#define OV9640_COM2		0x0009				// Common control 2
#define OV9640_PID		0x000A				// Product ID
#define OV9640_VER		0x000B				// Version
#define OV9640_COM3		0x000C
#define OV9640_COM4		0x000D
#define OV9640_COM5		0x000E
#define OV9640_COM6		0x000F
#define OV9640_AECH		0x0010
#define OV9640_CLKRC		0x0011
#define OV9640_COM7		0x0012
#define OV9640_COM8		0x0013
#define OV9640_COM9		0x0014
#define OV9640_COM10		0x0015
#define OV9640_WS		0x0016
#define OV9640_HSTART		0x0017
#define OV9640_HSTOP		0x0018
#define OV9640_VSTRT		0x0019
#define OV9640_VSTOP		0x001A
#define OV9640_PSHFT		0x001B
#define OV9640_MIDH		0x001C
#define OV9640_MIDL		0x001D
#define OV9640_DLY		0x001E
#define OV9640_LAEC		0x001F
#define OV9640_BOS		0x0020
#define OV9640_GBOS		0x0021
#define OV9640_GROS		0x0022
#define OV9640_ROS		0x0023
#define OV9640_AEW		0x0024
#define OV9640_AEB		0x0025
#define OV9640_VPT		0x0026
#define OV9640_BBIAS		0x0027
#define OV9640_GbBIAS		0x0028
#define OV9640_GrBIAS		0x0029
#define OV9640_EXHCH		0x002A
#define OV9640_EXHCL		0x002B
#define OV9640_RBIAS		0x002C
#define OV9640_ADVFL		0x002D
#define OV9640_ADVFH		0x002E
#define OV9640_YAVE		0x002F
#define OV9640_HSYST		0x0030
#define OV9640_HSYEN		0x0031
#define OV9640_HREF		0x0032
#define OV9640_CHLF		0x0033
#define OV9640_ARBLM		0x0034
#define OV9640_VRHL		0x0035
#define OV9640_VIDO		0x0036
#define OV9640_ADC		0x0037
#define OV9640_ACOM		0x0038
#define OV9640_OFON		0x0039
#define OV9640_TSLB		0x003A
#define OV9640_COM11		0x003B
#define OV9640_COM12		0x003C
#define OV9640_COM13		0x003D
#define OV9640_COM14		0x003E
#define OV9640_EDGE		0x003F
#define OV9640_COM15		0x0040
#define OV9640_COM16		0x0041
#define OV9640_COM17		0x0042
#define OV9640_AWBTH1		0x0043
#define OV9640_AWBTH2		0x0044
#define OV9640_AWBTH3		0x0045
#define OV9640_AWBTH4		0x0046
#define OV9640_AWBTH5		0x0047
#define OV9640_AWBTH6		0x0048
#define OV9640_MTX1		0x004F
#define OV9640_MTX2		0x0050
#define OV9640_MTX3		0x0051
#define OV9640_MTX4		0x0052
#define OV9640_MTX5		0x0053
#define OV9640_MTX6		0x0054
#define OV9640_MTX7		0x0055
#define OV9640_MTX8		0x0056
#define OV9640_MTX9		0x0057
#define OV9640_MTXS		0x0058
#define OV9640_AWBC1		0x0059
#define OV9640_AWBC2		0x005A
#define OV9640_AWBC3		0x005B
#define OV9640_AWBC4		0x005C
#define OV9640_AWBC5		0x005D
#define OV9640_AWBC6		0x005E
#define OV9640_AWBC7		0x005F
#define OV9640_AWBC8		0x0060
#define OV9640_AWBC9		0x0061
#define OV9640_LCC1		0x0062
#define OV9640_LCC2		0x0063
#define OV9640_LCC3		0x0064
#define OV9640_LCC4		0x0065
#define OV9640_LCC5		0x0066
#define OV9640_MANU		0x0067
#define OV9640_MANV		0x0068
#define OV9640_HV		0x0069
#define OV9640_MBD		0x006A
#define OV9640_DBLV		0x006B
#define OV9640_GSP0		0x006C
#define OV9640_GSP1		0x006D
#define OV9640_GSP2		0x006E
#define OV9640_GSP3		0x006F
#define OV9640_GSP4		0x0070
#define OV9640_GSP5		0x0071
#define OV9640_GSP6		0x0072
#define OV9640_GSP7		0x0073
#define OV9640_GSP8		0x0074
#define OV9640_GSP9		0x0075
#define OV9640_GSP10		0x0076
#define OV9640_GSP11		0x0077
#define OV9640_GSP12		0x0078
#define OV9640_GSP13		0x0079
#define OV9640_GSP14		0x007A
#define OV9640_GSP15		0x007B
#define OV9640_GST0		0x007C
#define OV9640_GST1		0x007D
#define OV9640_GST2		0x007E
#define OV9640_GST3		0x007F
#define OV9640_GST4		0x0080
#define OV9640_GST5		0x0081
#define OV9640_GST6		0x0082
#define OV9640_GST7		0x0083
#define OV9640_GST8		0x0084
#define OV9640_GST9		0x0085
#define OV9640_GST10		0x0086
#define OV9640_GST11		0x0087
#define OV9640_GST12		0x0088
#define OV9640_GST13		0x0089
#define OV9640_GST14		0x008A

// End of OV9640 register
#define OV9640_REGEND		( OV9640_GST14 + 1 )

#endif
