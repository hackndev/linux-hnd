/*
 * linux/drivers/video/imageon/w100_display.h
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
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */


#ifndef _w100_display_h_
#define _w100_display_h_

#define mmLCD_FORMAT		0x0410
#define mmGRAPHIC_CTRL		0x0414
#define mmGRAPHIC_OFFSET	0x0418
#define mmGRAPHIC_PITCH		0x041C
#define mmCRTC_TOTAL		0x0420

#define mmACTIVE_H_DISP		0x0424
#define mmACTIVE_V_DISP		0x0428
#define mmGRAPHIC_H_DISP	0x042C
#define mmGRAPHIC_V_DISP	0x0430

#define mmVIDEO_CTRL		0x0434
#define mmGRAPHIC_KEY		0x0438

#define mmVIDEO_Y_OFFSET	0x043C
#define mmVIDEO_Y_PITCH		0x0440
#define mmVIDEO_U_OFFSET	0x0444
#define mmVIDEO_U_PITCH		0x0448
#define mmVIDEO_V_OFFSET	0x044C
#define mmVIDEO_V_PITCH		0x0450

#define mmVIDEO_H_POS		0x0454
#define mmVIDEO_V_POS		0x0458

#define mmBRIGHTNESS_CNTL	0x045C
#define mmDISP_INT_CNTL		0x0488

#define mmCRTC_SS		0x048C
#define mmCRTC_LS		0x0490
#define mmCRTC_REV		0x0494
#define mmCRTC_DCLK		0x049C
#define mmCRTC_GS		0x04A0
#define mmCRTC_VPOS_GS		0x04A4
#define mmCRTC_GCLK		0x04A8
#define mmCRTC_GOE		0x04AC

#define mmCRTC_FRAME		0x04B0
#define mmCRTC_FRAME_VPOS	0x04B4

#define mmGPIO_DATA		0x04B8
#define mmGPIO_CNTL1		0x04BC
#define mmGPIO_CNTL2		0x04C0

#define mmLCDD_CNTL1		0x04C4
#define mmLCDD_CNTL2		0x04C8
#define mmGENLCD_CNTL1		0x04CC
#define mmGENLCD_CNTL2		0x04D0

#define mmDISP_DEBUG		0x04D4
#define mmDISP_DB_BUF_CNTL	0x04D8
#define mmDISP_CRC_SIG		0x04DC

#define mmCRTC_DEFAULT_COUNT	0x04E0
#define mmLCD_BACKGROUND_COLOR	0x04E4

#define mmCRTC_PS2		0x04E8
#define mmCRTC_PS2_VPOS		0x04EC
#define mmCRTC_PS1_ACTIVE	0x04F0
#define mmCRTC_PS1_NACTIVE	0x04F4
#define mmCRTC_GCLK_EXT		0x04F8
#define mmCRTC_ALW		0x04FC
#define mmCRTC_ALW_VPOS		0x0500
#define mmCRTC_PSK		0x0504
#define mmCRTC_PSK_HPOS		0x0508
#define mmCRTC_CV4_START	0x050C
#define mmCRTC_CV4_END		0x0510
#define mmCRTC_CV4_HPOS		0x0514
#define mmCRTC_ECK		0x051C

#define mmREFRESH_CNTL		0x0520
#define mmGENLCD_CNTL3		0x0524

#define mmGPIO_DATA2		0x0528
#define mmGPIO_CNTL3		0x052C
#define mmGPIO_CNTL4		0x0530

#define mmCHIP_STRAP		0x0534
#define mmDISP_DEBUG2		0x0538
#define mmDEBUG_BUS_CNTL	0x053C
#define mmGAMMA_VALUE1		0x0540
#define mmGAMMA_VALUE2		0x0544
#define mmGAMMA_SLOPE		0x0548
#define mmGEN_STATUS		0x054C
#define mmHW_INT		0x0550


