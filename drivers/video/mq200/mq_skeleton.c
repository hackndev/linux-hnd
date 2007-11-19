/*
 * Author: Holger Hans Peter Freyther
 *
 *
 * This implements the frame buffer driver interface to communicate
 * with the kernel.
 * It uses the mq200 routines from the ucLinux driver from Lineo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/types.h>
#include <linux/spinlock.h>

#include "mq200_data.h"

#if CONFIG_SA1100_SIMPAD
/*
 * Siemens SIMpad specefic data
 */
#include <asm/arch/simpad.h>
#define MQ200_REGIONS simpad_mq200_regions
#define MQ200_MONITOR simpad_mq200_panel

static struct mq200_io_regions simpad_mq200_regions = {
	.fb_size        = MQ200_FB_SIZE,
	.phys_mmio_base = 0x4be00000,
	.virt_mmio_base = 0xf2e00000,
	.phys_fb_base   = 0x4b800000,
	.virt_fb_base   = 0xf2800000,
};

static struct mq200_monitor_info simpad_mq200_panel = {
	.horizontal_res = 800,
	.vertical_res   = 600,
	.depth          = 16,
	.refresh        = 60,
	.line_length    = 1600,
	.flags          = 0x00130004,
};

extern long get_cs3_shadow(void);
extern void set_cs3_bit(int value);
extern void clear_cs3_bit(int value);
#endif



struct mq200_info {
	struct fb_info fb_info;
	struct mq200_io_regions io_regions;
	struct mq200_monitor_info monitor_info;

        /* palette */
	u32	pseudo_palette[17]; /* 16 colors + 1 in reserve not that well documented... */
	spinlock_t lock;
};



static int mq200_blank( int blank_mode, struct fb_info *info )
{
#ifdef CONFIG_SA1100_SIMPAD
	if(blank_mode ){
		clear_cs3_bit(DISPLAY_ON);
	}else {
		set_cs3_bit(DISPLAY_ON);
	}
#endif
	return 0;
}


static int mq200_check_var(struct fb_var_screeninfo *var,
			   struct fb_info *info )
{	/* TODO do we need sanity checks here */
	return 0;
}


static int mq200_set_par( struct fb_info *info )
{
	/* TODO set paraemeter */
	return 0;
}

static int mq200_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info )
{
	struct mq200_info *p;
	unsigned long color;
	u32* pal = info->pseudo_palette;

	p = info->par;

	if(regno > 255 )
		return 1;

	switch( info->var.bits_per_pixel ){
	case 16:
		pal[regno] =
			((red & 0xf800) >> 0) |
			((green & 0xf800) >> 5) | ((blue & 0xf800) >> 11);
		break;
	case 24:
		pal[regno] =
			((red & 0xff00) << 8) |
			((green & 0xff00)) | ((blue & 0xff00) >> 8);
		break;
	case 32:
		pal[regno] =
			((red & 0xff00) >> 8) |
			((green & 0xff00)) | ((blue & 0xff00) << 8);
		break;
	default:
		break;
	}

	red &= 0xFF;
	green &= 0xFF;
	blue &= 0xFF;

	color = red | (green << 8) | (blue << 16);
	mq200_external_setpal(regno, color, p->io_regions.virt_mmio_base);

	return 0;
}




static struct fb_ops mq200_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= mq200_check_var,
	.fb_set_par	= mq200_set_par,
	.fb_setcolreg	= mq200_setcolreg,
	.fb_cursor	= soft_cursor, /*  FIXME use hardware cursor */
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_blank	= mq200_blank,
};


/*********************************************************************
 *
 * Device driver and module init code
 * this will register to the fb layer later
 *
 *********************************************************************/
static void mq200_internal_init_color( struct fb_bitfield* red,
				       struct fb_bitfield* green,
				       struct fb_bitfield* blue,
				       int bpp )
{
	switch ( bpp )
	{
	case 16:
		red->offset	= 11;
		green->offset	= 5;
		blue->offset	= 0;

		red->length	= 5;
		green->length	= 6;
		blue->length	= 5;
		break;
	case 24:
		red->offset	= 16;
		green->offset	= 8;
		blue->offset	= 0;

		red->length	= 8;
		green->length	= 8;
		blue->length	= 8;
		break;
	case 32:
		red->offset	= 0;
		green->offset	= 8;
		blue->offset	= 16;

		red->length	= 8;
		green->length	= 8;
		blue->length	= 8;
	case 8: /* fall through */
	default:
		red->offset = green->offset = blue->offset = 0;
		red->length = green->length = blue->length = bpp;
		break;
	}

}


static struct mq200_info* __init mq200_internal_init_fbinfo(void)
{
	struct mq200_info	*info = NULL;

	info = (struct mq200_info*)kmalloc(sizeof(*info), GFP_KERNEL);
	if(!info)
		return NULL;

        /*
	 * Initialize memory
	 */
	memset(info, 0, sizeof(struct mq200_info) );
	spin_lock_init(&info->lock);

        /* set the base IO addresses */
	info->io_regions   = MQ200_REGIONS;
	info->monitor_info = MQ200_MONITOR;

	info->fb_info.screen_base = (char *)info->io_regions.virt_fb_base;

	/* fb_fix_screeninfo filling */
	strcpy(info->fb_info.fix.id, "MQ200_FB" );
	info->fb_info.fix.smem_start	= info->io_regions.phys_fb_base;
	info->fb_info.fix.smem_len	= info->io_regions.fb_size; /* - CURSOR_IMAGE */
	info->fb_info.fix.mmio_start	= info->io_regions.phys_mmio_base;
	info->fb_info.fix.mmio_len	= MQ200_REGS_SIZE;
	info->fb_info.fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fb_info.fix.accel		= FB_ACCEL_NONE;
	info->fb_info.fix.line_length	= MQ200_MONITOR_LINE_LENGTH(info);

