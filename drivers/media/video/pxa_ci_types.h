/*
 *  camera.h
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

Revision History:
 Modification     Tracking
	 Date         Description of Changes
 ------------   -------------------------
  12/19/2003     Created
  02/26/2004     Update algorithm for DMA transfer
  04/12/2006     fix sensor name

*/

/*================================================================================
								 INCLUDE FILES
================================================================================*/
#ifndef PXA_CI_TYPES_H_
#define PXA_CI_TYPES_H_

#include <linux/pxa_camera.h>
#include <linux/irq.h>
#include <linux/videodev.h> /* FIXME - needed for video_capability */

#define DEBUG   1

/*
Bpp definition
*/

#define YUV422_BPP              16
#define RGB565_BPP              16
#define RGB666_UNPACKED_BPP       32
#define RGB666_PACKED_BPP         24

//---------------------------------------------------------------------------
// Register definitions
//---------------------------------------------------------------------------

enum CI_REGBITS_CICR0
{
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

enum CI_REGBITS_CICR1
{
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

enum CI_REGBITS_CICR2
{
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

enum CI_REGBITS_CICR3
{
	CI_CICR3_LPF_SHIFT = 0,
	CI_CICR3_LPF_SMASK = 0x7FF,
	CI_CICR3_VSW_SHIFT = 11,
	CI_CICR3_VSW_SMASK = 0x1F,
	CI_CICR3_EFW_SHIFT = 16,
	CI_CICR3_EFW_SMASK = 0xFF,
	CI_CICR3_BFW_SHIFT = 24,
	CI_CICR3_BFW_SMASK = 0xFF
};

enum CI_REGBITS_CICR4
{
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

enum CI_REGBITS_CISR
{
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

enum CI_REGBITS_CIFR
{
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
typedef enum
{
	CI_RAW8 = 0,				 //RAW
	CI_RAW9,
	CI_RAW10,
	CI_YCBCR422,				 //YCBCR
	CI_YCBCR422_PLANAR,			 //YCBCR Planaried
	CI_RGB444,					 //RGB
	CI_RGB555,
	CI_RGB565,
	CI_RGB666,
	CI_RGB888,
	CI_RGBT555_0,				 //RGB+Transparent bit 0
	CI_RGBT888_0,
	CI_RGBT555_1,				 //RGB+Transparent bit 1
	CI_RGBT888_1,
	CI_RGB666_PACKED,			 //RGB Packed
	CI_RGB888_PACKED,
	CI_INVALID_FORMAT = 0xFF
} CI_IMAGE_FORMAT;

typedef enum
{
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

typedef enum
{
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

typedef enum CI_MODE
{
	CI_MODE_MP,					 // Master-Parallel
	CI_MODE_SP,					 // Slave-Parallel
	CI_MODE_MS,					 // Master-Serial
	CI_MODE_EP,					 // Embedded-Parallel
	CI_MODE_ES					 // Embedded-Serial
} CI_MODE;

typedef enum
{
	CI_FR_ALL = 0,				 // Capture all incoming frames
	CI_FR_1_2,					 // Capture 1 out of every 2 frames
	CI_FR_1_3,					 // Capture 1 out of every 3 frames
	CI_FR_1_4,
	CI_FR_1_5,
	CI_FR_1_6,
	CI_FR_1_7,
	CI_FR_1_8
} CI_FRAME_CAPTURE_RATE;

typedef enum
{
	CI_FIFO_THL_32 = 0,
	CI_FIFO_THL_64,
	CI_FIFO_THL_96
} CI_FIFO_THRESHOLD;

typedef struct
{
	unsigned int BFW;
	unsigned int BLW;
} CI_MP_TIMING, CI_MS_TIMING;

typedef struct
{
	unsigned int BLW;
	unsigned int ELW;
	unsigned int HSW;
	unsigned int BFPW;
	unsigned int FSW;
	unsigned int BFW;
	unsigned int EFW;
	unsigned int VSW;
} CI_SP_TIMING;

typedef enum
{
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
void ci_set_clock(int pclk_enable, int mclk_enable, unsigned int mclk_mhz);
unsigned int ci_get_clock(void);
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
void pxa_ci_dma_irq_y(int channel, void *data);
void pxa_ci_dma_irq_cb(int channel, void *data);
void pxa_ci_dma_irq_cr(int channel, void *data);


//-------------------------------------------------------------------------------------------------------
//      Control APIs
//-------------------------------------------------------------------------------------------------------
int  ci_init(void);
void ci_deinit(void);
void ci_enable( int dma_en);
int  ci_disable(int quick);

//debug
void ci_dump(void);
// IRQ
void pxa_fvgpio_interrupt(int irq, void *dev_id, struct pt_regs *regs);

#define err_print(fmt, args...) printk(KERN_ERR "fun %s "fmt"\n", __FUNCTION__, ##args)

#ifdef DEBUG

#define dbg_print(fmt, args...) printk(KERN_INFO "fun %s "fmt"\n", __FUNCTION__, ##args)

#if DEBUG > 1
#define ddbg_print(fmt, args...) printk(KERN_INFO "fun %s "fmt"\n", __FUNCTION__, ##args)
#else
#define ddbg_print(fmt, args...) ;
#endif

#else

#define dbg_print(fmt, args...)  ;
#define ddbg_print(fmt, args...) ;
#endif

								 /* subject to change */
#define VID_HARDWARE_PXA_CAMERA     50

#define STATUS_FAILURE  (0)
#define STATUS_SUCCESS  (1)
#define STATUS_WRONG_PARAMETER  -1

/*
Macros
*/

/*
Image format definition
*/
//#define CAMERA_IMAGE_FORMAT_MAX                CAMERA_IMAGE_FORMAT_YCBCR444_PLANAR

// Interrupt mask
#define CAMERA_INTMASK_FIFO_OVERRUN            0x0001
#define CAMERA_INTMASK_END_OF_FRAME            0x0002
#define CAMERA_INTMASK_START_OF_FRAME          0x0004
#define CAMERA_INTMASK_CI_DISABLE_DONE         0x0008
#define CAMERA_INTMASK_CI_QUICK_DISABLE        0x0010
#define CAMERA_INTMASK_PARITY_ERROR            0x0020
#define CAMERA_INTMASK_END_OF_LINE             0x0040
#define CAMERA_INTMASK_FIFO_EMPTY              0x0080
#define CAMERA_INTMASK_RCV_DATA_AVALIBLE       0x0100
#define CAMERA_INTMASK_TIME_OUT                0x0200
#define CAMERA_INTMASK_END_OF_DMA              0x0400

// Interrupt status
#define CAMERA_INTSTATUS_FIFO_OVERRUN_0        0x00000001
#define CAMERA_INTSTATUS_FIFO_OVERRUN_1        0x00000002
#define CAMERA_INTSTATUS_FIFO_OVERRUN_2        0x00000004
#define CAMERA_INTSTATUS_END_OF_FRAME          0x00000008
#define CAMERA_INTSTATUS_START_OF_FRAME        0x00000010
#define CAMERA_INTSTATUS_CI_DISABLE_DONE       0x00000020
#define CAMERA_INTSTATUS_CI_QUICK_DISABLE      0x00000040
#define CAMERA_INTSTATUS_PARITY_ERROR          0x00000080
#define CAMERA_INTSTATUS_END_OF_LINE           0x00000100
#define CAMERA_INTSTATUS_FIFO_EMPTY_0          0x00000200
#define CAMERA_INTSTATUS_FIFO_EMPTY_1          0x00000400
#define CAMERA_INTSTATUS_FIFO_EMPTY_2          0x00000800
#define CAMERA_INTSTATUS_RCV_DATA_AVALIBLE_0   0x00001000
#define CAMERA_INTSTATUS_RCV_DATA_AVALIBLE_1   0x00002000
#define CAMERA_INTSTATUS_RCV_DATA_AVALIBLE_2   0x00004000
#define CAMERA_INTSTATUS_TIME_OUT              0x00008000
#define CAMERA_INTSTATUS_END_OF_DMA            0x00010000

// Capture status
#define CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS 0x0001
#define CAMERA_STATUS_RING_BUFFER_FULL         0x0002

#define CAMERA_ERROR_NONE                      0
#define CAMERA_ERROR_FVNOTSYNC                 -1
#define CAMERA_ERROR_FIFONOTEMPTY              -2
#define CAMERA_ERROR_UNEXPECTEDINT             -3

/*
Structures
*/
typedef struct camera_context_s camera_context_t, *p_camera_context_t;

typedef struct
{
	int (*init)(p_camera_context_t context);
	int (*deinit)(p_camera_context_t);
	int (*set_capture_format)(p_camera_context_t);
	int (*start_capture)(p_camera_context_t, unsigned int frames);
	int (*stop_capture)(p_camera_context_t);
	int (*command)(p_camera_context_t, unsigned int cmd, void *param);
	int (*pm_management)(p_camera_context_t, int suspend);
} camera_function_t, *p_camera_function_t;
// context

struct camera_context_s
{
	// syncronization stuff
	atomic_t refcount;

	/*
	 * DRIVER FILLED PARAMTER
	 */

	// sensor info
	unsigned int sensor_type;

	// capture image info
	unsigned int capture_width;
	unsigned int capture_height;
	unsigned int sensor_width;
	unsigned int sensor_height;
	unsigned int still_width;
	unsigned int still_height;

	unsigned int camera_width;
	unsigned int camera_height;

	unsigned int    capture_input_format;
	unsigned int    capture_output_format;
	unsigned int    still_input_format;
	unsigned int    still_output_format;

	unsigned int    capture_digital_zoom;
	unsigned int    still_digital_zoom;

	int             plane_number;

	int             still_image_mode;
	int             camera_mode;

	int             jpeg_quality;

	int             waiting_frame;
	int             fv_rising_edge;
	int             preferred_block_num;
	int             still_image_rdy;
	int             task_waiting;
	int             detected_sensor_type;

	V4l_PIC_STYLE   capture_style;
	V4l_PIC_WB      capture_light;
	int             capture_bright;
	int             capture_contrast;
	int             flicker_freq;

	struct video_capability vc;

	// frame rate control
	unsigned int frame_rate;
	unsigned int fps;
	unsigned int mini_fps;

	unsigned int mclk;

	// ring buffers
	// note: must pass in 8 bytes aligned address
	void *buffer_virtual;
	void *buffer_physical;
	unsigned int buf_size;

	// memory for dma descriptors, layout:
	//  dma descriptor chain 0,
	//  dma descriptor chain 1,
	//  ...
	void *dma_descriptors_virtual;
	void *dma_descriptors_physical;
	unsigned int dma_descriptors_size;

	// os mapped register address
	unsigned int clk_reg_base;
	unsigned int ost_reg_base;
	unsigned int gpio_reg_base;
	unsigned int ci_reg_base;
	unsigned int board_reg_base;

	// function dispatch table
	p_camera_function_t camera_functions;

	/*
	 * FILLED PARAMTER
	 */
	int dma_channels[3];
	unsigned int capture_status;

	unsigned planeOffset[VIDEO_MAX_FRAME][3];
	unsigned planeBytes[VIDEO_MAX_FRAME][3];

	/*
	 * INTERNALLY USED: DON'T TOUCH!
	 */
	unsigned int block_number, block_size, block_number_max;
	unsigned int block_header, block_tail;
	unsigned int fifo0_descriptors_virtual, fifo0_descriptors_physical;
	unsigned int fifo1_descriptors_virtual, fifo1_descriptors_physical;
	unsigned int fifo2_descriptors_virtual, fifo2_descriptors_physical;
	unsigned int fifo0_num_descriptors;
	unsigned int fifo1_num_descriptors;
	unsigned int fifo2_num_descriptors;
	unsigned int fifo0_transfer_size;
	unsigned int fifo1_transfer_size;
	unsigned int fifo2_transfer_size;

	struct page **page_array;

	unsigned int pages_allocated;
	unsigned int page_aligned_block_size;
	unsigned int pages_per_block;
	unsigned int pages_per_fifo0;
	unsigned int pages_per_fifo1;
	unsigned int pages_per_fifo2;

	#ifdef CONFIG_DPM
	struct pm_dev *pmdev;
	#endif
	int dma_started;
};

/*
Prototypes
*/
/***********************************************************************
 *
 * Init/Deinit APIs
 *
 ***********************************************************************/
// Setup the sensor type, configure image capture format (RGB, yuv 444, yuv 422, yuv 420, packed | planar, MJPEG) regardless
// of current operating mode (i.e. sets mode for both still capture and video capture)
int camera_init( p_camera_context_t camera_context );

// Power off sensor
int camera_deinit( p_camera_context_t camera_context );

/***********************************************************************
 *
 * Capture APIs
 *
 ***********************************************************************/
// Set the image format
int camera_set_capture_format( p_camera_context_t camera_context);

// take a picture and copy it into the ring buffer
int camera_capture_still_image( p_camera_context_t camera_context, unsigned int block_id );

// capture motion video and copy it the ring buffer
int camera_start_video_capture( p_camera_context_t camera_context, unsigned int block_id);

// disable motion video image capture
void camera_stop_video_capture( p_camera_context_t camera_context );

// skip frame before capture video or still image
void camera_skip_frame( p_camera_context_t camera_context, int skip_frame_num );

int camera_func_ov9640_command(p_camera_context_t camera_context, unsigned int cmd, void *param);

/***********************************************************************
 *
 * Flow Control APIs
 *
 ***********************************************************************/
// continue capture image to next available buffer
// Returns the continued buffer id, -1 means buffer full and no transfer started
void camera_continue_transfer( p_camera_context_t camera_context );

// Return 1: there is available buffer, 0: buffer is full
int camera_next_buffer_available( p_camera_context_t camera_context );

// Application supplies the FrameBufferID to the driver to tell it that the application has completed processing of the given frame buffer, and that buffer is now available for re-use.
void camera_release_frame_buffer( p_camera_context_t camera_context, unsigned int frame_buffer_id );

// Returns the FrameBufferID for the first filled frame
// Note: -1 represents buffer empty
int camera_get_first_frame_buffer_id( p_camera_context_t camera_context );

/*
Returns the FrameBufferID for the last filled frame, this would be used if we were polling for image completion data,
or we wanted to make sure there were no frames waiting for us to process.
Note: -1 represents buffer empty
*/
int camera_get_last_frame_buffer_id( p_camera_context_t camera_context );

/***********************************************************************
 *
 * Buffer Info APIs
 *
 ***********************************************************************/
// Return: the number of frame buffers allocated for use.
unsigned int camera_get_num_frame_buffers( p_camera_context_t camera_context );

/* 
FrameBufferID is a number between 0 and N-1, where N is the total number of frame buffers in use.
Returns the address of the given frame buffer.
The application will call this once for each frame buffer at application initialization only.
*/
void* camera_get_frame_buffer_addr( p_camera_context_t camera_context, unsigned int frame_buffer_id );

// Return the block id
int camera_get_frame_buffer_id( p_camera_context_t camera_context, void* address );

/***********************************************************************
 *
 * Frame rate APIs
 *
 ***********************************************************************/
// Set desired frame rate
void camera_set_capture_frame_rate( p_camera_context_t camera_context );

// return current setting
void camera_get_capture_frame_rate( p_camera_context_t camera_context );

/***********************************************************************
 *
 * Interrupt APIs
 *
 ***********************************************************************/
// set interrupt mask
void camera_set_int_mask( p_camera_context_t camera_context, unsigned int mask );

// get interrupt mask
unsigned int camera_get_int_mask( p_camera_context_t camera_context );

// clear interrupt status
void camera_clear_int_status( p_camera_context_t camera_context, unsigned int status );

// gpio init
void camera_gpio_init(void);
void camera_gpio_deinit(void);

// ci functions
void ci_reset(void);

// dma functions
extern void start_dma_transfer(p_camera_context_t camera_context, unsigned block_id);
extern void stop_dma_transfer(p_camera_context_t camera_context);
extern int camera_ring_buf_init(p_camera_context_t camera_context);

// power management deep idle
#define USE_CAM_IPM_HOOK
extern int cam_ipm_hook(void);
extern void cam_ipm_unhook(void);

// lock functions
extern void camera_lock(void);
extern void camera_unlock(void);
extern int camera_trylock(void);

/*
Image format definition
*/
#define CAMERA_IMAGE_FORMAT_MAX                CAMERA_IMAGE_FORMAT_YCBCR444_PLANAR

#define CIBR0_PHY	(0x50000000 + 0x28)
#define CIBR1_PHY	(0x50000000 + 0x30)
#define CIBR2_PHY	(0x50000000 + 0x38)

/*
 * It is required to have at least 3 frames in buffer
 * in current implementation
 */
#define FRAMES_IN_BUFFER    	3
#define SINGLE_DESC_TRANS_MAX  	 PAGE_SIZE

#endif
