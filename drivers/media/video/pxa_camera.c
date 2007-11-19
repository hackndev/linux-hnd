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

wait_queue_head_t  camera_wait_q;	
camera_context_t  *g_camera_context;

/* /dev/videoX registration number */
static int    minor     = 0;
static int    ci_dma_y  = -1;
static int    ci_dma_cb = -1;
static int    ci_dma_cr = -1;

static int    task_waiting      = 0;
static int    still_image_mode  = 0;
static int    still_image_rdy   = 0;
static int    first_video_frame = 0;


static const CI_IMAGE_FORMAT FORMAT_MAPPINGS[] = 
{
        CI_RAW8,                   //RAW
        CI_RAW9,
        CI_RAW10,

        CI_RGB444,                 //RGB
        CI_RGB555,
        CI_RGB565,
        CI_RGB666_PACKED,          //RGB Packed 
        CI_RGB666,
        CI_RGB888_PACKED,
        CI_RGB888,
        CI_RGBT555_0,              //RGB+Transparent bit 0
        CI_RGBT888_0,
        CI_RGBT555_1,              //RGB+Transparent bit 1  
        CI_RGBT888_1,
    
        CI_INVALID_FORMAT,
        CI_YCBCR422,               //YCBCR
        CI_YCBCR422_PLANAR,        //YCBCR Planaried
        CI_INVALID_FORMAT,
        CI_INVALID_FORMAT
};

/***********************************************************************
 *
 * Interrupt APIs
 *
 ***********************************************************************/
// set interrupt mask 
void camera_set_int_mask(p_camera_context_t cam_ctx, unsigned int mask)
{
	pxa_dma_desc * end_des_virtual;
	int dma_interrupt_on, i;

	// set CI interrupt
	ci_set_int_mask( mask & CI_CICR0_INTERRUPT_MASK );

	// set dma end interrupt
	if( mask & CAMERA_INTMASK_END_OF_DMA )
		dma_interrupt_on = 1;
	else
		dma_interrupt_on = 0;

	// set fifo0 dma chains' flag
	end_des_virtual = (pxa_dma_desc*)cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;

    for(i=0; i<cam_ctx->block_number; i++) 
    {
        if(dma_interrupt_on)
            end_des_virtual->dcmd |= DCMD_ENDIRQEN;
        else
            end_des_virtual->dcmd &= ~DCMD_ENDIRQEN;

        end_des_virtual += cam_ctx->fifo0_num_descriptors;
    }
}
 
// get interrupt mask 
unsigned int camera_get_int_mask(p_camera_context_t cam_ctx)
{
	pxa_dma_desc *end_des_virtual;
	unsigned int ret;

	// get CI mask
	ret = ci_get_int_mask();
	
	// get dma end mask
	end_des_virtual = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;

	if(end_des_virtual->dcmd & DCMD_ENDIRQEN)
    {
		ret |= CAMERA_INTMASK_END_OF_DMA;
    }
	return ret;   
} 

// clear interrupt status
void camera_clear_int_status( p_camera_context_t camera_context, unsigned int status )
{
	ci_clear_int_status( (status & 0xFFFF) );   
}

static void camera_free_dma_irq()
{
     if(ci_dma_y>=0) 
    {
        pxa_free_dma(ci_dma_y);
        ci_dma_y = 0;
    }
    if(ci_dma_cb>=0) 
    {
        pxa_free_dma(ci_dma_cb);
        ci_dma_cb = 0;
    }
    if(ci_dma_cr>=0) 
    {
        pxa_free_dma(ci_dma_cr);
        ci_dma_cr = 0;
    }
	DRCMR68 = 0;
	DRCMR69 = 0;
	DRCMR70 = 0;
}

/***********************************************************************************
 * Application interface 							   *
 ***********************************************************************************/

static struct video_device vd;

int pxa_camera_mem_init(void)
{
   g_camera_context = kmalloc(sizeof(struct camera_context_s), GFP_KERNEL);

    if(g_camera_context == NULL)
    {
    	dbg_print( "PXA_CAMERA: Cann't allocate buffer for camera control structure \n");
        return -1;
    }
	
    memset(g_camera_context, 0, sizeof(struct camera_context_s));
    
    dbg_print("success!"); 
    return 0;
}

