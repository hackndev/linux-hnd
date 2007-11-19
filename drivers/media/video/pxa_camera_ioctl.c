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


extern camera_context_t  *g_camera_context;

struct reg_set_s 
{
	int  val1;
	int  val2;
};

/*ioctl sub functions*/
static int pxa_camera_VIDIOCGCAP(p_camera_context_t cam_ctx, void * param)
{
    dbg_print("VIDIOCGCAP");
    /*
      add a vc member to camera context
    */
    if(copy_to_user(param, &(cam_ctx->vc), sizeof(struct video_capability)))
    {
        return -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCGWIN(p_camera_context_t cam_ctx, void * param)
{
    struct video_window vw;
    dbg_print("VIDIOCGWIN");
    vw.width  = cam_ctx->capture_width;
    vw.height = cam_ctx->capture_height;
    if(copy_to_user(param, &vw, sizeof(struct video_window)))
    {
        return -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCSWIN(p_camera_context_t cam_ctx, void * param)
{
    struct video_window vw;
    dbg_print("VIDIOCSWIN");
    if(copy_from_user(&vw, param, sizeof(vw))) 
    {
        dbg_print("VIDIOCSWIN get parameter error!");
        return  -EFAULT;
    } 
    
    if(vw.width > cam_ctx->vc.maxwidth  ||
       vw.height > cam_ctx->vc.maxheight || 
       vw.width < cam_ctx->vc.minwidth   || 
       vw.height < cam_ctx->vc.minheight) 
    {
        dbg_print("VIDIOCSWIN error parameter!");
        dbg_print("vw.width:%d, MAX_WIDTH:%d, MIN_WIDTH:%d", vw.width, cam_ctx->vc.maxwidth, cam_ctx->vc.minwidth);
        dbg_print("vw.height:%d, MAX_HEIGHT:%d, MIN_HEIGHT:%d", vw.width, cam_ctx->vc.maxheight, cam_ctx->vc.minheight);	
        return  -EFAULT;
    }
    
    //make it in an even multiple of 8 
    
    cam_ctx->capture_width  = (vw.width+7)/8;
    cam_ctx->capture_width  *= 8;
    
    cam_ctx->capture_height = (vw.height+7)/8;
    cam_ctx->capture_height *= 8;
    
    return camera_set_capture_format(cam_ctx);
}

static int pxa_camera_VIDIOCSPICT(p_camera_context_t cam_ctx, void * param)
{
    struct video_picture vp;
    dbg_print("VIDIOCSPICT");
    if(copy_from_user(&vp, param, sizeof(vp))) 
    {
        return  -EFAULT;
    }
    cam_ctx->capture_output_format = vp.palette;

    return  camera_set_capture_format(cam_ctx);
    
}

static int pxa_camera_VIDIOCGPICT(p_camera_context_t cam_ctx, void * param)
{
    struct video_picture vp;
    dbg_print("VIDIOCGPICT");
    vp.palette = cam_ctx->capture_output_format;
    if(copy_to_user(param, &vp, sizeof(struct video_picture)))
    {
        return  -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCCAPTURE(p_camera_context_t cam_ctx, void * param)
{
    int capture_flag = (int)param;
    dbg_print("VIDIOCCAPTURE");
    if(capture_flag > 0) 
    {			
        dbg_print("Still Image capture!");
        camera_capture_still_image(cam_ctx, 0);
    }
    else if(capture_flag == 0) 
    {
        dbg_print("Video Image capture!");
        camera_start_video_capture(cam_ctx, 0);
    }
    else if(capture_flag == -1) 
    {
        dbg_print("Capture stop!"); 
        camera_set_int_mask(cam_ctx, 0x3ff);
        camera_stop_video_capture(cam_ctx);
    }
    else 
    {
        return  -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCGMBUF(p_camera_context_t cam_ctx, void * param)
{
    struct video_mbuf vm;
    int i;

    dbg_print("VIDIOCGMBUF");

    memset(&vm, 0, sizeof(vm));
    vm.size   = cam_ctx->buf_size;
    vm.frames = cam_ctx->block_number;
    for(i = 0; i < vm.frames; i++)
    {
        vm.offsets[i] = cam_ctx->page_aligned_block_size * i;
    }
    if(copy_to_user((void *)param, (void *)&vm, sizeof(vm)))
    {
        return  -EFAULT;
    }
    return 0;
}
static int pxa_camera_WCAM_VIDIOCSINFOR(p_camera_context_t cam_ctx, void * param)
{

    struct reg_set_s reg_s;
    int ret;
    dbg_print("WCAM_VIDIOCSINFOR");

    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    
    cam_ctx->capture_input_format = reg_s.val1;
    cam_ctx->capture_output_format = reg_s.val2;
    ret=camera_set_capture_format(cam_ctx);
    return  ret;
}
static int pxa_camera_WCAM_VIDIOCGINFOR(p_camera_context_t cam_ctx, void * param)
{
    struct reg_set_s reg_s;
    dbg_print("WCAM_VIDIOCGINFOR");
    reg_s.val1 = cam_ctx->capture_input_format;
    reg_s.val2 = cam_ctx->capture_output_format;
    if(copy_to_user(param, &reg_s, sizeof(int) * 2)) 
    {
        return -EFAULT;
    }
    return 0;
}
static int pxa_camera_WCAM_VIDIOCGCIREG(p_camera_context_t cam_ctx, void * param)
{

    int reg_value, offset;
    dbg_print("WCAM_VIDIOCGCIREG");
    if(copy_from_user(&offset, param, sizeof(int))) 
    {
        return  -EFAULT;
    }
    reg_value = ci_get_reg_value (offset);
    if(copy_to_user(param, &reg_value, sizeof(int)))
    {
        return -EFAULT;
    }
    return 0;
}

static int pxa_camera_WCAM_VIDIOCSCIREG(p_camera_context_t cam_ctx, void * param)
{

    struct reg_set_s reg_s;
    dbg_print("WCAM_VIDIOCSCIREG");
    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    ci_set_reg_value (reg_s.val1, reg_s.val2);
    return 0;

}
    
/*get sensor size */
static int pxa_cam_WCAM_VIDIOCGSSIZE(p_camera_context_t cam_ctx, void * param)
{
   struct adcm_window_size{u16 width, height;} size;
   dbg_print("WCAM_VIDIOCGSSIZE");  
   size.width = cam_ctx->sensor_width;
   size.height = cam_ctx->sensor_height;

   if(copy_to_user(param, &size, sizeof(struct adcm_window_size)))
   {
       return -EFAULT;
   }
  return 0;
}
         
/*get output size*/
static int pxa_cam_WCAM_VIDIOCGOSIZE(p_camera_context_t cam_ctx, void * param)
{

   struct adcm_window_size{u16 width, height;}size;
   dbg_print("WCAM_VIDIOCGOSIZE");  
   size.width  = cam_ctx->capture_width;
   size.height = cam_ctx->capture_height;
   if(copy_to_user(param, &size, sizeof(struct adcm_window_size)))
   {
       return -EFAULT;
   }
   return 0;
}



/*set frame buffer count*/
static int pxa_cam_WCAM_VIDIOCSBUFCOUNT(p_camera_context_t cam_ctx, void * param)
{
	int count;
	dbg_print("");
 	if (copy_from_user(&count, param, sizeof(int)))
		return -EFAULT;
   
   if(cam_ctx->block_number_max == 0) {
     dbg_print("windows size or format not setting!!");
     return -EFAULT;
   }
   
   if(count < FRAMES_IN_BUFFER)
   {
      count = FRAMES_IN_BUFFER;
   }
   
   if(count > cam_ctx->block_number_max)
   {
      count = cam_ctx->block_number_max;
   }
      

   cam_ctx->block_number = count;
   cam_ctx->block_header = cam_ctx->block_tail = 0;
   //generate dma descriptor chain
   update_dma_chain(cam_ctx);
     
   if(copy_to_user(param, &count, sizeof(int)))
   {
     return -EFAULT;
   }
   
   return 0;
}
         
/*get cur avaliable frames*/     
static int pxa_cam_WCAM_VIDIOCGCURFRMS(p_camera_context_t cam_ctx, void * param)
{
  //dbg_print("");
  struct {int first, last;}pos;
  
  pos.first = cam_ctx->block_tail;
  pos.last  = cam_ctx->block_header;
  
  if(copy_to_user(param, &pos, sizeof(pos)))
  {
     return -EFAULT;
  }
  return 0;
}

/*get sensor type*/
static int pxa_cam_WCAM_VIDIOCGSTYPE(p_camera_context_t cam_ctx, void * param)
{
  dbg_print("");
  if(copy_to_user(param, &(cam_ctx->sensor_type), sizeof(cam_ctx->sensor_type)))
  {
    return -EFAULT;
  }
  return 0;
}

int pxa_camera_ioctl(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	void *param = (void *) arg;

   	switch (cmd) 
    {
        /*get capture capability*/
    case VIDIOCGCAP:
        return pxa_camera_VIDIOCGCAP(g_camera_context, param);
        /* get capture size */
    case VIDIOCGWIN:
        return  pxa_camera_VIDIOCGWIN(g_camera_context, param);

        /* set capture size. */
    case VIDIOCSWIN:
        return pxa_camera_VIDIOCSWIN(g_camera_context, param);
        /*set capture output format*/
    case VIDIOCSPICT:
        return pxa_camera_VIDIOCSPICT(g_camera_context, param);

        /*get capture output format*/
    case VIDIOCGPICT:
        return pxa_camera_VIDIOCGPICT(g_camera_context, param);

        /*start capture */
    case VIDIOCCAPTURE:
        return pxa_camera_VIDIOCCAPTURE(g_camera_context, param);

        /* mmap interface */
    case VIDIOCGMBUF:
         return pxa_camera_VIDIOCGMBUF(g_camera_context, param);

        /* Application extended IOCTL.  */
        /* Register access interface	*/
    case WCAM_VIDIOCSINFOR:
         return pxa_camera_WCAM_VIDIOCSINFOR(g_camera_context, param);

        /*get capture format*/
    case WCAM_VIDIOCGINFOR:
         return pxa_camera_WCAM_VIDIOCGINFOR(g_camera_context, param);

        /*get ci reg value*/
    case WCAM_VIDIOCGCIREG:
        return pxa_camera_WCAM_VIDIOCGCIREG(g_camera_context, param);

        /*set ci reg*/
    case WCAM_VIDIOCSCIREG:
        return pxa_camera_WCAM_VIDIOCSCIREG(g_camera_context, param);
            
        /*get sensor size */  
    case WCAM_VIDIOCGSSIZE:
         return pxa_cam_WCAM_VIDIOCGSSIZE(g_camera_context, param);
    
         /*get output size*/
    case WCAM_VIDIOCGOSIZE:
         return pxa_cam_WCAM_VIDIOCGOSIZE(g_camera_context, param);

         /*set frame buffer count*/     
    case WCAM_VIDIOCSBUFCOUNT:
         return pxa_cam_WCAM_VIDIOCSBUFCOUNT(g_camera_context, param);
         
         /*get cur avaliable frames*/     
    case WCAM_VIDIOCGCURFRMS:
         return pxa_cam_WCAM_VIDIOCGCURFRMS(g_camera_context, param);
         
         /*get cur sensor type*/
    case WCAM_VIDIOCGSTYPE:
         return pxa_cam_WCAM_VIDIOCGSTYPE(g_camera_context, param);
    
    default:
         return  g_camera_context->camera_functions->command(g_camera_context, cmd, param);
    }
    
   return 0;
}
