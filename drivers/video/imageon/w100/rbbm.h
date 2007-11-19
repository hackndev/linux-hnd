/*
 * linux/drivers/video/imageon/w100_rbbm.h
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

#ifndef _w100_rbbm_h_
#define _w100_rbbm_h_

#define mmWAIT_UNTIL		0x1400
#define mmISYNC_CNTL		0x1404
#define mmRBBM_GUICNTL		0x1408
#define mmRBBM_STATUS_alt_1	0x140C

#define mmRBBM_STATUS		0x0140
#define mmRBBM_CNTL		0x0144
#define mmRBBM_SOFT_RESET	0x0148
#define mmNQWAIT_UNTIL		0x0150
#define mmRBBM_DEBUG		0x016C
#define mmRBBM_CMDFIFO_ADDR	0x0170
#define mmRBBM_CMDFIFO_DATAL	0x0174
#define mmRBBM_CMDFIFO_DATAH	0x0178
#define mmRBBM_CMDFIFO_STAT	0x017C


typedef union {
	unsigned long val				: 32;
	struct _wait_until_t {
		unsigned long wait_crtc_pflip		: 1;
		unsigned long wait_re_crtc_vline	: 1;
		unsigned long wait_fe_crtc_vline	: 1;
		unsigned long wait_crtc_vline		: 1;
		unsigned long wait_dma_viph0_idle	: 1;
		unsigned long wait_dma_viph1_idle	: 1;
		unsigned long wait_dma_viph2_idle	: 1;
		unsigned long wait_dma_viph3_idle	: 1;
		unsigned long wait_dma_vid_idle		: 1;
		unsigned long wait_dma_gui_idle		: 1;
		unsigned long wait_cmdfifo		: 1;
		unsigned long wait_ov0_flip		: 1;
		unsigned long wait_ov0_slicedone	: 1;
		unsigned long				: 1;
		unsigned long wait_2d_idle		: 1;
		unsigned long wait_3d_idle		: 1;
		unsigned long wait_2d_idleclean		: 1;
		unsigned long wait_3d_idleclean		: 1;
		unsigned long wait_host_idleclean	: 1;
		unsigned long wait_extern_sig		: 1;
		unsigned long cmdfifo_entries		: 7;
		unsigned long				: 3;
		unsigned long wait_both_crtc_pflip	: 1;
		unsigned long eng_display_select	: 1;
	} f;
} wait_until_u;


typedef union {
	unsigned long val				: 32;
	struct _isync_cntl_t {
		unsigned long isync_any2d_idle3d	: 1;
		unsigned long isync_any3d_idle2d	: 1;
		unsigned long isync_trig2d_idle3d	: 1;
		unsigned long isync_trig3d_idle2d	: 1;
		unsigned long isync_wait_idlegui	: 1;
		unsigned long isync_cpscratch_idlegui	: 1;
		unsigned long				: 26;
	} f;
} isync_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_guicntl_t {
		unsigned long host_data_swap	: 2;
		unsigned long			: 30;
	} f;
} rbbm_guicntl_u;


typedef union {
	unsigned long val : 32;
	struct _rbbm_status_t {
		unsigned long cmdfifo_avail	: 7;
		unsigned long			: 1;
		unsigned long hirq_on_rbb	: 1;
		unsigned long cprq_on_rbb	: 1;
		unsigned long cfrq_on_rbb	: 1;
		unsigned long hirq_in_rtbuf	: 1;
		unsigned long cprq_in_rtbuf	: 1;
		unsigned long cfrq_in_rtbuf	: 1;
		unsigned long cf_pipe_busy	: 1;
		unsigned long eng_ev_busy	: 1;
		unsigned long cp_cmdstrm_busy	: 1;
		unsigned long e2_busy		: 1;
		unsigned long rb2d_busy		: 1;
		unsigned long rb3d_busy		: 1;
		unsigned long se_busy		: 1;
		unsigned long re_busy		: 1;
		unsigned long tam_busy		: 1;
		unsigned long tdm_busy		: 1;
		unsigned long pb_busy		: 1;
		unsigned long			: 6;
		unsigned long gui_active	: 1;
	} f;
} rbbm_status_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_cntl_t {
		unsigned long rb_settle		: 4;
		unsigned long abortclks_hi	: 3;
		unsigned long			: 1;
		unsigned long abortclks_cp	: 3;
		unsigned long			: 1;
		unsigned long abortclks_cfifo	: 3;
		unsigned long			: 2;
		unsigned long cpq_data_swap	: 1;
		unsigned long			: 3;
		unsigned long no_abort_idct	: 1;
		unsigned long no_abort_bios	: 1;
		unsigned long no_abort_fb	: 1;
		unsigned long no_abort_cp	: 1;
		unsigned long no_abort_hi	: 1;
		unsigned long no_abort_hdp	: 1;
		unsigned long no_abort_mc	: 1;
		unsigned long no_abort_aic	: 1;
		unsigned long no_abort_vip	: 1;
		unsigned long no_abort_disp	: 1;
		unsigned long no_abort_cg	: 1;
	} f;
} rbbm_cntl_u;


typedef union {
	unsigned long val : 32;
	struct _rbbm_soft_reset_t {
		unsigned long soft_reset_cp	: 1;
		unsigned long soft_reset_hi	: 1;
		unsigned long reserved3		: 3;
		unsigned long soft_reset_e2	: 1;
		unsigned long reserved2		: 2;
		unsigned long soft_reset_mc	: 1;
		unsigned long reserved1		: 2;
		unsigned long soft_reset_disp	: 1;
		unsigned long soft_reset_cg	: 1;
		unsigned long			: 19;
	} f;
} rbbm_soft_reset_u;


typedef union {
	unsigned long val			: 32;
	struct _nqwait_until_t {
		unsigned long wait_gui_idle	: 1;
		unsigned long			: 31;
	} f;
} nqwait_until_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_debug_t {
		unsigned long rbbm_debug	: 32;
	} f;
} rbbm_debug_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_cmdfifo_addr_t {
		unsigned long cmdfifo_addr	: 6;
		unsigned long			: 26;
	} f;
} rbbm_cmdfifo_addr_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_cmdfifo_datal_t {
		unsigned long cmdfifo_data	: 32;
	} f;
} rbbm_cmdfifo_data_u;


typedef union {
	unsigned long val			: 32;
	struct _rbbm_cmdfifo_stat_t {
		unsigned long cmdfifo_rptr	: 6;
		unsigned long			: 2;
		unsigned long cmdfifo_wptr	: 6;
		unsigned long			: 18;
	} f;
} rbbm_cmdfifo_stat_u;

#endif /* _w100_rbbm_h_ */