int camera_init(p_camera_context_t camera_context)
{
#warning camera_init is unimplemented
    return 0;
}

static int pxa_dma_buffer_init(p_camera_context_t camera_context)
{
	struct page    *page;
	unsigned int	pages;
	unsigned int	page_count;

	camera_context->pages_allocated = 0;

	pages = (PAGE_ALIGN(camera_context->buf_size) / PAGE_SIZE);

	camera_context->page_array = (struct page **)
                                 kmalloc(pages * sizeof(struct page *),
                                 GFP_KERNEL);
                               
	if(camera_context->page_array == NULL)
	{
		return -ENOMEM;
	}
	memset(camera_context->page_array, 0, pages * sizeof(struct page *));

	for(page_count = 0; page_count < pages; page_count++)
	{
		page = alloc_page(GFP_KERNEL);
		if(page == NULL)
		{
			goto error;
		}
		camera_context->page_array[page_count] = page;
		init_page_count(page);
		SetPageReserved(page);
	}
	camera_context->buffer_virtual = vmap(camera_context->page_array,
						pages, VM_MAP, PAGE_KERNEL);
	if(camera_context->buffer_virtual == NULL)
	{
		goto error;
	}

	camera_context->pages_allocated = pages;

	return 0;

error:
	for(page_count = 0; page_count < pages; page_count++)
	{
		if((page = camera_context->page_array[page_count]) != NULL)
		{
			ClearPageReserved(page);
			init_page_count(page);
			put_page(page);
		}
	}
	kfree(camera_context->page_array);

	return -ENOMEM;
}

static void pxa_dma_buffer_free(p_camera_context_t camera_context)
{
	struct page *page;
	int page_count;

	if(camera_context->buffer_virtual == NULL)
		return;

	vfree(camera_context->buffer_virtual);

	for(page_count = 0; page_count < camera_context->pages_allocated; page_count++)
	{
		if((page = camera_context->page_array[page_count]) != NULL)
		{
			ClearPageReserved(page);
			init_page_count(page);
			put_page(page);
		}
	}
	kfree(camera_context->page_array);
}

int pxa_camera_mem_deinit(void)
{
    if(g_camera_context)
    {
        if(g_camera_context->dma_descriptors_virtual != NULL) 
        {
	    dma_free_coherent(NULL,
		    g_camera_context->dma_descriptors_size *
		    				sizeof(pxa_dma_desc),
		    g_camera_context->dma_descriptors_virtual,
		    g_camera_context->dma_descriptors_physical);
//            kfree(g_camera_context->dma_descriptors_virtual);

          g_camera_context->dma_descriptors_virtual = NULL;

        }
       if(g_camera_context->buffer_virtual != NULL)  
       {
        pxa_dma_buffer_free(g_camera_context);		     
        g_camera_context->buffer_virtual = NULL;
       }
       kfree(g_camera_context);
       g_camera_context = NULL;
    }
    
    return 0;
}