typedef union {
	unsigned long val 			: 32;
	struct _lcd_format_t {
		unsigned long lcd_type		: 4;
		unsigned long color_to_mono	: 1;
		unsigned long data_inv		: 1;
		unsigned long stn_fm		: 2;
		unsigned long tft_fm		: 2;
		unsigned long scan_lr_en	: 1;
		unsigned long scan_ud_en	: 1;
		unsigned long pol_inv		: 1;
		unsigned long rst_fm		: 1;
		unsigned long yuv_to_rgb	: 1;
		unsigned long hr_tft		: 1;
		unsigned long ulc_panel		: 1;
		unsigned long			: 15;
	} f;
} lcd_format_u;


typedef union {
	unsigned long val			: 32;
	struct _graphic_ctrl_t {
		unsigned long color_depth	: 3;
		unsigned long portrait_mode	: 2;
		unsigned long low_power_on	: 1;
		unsigned long req_freq		: 4;
		unsigned long en_crtc		: 1;
		unsigned long en_graphic_req	: 1;
		unsigned long en_graphic_crtc	: 1;
		unsigned long total_req_graphic	: 9;
		unsigned long lcd_pclk_on	: 1;
		unsigned long lcd_sclk_on	: 1;
		unsigned long pclk_running	: 1;
		unsigned long sclk_running	: 1;
		unsigned long			: 6;
	} f;
} graphic_ctrl_u;


typedef union {
	unsigned long val			: 32;
	struct _graphic_offset_t {
		unsigned long graphic_offset	: 24;
		unsigned long			: 8;
	} f;
} graphic_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _graphic_pitch_t {
		unsigned long graphic_pitch	: 11;
		unsigned long			: 21;
	} f;
} graphic_pitch_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_total_t {
		unsigned long crtc_h_total	: 10;
		unsigned long			: 6;
		unsigned long crtc_v_total	: 10;
		unsigned long			: 6;
	} f;
} crtc_total_u;


/* For active / graphic and h or v */
typedef union {
	unsigned long val			: 32;
	struct _disp_t {
		unsigned long start		: 10;
		unsigned long			: 6;
		unsigned long end		: 10;
		unsigned long			: 6;
	} f;
} disp_u;



typedef union {
	unsigned long val				: 32;
	struct _video_ctrl_t {
		unsigned long video_mode		: 1;
		unsigned long keyer_en			: 1;
		unsigned long en_video_req		: 1;
		unsigned long en_graphic_req_video	: 1;
		unsigned long en_video_crtc		: 1;
		unsigned long video_hor_exp		: 2;
		unsigned long video_ver_exp		: 2;
		unsigned long uv_combine		: 1;
		unsigned long total_req_video		: 9;
		unsigned long video_ch_sel		: 1;
		unsigned long video_portrait		: 2;
		unsigned long yuv2rgb_en		: 1;
		unsigned long yuv2rgb_option		: 1;
		unsigned long video_inv_hor		: 1;
		unsigned long video_inv_ver		: 1;
		unsigned long gamma_sel			: 2;
		unsigned long dis_limit			: 1;
		unsigned long en_uv_hblend		: 1;
		unsigned long rgb_gamma_sel		: 2;
	} f;
} video_ctrl_u;


typedef union {
	unsigned long val			: 32;
	struct _graphic_key_t {
		unsigned long keyer_color	: 16;
		unsigned long keyer_mask	: 16;
	} f;
} graphic_key_u;


typedef union {
	unsigned long val			: 32;
	struct _video_yuv_offset_t {
		unsigned long offset		: 24;
		unsigned long			: 8;
	} f;
} video_yuv_offset_u;


typedef union {
	unsigned long val			: 32;
	struct _video_yuv_pitch_t {
		unsigned long pitch		: 11;
		unsigned long			: 21;
	} f;
} video_yuv_pitch_u;


typedef union {
	unsigned long val			: 32;
	struct _video_h_pos_t {
		unsigned long video_start	: 10;
		unsigned long			: 6;
		unsigned long video_end		: 10;
		unsigned long			: 6;
	} f;
} video_pos_u;


