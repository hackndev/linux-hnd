/*
 * linux/drivers/video/imageon/w100_cfg.h
 *
 * Frame Buffer Device for ATI Imageon 100
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
 *  ATI's w100fb.h Wallaby code
 *
 * ChangeLog:
 *  2004-01-03 Created
 */

#ifndef _w100_cfg_h_
#define _w100_cfg_h_


#define cfgIND_ADDR_A_0		0x0000
#define cfgIND_ADDR_A_1		0x0001
#define cfgIND_ADDR_A_2		0x0002
#define cfgIND_DATA_A		0x0003
#define cfgREG_BASE		0x0004
#define cfgINTF_CNTL		0x0005
#define cfgSTATUS		0x0006
#define cfgCPU_DEFAULTS		0x0007
#define cfgIND_ADDR_B_0		0x0008
#define cfgIND_ADDR_B_1		0x0009
#define cfgIND_ADDR_B_2		0x000A
#define cfgIND_DATA_B		0x000B
#define cfgPM4_RPTR		0x000C
#define cfgSCRATCH		0x000D
#define cfgPM4_WRPTR_0		0x000E
#define cfgPM4_WRPTR_1		0x000F


typedef union {
	unsigned char val					: 8;
	struct _ind_addr_t {
		unsigned char ind_addr				: 8;
	} f;
} ind_addr_u;


typedef union {
	unsigned char val					: 8;
	struct _ind_data_t {
		unsigned char ind_data				: 8;
	} f;
} ind_data_u;


typedef union {
	unsigned char val					: 8;
	struct _reg_base_t {
		unsigned char reg_base				: 8;
	} f;
} reg_base_u;


typedef union {
	unsigned char val					: 8;
	struct _intf_cntl_t {
		unsigned char ad_inc_a				: 1;
		unsigned char ring_buf_a			: 1;
		unsigned char rd_fetch_trigger_a		: 1;
		unsigned char rd_data_rdy_a			: 1;
		unsigned char ad_inc_b				: 1;
		unsigned char ring_buf_b			: 1;
		unsigned char rd_fetch_trigger_b		: 1;
		unsigned char rd_data_rdy_b			: 1;
	} f;
} intf_cntl_u;


typedef union {
	unsigned char val					: 8;
	struct _status_t {
		unsigned char wr_fifo_available_space		: 2;
		unsigned char fbuf_wr_pipe_emp			: 1;
		unsigned char soft_reset			: 1;
		unsigned char system_pwm_mode			: 2;
		unsigned char mem_access_dis			: 1;
		unsigned char en_pre_fetch			: 1;
	} f;   
} status_u;


typedef union {
	unsigned char val					: 8;
	struct _cpu_defaults_t {
		unsigned char unpack_rd_data			: 1;
		unsigned char access_ind_addr_a			: 1;
		unsigned char access_ind_addr_b			: 1;
		unsigned char access_scratch_reg		: 1;
		unsigned char pack_wr_data			: 1;
		unsigned char transition_size			: 1;
		unsigned char en_read_buf_mode			: 1;
		unsigned char rd_fetch_scratch			: 1;
	} f;
} cpu_defaults_u;


typedef union {
	unsigned char val					: 8;
	struct _pm4_rptr_t {
		unsigned char pm4_rptr				: 8;
	} f;
} pm4_rptr_u;


typedef union {
	unsigned char val					: 8;
	struct _scratch_t {
		unsigned char scratch				: 8;
	} f;
} scratch_u;


typedef union {
	unsigned char val					: 8;
	struct _pm4_wrptr_0_t {
		unsigned char pm4_wrptr_0			: 8;
	} f;
} pm4_wrptr_0_u; 


typedef union {
	unsigned char val					: 8;
	struct _pm4_wrptr_1_t {
		unsigned char pm4_wrptr_1			: 6;
		unsigned char rd_fetch_pm4_rptr			: 1;
		unsigned char wrptr_atomic_update_w		: 1;
	} f;
} pm4_wrptr_1_u;


#endif /* _w100_cfg_h_ */
