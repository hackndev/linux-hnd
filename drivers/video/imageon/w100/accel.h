/*
 * linux/drivers/video/imageon/w100_accel.h
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

#ifndef _w100_accel_h_
#define _w100_accel_h_

#define mmDST_OFFSET		0x1004
#define mmDST_PITCH		0x1008
#define mmDST_WIDTH		0x100C
#define mmDST_HEIGHT		0x1010

#define mmSRC_X			0x1014
#define mmSRC_Y			0x1018
#define mmDST_X			0x101C
#define mmDST_Y			0x1020

#define mmSRC_PITCH_OFFSET	0x1028
#define mmDST_PITCH_OFFSET	0x102C
#define mmSRC_Y_X		0x1034
#define mmDST_Y_X		0x1038
#define mmDST_HEIGHT_WIDTH	0x103C

#define mmSRC_WIDTH		0x1040
#define mmSRC_HEIGHT		0x1044
#define mmSRC_INC		0x1048

#define mmSRC2_X		0x1050
#define mmSRC2_Y		0x1054
#define mmSRC2_X_Y		0x1058
#define mmSRC2_OFFSET		0x1060
#define mmSRC2_PITCH		0x1064
#define mmSRC2_PITCH_OFFSET	0x1068

#define mmDP_GUI_MASTER_CNTL	0x106C

#define mmBRUSH_OFFSET		0x108C
#define mmBRUSH_Y_X		0x1074

#define mmDP_BRUSH_BKGD_CLR	0x1078
#define mmDP_BRUSH_FRGD_CLR	0x107C

#define mmSRC2_WIDTH		0x1080
#define mmSRC2_HEIGHT		0x1084
#define mmSRC2_INC		0x1088

#define mmDST_LINE_START	0x1090
#define mmDST_LINE_END		0x1094

#define mmDEFAULT_PITCH_OFFSET	0x10A0
#define mmDEFAULT_SC_BOTTOM_RIGHT	0x10A8
#define mmDEFAULT2_SC_BOTTOM_RIGHT	0x10AC
#define mmREF1_PITCH_OFFSET	0x10B8
#define mmREF2_PITCH_OFFSET	0x10BC
#define mmREF3_PITCH_OFFSET	0x10C0
#define mmREF4_PITCH_OFFSET	0x10C4
#define mmREF5_PITCH_OFFSET	0x10C8
#define mmREF6_PITCH_OFFSET	0x10CC

#define mmSC_LEFT		0x1140
#define mmSC_RIGHT		0x1144
#define mmSC_TOP		0x1148
#define mmSC_BOTTOM		0x114C
#define mmSRC_SC_RIGHT		0x1154
#define mmSRC_SC_BOTTOM		0x115C

#define mmDST_WIDTH_X		0x1188
#define mmDST_HEIGHT_WIDTH_8	0x118C
#define mmSRC_X_Y		0x1190
#define mmDST_X_Y		0x1194
#define mmDST_WIDTH_HEIGHT	0x1198
#define mmDST_WIDTH_X_INCY	0x119C
#define mmDST_HEIGHT_Y		0x11A0

#define mmDP_CNTL		0x11C8
#define mmDP_CNTL_DST_DIR	0x11CC

#define mmSRC_OFFSET		0x11AC
#define mmSRC_PITCH		0x11B0

#define mmSC_TOP_LEFT		0x11BC
#define mmSC_BOTTOM_RIGHT	0x11C0
#define mmSRC_SC_BOTTOM_RIGHT	0x11C4

#define mmMVC_CNTL_START	0x11E0

#define mmGLOBAL_ALPHA		0x1210
#define mmFILTER_COEF		0x1214

#define mmE2_ARITHMETIC_CNTL	0x1220

#define mmCLR_CMP_CLR_SRC	0x1234
#define mmCLR_CMP_CLR_DST	0x1238
#define mmCLR_CMP_CNTL		0x1230
#define mmCLR_CMP_MSK		0x123C

#define mmDP_SRC_FRGD_CLR	0x1240
#define mmDP_SRC_BKGD_CLR	0x1244

#define mmDP_DATATYPE		0x12C4
#define mmDP_MIX		0x12C8
#define mmDP_WRITE_MSK		0x12CC

#define mmHOST_DATA0		0x13C0
#define mmHOST_DATA1		0x13C4
#define mmHOST_DATA2		0x13C8
#define mmHOST_DATA3		0x13CC
#define mmHOST_DATA4		0x13D0
#define mmHOST_DATA5		0x13D4
#define mmHOST_DATA6		0x13D8
#define mmHOST_DATA7		0x13DC
#define mmHOST_DATA_LAST	0x13E0

#define mmENG_CNTL		0x13E8
#define mmENG_PERF_CNT		0x13F0

#define mmDEBUG0		0x1280
#define mmDEBUG1		0x1284
#define mmDEBUG2		0x1288
#define mmDEBUG3		0x128C
#define mmDEBUG4		0x1290
#define mmDEBUG5		0x1294
#define mmDEBUG6		0x1298
#define mmDEBUG7		0x129C
#define mmDEBUG8		0x12A0
#define mmDEBUG9		0x12A4
#define mmDEBUG10		0x12A8
#define mmDEBUG11		0x12AC
#define mmDEBUG12		0x12B0
#define mmDEBUG13		0x12B4
#define mmDEBUG14		0x12B8
#define mmDEBUG15		0x12BC


typedef union {
	unsigned long val			: 32;
	struct _dst_offset_t {
		unsigned long dst_offset	: 24;
		unsigned long			: 8;
	} f;
} dst_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_pitch_t {
		unsigned long dst_pitch		: 14;
		unsigned long mc_dst_pitch_mul	: 2;
		unsigned long			: 16;
	} f;
} dst_pitch_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_pitch_offset_t {
		unsigned long dst_offset	: 20;
		unsigned long dst_pitch		: 10;
		unsigned long mc_dst_pitch_mul	: 2;
	} f;
} dst_pitch_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_x_t {
		unsigned long dst_x		: 14;
		unsigned long			: 18;
	} f;
} dst_x_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_y_t {
		unsigned long dst_y		: 14;
		unsigned long			: 18;
	} f;
} dst_y_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_x_y_t {
		unsigned long dst_y		: 14;
		unsigned long			: 2;
		unsigned long dst_x		: 14;
		unsigned long			: 2;
	} f;
} dst_x_y_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_y_x_t {
		unsigned long dst_x		: 14;
		unsigned long			: 2;
		unsigned long dst_y		: 14;
		unsigned long			: 2;
	} f;
} dst_y_x_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_width_t {
		unsigned long dst_width_b0	: 8;
		unsigned long dst_width_b1	: 6;
		unsigned long			: 18;
	} f;
} dst_width_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_height_t {
		unsigned long dst_height	: 14;
		unsigned long			: 18;
	} f;
} dst_height_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_width_height_t {
		unsigned long dst_height	: 14;
		unsigned long			: 2;
		unsigned long dst_width		: 14;
		unsigned long			: 2;
	} f;
} dst_width_height_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_height_width_t {
		unsigned long dst_width		: 14;
		unsigned long			: 2;
		unsigned long dst_height	: 14;
		unsigned long			: 2;
	} f;
} dst_height_width_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_height_width_8_t {
		unsigned long			: 16;
		unsigned long dst_width_b0	: 8;
		unsigned long dst_height	: 8;
	} f;
} dst_height_width_8_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_height_y_t {
		unsigned long dst_y		: 14;
		unsigned long			: 2;
		unsigned long dst_height	: 14;
		unsigned long			: 2;
	} f;
} dst_height_y_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_width_x_t {
		unsigned long dst_x		: 14;
		unsigned long			: 2;
		unsigned long dst_width		: 14;
		unsigned long			: 2;
	} f;
} dst_width_x_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_width_x_incy_t {
		unsigned long dst_x		: 14;
		unsigned long			: 2;
		unsigned long dst_width		: 14;
		unsigned long			: 2;
	} f;
} dst_width_x_incy_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_line_start_t {
		unsigned long dst_start_x	: 14;
		unsigned long			: 2;
		unsigned long dst_start_y	: 14;
		unsigned long			: 2;
	} f;
} dst_line_start_u;


typedef union {
	unsigned long val			: 32;
	struct _dst_line_end_t {
		unsigned long dst_end_x		: 14;
		unsigned long			: 2;
		unsigned long dst_end_y		: 14;
		unsigned long			: 2;
	} f;
} dst_line_end_u;


typedef union {
	unsigned long val			: 32;
	struct _brush_offset_t {
		unsigned long brush_offset	: 24;
		unsigned long			: 8;
	} f;
} brush_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _brush_y_x_t {
		unsigned long brush_x		: 5;
		unsigned long			: 3;
		unsigned long brush_y		: 3;
		unsigned long			: 21;
	} f;
} brush_y_x_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_brush_frgd_clr_t {
		unsigned long dp_brush_frgd_clr	: 32;
	} f;
} dp_brush_frgd_clr_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_brush_bkgd_clr_t {
		unsigned long dp_brush_bkgd_clr	: 32;
	} f;
} dp_brush_bkgd_clr_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_offset_t {
		unsigned long src2_offset	: 24;
		unsigned long			: 8;
	} f;
} src2_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_pitch_t {
		unsigned long src2_pitch	: 14;
		unsigned long src2_pitch_mul	: 2;
		unsigned long			: 16;
	} f;
} src2_pitch_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_pitch_offset_t {
		unsigned long src2_offset	: 20;
		unsigned long			: 2;
		unsigned long src2_pitch	: 8;
		unsigned long src2_pitch_mul	: 2;
	} f;
} src2_pitch_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_x_t {
		unsigned long src_x		: 14;
		unsigned long			: 18;
	} f;
} src2_x_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_y_t {
		unsigned long src_y		: 14;
		unsigned long			: 18;
	} f;
} src2_y_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_x_y_t {
		unsigned long src_y		: 14;
		unsigned long			: 2;
		unsigned long src_x		: 14;
		unsigned long			: 2;
	} f;
} src2_x_y_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_width_t {
		unsigned long src2_width	: 14;
		unsigned long			: 18;
	} f;
} src2_width_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_height_t {
		unsigned long src2_height	: 14;
		unsigned long			: 18;
	} f;
} src2_height_u;


typedef union {
	unsigned long val			: 32;
	struct _src2_inc_t {
		unsigned long src2_xinc		: 6;
		unsigned long			: 2;
		unsigned long src2_yinc		: 6;
		unsigned long			: 18;
	} f;
} src2_inc_u;


typedef union {
	unsigned long val			: 32;
	struct _src_offset_t {
		unsigned long src_offset	: 24;
		unsigned long			: 8;
	} f;
} src_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _src_pitch_t {
		unsigned long src_pitch		: 14;
		unsigned long src_pitch_mul	: 2;
		unsigned long			: 16;
	} f;
} src_pitch_u;


typedef union {
	unsigned long val			: 32;
	struct _src_pitch_offset_t {
		unsigned long src_offset	: 20;
		unsigned long src_pitch		: 10;
		unsigned long src_pitch_mul	: 2;
	} f;
} src_pitch_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _src_x_t {
		unsigned long src_x		: 14;
		unsigned long			: 18;
	} f;
} src_x_u;


typedef union {
	unsigned long val			: 32;
	struct _src_y_t {
		unsigned long src_y		: 14;
		unsigned long			: 18;
	} f;
} src_y_u;


typedef union {
	unsigned long val			: 32;
	struct _src_x_y_t {
		unsigned long src_y		: 14;
		unsigned long			: 2;
		unsigned long src_x		: 14;
		unsigned long			: 2;
	} f;
} src_x_y_u;


typedef union {
	unsigned long val			: 32;
	struct _src_y_x_t {
		unsigned long src_x		: 14;
		unsigned long			: 2;
		unsigned long src_y		: 14;
		unsigned long			: 2;
	} f;
} src_y_x_u;


typedef union {
	unsigned long val			: 32;
	struct _src_width_t {
		unsigned long src_width		: 14;
		unsigned long			: 18;
	} f;
} src_width_u;


typedef union {
	unsigned long val			: 32;
	struct _src_height_t {
		unsigned long src_height	: 14;
		unsigned long			: 18;
	} f;
} src_height_u;


typedef union {
	unsigned long val			: 32;
	struct _src_inc_t {
		unsigned long src_xinc		: 6;
		unsigned long			: 2;
		unsigned long src_yinc		: 6;
		unsigned long			: 18;
	} f;
} src_inc_u;


typedef union {
	unsigned long val			: 32;
	struct _host_data_t {
		unsigned long host_data		: 32;
	} f;
} host_data_u;


typedef union {
	unsigned long val			: 32;
	struct _host_data_last_t {
		unsigned long host_data_last	: 32;
	} f;
} host_data_last_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_src_frgd_clr_t {
		unsigned long dp_src_frgd_clr	: 32;
	} f;
} dp_src_frgd_clr_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_src_bkgd_clr_t {
		unsigned long dp_src_bkgd_clr	: 32;
	} f;
} dp_src_bkgd_clr_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_left_t {
		unsigned long sc_left		: 14;
		unsigned long			: 18;
	} f;
} sc_left_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_right_t {
		unsigned long sc_right		: 14;
		unsigned long			: 18;
	} f;
} sc_right_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_top_t {
		unsigned long sc_top		: 14;
		unsigned long			: 18;
	} f;
} sc_top_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_bottom_t {
		unsigned long sc_bottom		: 14;
		unsigned long			: 18;
	} f;
} sc_bottom_u;


typedef union {
	unsigned long val			: 32;
	struct _src_sc_right_t {
		unsigned long sc_right		: 14;
		unsigned long			: 18;
	} f;
} src_sc_right_u;


typedef union {
	unsigned long val			: 32;
	struct _src_sc_bottom_t {
		unsigned long sc_bottom		: 14;
		unsigned long			: 18;
	} f;
} src_sc_bottom_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_cntl_t {
		unsigned long dst_x_dir		: 1;
		unsigned long dst_y_dir		: 1;
		unsigned long src_x_dir		: 1;
		unsigned long src_y_dir		: 1;
		unsigned long dst_major_x	: 1;
		unsigned long src_major_x	: 1;
		unsigned long			: 26;
	} f;
} dp_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_cntl_dst_dir_t {
		unsigned long			: 15;
		unsigned long dst_y_dir		: 1;
		unsigned long			: 15;
		unsigned long dst_x_dir		: 1;
	} f;
} dp_cntl_dst_dir_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_datatype_t {
		unsigned long dp_dst_datatype	: 4;
		unsigned long			: 4;
		unsigned long dp_brush_datatype	: 4;
		unsigned long dp_src2_type	: 1;
		unsigned long dp_src2_datatype	: 3;
		unsigned long dp_src_datatype	: 3;
		unsigned long			: 11;
		unsigned long dp_byte_pix_order	: 1;
		unsigned long			: 1;
	} f;
} dp_datatype_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_mix_t {
		unsigned long			: 8;
		unsigned long dp_src_source	: 3;
		unsigned long dp_src2_source	: 3;
		unsigned long			: 2;
		unsigned long dp_rop3		: 8;
		unsigned long dp_op		: 1;
		unsigned long			: 7;
	} f;
} dp_mix_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_write_msk_t {
		unsigned long dp_write_msk	: 32;
	} f;
} dp_write_msk_u;


typedef union {
	unsigned long val			: 32;
	struct _clr_cmp_clr_src_t {
		unsigned long clr_cmp_clr_src	: 32;
	} f;
} clr_cmp_clr_src_u;


typedef union {
	unsigned long val			: 32;
	struct _clr_cmp_clr_dst_t {
		unsigned long clr_cmp_clr_dst	: 32;
	} f;
} clr_cmp_clr_dst_u;


typedef union {
	unsigned long val			: 32;
	struct _clr_cmp_cntl_t {
		unsigned long clr_cmp_fcn_src	: 3;
		unsigned long			: 5;
		unsigned long clr_cmp_fcn_dst	: 3;
		unsigned long			: 13;
		unsigned long clr_cmp_src	: 2;
		unsigned long			: 6;
	} f;
} clr_cmp_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _clr_cmp_msk_t {
		unsigned long clr_cmp_msk	: 32;
	} f;
} clr_cmp_msk_u;


typedef union {
	unsigned long val			: 32;
	struct _default_pitch_offset_t {
		unsigned long default_offset	: 20;
		unsigned long default_pitch	: 10;
		unsigned long			: 2;
	} f;
} default_pitch_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _default_sc_bottom_right_t {
		unsigned long default_sc_right	: 14;
		unsigned long			: 2;
		unsigned long default_sc_bottom	: 14;
		unsigned long			: 2;
	} f;
} default_sc_bottom_right_u;


typedef union {
	unsigned long val			: 32;
	struct _default2_sc_bottom_right_t {
		unsigned long default_sc_right	: 14;
		unsigned long			: 2;
		unsigned long default_sc_bottom	: 14;
		unsigned long			: 2;
	} f;
} default2_sc_bottom_right_u;


typedef union {
	unsigned long val			: 32;
	struct _ref_pitch_offset_t {
		unsigned long offset		: 20;
		unsigned long			: 2;
		unsigned long pitch		: 8;
		unsigned long			: 2;
	} f;
} ref_pitch_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _dp_gui_master_cntl_t {
		unsigned long gmc_src_pitch_offset_cntl	: 1;
		unsigned long gmc_dst_pitch_offset_cntl	: 1;
		unsigned long gmc_src_clipping		: 1;
		unsigned long gmc_dst_clipping		: 1;
		unsigned long gmc_brush_datatype	: 4;
		unsigned long gmc_dst_datatype		: 4;
		unsigned long gmc_src_datatype		: 3;
		unsigned long gmc_byte_pix_order	: 1;
		unsigned long gmc_default_sel		: 1;
		unsigned long gmc_rop3			: 8;
		unsigned long gmc_dp_src_source		: 3;
		unsigned long gmc_clr_cmp_fcn_dis	: 1;
		unsigned long				: 1;
		unsigned long gmc_wr_msk_dis		: 1;
		unsigned long gmc_dp_op			: 1;
	} f;
} dp_gui_master_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_top_left_t {
		unsigned long sc_left		: 14;
		unsigned long			: 2;
		unsigned long sc_top		: 14;
		unsigned long			: 2;
	} f;
} sc_top_left_u;


typedef union {
	unsigned long val			: 32;
	struct _sc_bottom_right_t {
		unsigned long sc_right		: 14;
		unsigned long			: 2;
		unsigned long sc_bottom		: 14;
		unsigned long			: 2;
	} f;
} sc_bottom_right_u;


typedef union {
	unsigned long val			: 32;
	struct _src_sc_bottom_right_t {
		unsigned long sc_right		: 14;
		unsigned long			: 2;
		unsigned long sc_bottom		: 14;
		unsigned long			: 2;
	} f;
} src_sc_bottom_right_u;


typedef union {
	unsigned long val			: 32;
	struct _global_alpha_t {
		unsigned long alpha_r		: 8;
		unsigned long alpha_g		: 8;
		unsigned long alpha_b		: 8;
		unsigned long alpha_a		: 8;
	} f;
} global_alpha_u;


typedef union {
	unsigned long val			: 32;
	struct _filter_coef_t {
		unsigned long c_4		: 4;
		unsigned long c_3		: 4;
		unsigned long c_2		: 4;
		unsigned long c_1		: 4;
		unsigned long c1		: 4;
		unsigned long c2		: 4;
		unsigned long c3		: 4;
		unsigned long c4		: 4;
	} f;
} filter_coef_u;


typedef union {
	unsigned long val			: 32;
	struct _mvc_cntl_start_t {
		unsigned long mc_cntl_src_1_index	: 4;
		unsigned long mc_cntl_dst_offset	: 20;
		unsigned long mc_dst_pitch_mul		: 2;
		unsigned long mc_cntl_src_2_index	: 3;
		unsigned long mc_cntl_width_height_sel	: 3;
	} f;
} mvc_cntl_start_u;


typedef union {
	unsigned long val			: 32;
	struct _e2_arithmetic_cntl_t {
		unsigned long opcode		: 5;
		unsigned long shiftright	: 4;
		unsigned long clamp		: 1;
		unsigned long rounding		: 2;
		unsigned long filter_n		: 3;
		unsigned long			: 1;
		unsigned long srcblend_inv	: 1;
		unsigned long srcblend		: 4;
		unsigned long			: 3;
		unsigned long dstblend_inv	: 1;
		unsigned long dstblend		: 4;
		unsigned long dst_signed	: 1;
		unsigned long autoinc		: 1;
		unsigned long			: 1;
	} f;
} e2_arithmetic_cntl_u;

/* For debug registers 0 through 3 */
typedef union {
	unsigned long val			: 32;
	struct {
		unsigned long r			: 8;
		unsigned long			: 8;
		unsigned long rw		: 8;
		unsigned long			: 8;
	} f;
} debug0_3_u;