typedef union {
	unsigned long val			: 32;
	struct _brightness_cntl_t {
		unsigned long brightness	: 7;
		unsigned long			: 25;
	} f;
} brightness_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_int_cntl_t {
		unsigned long vline_int_pos	: 10;
		unsigned long			: 6;
		unsigned long hpos_int_pos	: 10;
		unsigned long			: 4;
		unsigned long vblank_int_pol	: 1;
		unsigned long frame_int_pol	: 1;
	} f;
} disp_int_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ss_t {
		unsigned long ss_start		: 10;
		unsigned long			: 6;
		unsigned long ss_end		: 10;
		unsigned long			: 2;
		unsigned long ss_align		: 1;
		unsigned long ss_pol		: 1;
		unsigned long ss_run_mode	: 1;
		unsigned long ss_en		: 1;
	} f;
} crtc_ss_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ls_t {
		unsigned long ls_start		: 10;
		unsigned long			: 6;
		unsigned long ls_end		: 10;
		unsigned long			: 2;
		unsigned long ls_align		: 1;
		unsigned long ls_pol		: 1;
		unsigned long ls_run_mode	: 1;
		unsigned long ls_en		: 1;
	} f;
} crtc_ls_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_rev_t {
		unsigned long rev_pos		: 10;
		unsigned long			: 6;
		unsigned long rev_align		: 1;
		unsigned long rev_freq_nref	: 5;
		unsigned long rev_en		: 1;
		unsigned long			: 9;
	} f;
} crtc_rev_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_dclk_t {
		unsigned long dclk_start	: 10;
		unsigned long			: 6;
		unsigned long dclk_end		: 10;
		unsigned long			: 1;
		unsigned long dclk_run_mode	: 2;
		unsigned long dclk_pol		: 1;
		unsigned long dclk_align	: 1;
		unsigned long dclk_en		: 1;
	} f;
} crtc_dclk_u;


/* Common to a lot of the crtc stuff */
typedef union {
	unsigned long val			: 32;
	struct _seape_t {
		unsigned long gs_start		: 10;
		unsigned long			: 6;
		unsigned long gs_end		: 10;
		unsigned long			: 3;
		unsigned long gs_align		: 1;
		unsigned long gs_pol		: 1;
		unsigned long gs_en		: 1;
	} f;
} seape_u;

/* These use the common structure above */
typedef seape_u crtc_gs_u;
typedef seape_u crtc_gclk_u;
typedef seape_u crtc_goe_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_vpos_gs_t {
		unsigned long gs_vpos_start	: 10;
		unsigned long			: 6;
		unsigned long gs_vpos_end	: 10;
		unsigned long			: 6;
	} f;
} crtc_vpos_gs_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_frame_t {
		unsigned long crtc_fr_start	: 10;
		unsigned long			: 6;
		unsigned long crtc_fr_end	: 10;
		unsigned long			: 4;
		unsigned long crtc_frame_en	: 1;
		unsigned long crtc_frame_align	: 1;
	} f;
} crtc_frame_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_frame_vpos_t {
		unsigned long crtc_fr_vpos	: 10;
		unsigned long			: 22;
	} f;
} crtc_frame_vpos_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_data_t {
		unsigned long gio_out		: 16;
		unsigned long gio_in		: 16;
	} f;
} gpio_data_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_cntl1_t {
		unsigned long gio_pd		: 16;
		unsigned long gio_schmen	: 16;
	} f;
} gpio_cntl1_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_cntl2_t {
		unsigned long gio_oe		: 16;
		unsigned long gio_srp		: 1;
		unsigned long gio_srn		: 1;
		unsigned long gio_sp		: 4;
		unsigned long gio_sn		: 4;
		unsigned long			: 6;
	} f;
} gpio_cntl2_u;


typedef union {
	unsigned long val			: 32;
	struct _lcdd_cntl1_t {
		unsigned long lcdd_pd		: 18;
		unsigned long lcdd_srp		: 1;
		unsigned long lcdd_srn		: 1;
		unsigned long lcdd_sp		: 4;
		unsigned long lcdd_sn		: 4;
		unsigned long lcdd_align	: 1;
		unsigned long			: 3;
	} f;
} lcdd_cntl1_u;


