/*
 * linux/drivers/video/imageon/w100_memory.h
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

#ifndef _w100_memory_h_
#define _w100_memory_h_

#define mmMEM_CNTL			0x0180
#define mmMEM_ARB			0x0184
#define mmMC_FB_LOCATION		0x0188
#define mmMEM_EXT_CNTL			0x018C
#define mmMC_EXT_MEM_LOCATION		0x0190
#define mmMEM_EXT_TIMING_CNTL		0x0194
#define mmMEM_SDRAM_MODE_REG		0x0198
#define mmMEM_IO_CNTL			0x019C
#define mmMC_DEBUG			0x01A0
#define mmMC_BIST_CTRL			0x01A4
#define mmMC_BIST_COLLAR_READ		0x01A8
#define mmTC_MISMATCH			0x01AC
#define mmMC_PERF_MON_CNTL		0x01B0
#define mmMC_PERF_COUNTERS		0x01B4

#define mmBM_EXT_MEM_BANDWIDTH		0x0A00
#define mmBM_OFFSET			0x0A04
#define mmBM_MEM_EXT_TIMING_CNTL	0x0A08
#define mmBM_MEM_EXT_CNTL		0x0A0C
#define mmBM_MEM_MODE_REG		0x0A10
#define mmBM_MEM_IO_CNTL		0x0A18
#define mmBM_CONFIG			0x0A1C
#define mmBM_STATUS			0x0A20
#define mmBM_DEBUG			0x0A24
#define mmBM_PERF_MON_CNTL		0x0A28
#define mmBM_PERF_COUNTERS		0x0A2C
#define mmBM_PERF2_MON_CNTL		0x0A30
#define mmBM_PERF2_COUNTERS		0x0A34

typedef union {
	unsigned long val				: 32;
	struct _mem_cntl_t {
		unsigned long				: 1;
		unsigned long en_mem_ch1		: 1;
		unsigned long en_mem_ch2		: 1;
		unsigned long int_mem_mapping		: 1;
		unsigned long				: 28;
	} f;
} mem_cntl_u;


typedef union {
	unsigned long val				: 32;
        struct _mem_arb_t {
		unsigned long disp_time_slot		: 4;
		unsigned long disp_timer		: 4;
		unsigned long arb_option		: 1;
		unsigned long				: 23;
	} f;
} mem_arb_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_fb_location_t {
		unsigned long mc_fb_start		: 16;
		unsigned long mc_fb_top			: 16;
	} f;
} mc_fb_location_u;


typedef union {
	unsigned long val				: 32;
	struct _mem_ext_cntl_t {
		unsigned long mem_ext_enable		: 1;
		unsigned long mem_ap_enable		: 1;
		unsigned long mem_addr_mapping		: 2;
		unsigned long mem_wdoe_cntl		: 2;
		unsigned long mem_wdoe_extend		: 1;
		unsigned long				: 1;
		unsigned long mem_page_timer		: 8;
		unsigned long mem_dynamic_cke		: 1;
		unsigned long mem_sdram_tri_en		: 1;
		unsigned long mem_self_refresh_en	: 1;
		unsigned long mem_power_down		: 1;
		unsigned long mem_hw_power_down_en	: 1;
		unsigned long mem_power_down_stat	: 1;
		unsigned long				: 3;
		unsigned long mem_pd_mck		: 1;
		unsigned long mem_pd_ma			: 1;
		unsigned long mem_pd_mdq		: 1;
		unsigned long mem_tristate_mck		: 1;
		unsigned long mem_tristate_ma		: 1;
		unsigned long mem_tristate_mcke		: 1;
		unsigned long mem_invert_mck		: 1;
	} f;
} mem_ext_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_ext_mem_location_t {
		unsigned long mc_ext_mem_start		: 16;
		unsigned long mc_ext_mem_top		: 16;
	} f;
} mc_ext_mem_location_u;


typedef union {
	unsigned long val				: 32;
	struct _mem_ext_timing_cntl_t {
		unsigned long mem_trp			: 2;
		unsigned long mem_trcd			: 2;
		unsigned long mem_tras			: 3;
		unsigned long				: 1;
		unsigned long mem_trrd			: 2;
		unsigned long mem_tr2w			: 2;
		unsigned long mem_twr			: 2;
		unsigned long				: 4;
		unsigned long mem_twr_mode		: 1;
		unsigned long				: 1;
		unsigned long mem_refresh_dis		: 1;
		unsigned long				: 3;
		unsigned long mem_refresh_rate		: 8;
	} f;
} mem_ext_timing_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _mem_sdram_mode_reg_t {
		unsigned long mem_mode_reg		: 14;
		unsigned long				: 2;
		unsigned long mem_read_latency		: 2;
		unsigned long mem_schmen_latency	: 2;
		unsigned long mem_cas_latency		: 2;
		unsigned long mem_schmen_extend		: 1;
		unsigned long				: 8;
		unsigned long mem_sdram_reset		: 1;
	} f;
} mem_sdram_mode_reg_u;


typedef union {
	unsigned long val				: 32;
	struct _mem_io_cntl_t {
		unsigned long mem_sn_mck		: 4;
		unsigned long mem_sn_ma			: 4;
		unsigned long mem_sn_mdq		: 4;
		unsigned long mem_srn_mck		: 1;
		unsigned long mem_srn_ma		: 1;
		unsigned long mem_srn_mdq		: 1;
		unsigned long				: 1;
		unsigned long mem_sp_mck		: 4;
		unsigned long mem_sp_ma			: 4;
		unsigned long mem_sp_mdq		: 4;
		unsigned long mem_srp_mck		: 1;
		unsigned long mem_srp_ma		: 1;
		unsigned long mem_srp_mdq		: 1;
		unsigned long				: 1;
	} f;
} mem_io_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_debug_t {
		unsigned long mc_debug			: 32;
	} f;
} mc_debug_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_bist_ctrl_t {
		unsigned long mc_bist_ctrl		: 32;
	} f;
} mc_bist_ctrl_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_bist_collar_read_t {
		unsigned long mc_bist_collar_read	: 32;
	} f;
} mc_bist_collar_read_u;


typedef union {
	unsigned long val				: 32;
	struct _tc_mismatch_t {
		unsigned long tc_mismatch		: 24;
		unsigned long				: 8;
	} f;
} tc_mismatch_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_perf_mon_cntl_t {
		unsigned long clr_perf			: 1;
		unsigned long en_perf			: 1;
		unsigned long				: 2;
		unsigned long perf_op_a			: 2;
		unsigned long perf_op_b			: 2;
		unsigned long				: 8;
		unsigned long monitor_period		: 8;
		unsigned long perf_count_a_overflow	: 1;
		unsigned long perf_count_b_overflow	: 1;
		unsigned long				: 6;
	} f;
} mc_perf_mon_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _mc_perf_counters_t {
		unsigned long mc_perf_counter_a		: 16;
		unsigned long mc_perf_counter_b		: 16;
	} f;
} mc_perf_counters_u;


#endif /* _w100_memory_h_ */
