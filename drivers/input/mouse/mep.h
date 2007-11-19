/*
 * Synaptics Modular Embedded Protocol (MEP)
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _MEP_H
#define _MEP_H

#include <linux/kernel.h>

/* Host -> Device Packets */
#define MEP_HOST_PKT_ADDR(hdr)		(((hdr)&7)<<5)
#define MEP_HOST_PKT_GLOBAL		0x10
#define MEP_HOST_PKT_FORMAT_SHORT	0x08

#define MEP_HOST_CMD_TYPE_QUERY		0x80
#define MEP_HOST_CMD_TYPE_PARM		0x40

/* short commands */
#define MEP_HOST_CMD_RESET			0
#define MEP_HOST_CMD_ACTIVE			1
#define MEP_HOST_CMD_SLEEP			2
#define MEP_HOST_CMD_ENABLE_AUTO_REPORT		3
#define MEP_HOST_CMD_DISABLE_AUTO_REPORT	4
#define MEP_HOST_CMD_POLL_REPORT		5

/* long commands */
#define MEP_HOST_CMD_SET_DEFAULT		0x08
#define MEP_HOST_PKT_SIZE			8
#define MEP_HOST_HEADER				0
#define MEP_HOST_LONG_CMD			1
#define MEP_HOST_DATA_START			2

#define MEP_MODULE_PKT_ADDR(hdr)	(((hdr)>>5)&7)
#define MEP_MODULE_PKT_CTRL(hdr)	(((hdr)>>3)&3)
#define MEP_MODULE_PKT_LEN(hdr)		((hdr)&7)
#define MEP_MODULE_PKT_CTRL_NATIVE	1
#define MEP_MODULE_PKT_CTRL_EXT		2
#define MEP_MODULE_PKT_CTRL_GEN		3

#define MEP_MODULE_PKT_PID_ACK		2

#define MEP_MODULE_PKT_ERR_CODE(byte)	((byte) & 0xf)

enum mep_module_err {
	MEP_MODULE_ERR_GENERAL = 0,
	MEP_MODULE_ERR_BAD_CMD,
	MEP_MODULE_ERR_BAD_PARM,
	MEP_MODULE_ERR_INCOMP_TX,	/* incomplete transmission when sent guest-ward */
	MEP_MODULE_ERR_ADDR,
	MEP_MODULE_ERR_PARITY,
	MEP_MODULE_ERR_RESERVED1,
	MEP_MODULE_ERR_RESERVED2
};

#define MEP_MODULE_CFG_EVENT_WAKE	0x0001
#define MEP_MODULE_CFG_SENSORY_WAKE	0x0002
#define MEP_MODULE_CFG_HIBERNATE	0x0004
#define MEP_MODULE_CFG_SLEEP_MODE	0x0008
#define MEP_MODULE_CFG_NO_DOZE		0x0010
/* reserved				0x0020 */
/* reserved				0x0040 */
#define MEP_MODULE_CFG_AUTO_RPTOFF	0x0080

struct mep_module_pkt {
	u_int8_t	header;
	u_int8_t	data[7];
};

/*
 * Packet descriptor for a MEP stream. Use same stream descriptor for each
 * bus that has MEP devices. Parses out addr, ctrl, len, but also provides
 * the intact header (pkt.header)
 */
struct mep_pkt_stream_desc {
	struct mep_module_pkt pkt;
	u_int8_t addr;
	u_int8_t ctrl;
	u_int8_t len;
	/* private, do not use */
	u_int8_t *datap;
	int remain_len;		/* count down filling packet */
	int state;
};

static inline int
mep_pkt_get_pid( struct mep_pkt_stream_desc *mp )
{
    return mp->pkt.data[0] >> 4;
}

/*
 * general MEP usage section
 */
struct mep_module_info {
	unsigned char	manfid;
	unsigned	mep_maj_ver:4;
	unsigned	mep_min_ver:4;
	unsigned	class_maj_ver:4;
	unsigned	class_min_ver:4;
	unsigned char	classid;
	unsigned	subclass_maj_ver:4;
	unsigned	subclass_min_ver:4;
	unsigned char	subclassid;
};

struct mep_prod_info {
	unsigned char	mod_maj_ver;
	unsigned char	mod_min_ver;
	unsigned	prodid;
	unsigned	can_doze:1;
	unsigned	guest_port:1;
};

struct mep_serial_info {
	unsigned char	year;
	unsigned char	month;
	unsigned char	date;
	unsigned	tester_id;
	unsigned	serial;
};

struct mep_dev {
    int				unit;
    struct mep_module_info	minfo;
    struct mep_prod_info	pinfo;
    struct mep_serial_info	sinfo;
    unsigned			in_use:1;
};

enum mep_rx_result {
    MEP_RX_PKT_DONE=0,
    MEP_RX_PKT_MORE,
    MEP_RX_PKT_ERR
};

enum mep_rx_result mep_rx( struct mep_pkt_stream_desc *pkt, u_int8_t octet );
int mep_host_format_cmd( u_int8_t *pkt, int addr,
	int command, int num_data, unsigned char *data );
int mep_get_module_info( struct mep_dev *dev, struct mep_module_info *info );
int mep_get_product_info( struct mep_dev *dev, struct mep_prod_info *info );
int mep_get_serial_info( struct mep_dev *dev, struct mep_serial_info *info );
void mep_report_base_info( void *nu, u_int8_t *data );
void mep_report_prod_info( void *nu, u_int8_t *data );

#endif /* _MEP_H */