/* For debug registers 4 through 15 */
typedef union {
	unsigned long val			: 32;
	struct {
		unsigned long			: 32;
	} f;
} debug_u;


typedef union {
	unsigned long val			: 32;
	struct _eng_cntl_t {
		unsigned long erc_reg_rd_ws		: 1;
		unsigned long erc_reg_wr_ws		: 1;
		unsigned long erc_idle_reg_wr		: 1;
		unsigned long dis_engine_triggers	: 1;
		unsigned long dis_rop_src_uses_dst_w_h	: 1;
		unsigned long dis_src_uses_dst_dirmaj	: 1;
		unsigned long				: 6;
		unsigned long force_3dclk_when_2dclk	: 1;
		unsigned long				: 19;
	} f;
} eng_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _eng_perf_cnt_t {
		unsigned long perf_cnt		: 20;
		unsigned long perf_sel		: 4;
		unsigned long perf_en		: 1;
		unsigned long			: 3;
		unsigned long perf_clr		: 1;
		unsigned long			: 3;
	} f;
} eng_perf_cnt_u;


// DP_GUI_MASTER_CNTL.GMC_Brush_DataType
// DP_DATATYPE.Brush_DataType
#define DP_BRUSH_8x8MONOOPA		0   //8x8 mono pattern (expanded to frgd, bkgd)
#define DP_BRUSH_8x8MONOTRA		1   //8x8 mono pattern (expanded to frgd, leave_alone)
#define DP_PEN_32x1MONOOPA		6   //32x1 mono pattern (expanded to frgd, bkgd)
#define DP_PEN_32x1MONOTRA		7   //32x1 mono pattern (expanded to frgd, leave_alone)
#define DP_BRUSH_8x8COLOR		10  //8x8 color pattern
#define DP_BRUSH_SOLIDCOLOR		13  //solid color pattern (frgd)
#define DP_BRUSH_NONE			15	//no brush used

