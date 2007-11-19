/*
 * linux/drivers/video/imageon/w100_power.h
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


#ifndef _w100_power_h_
#define _w100_power_h_

#define mmCLK_PIN_CNTL		0x0080
#define mmPLL_REF_FB_DIV	0x0084
#define mmPLL_CNTL		0x0088
#define mmSCLK_CNTL		0x008C
#define mmPCLK_CNTL		0x0090
#define mmCLK_TEST_CNTL		0x0094
#define mmPWRMGT_CNTL		0x0098
#define mmPWRMGT_STATUS		0x009C


typedef union {
	unsigned long val			: 32;
	struct _clk_pin_cntl_t {
		unsigned long osc_en		: 1;
		unsigned long osc_gain		: 5;
		unsigned long dont_use_xtalin	: 1;
		unsigned long xtalin_pm_en	: 1;
		unsigned long xtalin_dbl_en	: 1;
		unsigned long			: 7;
		unsigned long cg_debug		: 16;
	} f;
} clk_pin_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _pll_ref_fb_div_t {
		unsigned long pll_ref_div	: 4;
		unsigned long			: 4;
		unsigned long pll_fb_div_int	: 6;
		unsigned long			: 2;
		unsigned long pll_fb_div_frac	: 3;
		unsigned long			: 1;
		unsigned long pll_reset_time	: 4;
		unsigned long pll_lock_time	: 8;
	} f;
} pll_ref_fb_div_u;


typedef union {
	unsigned long val			: 32;
	struct _pll_cntl_t {
		unsigned long pll_pwdn		: 1;
		unsigned long pll_reset		: 1;
		unsigned long pll_pm_en		: 1;
		unsigned long pll_mode		: 1;
		unsigned long pll_refclk_sel	: 1;
		unsigned long pll_fbclk_sel	: 1;
		unsigned long pll_tcpoff	: 1;
		unsigned long pll_pcp		: 3;
		unsigned long pll_pvg		: 3;
		unsigned long pll_vcofr		: 1;
		unsigned long pll_ioffset	: 2;
		unsigned long pll_pecc_mode	: 2;
		unsigned long pll_pecc_scon	: 2;
		unsigned long pll_dactal	: 4;
		unsigned long pll_cp_clip	: 2;
		unsigned long pll_conf		: 3;
		unsigned long pll_mbctrl	: 2;
		unsigned long pll_ring_off	: 1;
	} f;
} pll_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _sclk_cntl_t {
		unsigned long sclk_src_sel	: 2;
		unsigned long			: 2;
		unsigned long sclk_post_div_fast: 4;
		unsigned long sclk_clkon_hys	: 3;
		unsigned long sclk_post_div_slow: 4;
		unsigned long disp_cg_ok2switch_en	: 1;
		unsigned long sclk_force_reg	: 1;
		unsigned long sclk_force_disp	: 1;
		unsigned long sclk_force_mc	: 1;
		unsigned long sclk_force_extmc	: 1;
		unsigned long sclk_force_cp	: 1;
		unsigned long sclk_force_e2	: 1;
		unsigned long sclk_force_e3	: 1;
		unsigned long sclk_force_idct	: 1;
		unsigned long sclk_force_bist	: 1;
		unsigned long busy_extend_cp	: 1;
		unsigned long busy_extend_e2	: 1;
		unsigned long busy_extend_e3	: 1;
		unsigned long busy_extend_idct	: 1;
		unsigned long			: 3;
	} f;
} sclk_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _pclk_cntl_t {
		unsigned long pclk_src_sel	: 2;
		unsigned long			: 2;
		unsigned long pclk_post_div	: 4;
		unsigned long			: 8;
		unsigned long pclk_force_disp	: 1;
		unsigned long			: 15;
	} f;
} pclk_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _clk_test_cntl_t {
		unsigned long testclk_sel	: 4;
		unsigned long			: 3;
		unsigned long start_check_freq	: 1;
		unsigned long tstcount_rst	: 1;
		unsigned long			: 15;
		unsigned long test_count	: 8;
	} f;
} clk_test_cntl_u;


typedef union {
	unsigned long val				: 32;
	struct _pwrmgt_cntl_t {
		unsigned long pwm_enable		: 1;
		unsigned long				: 1;
		unsigned long pwm_mode_req		: 2;
		unsigned long pwm_wakeup_cond		: 2;
		unsigned long pwm_fast_noml_hw_en	: 1;
		unsigned long pwm_noml_fast_hw_en	: 1;
		unsigned long pwm_fast_noml_cond	: 4;
		unsigned long pwm_noml_fast_cond	: 4;
		unsigned long pwm_idle_timer		: 8;
		unsigned long pwm_busy_timer		: 8;
	} f;
} pwrmgt_cntl_u;


typedef union {
	unsigned long val		: 32;
	struct _pwrmgt_status_t {
		unsigned long pwm_mode	: 2;
		unsigned long		: 30;
	} f;
} pwrmgt_status_u;

#endif /* _imageon_power_h_ */