static int pxa_camera_open(struct inode *inode, struct file *file)
{
    unsigned int minor = iminor(inode);
    camera_context_t *cam_ctx;
    dbg_print("start...");
    /*
      According to Peter's suggestion, move the code of request camera IRQ and DMQ channel to here
    */     
    /* 1. mapping CI registers, so that we can access the CI */
   	
    ci_dma_y = pxa_request_dma("CI_Y",DMA_PRIO_HIGH, pxa_ci_dma_irq_y, &vd);

    if(ci_dma_y < 0) 
    {
	    camera_free_dma_irq();
      dbg_print( "PXA_CAMERA: Cann't request DMA for Y\n");
      return -EIO;
    }
    dbg_print( "PXA_CAMERA: Request DMA for Y successfully [%d]\n",ci_dma_y);
   
    ci_dma_cb = pxa_request_dma("CI_Cb",DMA_PRIO_HIGH, pxa_ci_dma_irq_cb, &vd);
    if(ci_dma_cb < 0) 
    {
	    camera_free_dma_irq();
	dbg_print( "PXA_CAMERA: Cann't request DMA for Cb\n");
	return -EIO;
    } 
    dbg_print( "PXA_CAMERA: Request DMA for Cb successfully [%d]\n",ci_dma_cb);
    
    ci_dma_cr = pxa_request_dma("CI_Cr",DMA_PRIO_HIGH, pxa_ci_dma_irq_cr, &vd);
    if(ci_dma_cr < 0) 
    {
	    camera_free_dma_irq();
	dbg_print( "PXA_CAMERA: Cann't request DMA for Cr\n");
	return -EIO;
    }
    
    dbg_print( "PXA_CAMERA: Request DMA for Cr successfully [%d]\n",ci_dma_cr);
   
    DRCMR68 = ci_dma_y | DRCMR_MAPVLD;
    DRCMR69 = ci_dma_cb | DRCMR_MAPVLD;
    DRCMR70 = ci_dma_cr | DRCMR_MAPVLD;	


    init_waitqueue_head(&camera_wait_q);
   
    /* alloc memory for camera context */
    if(pxa_camera_mem_init())
    {
	    camera_free_dma_irq();
      dbg_print("memory allocate failed!");
      return -1;
    }

    cam_ctx = g_camera_context;
    
    /*
     camera_init call init function for E680
    */
    if(camera_init(cam_ctx))
    {
       dbg_print("camera_init faile!");
	    camera_free_dma_irq();
       pxa_camera_mem_deinit();
       return -1;
    }
    
    /*
       allocate memory for dma descriptors 
       init function of each sensor should set proper value for cam_ctx->buf_size 
    */
   	cam_ctx->dma_started = 0;
    cam_ctx->dma_descriptors_virtual = dma_alloc_coherent(NULL, (cam_ctx->dma_descriptors_size) * sizeof(pxa_dma_desc),
                                                        (void *)&(cam_ctx->dma_descriptors_physical), GFP_KERNEL);
    if(cam_ctx->dma_descriptors_virtual == NULL)
    {
       dbg_print("consistent alloc memory for dma_descriptors_virtual fail!");
	    camera_free_dma_irq();
       pxa_camera_mem_deinit();
       return -1;
    }


    /*
      alloc memory for picture buffer 
      init function of each sensor should set proper value for cam_ctx->buf_size 
    */    
    if(pxa_dma_buffer_init(cam_ctx) != 0)
    {
      dbg_print("alloc memory for buffer_virtual  %d bytes fail!", g_camera_context->buf_size);
	    camera_free_dma_irq();
      pxa_camera_mem_deinit();
      return -1;
    }
     
    /*
      set default size and capture format
      init function of each sensor should set proper value 
      for capture_width, capture_height, etc. of camera context 
    */    
	if(camera_set_capture_format(cam_ctx) != 0)
	{
		dbg_print("camera function init error! capture format!");
	    camera_free_dma_irq();
		pxa_camera_mem_deinit();
		return -1;
    }
    dbg_print("PXA_CAMERA: pxa_camera_open success!");
    return 0;
}



static int pxa_camera_close(struct inode *inode, struct file *file)
{
    camera_deinit(g_camera_context);
    pxa_camera_mem_deinit();
    camera_free_dma_irq();
    dbg_print("PXA_CAMERA: pxa_camera_close\n");
    return 0;
}

#define PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, size) \
do { \
	unsigned int len; \
	unsigned int remain_size = size; \
	while (remain_size > 0) { \
		if(remain_size > PAGE_SIZE) \
			len = PAGE_SIZE; \
		else \
			len = remain_size; \
		if(copy_to_user(buf, page_address(*p_page), len)) \
			return -EFAULT; \
		remain_size -= len; \
		buf += len; \
		p_page++; \
	} \
} while (0);

