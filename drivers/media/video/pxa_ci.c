/*
 *  pxa_camera.c
 *
 *  Bulverde Processor Camera Interface driver.
 *
 *  Copyright (C) 2003, Intel Corporation
 *  Copyright (C) 2003, Montavista Software Inc.
 *  Copyright (C) 2003-2006 Motorola Inc.
 *
 *  Author: Intel Corporation Inc.
 *          MontaVista Software, Inc.
 *           source@mvista.com
 *          Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  V4L2 conversion (C) 2007 Sergey Lapin
 */
#warning "Not finished yet, don't try to use!!!"
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>

// #include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
// #include <linux/wrapper.h>
#include <media/v4l2-dev.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <linux/types.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>
#include <linux/interrupt.h>

#include <asm/arch/pxa-regs.h>

#ifdef CONFIG_VIDEO_V4L1_COMPAT
/* Include V4L1 specific functions. Should be removed soon */
#include <linux/videodev.h>
#endif

#include "pxa_ci_types.h"

void ci_set_image_format(CI_IMAGE_FORMAT input_format, CI_IMAGE_FORMAT output_format)
{

	unsigned int value, tbit, rgbt_conv, rgb_conv, rgb_f, ycbcr_f, rgb_bpp, raw_bpp, cspace;
	// write cicr1: preserve ppl value and data width value
	dbg_print("0");
	value = CICR1;
	dbg_print("1");
	value &= ( (CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT) | ((CI_CICR1_DW_SMASK) << CI_CICR1_DW_SHIFT));
	tbit = rgbt_conv = rgb_conv = rgb_f = ycbcr_f = rgb_bpp = raw_bpp = cspace = 0;
	switch(input_format)
	{
		case CI_RAW8:
			cspace = 0;
			raw_bpp = 0;
			break;
		case CI_RAW9:
			cspace = 0;
			raw_bpp = 1;
			break;
		case CI_RAW10:
			cspace = 0;
			raw_bpp = 2;
			break;
		case CI_YCBCR422:
		case CI_YCBCR422_PLANAR:
			cspace = 2;
			if(output_format == CI_YCBCR422_PLANAR) {
				ycbcr_f = 1;
			}
			break;
		case CI_RGB444:
			cspace = 1;
			rgb_bpp = 0;
			break;
		case CI_RGB555:
			cspace = 1;
			rgb_bpp = 1;
			if(output_format == CI_RGBT555_0) {
				rgbt_conv = 2;
				tbit = 0;
			}
			else if(output_format == CI_RGBT555_1) {
				rgbt_conv = 2;
				tbit = 1;
			}
			break;
		case CI_RGB565:
			cspace = 1;
			rgb_bpp = 2;
			rgb_f = 1;
			break;
		case CI_RGB666:
			cspace = 1;
			rgb_bpp = 3;
			if(output_format == CI_RGB666_PACKED) {
				rgb_f = 1;
			}
			break;
		case CI_RGB888:
		case CI_RGB888_PACKED:
			cspace = 1;
			rgb_bpp = 4;
			switch(output_format)
			{
				case CI_RGB888_PACKED:
					rgb_f = 1;
					break;
				case CI_RGBT888_0:
					rgbt_conv = 1;
					tbit = 0;
					break;
				case CI_RGBT888_1:
					rgbt_conv = 1;
					tbit = 1;
					break;
				case CI_RGB666:
					rgb_conv = 1;
					break;
					// RGB666 PACKED - JamesL
				case CI_RGB666_PACKED:
					rgb_conv = 1;
					rgb_f = 1;
					break;
					// end
				case CI_RGB565:
					dbg_print("format : 565");
					rgb_conv = 2;
					break;
				case CI_RGB555:
					rgb_conv = 3;
					break;
				case CI_RGB444:
					rgb_conv = 4;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	dbg_print("2");
	value |= (tbit==1) ? CI_CICR1_TBIT : 0;
	value |= rgbt_conv << CI_CICR1_RGBT_CONV_SHIFT;
	value |= rgb_conv << CI_CICR1_RGB_CONV_SHIFT;
	value |= (rgb_f==1) ? CI_CICR1_RBG_F : 0;
	value |= (ycbcr_f==1) ? CI_CICR1_YCBCR_F : 0;
	value |= rgb_bpp << CI_CICR1_RGB_BPP_SHIFT;
	value |= raw_bpp << CI_CICR1_RAW_BPP_SHIFT;
	value |= cspace << CI_CICR1_COLOR_SP_SHIFT;
	CICR1 = value;

}


void ci_set_mode(CI_MODE mode, CI_DATA_WIDTH data_width)
{
	unsigned int value;

	// write mode field in cicr0
	value = CICR0;
	value &= ~(CI_CICR0_SIM_SMASK << CI_CICR0_SIM_SHIFT);
	value |= (unsigned int)mode << CI_CICR0_SIM_SHIFT;
	CICR0 = value;

	// write data width cicr1
	value = CICR1;
	value &= ~(CI_CICR1_DW_SMASK << CI_CICR1_DW_SHIFT);
	value |= ((unsigned)data_width) << CI_CICR1_DW_SHIFT;
	CICR1 = value;
	return;
}


void ci_configure_mp(unsigned int ppl, unsigned int lpf, CI_MP_TIMING* timing)
{
	unsigned int value;
	// write ppl field in cicr1
	value = CICR1;
	value &= ~(CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT);
	value |= (ppl & CI_CICR1_PPL_SMASK) << CI_CICR1_PPL_SHIFT;
	CICR1 = value;

	// write BLW, ELW in cicr2
	value = CICR2;
	value &= ~(CI_CICR2_BLW_SMASK << CI_CICR2_BLW_SHIFT | CI_CICR2_ELW_SMASK << CI_CICR2_ELW_SHIFT );
	value |= (timing->BLW & CI_CICR2_BLW_SMASK) << CI_CICR2_BLW_SHIFT;
	CICR2 = value;

	// write BFW, LPF in cicr3
	value = CICR3;
	value &= ~(CI_CICR3_BFW_SMASK << CI_CICR3_BFW_SHIFT | CI_CICR3_LPF_SMASK << CI_CICR3_LPF_SHIFT );
	value |= (timing->BFW & CI_CICR3_BFW_SMASK) << CI_CICR3_BFW_SHIFT;
	value |= (lpf & CI_CICR3_LPF_SMASK) << CI_CICR3_LPF_SHIFT;
	CICR3 = value;

}


void ci_configure_sp(unsigned int ppl, unsigned int lpf, CI_SP_TIMING* timing)
{
	unsigned int value;

	// write ppl field in cicr1
	value = CICR1;
	value &= ~(CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT);
	value |= (ppl & CI_CICR1_PPL_SMASK) << CI_CICR1_PPL_SHIFT;
	CICR1 = value;

	// write cicr2
	value = CICR2;
	value |= (timing->BLW & CI_CICR2_BLW_SMASK) << CI_CICR2_BLW_SHIFT;
	value |= (timing->ELW & CI_CICR2_ELW_SMASK) << CI_CICR2_ELW_SHIFT;
	value |= (timing->HSW & CI_CICR2_HSW_SMASK) << CI_CICR2_HSW_SHIFT;
	value |= (timing->BFPW & CI_CICR2_BFPW_SMASK) << CI_CICR2_BFPW_SHIFT;
	value |= (timing->FSW & CI_CICR2_FSW_SMASK) << CI_CICR2_FSW_SHIFT;
	CICR2 = value;

	// write cicr3
	value = CICR3;
	value |= (timing->BFW & CI_CICR3_BFW_SMASK) << CI_CICR3_BFW_SHIFT;
	value |= (timing->EFW & CI_CICR3_EFW_SMASK) << CI_CICR3_EFW_SHIFT;
	value |= (timing->VSW & CI_CICR3_VSW_SMASK) << CI_CICR3_VSW_SHIFT;
	value |= (lpf & CI_CICR3_LPF_SMASK) << CI_CICR3_LPF_SHIFT;
	CICR3 = value;
	return;
}


void ci_configure_ms(unsigned int ppl, unsigned int lpf, CI_MS_TIMING* timing)
{
	// the operation is same as Master-Parallel
	ci_configure_mp(ppl, lpf, (CI_MP_TIMING*)timing);
}


void ci_configure_ep(int parity_check)
{
	unsigned int value;

	// write parity_enable field in cicr0
	value = CICR0;
	if(parity_check)
	{
		value |= CI_CICR0_PAR_EN;
	}
	else
	{
		value &= ~CI_CICR0_PAR_EN;
	}
	CICR0 = value;
	return;
}


void ci_configure_es(int parity_check)
{
	// the operationi is same as Embedded-Parallel
	ci_configure_ep(parity_check);
}


void ci_set_clock(unsigned int clk_regs_base, int pclk_enable, int mclk_enable, unsigned int mclk_khz)
{
	unsigned int ciclk,  value, div, cccr_l, K;

	// determine the LCLK frequency programmed into the CCCR.
	cccr_l = (CCCR & 0x0000001F);

	if(cccr_l < 8)
		K = 1;
	else if(cccr_l < 17)
		K = 2;
	else
		K = 3;

	ciclk = (13 * cccr_l) / K;

	div = (ciclk + mclk_khz) / ( 2 * mclk_khz ) - 1;
	dbg_print("cccr=%xciclk=%d,cccr_l=%d,K=%d,div=%d,mclk=%d\n",CCCR,ciclk,cccr_l,K,div,mclk_khz);
	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCLK_EN | CI_CICR4_MCLK_EN | CI_CICR4_DIV_SMASK<<CI_CICR4_DIV_SHIFT);
	value |= (pclk_enable) ? CI_CICR4_PCLK_EN : 0;
	value |= (mclk_enable) ? CI_CICR4_MCLK_EN : 0;
	value |= div << CI_CICR4_DIV_SHIFT;
	CICR4 = value;
	return;
}


void ci_set_polarity(int pclk_sample_falling, int hsync_active_low, int vsync_active_low)
{
	unsigned int value;
	dbg_print("");

	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCP | CI_CICR4_HSP | CI_CICR4_VSP);
	value |= (pclk_sample_falling)? CI_CICR4_PCP : 0;
	value |= (hsync_active_low) ? CI_CICR4_HSP : 0;
	value |= (vsync_active_low) ? CI_CICR4_VSP : 0;
	CICR4 = value;
	return;
}


