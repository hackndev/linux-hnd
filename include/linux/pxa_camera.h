/*
 *  pxa_camera.h
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
Author                 Date          Number     Description of Changes
----------------   ------------    ----------   -------------------------
Wang Fei/w20239     12/19/2003      LIBdd35749   Created
Wang Fei/w20239     02/05/2004      LIBdd74309   Set frame rate in video mode
Wang Fei/w20239     03/08/2004      LIBdd84578   Photo effects setting
Wang Fei/w20239     11/01/2004      LIBxxxxxxx   Change to SDK style
*/

/*================================================================================
                                 INCLUDE FILES
================================================================================*/
#ifndef __PXA_CAMERA_H__ 
#define __PXA_CAMERA_H__ 

/*!
 *  General description of the Motorola A780/E680 video device driver:
 *
 *  The Motorola A780/E680 video device is based on V4L (video for linux) 1.0, however not 
 *  all V4L features are supported. 
 *  There are also some additional extensions included for specific requirements beyond V4L.
 *   
 *  The video device driver has a "character special" file named /dev/video0. Developers can 
 *  access the video device via the file operator interfaces.
 *  Six file operator interfaces are supported:
 *     open
 *     ioctl
 *     mmap
 *     poll/select
 *     read
 *     close
 *  For information on using these fuctions, please refer to the standard linux 
 *  development documents.
 *  
 *  These four ioctl interfaces are important for getting the video device to work properly:
 *     VIDIOCGCAP       Gets the video device capability
 *     VIDIOCCAPTURE    Starts/stops the video capture 
 *     VIDIOCGMBUF      Gets the image frame buffer map info
 *     VIDIOCSWIN       Sets the picture size    
 *  These interfaces are compatible with V4L 1.0. Please refer to V4L documents for more details.
 *  sample.c demonstrates their use.
 * 
 *  The following ioctl interfaces are Motorola-specific extensions. These are not compatible with V4L 1.0.   
 *   WCAM_VIDIOCSCAMREG   	
 *   WCAM_VIDIOCGCAMREG   	
 *   WCAM_VIDIOCSCIREG    	
 *   WCAM_VIDIOCGCIREG    	
 *   WCAM_VIDIOCSINFOR	    
 *   WCAM_VIDIOCGINFOR
 *   WCAM_VIDIOCSSSIZE
 *   WCAM_VIDIOCSOSIZE
 *   WCAM_VIDIOCGSSIZE
 *   WCAM_VIDIOCGOSIZE
 *   WCAM_VIDIOCSFPS
 *   WCAM_VIDIOCSNIGHTMODE
 *   WCAM_VIDIOCSSTYLE      
 *   WCAM_VIDIOCSLIGHT      
 *   WCAM_VIDIOCSBRIGHT     
 *   WCAM_VIDIOCSBUFCOUNT   
 *   WCAM_VIDIOCGCURFRMS    
 *   WCAM_VIDIOCGSTYPE      
 *   WCAM_VIDIOCSCONTRAST   
 *   WCAM_VIDIOCSFLICKER 
 *   Detailed information about these constants are described below.   
 * 
 *  sample.c demonstrates most features of the Motorola A780/E680 video device driver.
 *    - Opening/closing the video device
 *    - Initializing the video device driver
 *    - Displaying the video image on a A780/E680 LCD screen     
 *    - Changing the image size
 *    - Changing the style
 *    - Changing the light mode
 *    - Changing the brightness
 *    - Capturing and saving a still picture
 */

/*!
 * These are the registers for the read/write camera module and the CIF 
 * (Intel PXA27x processer quick capture interface)
 * The following 4 ioctl interfaces are used for debugging and are not open to developers
 */
#define WCAM_VIDIOCSCAMREG   	211
#define WCAM_VIDIOCGCAMREG   	212
#define WCAM_VIDIOCSCIREG    	213
#define WCAM_VIDIOCGCIREG    	214

/*!
 * WCAM_VIDIOCSINFOR Sets the image data format
 *  
 * The following code sets the image format to YCbCr422_planar
 *
 *   struct {int val1, val2;}format;
 *   format.val1 = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
 *   format.val2 = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
 *   //dev is the video device handle 
 *   ioctl(dev, WCAM_VIDIOCSINFOR, &format);
 *
 * Remarks:
 *   val1 is the output format of the camera module, val2 is the output format of the CIF (capture  
 *   interface). Image data from the camera module can be converted to other formats through
 *   the CIF. val2 specifies the final output format of the video device.
 *   
 *   For more description on CIF please refer to the Intel PXA27x processor family developer's manual.
 *     http://www.intel.com/design/pca/prodbref/253820.html 
 */