typedef union {
	unsigned long val			: 32;
	struct _lcdd_cntl2_t {
		unsigned long lcdd_oe		: 18;
		unsigned long			: 14;
	} f;
} lcdd_cntl2_u;


typedef union {
	unsigned long val			: 32;
	struct _genlcd_cntl1_t {
		unsigned long dclk_oe		: 1;
		unsigned long dclk_pd		: 1;
		unsigned long dclk_srp		: 1;
		unsigned long dclk_srn		: 1;
		unsigned long dclk_sp		: 4;
		unsigned long dclk_sn		: 4;
		unsigned long ss_oe		: 1;
		unsigned long ss_pd		: 1;
		unsigned long ls_oe		: 1;
		unsigned long ls_pd		: 1;
		unsigned long gs_oe		: 1;
		unsigned long gs_pd		: 1;
		unsigned long goe_oe		: 1;
		unsigned long goe_pd		: 1;
		unsigned long rev_oe		: 1;
		unsigned long rev_pd		: 1;
		unsigned long frame_oe		: 1;
		unsigned long frame_pd		: 1;
		unsigned long	: 8;
	} f;
} genlcd_cntl1_u;


typedef union {
	unsigned long val			: 32;
	struct _genlcd_cntl2_t {
		unsigned long gclk_oe		: 1;
		unsigned long gclk_pd		: 1;
		unsigned long gclk_srp		: 1;
		unsigned long gclk_srn		: 1;
		unsigned long gclk_sp		: 4;
		unsigned long gclk_sn		: 4;
		unsigned long genlcd_srp	: 1;
		unsigned long genlcd_srn	: 1;
		unsigned long genlcd_sp		: 4;
		unsigned long genlcd_sn		: 4;
		unsigned long	: 10;
	} f;
} genlcd_cntl2_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_debug_t {
		unsigned long disp_debug	: 32;
	} f;
} disp_debug_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_db_buf_cntl_rd_t {
		unsigned long en_db_buf		: 1;
		unsigned long update_db_buf_done: 1;
		unsigned long db_buf_cntl	: 6;
		unsigned long			: 24;
	} f;
} disp_db_buf_cntl_rd_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_db_buf_cntl_wr_t {
		unsigned long en_db_buf		: 1;
		unsigned long update_db_buf	: 1;
		unsigned long db_buf_cntl	: 6;
		unsigned long			: 24;
	} f;
} disp_db_buf_cntl_wr_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_crc_sig_t {
		unsigned long crc_sig_r		: 6;
		unsigned long crc_sig_g		: 6;
		unsigned long crc_sig_b		: 6;
		unsigned long crc_cont_en	: 1;
		unsigned long crc_en		: 1;
		unsigned long crc_mask_en	: 1;
		unsigned long crc_sig_cntl	: 6;
		unsigned long			: 5;
	} f;
} disp_crc_sig_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_default_count_t {
		unsigned long crtc_hcount_def	: 10;
		unsigned long			: 6;
		unsigned long crtc_vcount_def	: 10;
		unsigned long			: 6;
	} f;
} crtc_default_count_u;