void ci_set_fifo(unsigned int timeout, CI_FIFO_THRESHOLD threshold, int fifo1_enable,
int fifo2_enable)
{
	unsigned int value;
	dbg_print("");
	// write citor
	CITOR = timeout;

	// write cifr: always enable fifo 0! also reset input fifo
	value = CIFR;
	value &= ~(CI_CIFR_FEN0 | CI_CIFR_FEN1 | CI_CIFR_FEN2 | CI_CIFR_RESETF |
		CI_CIFR_THL_0_SMASK<<CI_CIFR_THL_0_SHIFT);
	value |= (unsigned int)threshold << CI_CIFR_THL_0_SHIFT;
	value |= (fifo1_enable) ? CI_CIFR_FEN1 : 0;
	value |= (fifo2_enable) ? CI_CIFR_FEN2 : 0;
	value |= CI_CIFR_RESETF | CI_CIFR_FEN0;
	CIFR = value;
}


void ci_reset_fifo()
{
	unsigned int value;
	value = CIFR;
	value |= CI_CIFR_RESETF;
	CIFR = value;
}


void ci_set_int_mask(unsigned int mask)
{
	unsigned int value;

	// write mask in cicr0
	value = CICR0;
	value &= ~CI_CICR0_INTERRUPT_MASK;
	value |= (mask & CI_CICR0_INTERRUPT_MASK);
	dbg_print("-----------value=0x%x\n",value);
	CICR0 = value;
	return;
}