#define SIZE_BRUSH_8x8MONO		2
#define SIZE_PEN_32x1MONO		1
#define SIZE_BRUSH_8x8COLOR_8		16
#define SIZE_BRUSH_8x8COLOR_16		32
#define MAX_BRUSH_SIZE				SIZE_BRUSH_8x8COLOR_16

// DP_GUI_MASTER_CNTL.GMC_Dst_DataType
// DP_DATATYPE.Dp_Dst_DataType
#define DP_DST_8BPP			2   // 8 bpp grey scale
#define DP_DST_16BPP_1555		3   //16 bpp aRGB 1555
#define DP_DST_16BPP_444		5   //16 bpp aRGB 4444

// DP_GUI_MASTER_CNTL.GMC_Src_DataType
// DP_DATATYPE.Dp_Src_DataType
#define DP_SRC_1BPP_OPA			0   //mono (expanded to frgd, bkgd)
#define DP_SRC_1BPP_TRA			1   //mono (expanded to frgd, leave_alone)
#define DP_SRC_COLOR_SAME_AS_DST	3   //color (same as DST)
#define	DP_SRC_SOLID_COLOR_BLT		4   //solid color for Blt (use frgd)
#define	DP_SRC_4BPP			5   //4 bpp
#define	DP_SRC_12BPP_PACKED		6   //12 bpp packed

