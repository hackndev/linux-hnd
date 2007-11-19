/*
* Abstraction interface for microcontroller connection to rest of system
*
* Copyright 2000,1 Compaq Computer Corporation.
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
*
*/

#ifndef __IPAQ_ASSET_H
#define __IPAQ_ASSET_H

#define TTYPE(_type)           (((unsigned int)_type) << 8)
#define TCHAR(_len)            (TTYPE(ASSET_TCHAR) | (_len))
#define TSHORT                 TTYPE(ASSET_SHORT)
#define TLONG                  TTYPE(ASSET_LONG)
#define ASSET(_type,_num)      ((((unsigned int)_type)<<16) | (_num))

#define ASSET_HM_VERSION        ASSET( TCHAR(10), 0 )   /* 1.1, 1.2 */
#define ASSET_SERIAL_NUMBER     ASSET( TCHAR(40), 1 )   /* Unique iPAQ serial number */
#define ASSET_MODULE_ID         ASSET( TCHAR(20), 2 )   /* E.g., "iPAQ 3700" */    
#define ASSET_PRODUCT_REVISION  ASSET( TCHAR(10), 3 )   /* 1.0, 2.0 */
#define ASSET_PRODUCT_ID        ASSET( TSHORT,    4 )   /* 2 = Palm-sized computer */
#define ASSET_FRAME_RATE        ASSET( TSHORT,    5 )
#define ASSET_PAGE_MODE         ASSET( TSHORT,    6 )   /* 0 = Flash memory */
#define ASSET_COUNTRY_ID        ASSET( TSHORT,    7 )   /* 0 = USA */
#define ASSET_IS_COLOR_DISPLAY  ASSET( TSHORT,    8 )   /* Boolean, 1 = yes */
#define ASSET_ROM_SIZE          ASSET( TSHORT,    9 )   /* 16, 32 */
#define ASSET_RAM_SIZE          ASSET( TSHORT,   10 )   /* 32768 */
#define ASSET_HORIZONTAL_PIXELS ASSET( TSHORT,   11 )   /* 240 */
#define ASSET_VERTICAL_PIXELS   ASSET( TSHORT,   12 )   /* 320 */

#define ASSET_TYPE(_asset)       (((_asset)&0xff000000)>>24)
#define ASSET_TCHAR_LEN(_asset)  (((_asset)&0x00ff0000)>>16)
#define ASSET_NUMBER(_asset)     ((_asset)&0x0000ffff)

#define MAX_TCHAR_LEN 40

struct ipaq_asset {
	unsigned int type;
	union {
		unsigned char  tchar[ MAX_TCHAR_LEN ];
		unsigned short vshort;
		unsigned long  vlong;
	} a;
};

#endif