unsigned int ci_get_int_mask()
{
	unsigned int value;

	// write mask in cicr0
	value = CICR0;
	return (value & CI_CICR0_INTERRUPT_MASK);
}


void ci_clear_int_status(unsigned int status)
{
	// write 1 to clear
	CISR = status;
}


unsigned int ci_get_int_status()
{
	int value;

	value = CISR;

	return  value;
}


void ci_set_reg_value(unsigned int reg_offset, unsigned int value)
{
	switch(reg_offset)
	{
		case 0:
			CICR0 = value;
			break;
		case 1:
			CICR1 = value;
			break;
		case 2:
			CICR2 = value;
			break;
		case 3:
			CICR3 = value;
			break;
		case 4:
			CICR4 = value;
			break;
		case 5:
			CISR  = value;
			break;
		case 6:
			CIFR = value;
			break;
		case 7:
			CITOR = value;
			break;
	}
}


int ci_get_reg_value(unsigned int reg_offset)
{

	unsigned values[] = {CICR0, CICR1, CICR2, CICR3, CICR4, CISR, CIFR, CITOR};

	if(reg_offset >=0 && reg_offset <= 7)
		return values[reg_offset];
	else
		return CISR;
}

void ci_enable(int dma_en)
{        
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value |= CI_CICR0_ENB;
	if(dma_en) {
		value |= CI_CICR0_DMA_EN;
	}
	CICR0 = value;   
	return; 
}