// DP_GUI_MASTER_CNTL.GMC_Byte_Pix_Order
// DP_DATATYPE.Dp_Byte_Pix_Order
#define DP_PIX_ORDER_MSB2LSB		0   //monochrome pixel order from MSBit to LSBit
#define DP_PIX_ORDER_LSB2MSB		1   //monochrome pixel order from LSBit to MSBit

// DP_GUI_MASTER_CNTL.GMC_Dp_Src_Source
#define DP_SRC_MEM_LINEAR		1   //loaded from memory (linear trajectory)
#define DP_SRC_MEM_RECTANGULAR		2   //loaded from memory (rectangular trajectory)
#define DP_SRC_HOSTDATA_BIT		3   //loaded from hostdata (linear trajectory)
#define DP_SRC_HOSTDATA_BYTE		4   //loaded from hostdata (linear trajectory & byte-aligned)

// DP_GUI_MASTER_CNTL.GMC_Dp_Op
#define	DP_OP_ROP			0
#define	DP_OP_ARITHMETIC		1

// E2_ARITHMETIC_CNTL.opcode
#define	E2_OPC_GLBALP_ADD_SRC2		0
#define	E2_OPC_GLBALP_SUB_SRC2		1
#define	E2_OPC_SRC1_ADD_SRC2		2
#define	E2_OPC_SRC1_SUB_SRC2		3
#define	E2_OPC_DST_SADDBLEND_SRC2	4
#define	E2_OPC_DST_CADDBLEND_SRC2	5
#define	E2_OPC_DST_CSUBBLEND_SRC2	6
#define	E2_OPC_LF_SRC2			7
#define	E2_OPC_SCALE_SRC2		8
#define	E2_OPC_STRETCH_SRC2		9
#define	E2_OPC_SRC1_4BPPCPYWEXP		10
#define	E2_OPC_MC1			11
#define	E2_OPC_MC2			12
#define E2_OPC_MC1_IDCT			13
#define	E2_OPC_MC2_IDCT			14
#define	E2_OPC_IDCT_ONLY_IFRAME		15

// E2_ARITHMETIC_CNTL.clamp
#define	E2_CLAMP_OFF			0
#define	E2_CLAMP_ON			1

// E2_ARITHMETIC_CNTL.rounding
#define	E2_ROUNDING_TRUNCATE		0
#define	E2_ROUNDING_TO_INFINITY		1

// E2_ARITHMETIC_CNTL.srcblend
#define	E2_SRCBLEND_GLOBALALPHA		0
#define	E2_SRCBLEND_ZERO		1
#define	E2_SRCBLEND_SRC2ALPHA		2
#define	E2_SRCBLEND_DSTALPHA		3
#define	E2_SRCBLEND_ALPHA1PLANE		4

// E2_ARITHMETIC_CNTL.destblend
#define	E2_DSTBLEND_GLOBALALPHA		0
#define	E2_DSTBLEND_ZERO		1
#define	E2_DSTBLEND_SRC2ALPHA		2
#define	E2_DSTBLEND_DSTALPHA		3
#define	E2_DSTBLEND_ALPHA1PLANE		4

#endif /* _w100_accel_h_ */
