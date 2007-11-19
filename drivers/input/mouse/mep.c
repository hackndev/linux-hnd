/*
 * Synaptics Modular Embedded Protocol (MEP) implementation
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/module.h>
#include "mep.h"

int
mep_host_format_cmd( u_int8_t *pkt, int addr, int command,
	int num_data, unsigned char *data )
{
	int size=1;

	if (command < MEP_HOST_CMD_SET_DEFAULT) {
		pkt[MEP_HOST_HEADER] = (u_int8_t)command | MEP_HOST_PKT_FORMAT_SHORT;
	}
	else {
		int i;

		size += num_data+1;		/* long command counts */
		for (i=0; i<num_data; i++)
			pkt[MEP_HOST_DATA_START+i] = data[i];
		pkt[MEP_HOST_HEADER] = num_data+1;
		pkt[MEP_HOST_LONG_CMD] = (u_int8_t)command;
	}

	/* addr of -1 is global */
	if (addr==-1)
		pkt[MEP_HOST_HEADER] |= MEP_HOST_PKT_GLOBAL;
	else
		pkt[MEP_HOST_HEADER] |= MEP_HOST_PKT_ADDR(addr);
	return size;
}
EXPORT_SYMBOL( mep_host_format_cmd );

#define MEP_IDLE 0
#define MEP_PROCESSING 1

enum mep_rx_result
mep_rx( struct mep_pkt_stream_desc *ps, u_int8_t octet )
{
	if (ps->state == MEP_IDLE) {
		if (octet != 0) {
			ps->pkt.header = octet;
			ps->addr = MEP_MODULE_PKT_ADDR(octet);
			ps->ctrl = MEP_MODULE_PKT_CTRL(octet);
			ps->len = MEP_MODULE_PKT_LEN(octet);
			ps->remain_len = ps->len;
			ps->datap = ps->pkt.data;
			
			if (ps->len == 0) // This is ACK, most likely
				return MEP_RX_PKT_DONE;
			else
				ps->state = MEP_PROCESSING;
		}
	}
	else {
		/* processing state */
		*ps->datap++ = octet;
		if (--ps->remain_len == 0) {
			ps->state = MEP_IDLE;
			return MEP_RX_PKT_DONE;
		}
	}
	return MEP_RX_PKT_MORE;
}
EXPORT_SYMBOL( mep_rx );

void
mep_report_base_info( void *notused, u_int8_t *data )
{
	printk( "MEP Base: Manufacturer ID: %d\n", (int)data[1] );
	printk( "MEP Base: Major version: %d\n", (int)data[2]>>4 );
	printk( "MEP Base: Minor version: %d\n", (int)data[2] & 0xf );
	printk( "MEP Base: Class major version: %d\n", (int)data[3]>>4 );
	printk( "MEP Base: Class minor version: %d\n", (int)data[3] & 0xf );
	printk( "MEP Base: Module class ID: %d\n", (int)data[4] );
	printk( "MEP Base: Sub-class major version: %d\n", (int)data[5]>>4 );
	printk( "MEP Base: Sub-class minor version: %d\n", (int)data[5] & 0xf );
	printk( "MEP Base: Module sub-class ID: %d\n", (int)data[6] );
}
EXPORT_SYMBOL( mep_report_base_info );

void
mep_report_prod_info( void *notused, u_int8_t *data )
{
	printk( "MEP Product: Module major version: %d\n", (int)data[1] );
	printk( "MEP Product: Module minor version: %d\n", (int)data[2] );
	printk( "MEP Product: Module product ID: %d\n", (int)(data[3]<<8) | data[4] );
	printk( "MEP Product: Features: %s %s\n", (data[6]&2)?"CanDoze":"",
		(data[6]&1)?"GuestPort":"" );
}
EXPORT_SYMBOL(mep_report_prod_info);

MODULE_LICENSE("GPL");