int ci_disable(int quick)
{
	volatile unsigned int value, mask;
	int retry;

	// write control bit in cicr0   
	value = CICR0;
	if(quick)
        {
		value &= ~CI_CICR0_ENB;
		mask = CI_CISR_CQD;
	}
	else 
        {
		value |= CI_CICR0_DIS;
		mask = CI_CISR_CDD;
	}
	CICR0 = value;   
	
	// wait shutdown complete
	retry = 50;
	while ( retry-- > 0 ) 
    {
 	value = CISR;
	 if( value & mask ) 
     {
		CISR = mask;
		return 0;
	 }
	 mdelay(10);
	}
	return -1; 
}

void ci_slave_capture_enable()
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value |= CI_CICR0_SL_CAP_EN;
	CICR0 = value;   
	return; 
}

void ci_slave_capture_disable()
{
	unsigned int value;
	
	// write mask in cicr0  
	value = CICR0;
	value &= ~CI_CICR0_SL_CAP_EN;
	CICR0 = value;   
	return; 
}

void ci_set_frame_rate(CI_FRAME_CAPTURE_RATE frate)
{
	unsigned int value;
	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_FR_RATE_SMASK << CI_CICR4_FR_RATE_SHIFT);
	value |= (unsigned)frate << CI_CICR4_FR_RATE_SHIFT;
	CICR4 = value;
}


CI_FRAME_CAPTURE_RATE ci_get_frame_rate(void)
{
	unsigned int value;
	value = CICR4;
	return (CI_FRAME_CAPTURE_RATE)((value >> CI_CICR4_FR_RATE_SHIFT) & CI_CICR4_FR_RATE_SMASK);
}


/* Debug */

void ci_dump(void)
{
    dbg_print ("CICR0 = 0x%8x ", CICR0);
    dbg_print ("CICR1 = 0x%8x ", CICR1);
    dbg_print ("CICR2 = 0x%8x ", CICR2);
    dbg_print ("CICR3 = 0x%8x ", CICR3);
    dbg_print ("CICR4 = 0x%8x ", CICR4);
    dbg_print ("CISR  = 0x%8x ", CISR);
    dbg_print ("CITOR = 0x%8x ", CITOR);
    dbg_print ("CIFR  = 0x%8x ", CIFR);
}

