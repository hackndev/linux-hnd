/*
 * linux/drivers/video/imageon/w100_cif.h
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


#ifndef _w100_cif_h_
#define _w100_cif_h_

#define mmCHIP_ID		0x0000
#define mmREVISION_ID		0x0004
#define mmWRAP_BUF_A		0x0008
#define mmWRAP_BUF_B		0x000C
#define mmWRAP_TOP_DIR		0x0010
#define mmWRAP_START_DIR	0x0014
#define mmCIF_CNTL		0x0018
#define mmCFGREG_BASE		0x001C
#define mmCIF_IO		0x0020
#define mmCIF_READ_DBG		0x0024
#define mmCIF_WRITE_DBG		0x0028

typedef union {
	unsigned long val				: 32;
	struct _chip_id_t {
		unsigned long vendor_id			: 16;
		unsigned long device_id			: 16;
	} f;
} chip_id_u;


typedef union {
	unsigned long val				: 32;
	struct _revision_id_t {
		unsigned long minor_rev_id		: 4;
		unsigned long major_rev_id		: 4;
		unsigned long				: 24;
	} f;
} revision_id_u;


typedef union {
	unsigned long val				: 32;
	struct _wrap_buf_t {
		unsigned long offset_addr		: 24;
		unsigned long block_size		: 3;
		unsigned long				: 5;
	} f;
} wrap_buf_u;


typedef union {
	unsigned long val				: 32;
	struct _wrap_top_dir_t {
		unsigned long top_addr			: 23;
		unsigned long				: 9;
	} f;
} wrap_top_dir_u;


typedef union {
	unsigned long val				: 32;
	struct _wrap_start_dir_t {
		unsigned long start_addr		: 23;
		unsigned long				: 9;
	} f;
} wrap_start_dir_u;


typedef union {
	unsigned long val				: 32;
	struct _cif_cntl_t {
		unsigned long swap_reg			: 2;
		unsigned long swap_fbuf_1		: 2;
		unsigned long swap_fbuf_2		: 2;
		unsigned long swap_fbuf_3		: 2;
		unsigned long pmi_int_disable		: 1;
		unsigned long pmi_schmen_disable	: 1;
		unsigned long intb_oe			: 1;
		unsigned long en_wait_to_compensate_dq_prop_dly	: 1;
		unsigned long compensate_wait_rd_size	: 2;
		unsigned long wait_asserted_timeout_val	: 2;
		unsigned long wait_masked_val		: 2;
		unsigned long en_wait_timeout		: 1;
		unsigned long en_one_clk_setup_before_wait	: 1;
		unsigned long interrupt_active_high	: 1;
		unsigned long en_overwrite_straps	: 1;
		unsigned long strap_wait_active_hi	: 1;
		unsigned long lat_busy_count		: 2;
		unsigned long lat_rd_pm4_sclk_busy	: 1;
		unsigned long dis_system_bits		: 1;
		unsigned long dis_mr			: 1;
		unsigned long cif_spare_1		: 4;
	} f;
} cif_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _cfgreg_base_t {
		unsigned long cfgreg_base		: 24;
		unsigned long				: 8;
	} f;
} cfgreg_base_u;


typedef union {
	unsigned long val				: 32;
	struct _cif_io_t {
		unsigned long dq_srp			: 1;
		unsigned long dq_srn			: 1;
		unsigned long dq_sp			: 4;
		unsigned long dq_sn			: 4;
		unsigned long waitb_srp			: 1;
		unsigned long waitb_srn			: 1;
		unsigned long waitb_sp			: 4;
		unsigned long waitb_sn			: 4;
		unsigned long intb_srp			: 1;
		unsigned long intb_srn			: 1;
		unsigned long intb_sp			: 4;
		unsigned long intb_sn			: 4;
		unsigned long				: 2;
	} f;
} cif_io_u;


typedef union {
	unsigned long val					: 32;
	struct _cif_read_dbg_t {
		unsigned long unpacker_pre_fetch_trig_gen	: 2;
		unsigned long dly_second_rd_fetch_trig		: 1;
		unsigned long rst_rd_burst_id			: 1;
		unsigned long dis_rd_burst_id			: 1;
		unsigned long en_block_rd_when_packer_is_not_emp: 1;
		unsigned long dis_pre_fetch_cntl_sm		: 1;
		unsigned long rbbm_chrncy_dis			: 1;
		unsigned long rbbm_rd_after_wr_lat		: 2;
		unsigned long dis_be_during_rd			: 1;
		unsigned long one_clk_invalidate_pulse		: 1;
		unsigned long dis_chnl_priority			: 1;
		unsigned long rst_read_path_a_pls		: 1;
		unsigned long rst_read_path_b_pls		: 1;
		unsigned long dis_reg_rd_fetch_trig		: 1;
		unsigned long dis_rd_fetch_trig_from_ind_addr	: 1;
		unsigned long dis_rd_same_byte_to_trig_fetch	: 1;
		unsigned long dis_dir_wrap			: 1;
		unsigned long dis_ring_buf_to_force_dec		: 1;
		unsigned long dis_addr_comp_in_16bit		: 1;
		unsigned long clr_w				: 1;
		unsigned long err_rd_tag_is_3			: 1;
		unsigned long err_load_when_ful_a		: 1;
		unsigned long err_load_when_ful_b		: 1;
		unsigned long					: 7;
	} f;
} cif_read_dbg_u;


typedef union {
	unsigned long val					: 32;
	struct _cif_write_dbg_t {
		unsigned long packer_timeout_count		: 2;
		unsigned long en_upper_load_cond		: 1;
		unsigned long en_chnl_change_cond		: 1;
		unsigned long dis_addr_comp_cond		: 1;
		unsigned long dis_load_same_byte_addr_cond	: 1;
		unsigned long dis_timeout_cond			: 1;
		unsigned long dis_timeout_during_rbbm		: 1;
		unsigned long dis_packer_ful_during_rbbm_timeout: 1;
		unsigned long en_dword_split_to_rbbm		: 1;
		unsigned long en_dummy_val			: 1;
		unsigned long dummy_val_sel			: 1;
		unsigned long mask_pm4_wrptr_dec		: 1;
		unsigned long dis_mc_clean_cond			: 1;
		unsigned long err_two_reqi_during_ful		: 1;
		unsigned long err_reqi_during_idle_clk		: 1;
		unsigned long err_global			: 1;
		unsigned long en_wr_buf_dbg_load		: 1;
		unsigned long en_wr_buf_dbg_path		: 1;
		unsigned long sel_wr_buf_byte			: 3;
		unsigned long dis_rd_flush_wr			: 1;
		unsigned long dis_packer_ful_cond		: 1;
		unsigned long dis_invalidate_by_ops_chnl	: 1;
		unsigned long en_halt_when_reqi_err		: 1;
		unsigned long cif_spare_2			: 5;
		unsigned long					: 1;
	} f;
} cif_write_dbg_u;

#endif /* _w100_cif_h__ */
