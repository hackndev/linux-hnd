/* 
    ci - header file for pxa capture interface

    Copyright (C) 2003, Intel Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/  

#ifndef _CI_H_
#define _CI_H_

//---------------------------------------------------------------------------
// Register definitions
//---------------------------------------------------------------------------

enum CI_REGBITS_CICR0 {
        CI_CICR0_FOM       = 0x00000001,
        CI_CICR0_EOFM      = 0x00000002,
        CI_CICR0_SOFM      = 0x00000004,
        CI_CICR0_CDM       = 0x00000008,
        CI_CICR0_QDM       = 0x00000010,
        CI_CICR0_PERRM     = 0x00000020,
        CI_CICR0_EOLM      = 0x00000040,
        CI_CICR0_FEM       = 0x00000080,
        CI_CICR0_RDAVM     = 0x00000100,
        CI_CICR0_TOM       = 0x00000200,
        CI_CICR0_RESERVED  = 0x03FFFC00,
        CI_CICR0_SIM_SHIFT = 24,
        CI_CICR0_SIM_SMASK = 0x7,
        CI_CICR0_DIS       = 0x08000000,
        CI_CICR0_ENB       = 0x10000000,
        CI_CICR0_SL_CAP_EN = 0x20000000,
        CI_CICR0_PAR_EN    = 0x40000000,
        CI_CICR0_DMA_EN    = 0x80000000,
        CI_CICR0_INTERRUPT_MASK = 0x3FF
};

enum CI_REGBITS_CICR1 {
        CI_CICR1_DW_SHIFT       = 0,
        CI_CICR1_DW_SMASK       = 0x7,
        CI_CICR1_COLOR_SP_SHIFT = 3,
        CI_CICR1_COLOR_SP_SMASK = 0x3,
        CI_CICR1_RAW_BPP_SHIFT  = 5,
        CI_CICR1_RAW_BPP_SMASK  = 0x3,
        CI_CICR1_RGB_BPP_SHIFT  = 7,
        CI_CICR1_RGB_BPP_SMASK  = 0x7,
        CI_CICR1_YCBCR_F        = 0x00000400,
        CI_CICR1_RBG_F          = 0x00000800,
        CI_CICR1_RGB_CONV_SHIFT = 12,
        CI_CICR1_RGB_CONV_SMASK = 0x7,
        CI_CICR1_PPL_SHIFT      = 15,
        CI_CICR1_PPL_SMASK      = 0x7FF,
        CI_CICR1_RESERVED       = 0x1C000000,
        CI_CICR1_RGBT_CONV_SHIFT= 29,
        CI_CICR1_RGBT_CONV_SMASK= 0x3,
        CI_CICR1_TBIT           = 0x80000000
};

enum CI_REGBITS_CICR2 {
        CI_CICR2_FSW_SHIFT = 0,
        CI_CICR2_FSW_SMASK = 0x3,
        CI_CICR2_BFPW_SHIFT= 3,
        CI_CICR2_BFPW_SMASK= 0x3F,
        CI_CICR2_RESERVED  = 0x00000200,
        CI_CICR2_HSW_SHIFT = 10,
        CI_CICR2_HSW_SMASK = 0x3F,
        CI_CICR2_ELW_SHIFT = 16,
        CI_CICR2_ELW_SMASK = 0xFF,
        CI_CICR2_BLW_SHIFT = 24,     
        CI_CICR2_BLW_SMASK = 0xFF    
};

enum CI_REGBITS_CICR3 {
    CI_CICR3_LPF_SHIFT = 0,
    CI_CICR3_LPF_SMASK = 0x7FF,
    CI_CICR3_VSW_SHIFT = 11,
    CI_CICR3_VSW_SMASK = 0x1F,
    CI_CICR3_EFW_SHIFT = 16,
    CI_CICR3_EFW_SMASK = 0xFF,
    CI_CICR3_BFW_SHIFT = 24,
    CI_CICR3_BFW_SMASK = 0xFF
};

enum CI_REGBITS_CICR4 {
    CI_CICR4_DIV_SHIFT = 0,
    CI_CICR4_DIV_SMASK = 0xFF,
    CI_CICR4_FR_RATE_SHIFT = 8,
    CI_CICR4_FR_RATE_SMASK = 0x7,
    CI_CICR4_RESERVED1 = 0x0007F800,
    CI_CICR4_MCLK_EN   = 0x00080000,
    CI_CICR4_VSP       = 0x00100000,
    CI_CICR4_HSP       = 0x00200000,
    CI_CICR4_PCP       = 0x00400000,
    CI_CICR4_PCLK_EN   = 0x00800000,
    CI_CICR4_RESERVED2 = 0xFF000000,
    CI_CICR4_RESERVED  = CI_CICR4_RESERVED1 | CI_CICR4_RESERVED2
};

enum CI_REGBITS_CISR {
    CI_CISR_IFO_0      = 0x00000001,
    CI_CISR_IFO_1      = 0x00000002,
    CI_CISR_IFO_2      = 0x00000004,
    CI_CISR_EOF        = 0x00000008,
    CI_CISR_SOF        = 0x00000010,
    CI_CISR_CDD        = 0x00000020,
    CI_CISR_CQD        = 0x00000040,
    CI_CISR_PAR_ERR    = 0x00000080,
    CI_CISR_EOL        = 0x00000100,
    CI_CISR_FEMPTY_0   = 0x00000200,
    CI_CISR_FEMPTY_1   = 0x00000400,
    CI_CISR_FEMPTY_2   = 0x00000800,
    CI_CISR_RDAV_0     = 0x00001000,
    CI_CISR_RDAV_1     = 0x00002000,
    CI_CISR_RDAV_2     = 0x00004000, 
    CI_CISR_FTO        = 0x00008000,
    CI_CISR_RESERVED   = 0xFFFF0000
};

enum CI_REGBITS_CIFR {
    CI_CIFR_FEN0       = 0x00000001,
    CI_CIFR_FEN1       = 0x00000002,
    CI_CIFR_FEN2       = 0x00000004,
    CI_CIFR_RESETF     = 0x00000008,
    CI_CIFR_THL_0_SHIFT= 4,
    CI_CIFR_THL_0_SMASK= 0x3,
    CI_CIFR_RESERVED1  = 0x000000C0,
    CI_CIFR_FLVL0_SHIFT= 8,
    CI_CIFR_FLVL0_SMASK= 0xFF,
    CI_CIFR_FLVL1_SHIFT= 16,
    CI_CIFR_FLVL1_SMASK= 0x7F,
    CI_CIFR_FLVL2_SHIFT= 23,
    CI_CIFR_FLVL2_SMASK= 0x7F,
    CI_CIFR_RESERVED2  = 0xC0000000,
    CI_CIFR_RESERVED   = CI_CIFR_RESERVED1 | CI_CIFR_RESERVED2 
};

//---------------------------------------------------------------------------
//     Parameter Type definitions
//---------------------------------------------------------------------------
typedef enum  {
        CI_RAW8 = 0,                   //RAW
        CI_RAW9,
        CI_RAW10,
        CI_YCBCR422,               //YCBCR
        CI_YCBCR422_PLANAR,        //YCBCR Planaried
        CI_RGB444,                 //RGB
        CI_RGB555,
        CI_RGB565,
        CI_RGB666,
        CI_RGB888,
        CI_RGBT555_0,              //RGB+Transparent bit 0
        CI_RGBT888_0,
        CI_RGBT555_1,              //RGB+Transparent bit 1  
        CI_RGBT888_1,
        CI_RGB666_PACKED,          //RGB Packed 
        CI_RGB888_PACKED,
        CI_INVALID_FORMAT = 0xFF
} CI_IMAGE_FORMAT;

typedef enum {
    CI_INTSTATUS_IFO_0      = 0x00000001,
    CI_INTSTATUS_IFO_1      = 0x00000002,
    CI_INTSTATUS_IFO_2      = 0x00000004,
    CI_INTSTATUS_EOF        = 0x00000008,
    CI_INTSTATUS_SOF        = 0x00000010,
    CI_INTSTATUS_CDD        = 0x00000020,
    CI_INTSTATUS_CQD        = 0x00000040,
    CI_INTSTATUS_PAR_ERR    = 0x00000080,
    CI_INTSTATUS_EOL        = 0x00000100,
    CI_INTSTATUS_FEMPTY_0   = 0x00000200,
    CI_INTSTATUS_FEMPTY_1   = 0x00000400,
    CI_INTSTATUS_FEMPTY_2   = 0x00000800,
    CI_INTSTATUS_RDAV_0     = 0x00001000,
    CI_INTSTATUS_RDAV_1     = 0x00002000,
    CI_INTSTATUS_RDAV_2     = 0x00004000, 
    CI_INTSTATUS_FTO        = 0x00008000,
    CI_INTSTATUS_ALL       = 0x0000FFFF
} CI_INTERRUPT_STATUS;

typedef enum {
    CI_INT_IFO      = 0x00000001,
    CI_INT_EOF      = 0x00000002,
    CI_INT_SOF      = 0x00000004,
    CI_INT_CDD      = 0x00000008,
    CI_INT_CQD      = 0x00000010,
    CI_INT_PAR_ERR  = 0x00000020,
    CI_INT_EOL      = 0x00000040,
    CI_INT_FEMPTY   = 0x00000080,
    CI_INT_RDAV     = 0x00000100,
    CI_INT_FTO      = 0x00000200,
    CI_INT_ALL      = 0x000003FF
} CI_INTERRUPT_MASK;
#define CI_INT_MAX 10

typedef enum CI_MODE {
        CI_MODE_MP,             // Master-Parallel
        CI_MODE_SP,             // Slave-Parallel
        CI_MODE_MS,             // Master-Serial
        CI_MODE_EP,             // Embedded-Parallel
        CI_MODE_ES              // Embedded-Serial
} CI_MODE;


typedef enum  {
        CI_FR_ALL = 0,          // Capture all incoming frames
        CI_FR_1_2,              // Capture 1 out of every 2 frames
        CI_FR_1_3,              // Capture 1 out of every 3 frames
        CI_FR_1_4,
        CI_FR_1_5,
        CI_FR_1_6,
        CI_FR_1_7,
        CI_FR_1_8
} CI_FRAME_CAPTURE_RATE;


typedef enum  {
        CI_FIFO_THL_32 = 0,
        CI_FIFO_THL_64,
        CI_FIFO_THL_96
} CI_FIFO_THRESHOLD;

typedef struct {
    unsigned int BFW;
    unsigned int BLW;
} CI_MP_TIMING, CI_MS_TIMING;

typedef struct {
    unsigned int BLW;
    unsigned int ELW; 
    unsigned int HSW;
    unsigned int BFPW;
    unsigned int FSW; 
    unsigned int BFW;
    unsigned int EFW;
    unsigned int VSW; 
} CI_SP_TIMING;

typedef enum {
    CI_DATA_WIDTH4 = 0x0,
    CI_DATA_WIDTH5 = 0x1,
    CI_DATA_WIDTH8 = 0x2,  
    CI_DATA_WIDTH9 = 0x3,  
    CI_DATA_WIDTH10= 0x4   
} CI_DATA_WIDTH;

//-------------------------------------------------------------------------------------------------------
//      Configuration APIs
//-------------------------------------------------------------------------------------------------------

void ci_set_frame_rate(CI_FRAME_CAPTURE_RATE frate);
CI_FRAME_CAPTURE_RATE ci_get_frame_rate(void);
void ci_set_image_format(CI_IMAGE_FORMAT input_format, CI_IMAGE_FORMAT output_format); 
void ci_set_mode(CI_MODE mode, CI_DATA_WIDTH data_width);
void ci_configure_mp(unsigned int PPL, unsigned int LPF, CI_MP_TIMING* timing);
void ci_configure_sp(unsigned int PPL, unsigned int LPF, CI_SP_TIMING* timing);
void ci_configure_ms(unsigned int PPL, unsigned int LPF, CI_MS_TIMING* timing);
void ci_configure_ep(int parity_check);
void ci_configure_es(int parity_check);
void ci_set_clock(unsigned int clk_regs_base, int pclk_enable, int mclk_enable, unsigned int mclk_khz);
void ci_set_polarity(int pclk_sample_falling, int hsync_active_low, int vsync_active_low);
void ci_set_fifo( unsigned int timeout, CI_FIFO_THRESHOLD threshold, int fifo1_enable, 
                   int fifo2_enable);
void ci_set_int_mask( unsigned int mask);
void ci_clear_int_status( unsigned int status);
void ci_set_reg_value( unsigned int reg_offset, unsigned int value);
int ci_get_reg_value(unsigned int reg_offset);

void ci_reset_fifo(void);
unsigned int ci_get_int_mask(void);
unsigned int ci_get_int_status(void);
void ci_slave_capture_enable(void);
void ci_slave_capture_disable(void);

//-------------------------------------------------------------------------------------------------------
//      Control APIs
//-------------------------------------------------------------------------------------------------------
int ci_init(void);
void ci_deinit(void);
void ci_enable( int dma_en);
int  ci_disable(int quick);

//debug
void ci_dump(void);
// IRQ
irqreturn_t pxa_camera_irq(int irq, void *dev_id, struct pt_regs *regs);

#endif
