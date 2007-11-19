/*
 * linux/drivers/video/imageon/w100_cp.h
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


#ifndef _w100_cp_h_

#define mmGEN_INT_CNTL		0x0200
#define mmGEN_INT_STATUS	0x0204

#define mmCP_RB_CNTL		0x0210
#define mmCP_RB_BASE		0x0214
#define mmCP_RB_RPTR_ADDR	0x0218
#define mmCP_RB_RPTR		0x021C
#define mmCP_RB_WPTR		0x0220
#define mmCP_IB_BASE		0x0228
#define mmCP_IB_BUFSZ		0x022C
#define mmCP_CSQ_CNTL		0x0230
#define mmCP_CSQ_APER_PRIMARY	0x0300
#define mmCP_CSQ_APER_INDIRECT	0x0340

#define mmCP_ME_CNTL		0x0240
#define mmCP_ME_RAM_ADDR	0x0244
#define mmCP_ME_RAM_RADDR	0x0248
#define mmCP_ME_RAM_DATAH	0x024C
#define mmCP_ME_RAM_DATAL	0x0250
#define mmCP_DEBUG		0x025C

#define mmSCRATCH_REG0		0x0260
#define mmSCRATCH_REG1		0x0264
#define mmSCRATCH_REG2		0x0268
#define mmSCRATCH_REG3		0x026C
#define mmSCRATCH_REG4		0x0270
#define mmSCRATCH_REG5		0x0274
#define mmSCRATCH_UMSK		0x0280
#define mmSCRATCH_ADDR		0x0284

#define mmCP_CSQ_ADDR		0x02E4
#define mmCP_CSQ_DATA		0x02E8
#define mmCP_CSQ_STAT		0x02EC
#define mmCP_STAT		0x02F0
#define mmCP_RB_RPTR_WR		0x02F8

// CP_RB_CNTL.rb_bufsz
#define	RB_SIZE_2K	8
#define	RB_SIZE_4K	9
#define	RB_SIZE_8K	10
#define	RB_SIZE_16K	11
#define	RB_SIZE_32K	12
#define	RB_SIZE_64K	13

typedef union {
	unsigned long val			: 32;
	struct _cp_rb_cntl_t {
		unsigned long rb_bufsz		: 6;
		unsigned long			: 2;
		unsigned long rb_blksz		: 6;
		unsigned long			: 2;
		unsigned long buf_swap		: 2;
		unsigned long max_fetch		: 2;
		unsigned long			: 7;
		unsigned long rb_no_update	: 1;
		unsigned long			: 3;
		unsigned long rb_rptr_wr_ena	: 1;
	} f;
} cp_rb_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_rb_base_t {
		unsigned long			: 2;
		unsigned long rb_base		: 22;
		unsigned long			: 8;
	} f;
} cp_rb_base_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_rb_rptr_addr_t {
		unsigned long rb_rptr_swap	: 2;
		unsigned long rb_rptr_addr	: 22;
		unsigned long			: 8;
	} f;
} cp_rb_rptr_addr_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_rb_ptr_t {
		unsigned long ptr		: 23;
		unsigned long			: 9;
	} f;
} cp_rb_ptr_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_ib_base_t {
		unsigned long			: 2;
		unsigned long ib_base		: 22;
		unsigned long			: 8;
	} f;
} cp_ib_base_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_ib_bufsz_t {
		unsigned long ib_bufsz		: 23;
		unsigned long			: 9;
	} f;
} cp_ib_bufsz_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_csq_cntl_t {
		unsigned long csq_cnt_primary	: 8;
		unsigned long csq_cnt_indirect	: 8;
		unsigned long			: 12;
		unsigned long csq_mode		: 4;
	} f;
} cp_csq_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_csq_aper_t {
		unsigned long cp_csq_aper	: 32;
	} f;
} cp_csq_aper_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_me_cntl_t {
		unsigned long me_stat		: 16;
		unsigned long me_statmux	: 5;
		unsigned long			: 8;
		unsigned long me_busy		: 1;
		unsigned long me_mode		: 1;
		unsigned long me_step		: 1;
	} f;
} cp_me_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_me_ram_addr_t {
		unsigned long me_ram_addr	: 8;
		unsigned long			: 24;
	} f;
} cp_me_ram_addr_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_me_ram_datah_t {
		unsigned long me_ram_datah	: 6;
		unsigned long			: 26;
	} f;
} cp_me_ram_datah_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_me_ram_datal_t {
		unsigned long me_ram_datal	: 32;
	} f;
} cp_me_ram_datal_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_debug_t {
		unsigned long cp_debug		: 32;
	} f;
} cp_debug_u;


typedef union {
	unsigned long val			: 32;
	struct _scratch_reg_t {
		unsigned long scratch_reg	: 32;
	} f;
} scratch_reg_u;


typedef union {
	unsigned long val			: 32;
	struct _scratch_umsk_t {
		unsigned long scratch_umsk	: 6;
		unsigned long			: 10;
		unsigned long scratch_swap	: 2;
		unsigned long			: 14;
	} f;
} scratch_umsk_u;


typedef union {
	unsigned long val			: 32;
	struct _scratch_addr_t {
		unsigned long			: 5;
		unsigned long scratch_addr	: 27;
	} f;
} scratch_addr_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_csq_addr_t {
		unsigned long			: 2;
		unsigned long csq_addr		: 8;
		unsigned long			: 22;
	} f;
} cp_csq_addr_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_csq_data_t {
		unsigned long csq_data		: 32;
	} f;
} cp_csq_data_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_csq_stat_t {
		unsigned long csq_rptr_primary	: 8;
		unsigned long csq_wptr_primary	: 8;
		unsigned long csq_rptr_indirect	: 8;
		unsigned long csq_wptr_indirect	: 8;
	} f;
} cp_csq_stat_u;


typedef union {
	unsigned long val			: 32;
	struct _cp_stat_t {
		unsigned long mru_busy		: 1;
		unsigned long mwu_busy		: 1;
		unsigned long rsiu_busy		: 1;
		unsigned long rciu_busy		: 1;
		unsigned long			: 5;
		unsigned long csf_primary_busy	: 1;
		unsigned long csf_indirect_busy	: 1;
		unsigned long csq_primary_busy	: 1;
		unsigned long csq_indirect_busy	: 1;
		unsigned long csi_busy		: 1;
		unsigned long			: 14;
		unsigned long guidma_busy	: 1;
		unsigned long viddma_busy	: 1;
		unsigned long cmdstrm_busy	: 1;
		unsigned long cp_busy		: 1;
	} f;
} cp_stat_u;

typedef union {
	unsigned long val			: 32;
	struct _gen_int_t {
		unsigned long crtc_vblank_mask	: 1;
		unsigned long crtc_vline_mask	: 1;
		unsigned long crtc_hwint1_mask	: 1;
		unsigned long crtc_hwint2_mask	: 1;
		unsigned long			: 15;
		unsigned long gui_idle_mask	: 1;
		unsigned long			: 8;
		unsigned long pm4_idle_int_mask	: 1;
		unsigned long dvi_i2c_int_mask	: 1;
		unsigned long			: 2;
	} f;
} gen_int_u;


#endif /* _w100_cp_h_ */
