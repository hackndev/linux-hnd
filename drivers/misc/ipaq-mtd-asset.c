/*
* Access to iPAQ Asset Information store in flash
*
* Copyright 2001,2002,2003 Compaq Computer Corporation.
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
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restrutured June 2002
*
* Moved into separate modules March 2003
* Jamey Hicks <jamey.hicks@hp.com>
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <asm/arch/hardware.h>
#include <asm/arch-sa1100/h3600_hal.h>
#include <asm/arch-sa1100/h3600_asic.h>

/***********************************************************************************
 *   Assets
 ***********************************************************************************/

struct mtd_info *g_asset_mtd;

#if 0 /* dead code */
#define DUMP_BUF_SIZE   16
#define MYPRINTABLE(x) \
    ( ((x)<='z' && (x)>='a') || ((x)<='Z' && (x)>='A') || isdigit(x))

static void dump_mem( struct mtd_info *mtd, loff_t from, size_t count )
{
	size_t retlen;
	u_char buf[DUMP_BUF_SIZE];
	u_char pbuf[DUMP_BUF_SIZE + 1];

	int    ret;
	size_t len;
	int    i;

	printk(__FUNCTION__ ": dumping 0x%08X, len %dd\n", from, count);
	while ( count ) {
		len = count;
		if ( len > DUMP_BUF_SIZE )
			len = DUMP_BUF_SIZE;

		ret = mtd->read( mtd, from, len, &retlen, buf );
		if (!ret) {
			for ( i = 0 ; i < retlen ; i++ ) {
				printk(" %02x", buf[i]);
				pbuf[i] = ( MYPRINTABLE(buf[i]) ? buf[i] : '.' );
				from++;
				count--;
				if ( (from % DUMP_BUF_SIZE) == 0 ) {
					pbuf[i+1] = '\0';
					printk(" %s\n", pbuf);
				}
			}
		}
		else {
			printk(__FUNCTION__ ": error %d reading at %d, count=%d", ret,from,count);
			return;
		}
	}

}
#endif /* dead code */


static int copytchar( struct mtd_info *mtd, unsigned char *dest, loff_t from, size_t count )
{
	size_t retlen;

	if (0) printk("%s: %p %lld %u\n", __FUNCTION__, dest, from, count );

	while ( count ) {
		int ret = mtd->read( mtd, from, count, &retlen, dest );
		if (!ret) {
			count -= retlen;
			dest += retlen;
			from += retlen;
		}
		else {
			printk("%s: error %d reading at %lld, count=%d\n", __FUNCTION__, ret, from, count );
			return ret;
		}
	}
	return 0;
}

static int copyword( struct mtd_info *mtd, unsigned short *dest, loff_t from )
{
	unsigned char buf[2];
	int ret = copytchar( mtd, buf, from, 2 );
	if ( ret ) 
		return ret;
	*dest = (((unsigned short) buf[1]) << 8 | (unsigned short) buf[0]);
	return 0;
}

#define COPYTCHAR(_p,_offset,_len) \
        ( g_asset_mtd ? \
        copytchar( g_asset_mtd, _p, _offset + 0x30000, _len ) \
        : 0)

#define COPYWORD(_w,_offset) \
        ( g_asset_mtd ? \
        copyword( g_asset_mtd, &_w, _offset + 0x30000 ) \
          : 0)

int ipaq_mtd_asset_read( struct h3600_asset *asset )
{
	int retval = -1;

	if (0) printk("%s\n", __FUNCTION__);

	switch (asset->type) {
	case ASSET_HM_VERSION:
		retval = COPYTCHAR( asset->a.tchar, 0, 10 );
		break;
	case ASSET_SERIAL_NUMBER:
		retval = COPYTCHAR( asset->a.tchar, 10, 40 );
		break;
	case ASSET_MODULE_ID:
		retval = COPYTCHAR( asset->a.tchar, 152, 20 );
		break;
	case ASSET_PRODUCT_REVISION:
		retval = COPYTCHAR( asset->a.tchar, 182, 10 );
		break;
	case ASSET_PRODUCT_ID:
		retval = COPYWORD( asset->a.vshort, 110 );
		break;
	case ASSET_FRAME_RATE:
		retval = COPYWORD( asset->a.vshort, 966 );
		break;
	case ASSET_PAGE_MODE:    
		retval = COPYWORD( asset->a.vshort, 976 );
		break;
	case ASSET_COUNTRY_ID:
		retval = COPYWORD( asset->a.vshort, 980 );
		break;
	case ASSET_IS_COLOR_DISPLAY:
		retval = COPYWORD( asset->a.vshort, 292 );
		break;
	case ASSET_ROM_SIZE:
		retval = COPYWORD( asset->a.vshort, 1514 );
		break;
	case ASSET_RAM_SIZE:
		asset->a.vshort = 0;
		retval = 0;
		break;
	case ASSET_HORIZONTAL_PIXELS:
		retval = COPYWORD( asset->a.vshort, 276 );
		break;
	case ASSET_VERTICAL_PIXELS:
		retval = COPYWORD( asset->a.vshort, 278 );
		break;
	}

	return retval;
}


int ipaq_mtd_asset_init( void )
{
	int i;
	struct mtd_info *mtd;

	for ( i = MAX_MTD_DEVICES-1 ; i >=  0 ; i-- ) {
		if ( (mtd = get_mtd_device( NULL, i )) != NULL ) {
			if (strstr(mtd->name, "asset" )) {
				g_asset_mtd = mtd;
				return 0;
			}
			else {
				put_mtd_device(mtd);
			}
		}
	}
	return 0;
}

void ipaq_mtd_asset_cleanup( void )
{
	if ( g_asset_mtd )
		put_mtd_device(g_asset_mtd);
}

module_init(ipaq_mtd_asset_init)
module_exit(ipaq_mtd_asset_cleanup)

MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>, Jamey Hicks <jamey@handhelds.org>");
MODULE_DESCRIPTION("Driver for Compaq iPAQ asset tags stored in MTD flash partitions");
MODULE_LICENSE("Dual BSD/GPL");