static ssize_t pxa_camera_write(struct file *file, const char __user *data,
				size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t pxa_camera_read(struct file *file, char   __user *buf,
                            size_t count, loff_t *ppos)
{

	struct page **p_page;

	camera_context_t *cam_ctx = g_camera_context;

	if(still_image_mode == 1 && still_image_rdy == 1) 
    {
		p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

		still_image_rdy = 0;
		return cam_ctx->block_size;
	}

	if(still_image_mode == 0)
	{
		if(first_video_frame == 1)
			cam_ctx->block_tail = cam_ctx->block_header;
		else
			cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;
	}

	first_video_frame = 0;

	if(cam_ctx->block_header == cam_ctx->block_tail)  
    {
		task_waiting = 1;
		interruptible_sleep_on (&camera_wait_q);
	}

	p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

	return cam_ctx->block_size;
}

static int pxa_camera_mmap(struct file *file, struct vm_area_struct *vma)
{
   	unsigned long start = vma->vm_start;
	unsigned long size = (vma->vm_end - vma->vm_start);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	camera_context_t *cam_ctx = g_camera_context;
	struct page **p_page = cam_ctx->page_array;

	size = PAGE_ALIGN(size);
	if (remap_pfn_range(vma, start, page_to_phys(*p_page), size,
			    PAGE_SHARED)) {
			return -EFAULT;
	}
	return 0;
 }

static unsigned int pxa_camera_poll(struct file *file, poll_table *wait) 
{
    static int waited = 0;
    camera_context_t *cam_ctx = g_camera_context;

    poll_wait(file, &camera_wait_q, wait);
    
    if(still_image_mode == 1 && still_image_rdy == 1) 
    {
        still_image_rdy = 0;
        waited = 0;
        return POLLIN | POLLRDNORM;
	}
    
    if(first_video_frame == 1)
    {
       first_video_frame = 0;
    }
    else if(still_image_mode == 0 && waited != 1)
    {
       cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;
    }

    if(cam_ctx->block_header == cam_ctx->block_tail)  
    {
        task_waiting = 1;
        waited = 1;
        return 0;
    }
    waited = 0;
    return POLLIN | POLLRDNORM;
}

int pxa_camera_ioctl(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg);

static struct file_operations vd_fops =
{
	.owner      = THIS_MODULE,
	.open       = pxa_camera_open,
	.release    = pxa_camera_close,
	.read       = pxa_camera_read,
	.poll       = pxa_camera_poll,
	.ioctl      = pxa_camera_ioctl,
	.mmap       = pxa_camera_mmap,
	.llseek     = no_llseek,
	.write      = pxa_camera_write,
};

static struct video_device vd =
{
	.owner      = THIS_MODULE,
	.name       = "PXA camera",
	.type       = VID_TYPE_CAPTURE,
#if 0
		.hardware	= VID_HARDWARE_PXA_CAMERA,      /* FIXME */
#endif
	.hardware   = 0,
	.fops       = &vd_fops,
	.minor      = -1,
};

void pxa_ci_dma_irq_y(int channel, void *data)
{

    static int dma_repeated = 0;
    camera_context_t  *cam_ctx = g_camera_context;
    int        dcsr;
    
    dcsr = DCSR(channel);
    DCSR(channel) = dcsr & ~DCSR_STOPIRQEN;
 
    if(still_image_mode == 1) 
    {
        if(task_waiting == 1) 
        {
            wake_up_interruptible (&camera_wait_q);
            task_waiting = 0;
        }
        still_image_rdy = 1;
    	stop_dma_transfer(cam_ctx);
    } 
    else if(dma_repeated == 0 &&
           (cam_ctx->block_tail == ((cam_ctx->block_header + 2) % cam_ctx->block_number)))  
    {
        dma_repeated = 1;
        pxa_dma_repeat(cam_ctx);
        //dbg_print ("DMA repeated.");
        cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    }
    else if(dma_repeated == 1 && 
        (cam_ctx->block_tail != ((cam_ctx->block_header + 1) % cam_ctx->block_number)) && 
        (cam_ctx->block_tail != ((cam_ctx->block_header + 2) % cam_ctx->block_number)))  
    {
        pxa_dma_continue(cam_ctx);
        //dbg_print ("DMA continue.");
        dma_repeated = 0;
    }
    else if(dma_repeated == 0) 
    {
        cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    }
    
    if(task_waiting == 1 && !(cam_ctx->block_header == cam_ctx->block_tail)) 
    {
        wake_up_interruptible (&camera_wait_q);
        task_waiting = 0;
    }

}

void pxa_ci_dma_irq_cb(int channel, void *data)
{
    return;
}

void pxa_ci_dma_irq_cr(int channel, void *data)
{
    return;
}


static irqreturn_t pxa_camera_irq(int irq, void *dev_id)
{
	unsigned int cisr;

	disable_irq(IRQ_CAMERA);
	cisr = CISR;

	if (cisr & CI_CISR_SOF)
		CISR |= CI_CISR_SOF;

	if (cisr & CI_CISR_EOF)
		CISR |= CI_CISR_EOF;

	enable_irq(IRQ_CAMERA);
	return IRQ_HANDLED;
}

static int __init pxa_camera_init(void)
{
	if (request_irq(IRQ_CAMERA, pxa_camera_irq, 0, "PXA Camera", &vd))
	{
		dbg_print("Camera interrupt register failed failed number \n");
		return -EIO;
	}
	dbg_print ("Camera interrupt register successful \n");

	if(video_register_device(&vd, VFL_TYPE_GRABBER, 0) < 0)
	{
		dbg_print("PXA_CAMERA: video_register_device failed\n");
		return -EIO;
	}

	dbg_print("PXA_CAMERA: video_register_device successfully. /dev/video%d \n", 0);

	return 0;
}

static void __exit pxa_camera_exit(void)
{
	free_irq(IRQ_CAMERA,  &vd);
	video_unregister_device(&vd);
}


//-------------------------------------------------------------------------------------------------------
//      Configuration APIs
//-------------------------------------------------------------------------------------------------------

void pxa_dma_repeat(camera_context_t  *cam_ctx)
{
        pxa_dma_desc *cnt_head, *cnt_tail;
	int cnt_block;

	cnt_block = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    
    // FIFO0
	cnt_head = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + 
                                cnt_block * cam_ctx->fifo0_num_descriptors;
                                
	cnt_tail = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
	cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
    
    // FIFO1
	if(cam_ctx->fifo1_transfer_size) 
    {
		cnt_head = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo1_num_descriptors;
                    
		cnt_tail = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
    
    // FIFO2
	if(cam_ctx->fifo2_transfer_size) 
    {
		cnt_head = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo2_num_descriptors;
                    
		cnt_tail = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
}

void pxa_dma_continue(camera_context_t *cam_ctx)
{
   	pxa_dma_desc *cnt_head, *cnt_tail;
	pxa_dma_desc *next_head;
	int cnt_block, next_block;

	cnt_block = cam_ctx->block_header;
	next_block = (cnt_block + 1) % cam_ctx->block_number;
    
    // FIFO0
	cnt_head  = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + 
                cnt_block * cam_ctx->fifo0_num_descriptors;
                
	cnt_tail  = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
    
	next_head = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual +
                next_block * cam_ctx->fifo0_num_descriptors;

    cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
    
    // FIFO1
	if(cam_ctx->fifo1_transfer_size) 
    {
		cnt_head  = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo1_num_descriptors;
                    
		cnt_tail  = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
        
		next_head = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                     next_block * cam_ctx->fifo1_num_descriptors;
                     
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
    
    // FIFO2
	if(cam_ctx->fifo2_transfer_size) 
    {
		cnt_head  = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                   cnt_block * cam_ctx->fifo2_num_descriptors;
                   
		cnt_tail  = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
        
		next_head = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                    next_block * cam_ctx->fifo2_num_descriptors;
                    
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
	return;

}

/*
Generate dma descriptors
Pre-condition: these variables must be set properly
                block_number, fifox_transfer_size 
                dma_descriptors_virtual, dma_descriptors_physical, dma_descirptors_size
Post-condition: these variables will be set
                fifox_descriptors_virtual, fifox_descriptors_physical              
                fifox_num_descriptors 
*/
//#ifdef  CONFIG_PXA_EZX_E680
static int generate_fifo2_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;

    cur_des_virtual  = (pxa_dma_desc *)camera_context->fifo2_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo2_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++) 
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo2_transfer_size;

        target_page_num = camera_context->pages_per_block * i +
        camera_context->pages_per_fifo0 +
        camera_context->pages_per_fifo1;

        if (camera_context->pages_per_fifo1 > 1) 
        {
            for(j=0; j<camera_context->fifo2_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                    
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR2_PHY;      // FIFO2 physical address
                cur_des_virtual->dtadr =
                virt_to_bus(camera_context->page_array[target_page_num]);
                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                target_page_num++;
            }
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
        }
        else 
        {
            for(j=0; j<camera_context->fifo2_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR2_PHY;      // FIFO2 physical address
                if (!(j % 2))
                {
                    cur_des_virtual->dtadr =
                    virt_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr =
                    virt_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE/2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
    
                ddbg_print(" CR: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x \n", 
                            cur_des_virtual->ddadr,
                            cur_des_virtual->dtadr,
                            cur_des_virtual->dcmd,
                            virt_to_bus(camera_context->page_array[target_page_num]) );
    
                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
        
                if (j % 2)
                {
                    target_page_num++;
                }
            }

            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }
    }
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo2_descriptors_physical);
    
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;
    return 0;
}

static int generate_fifo1_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;

    cur_des_virtual  = (pxa_dma_desc *)camera_context->fifo1_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo1_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++)
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo1_transfer_size;

        target_page_num = camera_context->pages_per_block * i +
        camera_context->pages_per_fifo0;

        if (camera_context->pages_per_fifo1 > 1) 
        {
            for(j=0; j<camera_context->fifo1_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX) 
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                else
                    des_transfer_size = remain_size;
                
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR1_PHY;      // FIFO1 physical address
                cur_des_virtual->dtadr =
                virt_to_bus(camera_context->page_array[target_page_num]);
                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;

                target_page_num++;
            }

            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }
        else  
        {
            for(j=0; j<camera_context->fifo1_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR1_PHY;      // FIFO1 physical address
                if(!(j % 2))
                {
                    cur_des_virtual->dtadr =
                    virt_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr =
                    virt_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE/2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                ddbg_print("CB: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x \n", 
                        cur_des_virtual->ddadr,
                        cur_des_virtual->dtadr,
                        cur_des_virtual->dcmd,
                        virt_to_bus(camera_context->page_array[target_page_num]));

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                if (j % 2)
                {
                    target_page_num++;
                }
            }//end of for j...
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
          }// end of else
    }//end of for i...
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo1_descriptors_physical);
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;

    return 0;
}