typedef union {
	unsigned long val			: 32;
	struct _lcd_background_color_t {
		unsigned long lcd_bg_red	: 8;
		unsigned long lcd_bg_green	: 8;
		unsigned long lcd_bg_blue	: 8;
		unsigned long			: 8;
	} f;
} lcd_background_color_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ps2_t {
		unsigned long ps2_start		: 10;
		unsigned long			: 6;
		unsigned long ps2_end		: 10;
		unsigned long			: 4;
		unsigned long ps2_pol		: 1;
		unsigned long ps2_en		: 1;
	} f;
} crtc_ps2_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ps2_vpos_t {
		unsigned long ps2_vpos_start	: 10;
		unsigned long			: 6;
		unsigned long ps2_vpos_end	: 10;
		unsigned long			: 6;
	} f;
} crtc_ps2_vpos_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ps1_active_t {
		unsigned long ps1_h_start	: 10;
		unsigned long			: 6;
		unsigned long ps1_h_end		: 10;
		unsigned long			: 3;
		unsigned long ps1_pol		: 1;
		unsigned long ps1_en		: 1;
		unsigned long ps1_use_nactive	: 1;
	} f;
} crtc_ps1_active_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_ps1_nactive_t {
		unsigned long ps1_h_start_na	: 10;
		unsigned long			: 6;
		unsigned long ps1_h_end_na	: 10;
		unsigned long			: 5;
		unsigned long ps1_en_na		: 1;
	} f;
} crtc_ps1_nactive_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_gclk_ext_t {
		unsigned long gclk_alter_start	: 10;
		unsigned long			: 6;
		unsigned long gclk_alter_width	: 2;
		unsigned long gclk_en_alter	: 1;
		unsigned long gclk_db_width	: 2;
		unsigned long			: 11;
	} f;
} crtc_gclk_ext_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_alw_t {
		unsigned long alw_hstart	: 10;
		unsigned long			: 6;
		unsigned long alw_hend		: 10;
		unsigned long			: 4;
		unsigned long alw_delay		: 1;
		unsigned long alw_en		: 1;
	} f;
} crtc_alw_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_alw_vpos_t {
		unsigned long alw_vstart	: 10;
		unsigned long			: 6;
		unsigned long alw_vend		: 10;
		unsigned long			: 6;
	} f;
} crtc_alw_vpos_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_psk_t {
		unsigned long psk_vstart	: 10;
		unsigned long			: 6;
		unsigned long psk_vend		: 10;
		unsigned long			: 4;
		unsigned long psk_pol		: 1;
		unsigned long psk_en		: 1;
	} f;
} crtc_psk_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_psk_hpos_t {
		unsigned long psk_hstart	: 10;
		unsigned long			: 6;
		unsigned long psk_hend		: 10;
		unsigned long			: 6;
	} f;
} crtc_psk_hpos_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_cv4_start_t {
		unsigned long cv4_vstart	: 10;
		unsigned long			: 20;
		unsigned long cv4_pol		: 1;
		unsigned long cv4_en		: 1;
	} f;
} crtc_cv4_start_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_cv4_end_t {
		unsigned long cv4_vend1		: 10;
		unsigned long			: 6;
		unsigned long cv4_vend2		: 10;
		unsigned long			: 6;
	} f;
} crtc_cv4_end_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_cv4_hpos_t {
		unsigned long cv4_hstart	: 10;
		unsigned long			: 6;
		unsigned long cv4_hend		: 10;
		unsigned long			: 6;
	} f;
} crtc_cv4_hpos_u;


typedef union {
	unsigned long val			: 32;
	struct _crtc_eck_t {
		unsigned long eck_freq1		: 3;
		unsigned long eck_en		: 1;
		unsigned long			: 28;
	} f;
} crtc_eck_u;


typedef union {
	unsigned long val			: 32;
	struct _refresh_cntl_t {
		unsigned long ref_frame		: 3;
		unsigned long nref_frame	: 5;
		unsigned long ref_cntl		: 1;
		unsigned long stop_sm_nref	: 1;
		unsigned long stop_req_nref	: 1;
		unsigned long			: 21;
	} f;
} refresh_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _genlcd_cntl3_t {
		unsigned long ps1_oe		: 1;
		unsigned long ps1_pd		: 1;
		unsigned long ps2_oe		: 1;
		unsigned long ps2_pd		: 1;
		unsigned long rev2_oe		: 1;
		unsigned long rev2_pd		: 1;
		unsigned long awl_oe		: 1;
		unsigned long awl_pd		: 1;
		unsigned long dinv_oe		: 1;
		unsigned long dinv_pd		: 1;
		unsigned long psk_out		: 1;
		unsigned long psd_out		: 1;
		unsigned long eck_out		: 1;
		unsigned long cv4_out		: 1;
		unsigned long ps1_out		: 1;
		unsigned long ps2_out		: 1;
		unsigned long rev_out		: 1;
		unsigned long rev2_out		: 1;
		unsigned long			: 14;
	} f;
} genlcd_cntl3_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_data2_t {
		unsigned long gio2_out		: 16;
		unsigned long gio2_in		: 16;
	} f;
} gpio_data2_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_cntl3_t {
		unsigned long gio2_pd		: 16;
		unsigned long gio2_schmen	: 16;
	} f;
} gpio_cntl3_u;