#define WCAM_VIDIOCSINFOR	    215

/*
 * WCAM_VIDIOCGINFOR Gets the image data format
 *
 *  struct {int val1, val2;}format;
 *  ioctl(dev, WCAM_VIDIOCGINFOR, &format);
 */
#define WCAM_VIDIOCGINFOR	    216
 
/*! 
 *  WCAM_VIDIOCSSSIZE Sets the sensor window size
 *
 *   The following code sets the sensor size to 640 X 480:
 *
 *   struct {unsigned short w, h;}sensor_size;
 *   sensor_size.w = 640;
 *   sensor_size.h = 480;
 *   //dev is the video device handle 
 *   ioctl(dev, WCAM_VIDIOCSSSIZE, &sensor_size);
 *
 *  Remarks:
 *    The sensor size is restricted by the video device capability. 
 *    VIDIOCGCAP can get the video device capability.
 *    The sensor size must be an even of multiple of 8. If not, the driver changes the sensor size to a multiple of 8.
 */
#define WCAM_VIDIOCSSSIZE        217

/*!
 * WCAM_VIDIOCSOSIZE Sets output size of the video device
 *
 *   The following code segment shows how to set the output size to 240 X 320:
 *
 *   struct {unsigned short w, h;}out_size;
 *   out_size.w = 240;
 *   out_size.h = 320;
 *   //dev is the video device handle 
 *   ioctl(dev, WCAM_VIDIOCSSSIZE, &out_size);
 *
 *  Remarks:
 *   In video mode, the output size must be less than 240X320. However, in still mode, the output  
 *   size is restricted by the video device capability and the sensor size.
 *   The output size must always be less than the sensor size, so if the developer changes the output size  
 *   to be greater than the sensor size, the video device driver may work abnormally.
 *   The width and height must also be a multiple of 8. If it is not, the driver changes the width and height size to a multiple of 8.
 *   The developer can modify the sensor size and the output size to create a digital zoom. 
 */
#define WCAM_VIDIOCSOSIZE        218 

/*!
 * WCAM_VIDIOCGSSIZE Gets the current sensor size.
 * 
 * The following code segment shows how to use this function:
 *
 *   struct {unsigned short w, h;}sensor_size;
 *   //dev is the video device handle 
 *   ioctl(dev, WCAM_VIDIOCGSSIZE, &sensor_size); 
 *   printf("sensor width is %d, sensor_height is %d\n", sensor_size.w, sensor_size.h);
 *
 */
#define WCAM_VIDIOCGSSIZE        219

/*!
 * WCAM_VIDIOCGOSIZE Gets the current output size.
 * 
 * The following code segment shows how to use this function:
 *
 *   struct {unsigned short w, h;}out_size;
 *   //dev is the video device handle 
 *   ioctl(dev, WCAM_VIDIOCGOSIZE, &out_size); 
 *   printf("output width is %d, output height is %d\n", out_size.w, out_size.h);
 *
 */
#define WCAM_VIDIOCGOSIZE        220 

/*!
 * WCAM_VIDIOCSFPS Sets the output frame rate (fps- frames per second) of the video device
 *
 * The following code segment shows how to use this function:
 *
 *   struct {int maxfps, minfps;}fps;
 *   fps.maxfps  = 15;
 *   fps.minfps  = 12;
 *   ioctl(dev, WCAM_VIDIOCSFPS, &fps);
 *
 * Remarks:
 *   The minimum value of maxfps is 1; the maximum value is 15.  minfps must not exceed maxfps. 
 *   The default value of fps is [15, 10].
 *   minfps and maxfps only suggest a fps range. The video device driver will select 
 *   an appropriate value automatically. The actual fps depends on environmental circumstances  
 *   such as brightness, illumination, etc. 
 *   sample.c illustrates how to calculate actual frame rate.
 *   
 */
#define WCAM_VIDIOCSFPS          221 