static int generate_fifo0_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;
    
    cur_des_virtual = (pxa_dma_desc *)camera_context->fifo0_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo0_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++) 
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo0_transfer_size;

        // assume the blocks are stored consecutively
        target_page_num = camera_context->pages_per_block * i;

        if (camera_context->pages_per_fifo0 > 2) 
        {
            for(j=0; j<camera_context->fifo0_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX)
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
                cur_des_virtual->dtadr =
                virt_to_bus(camera_context->page_array[target_page_num]);
                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                target_page_num++;
            }
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }
        else
        {
            for(j=0; j<camera_context->fifo0_num_descriptors; j++) 
            {
                // set descriptor
                if(remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
                if(!(j % 2))
                {
                    cur_des_virtual->dtadr = virt_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr = virt_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE / 2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
 
                ddbg_print(" Y: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x, page_num = %d \n", 
                            cur_des_virtual->ddadr,
                            cur_des_virtual->dtadr,
                            cur_des_virtual->dcmd,
                            virt_to_bus(camera_context->page_array[target_page_num]),
                            target_page_num);
                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                if (j % 2)
                {
                    target_page_num++;
                }
            }//end of for j..
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }// end of else	
    }
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo0_descriptors_physical);
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;
    return 0;
}


int update_dma_chain(p_camera_context_t camera_context)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical; 
    // clear descriptor pointers
    camera_context->fifo0_descriptors_virtual = camera_context->fifo0_descriptors_physical = 0;
    camera_context->fifo1_descriptors_virtual = camera_context->fifo1_descriptors_physical = 0;
    camera_context->fifo2_descriptors_virtual = camera_context->fifo2_descriptors_physical = 0;

    // calculate how many descriptors are needed per frame
    camera_context->fifo0_num_descriptors =
    camera_context->pages_per_fifo0 > 2 ? camera_context->pages_per_fifo0 : (camera_context->fifo0_transfer_size / (PAGE_SIZE/2) + 1);

    camera_context->fifo1_num_descriptors =
    camera_context->pages_per_fifo1  > 1 ? camera_context->pages_per_fifo1 : (camera_context->fifo1_transfer_size / (PAGE_SIZE/2) + 1);

    camera_context->fifo2_num_descriptors =
    camera_context->pages_per_fifo2  > 1 ? camera_context->pages_per_fifo2 : (camera_context->fifo2_transfer_size / (PAGE_SIZE/2) + 1);

    // check if enough memory to generate descriptors
    if((camera_context->fifo0_num_descriptors + camera_context->fifo1_num_descriptors + 
        camera_context->fifo2_num_descriptors) * camera_context->block_number 
         > camera_context->dma_descriptors_size)
    {
      return -1;
    }

    // generate fifo0 dma chains
    camera_context->fifo0_descriptors_virtual = (unsigned)camera_context->dma_descriptors_virtual;
    camera_context->fifo0_descriptors_physical = (unsigned)camera_context->dma_descriptors_physical;
    // generate fifo0 dma chains
    generate_fifo0_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
     
    // generate fifo1 dma chains
    if(!camera_context->fifo1_transfer_size)
    {
       return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo1_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo1_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo1 dma chains
    generate_fifo1_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
    
    if(!camera_context->fifo2_transfer_size) 
    {
      return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo2_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo2_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo2 dma chains
    generate_fifo2_dma_chain(camera_context,  &cur_des_virtual, &cur_des_physical);
        
    return 0;   
}

int camera_deinit( p_camera_context_t camera_context )
{
#if 0
    // deinit sensor
	if(camera_context->camera_functions)
		camera_context->camera_functions->deinit(camera_context);  
	
	// capture interface deinit
	ci_deinit();
	//camera_gpio_deinit();
	return 0;
#else
#warning TODO: camera_deinit is unimplemented
	return 0;
#endif
}

int camera_ring_buf_init(p_camera_context_t camera_context)
{
	unsigned         frame_size;
    dbg_print("");    
    switch(camera_context->capture_output_format)
    {
    case CAMERA_IMAGE_FORMAT_RGB565:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size / 2;
        camera_context->fifo1_transfer_size = frame_size / 4;
        camera_context->fifo2_transfer_size = frame_size / 4;
        break;
    case CAMERA_IMAGE_FORMAT_RGB666_PLANAR:
        frame_size = camera_context->capture_width * camera_context->capture_height * 4;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_RGB666_PACKED:
        frame_size = camera_context->capture_width * camera_context->capture_height * 3;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    default:
        return STATUS_WRONG_PARAMETER;
        break;
    }

    camera_context->block_size = frame_size;

	camera_context->pages_per_fifo0 =
		(PAGE_ALIGN(camera_context->fifo0_transfer_size) / PAGE_SIZE);
	camera_context->pages_per_fifo1 =
		(PAGE_ALIGN(camera_context->fifo1_transfer_size) / PAGE_SIZE);
	camera_context->pages_per_fifo2 =
		(PAGE_ALIGN(camera_context->fifo2_transfer_size) / PAGE_SIZE);

	camera_context->pages_per_block =
		camera_context->pages_per_fifo0 +
		camera_context->pages_per_fifo1 +
		camera_context->pages_per_fifo2;

	camera_context->page_aligned_block_size =
		camera_context->pages_per_block * PAGE_SIZE;

	camera_context->block_number_max =
		camera_context->pages_allocated /
		camera_context->pages_per_block;


    //restrict max block number 
    if(camera_context->block_number_max > FRAMES_IN_BUFFER)
    {
       camera_context->block_number = FRAMES_IN_BUFFER; 
    }
    else
    {
       camera_context->block_number = camera_context->block_number_max;
    }
	camera_context->block_header = camera_context->block_tail = 0;
	// generate dma descriptor chain
	return update_dma_chain(camera_context);

}


/***********************************************************************
 *
 * Capture APIs
 *
 ***********************************************************************/
// Set the image format
int camera_set_capture_format(p_camera_context_t camera_context)
{

	int status=-1;
	CI_IMAGE_FORMAT  ci_input_format, ci_output_format;
	CI_MP_TIMING     timing;

	if(camera_context->capture_input_format >  CAMERA_IMAGE_FORMAT_MAX ||
	   camera_context->capture_output_format > CAMERA_IMAGE_FORMAT_MAX )
    {
		return STATUS_WRONG_PARAMETER;
    }

	ci_input_format  = FORMAT_MAPPINGS[camera_context->capture_input_format];
	ci_output_format = FORMAT_MAPPINGS[camera_context->capture_output_format];
    
	if(ci_input_format == CI_INVALID_FORMAT || ci_output_format == CI_INVALID_FORMAT)
    {
	  return STATUS_WRONG_PARAMETER;
    }
    
	ci_set_image_format(ci_input_format, ci_output_format);

 
    timing.BFW = 0;
    timing.BLW = 0;
 
    ci_configure_mp(camera_context->capture_width-1, camera_context->capture_height-1, &timing);

    if(camera_context == NULL || camera_context->camera_functions == NULL || 
       camera_context->camera_functions->set_capture_format == NULL)
    {
	  dbg_print("camera_context point NULL!!!");
	  return -1;
    } 
    status = camera_ring_buf_init(camera_context);
	// set sensor setting
	if(camera_context->camera_functions->set_capture_format(camera_context))
    {
 	   return -1;
    }
   
    // ring buffer init
    return status;
    
}

// take a picture and copy it into the ring buffer
int camera_capture_still_image(p_camera_context_t camera_context, unsigned int block_id)
{
	// init buffer status & capture
    camera_set_int_mask(camera_context, 0x3ff | 0x0400);
    still_image_mode = 1;
    first_video_frame = 0;
    task_waiting = 0;
	camera_context->block_header   = camera_context->block_tail = block_id;  
	camera_context->capture_status = 0;
    still_image_rdy = 0;
	return  start_capture(camera_context, block_id, 1);
    
}

// capture motion video and copy it to the ring buffer
int camera_start_video_capture( p_camera_context_t camera_context, unsigned int block_id )
{
	//init buffer status & capture
    camera_set_int_mask(camera_context, 0x3ff | 0x0400);
    still_image_mode = 0;
    first_video_frame = 1;
	camera_context->block_header   = camera_context->block_tail = block_id; 
	camera_context->capture_status = CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
	return start_capture(camera_context, block_id, 0);
}

void stop_dma_transfer(p_camera_context_t camera_context)
{	
    int ch0, ch1, ch2;

    ch0 = camera_context->dma_channels[0];
    ch1 = camera_context->dma_channels[1];
    ch2 = camera_context->dma_channels[2];

    DCSR(ch0) &= ~DCSR_RUN;
    DCSR(ch1) &= ~DCSR_RUN;
    DCSR(ch2) &= ~DCSR_RUN;
	camera_context->dma_started = 0;

    return;
}

int start_capture(p_camera_context_t camera_context, unsigned int block_id, unsigned int frames)
{
#if 0
    int status;
    status = camera_context->camera_functions->start_capture(camera_context, frames);

	return status;
#else
#warning start_capture is not implemented
	return 0;
#endif
}


// disable motion video image capture
void camera_stop_video_capture( p_camera_context_t camera_context )
{
	
	//stop capture
	camera_context->camera_functions->stop_capture(camera_context);
  
	//stop dma
	stop_dma_transfer(camera_context);
    
	//update the flag
	if(!(camera_context->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
    {
		camera_context->capture_status &= ~CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
    }
}


module_init(pxa_camera_init);
module_exit(pxa_camera_exit);

MODULE_DESCRIPTION("Bulverde Camera Interface driver");
MODULE_LICENSE("GPL");