typedef union {
	unsigned long val			: 32;
	struct _gpio_cntl4_t {
		unsigned long gio2_oe		: 16;
		unsigned long			: 16;
	} f;
} gpio_cntl4_u;


typedef union {
	unsigned long val			: 32;
	struct _chip_strap_t {
		unsigned long config_strap	: 8;
		unsigned long pkg_strap		: 1;
		unsigned long			: 23;
	} f;
} chip_strap_u;


typedef union {
	unsigned long val			: 32;
	struct _disp_debug2_t {
		unsigned long disp_debug2	: 32;
	} f;
} disp_debug2_u;


typedef union {
	unsigned long val			: 32;
	struct _debug_bus_cntl_t {
		unsigned long debug_testmux	: 4;
		unsigned long debug_testsel	: 4;
		unsigned long debug_gioa_sel	: 2;
		unsigned long debug_giob_sel	: 2;
		unsigned long debug_clk_sel	: 1;
		unsigned long debug_clk_inv	: 1;
		unsigned long			: 2;
		unsigned long debug_bus		: 16;
	} f;
} debug_bus_cntl_u;


typedef union {
	unsigned long val			: 32;
	struct _gamma_value1_t {
		unsigned long gamma1		: 8;
		unsigned long gamma2		: 8;
		unsigned long gamma3		: 8;
		unsigned long gamma4		: 8;
	} gamma_value1_t;
} gamma_value1_u;


typedef union {
	unsigned long val			: 32;
	struct _gamma_value2_t {
		unsigned long gamma5		: 8;
		unsigned long gamma6		: 8;
		unsigned long gamma7		: 8;
		unsigned long gamma8		: 8;
	} f;
} gamma_value2_u;


typedef union {
	unsigned long val			: 32;
	struct _gamma_slope_t {
		unsigned long slope1		: 3;
		unsigned long slope2		: 3;
		unsigned long slope3		: 3;
		unsigned long slope4		: 3;
		unsigned long slope5		: 3;
		unsigned long slope6		: 3;
		unsigned long slope7		: 3;
		unsigned long slope8		: 3;
		unsigned long			: 8;
	} f;
} gamma_slope_u;


typedef union {
	unsigned long val			: 32;
	struct _gen_status_t {
		unsigned long status		: 16;
		unsigned long			: 16;
	} f;
} gen_status_u;


typedef union {
	unsigned long val			: 32;
	struct _hw_int_t {
		unsigned long hwint1_pos	: 5;
		unsigned long hwint2_pos	: 5;
		unsigned long hwint1_pol	: 1;
		unsigned long hwint2_pol	: 1;
		unsigned long hwint1_en_db	: 1;
		unsigned long hwint2_en_db	: 1;
		unsigned long			: 18;
	} f;
} hw_int_u;


// LCD_FORMAT.lcd_type
#define	LCDTYPE_TFT333			0
#define	LCDTYPE_TFT444			1
#define	LCDTYPE_TFT555			2
#define	LCDTYPE_TFT666			3
#define	LCDTYPE_COLSTNPACK4		4
#define	LCDTYPE_COLSTNPACK8F1		5
#define	LCDTYPE_COLSTNPACK8F2		6
#define	LCDTYPE_COLSTNPACK16		7
#define	LCDTYPE_MONSTNPACK4		8
#define	LCDTYPE_MONSTNPACK8		9

// GRAPHIC_CTRL.color_depth
#define	COLOR_DEPTH_1BPP		0
#define	COLOR_DEPTH_2BPP		1
#define	COLOR_DEPTH_4BPP		2
#define	COLOR_DEPTH_8BPP		3
#define	COLOR_DEPTH_332			4
#define	COLOR_DEPTH_A444		5
#define	COLOR_DEPTH_A555		6

// VIDEO_CTRL.video_mode
#define	VIDEO_MODE_422			0
#define	VIDEO_MODE_420			1

#endif /* _w100_display_h_ */