/*!
 * WCAM_VIDIOCSNIGHTMODE Sets the video device capture mode. 
 *
 * The capture mode can use the following values
 *
 *   V4l_NM_AUTO     Auto mode(default value)
 *   V4l_NM_NIGHT    Night mode
 *   V4l_NM_ACTION   Action mode
 *  
 * The following code segment shows how to set the video device to night mode:
 *
 *   ioctl(dev, WCAM_VIDIOCSNIGHTMODE, V4l_NM_NIGHT);
 *
 * Remarks:
 *   Different capture modes represent different sensor exposure times. Night mode represents a longer 
 *   exposure time. Setting the video device to night mode can capture high quality image data in low light environments.
 *   Action mode represents a shorter exposure time. This is used for capture moving objects. When working in auto mode, the 
 *   video device will select an appropriate exposure time automatically.
 *
 *   Not all camera modules support this interface. Developers can also use WCAM_VIDIOCSFPS to achieve similar results.
 *   Smaller minfps represent longer exposure times.
 *
 */
#define WCAM_VIDIOCSNIGHTMODE    222 

/*!
 * WCAM_VIDIOCSSTYLE Sets the image style.
 *
 * The following styles are supported:
 *
 *   V4l_STYLE_NORMAL        Normal (default value)
 *   V4l_STYLE_BLACK_WHITE   Black and white 
 *   V4l_STYLE_SEPIA         Sepia
 *   V4l_STYLE_SOLARIZE      Solarized (not supported by all camera modules)
 *   V4l_STYLE_NEG_ART       Negative (not supported by all camera modules)
 *
 * The following code segment demonstrates how to set the image style to black and white:
 *
 *   ioctl(dev, WCAM_VIDIOCSSTYLE, V4l_STYLE_BLACK_WHITE);
 *
 */
#define WCAM_VIDIOCSSTYLE        250  

/*!
 * WCAM_VIDIOCSLIGHT Sets the image light mode
 * 
 * The following light modes are supported:
 *   V4l_WB_AUTO           Auto mode(default)
 *   V4l_WB_DIRECT_SUN     Direct sun
 *   V4l_WB_INCANDESCENT   Incandescent
 *   V4l_WB_FLUORESCENT    Fluorescent
 * 
 * The following code sets the image light mode to incandescent:
 *   ioctl(dev, WCAM_VIDIOCSLIGHT, V4l_WB_INCANDESCENT);
 */
#define WCAM_VIDIOCSLIGHT        251

/*!
 * WCAM_VIDIOCSBRIGHT Sets the brightness of the image (exposure compensation value)
 *  
 *  parameter value      exposure value
 *   -4                     -2.0 EV
 *   -3                     -1.5 EV
 *   -2                     -1.0 EV
 *   -1                     -0.5 EV
 *    0                      0.0 EV(default value)
 *    1                     +0.5 EV
 *    2                     +1.0 EV
 *    3                     +1.5 EV
 *    4                     +2.0 EV
 *
 * The following code segment sets the brightness to 2.0 EV
 *   ioctl(dev, WCAM_VIDIOCSBRIGHT, 4);
 */
#define WCAM_VIDIOCSBRIGHT       252

/*!
 * Sets the frame buffer count for video mode. The default value is 3.
 *
 * Remarks:
 * The video device driver maintains some memory for buffering image data in the kernel space. When working in video mode,
 * there are at least 3 frame buffers in the driver.  In still mode, there is only 1 frame buffer.
 * This interface is not open to SDK developers.
 * 
 */
#define WCAM_VIDIOCSBUFCOUNT     253  

/*!
 * Gets the current available frames
 *
 * The following code demonstrates getting the current available frames:
 *
 *   struct {int first, last;}cur_frms;
 *   ioctl(dev, WCAM_VIDIOCGCURFRMS, &cur_frms);
 *
 * Remarks:
 *   cur_frms.first represents the earliest frame in frame buffer  
 *   cur_frms.last  represents the latest or most recent frame in frame buffer.
 */
#define WCAM_VIDIOCGCURFRMS      254  

/*!
 * Gets the camera sensor type
 *
 *  unsigned int sensor_type
 *  ioctl(dev, WCAM_VIDIOCGSTYPE, &sensor_type);
 *  if(sensor_type == CAMERA_TYPE_ADCM_2700)
 *  {
 *     printf("Agilent ADCM2700");
 *  }
 *
 * Remarks:
 *   For all possible values of sensor_type please refer to the sensor definitions below.
 */