	if(MQ200_MONITOR_DEPTH(info) <= 8 )
		info->fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else if( MQ200_MONITOR_DEPTH(info) >= 16 )
		info->fb_info.fix.visual = FB_VISUAL_DIRECTCOLOR;
	else
		panic("Calling mq200 with wrong display data\n");

	/* set the variable screen info */
	info->fb_info.var.xres	= MQ200_MONITOR_HORI_RES(info);
	info->fb_info.var.yres	= MQ200_MONITOR_VERT_RES(info);
	info->fb_info.var.xres_virtual = MQ200_MONITOR_HORI_RES(info);
	info->fb_info.var.yres_virtual = MQ200_MONITOR_VERT_RES(info);
	info->fb_info.var.bits_per_pixel = MQ200_MONITOR_DEPTH(info);

	mq200_internal_init_color(&info->fb_info.var.red,
				  &info->fb_info.var.green,
				  &info->fb_info.var.blue,
				  MQ200_MONITOR_DEPTH(info) );

	info->fb_info.var.transp.length = info->fb_info.var.transp.offset = 0;
	info->fb_info.var.height = info->fb_info.var.width = -1;

	info->fb_info.var.vmode = FB_VMODE_NONINTERLACED;
	info->fb_info.var.pixclock = 10000;
	info->fb_info.var.left_margin = info->fb_info.var.right_margin = 16;
	info->fb_info.var.upper_margin = info->fb_info.var.lower_margin = 16;
	info->fb_info.var.hsync_len = info->fb_info.var.vsync_len = 8;

	info->fb_info.var.nonstd	= 0;
	info->fb_info.var.activate	= FB_ACTIVATE_NOW;
	info->fb_info.var.accel_flags	= 0;

	return info;
}


extern void mq200_register_attributes(struct device* );
/*
 * gets called from the bus
 * we will register our framebuffer from here
 */
static int __init mq200_probe(struct device *dev)
{
	struct mq200_info	*info = NULL;
	int retv= 0;

	info = mq200_internal_init_fbinfo();
	if(!mq200_external_probe(info->io_regions.virt_mmio_base))
	    goto error_out;


	GAFR &= ~(1<<3);
	GPSR |=  (1<<3);
	GPDR |=  (1<<3);

	mq200_external_setqmode(&info->monitor_info,
				info->io_regions.virt_mmio_base,
				&info->lock);

	info->fb_info.fbops = &mq200_ops;
	info->fb_info.flags = FBINFO_FLAG_DEFAULT;

	mq200_check_var(&info->fb_info.var, &info->fb_info );

	fb_alloc_cmap(&info->fb_info.cmap, 1 << MQ200_MONITOR_DEPTH(info), 0 );

	info->fb_info.pseudo_palette = (void*)info->pseudo_palette;

	/* save the pointer to the mq200 struct in var */
	info->fb_info.par = info;

	retv = register_framebuffer(&info->fb_info );
	if(retv < 0)
		goto error_out;


	/* will get unset if retv != 0 */
	dev_set_drvdata(dev, info );
	return retv;

/*
 * Free the info and exit
 */
error_out:
	kfree(info);
	return -EINVAL;
}

#ifdef CONFIG_PM
static struct mq200_info* get_mq200_info( struct device *dev)
{
	return dev_get_drvdata(dev);
}

static unsigned long  get_mmio_base( struct device *dev )
{
	struct mq200_info *info = get_mq200_info(dev);
	return info->io_regions.virt_mmio_base;
}

static struct mq200_monitor_info* get_monitor_info( struct device *dev)
{
	struct mq200_info *info = get_mq200_info(dev);
	return &info->monitor_info;
}

static spinlock_t* get_spinlock( struct device *dev)
{
	return &get_mq200_info(dev)->lock;
}

/*
 * FIXME: make sure we only call mq200_external_offdisplay only once
 * a 2nd time will hang the kernel -zecke
 *
 * FIXME: save the content of the framebuffer inside dev->saved_state
 *        so on resume we can memcpy it back into the buffer and userspace
 *        does not need to redraw
 *
 * functions for suspending and resuming
 */
static int mq200_suspend(struct device *dev, u32 state,
			 u32 level )
{
	if( level == SUSPEND_POWER_DOWN ) {
		mq200_external_offdisplay( get_mmio_base(dev) );
		clear_cs3_bit(DISPLAY_ON);
	}

	return 0;
}

static int mq200_resume(struct device *dev, u32 level )
{
	if( level == RESUME_ENABLE ){
		unsigned long mem = get_mmio_base(dev);
		struct mq200_monitor_info *monitor = get_monitor_info(dev);
		mq200_external_setqmode(monitor, mem, get_spinlock(dev) );

                /*
		 * Set display on if it was on
		 */
		set_cs3_bit(DISPLAY_ON);
	}

	return 0;
}


#endif


static struct device_driver mq200fb_driver = {
	.name		= "simpad-mq200",
	.bus		= &platform_bus_type,
	.probe		= mq200_probe, /* will be called after we've registered the driver */
	.suspend	= mq200_suspend,
	.resume		= mq200_resume
};

int __devinit mq200_init(void)
{
	return driver_register(&mq200fb_driver);
}

module_init(mq200_init);
MODULE_DESCRIPTION("MQ200 framebuffer driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Holger Hans Peter Freyther");