#define WCAM_VIDIOCGSTYPE        255 

/*!
 * Sets the image contrast
 * Not open to SDK developers
 */
#define WCAM_VIDIOCSCONTRAST     256

/*!
 * Sets the flicker frequency(50hz/60hz)
 * Not open to SDK developers
 */
#define WCAM_VIDIOCSFLICKER      257



typedef enum V4l_NIGHT_MODE
{
   V4l_NM_AUTO,
   V4l_NM_NIGHT,
   V4l_NM_ACTION
}V4l_NM;

typedef enum V4l_PIC_STYLE
{
   V4l_STYLE_NORMAL,
   V4l_STYLE_BLACK_WHITE,
   V4l_STYLE_SEPIA,
   V4l_STYLE_SOLARIZE,
   V4l_STYLE_NEG_ART
}V4l_PIC_STYLE;

typedef enum V4l_PIC_WB
{
   V4l_WB_AUTO,
   V4l_WB_DIRECT_SUN,
   V4l_WB_INCANDESCENT,
   V4l_WB_FLUORESCENT
}V4l_PIC_WB;



/*!
 *Image format definitions
 *Remarks:
 *  Although not all formats are supported by all camera modules, YCBCR422_PLANAR is widely supported. 
 *  For detailed information on each format please refer to the Intel PXA27x processor family developer's manual. 
 *     http://www.intel.com/design/pca/prodbref/253820.html
 * 
 */
#define CAMERA_IMAGE_FORMAT_RAW8                0
#define CAMERA_IMAGE_FORMAT_RAW9                1
#define CAMERA_IMAGE_FORMAT_RAW10               2
                                                                                                                             
#define CAMERA_IMAGE_FORMAT_RGB444              3
#define CAMERA_IMAGE_FORMAT_RGB555              4
#define CAMERA_IMAGE_FORMAT_RGB565              5
#define CAMERA_IMAGE_FORMAT_RGB666_PACKED       6
#define CAMERA_IMAGE_FORMAT_RGB666_PLANAR       7
#define CAMERA_IMAGE_FORMAT_RGB888_PACKED       8
#define CAMERA_IMAGE_FORMAT_RGB888_PLANAR       9
#define CAMERA_IMAGE_FORMAT_RGBT555_0          10  //RGB+Transparent bit 0
#define CAMERA_IMAGE_FORMAT_RGBT888_0          11
#define CAMERA_IMAGE_FORMAT_RGBT555_1          12  //RGB+Transparent bit 1
#define CAMERA_IMAGE_FORMAT_RGBT888_1          13
                                                                                                                             
#define CAMERA_IMAGE_FORMAT_YCBCR400           14
#define CAMERA_IMAGE_FORMAT_YCBCR422_PACKED    15
#define CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR    16
#define CAMERA_IMAGE_FORMAT_YCBCR444_PACKED    17
#define CAMERA_IMAGE_FORMAT_YCBCR444_PLANAR    18

/*!
 *VIDIOCCAPTURE arguments
 */
#define STILL_IMAGE				1
#define VIDEO_START				0
#define VIDEO_STOP				-1

/*!
 *Sensor type definitions
 */
#define CAMERA_TYPE_ADCM_2650               1
#define CAMERA_TYPE_ADCM_2670               2
#define CAMERA_TYPE_ADCM_2700               3
#define CAMERA_TYPE_OMNIVISION_9640         4
#define CAMERA_TYPE_MT9M111                 5
#define CAMERA_TYPE_MT9V111                 6
#define CAMERA_TYPE_ADCM3800                7
#define CAMERA_TYPE_OV9650                  8
#define CAMERA_TYPE_MAX                     CAMERA_TYPE_OV9650


/*
 * Definitions of the camera's i2c device
 */
#define CAMERA_I2C_WRITEW    101
#define CAMERA_I2C_WRITEB    102
#define CAMERA_I2C_READW     103
#define CAMERA_I2C_READB     104
#define CAMERA_I2C_DETECTID  105

struct camera_i2c_register {
    unsigned short  addr;
    union {
        unsigned short w;
        unsigned char b;
    } value;
};

struct camera_i2c_detectid {
    int buflen;
    char data[256];
};

//End of the camera's i2c device

#endif // __PXA_CAMERA_H__ 

