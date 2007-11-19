/*
 * linux/drivers/video/imageon/w100.c
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
 *  Ian Molton's vsfb.c (based on acornfb by Russell King)
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */


#include <linux/types.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>

#include <asm/io.h>

#include "default.h"
#include "w100.h"

void *cfg;
void *regs;
void *fb;

/******************************************************************************/

static int
w100fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
               u_int trans, struct fb_info *info)
{
	if (regno > 16)
		return 1;

	((u32 *)(info->pseudo_palette))[regno] = (red & 0xf800) | (green & 0xfc00 >> 5) | (blue & 0xf800 >> 11);

	return 0;
}

/******************************************************************************/

/* Freeze the output (disable updates) */
static inline void w100fb_freeze(void)
{
	disp_db_buf_cntl_wr_u disp_db_buf_cntl_wr;

	disp_db_buf_cntl_wr.f.db_buf_cntl = 0x1e;
	disp_db_buf_cntl_wr.f.update_db_buf = 0;
	disp_db_buf_cntl_wr.f.en_db_buf = 0;

	fb_writel((u32)(disp_db_buf_cntl_wr.val), regs + mmDISP_DB_BUF_CNTL);
}

/* Unfreeze the output (enable updates) */
static inline void w100fb_thaw(void)
{
	disp_db_buf_cntl_wr_u disp_db_buf_cntl_wr;

	disp_db_buf_cntl_wr.f.db_buf_cntl = 0x1e;
	disp_db_buf_cntl_wr.f.update_db_buf = 1;
	disp_db_buf_cntl_wr.f.en_db_buf = 1;
 
	fb_writel((u32)(disp_db_buf_cntl_wr.val), regs + mmDISP_DB_BUF_CNTL);
}


/******************************************************************************/

/* Perform a soft reset of the chip */
void w100fb_soft_reset(void)
{  
	u8 val8 = fb_readb(cfg + cfgSTATUS);
	fb_writeb(val8|0x08, cfg + cfgSTATUS); /* bit4: soft reset */
	udelay(100);
	fb_writeb(0x00, cfg + cfgSTATUS);	/* clear all bits */
	udelay(100);
} 

/* Display some registers to output - useful if you want to display a 
   register at some stage during init */
void w100fb_dump_registers(void)
{  
        printk("  dpDatatype  %08x\n", fb_readl(regs+0x12c4));
}

/* Initial GPIO settings */
void w100fb_init_gpio(void)
{
	fb_writel(0xffffffff, regs +mmGPIO_DATA);
	fb_writel(0xffffffff, regs +mmGPIO_CNTL1);
	fb_writel(0xffffffff, regs +mmGPIO_CNTL2);
	fb_writel(0xffffffff, regs +mmGPIO_DATA2);
	fb_writel(0xffffffff, regs +mmGPIO_CNTL3);
	fb_writel(0xffffffff, regs +mmGPIO_CNTL4);
}

/******************************************************************************/

/* Init the power to default settings */
void w100fb_init_power(void)
{
	fb_writel(0xffff11c5, regs + mmPWRMGT_CNTL);
}

/* Init the clock to the default settings */
void w100fb_init_clock(void)
{
	fb_writel(0x000401bf, regs + mmCLK_PIN_CNTL);
	fb_writel(0x50500d04, regs + mmPLL_REF_FB_DIV);
	fb_writel(0x4b000200, regs + mmPLL_CNTL);
	fb_writel(0x00000b11, regs + mmSCLK_CNTL);
	fb_writel(0x00008041, regs + mmPCLK_CNTL);
	fb_writel(0x00000001, regs + mmCLK_TEST_CNTL);
}

/* Suspend the display and output */
void w100fb_suspend(void)
{
}

/* Resume the display and output */
void w100fb_resume(void)
{
}

/******************************************************************************/

/*
 * All memory locations are relative to the chip. Because the start and 
 * top variables are only 16 bit, they are shifted. Thus a value of
 * 0x15FF1000 to MC_FB_LOCATION or MC_EXT_MEM_LOCATION will mean the
 * memory area is located at the (chip base address + (0x1000 << 8)),
 * and has a size of ((0x15FF - 0x1000) << 8), or 0x5FF00, which turns
 * out to be just shy of 384 K - the internal memory of the chip.
 *
 * As a result the maximum offset of the memory is (0xFFFF << 8).
 */

/* Set the frame buffer */
void w100fb_init_memory(void)
{
	/* Set the frame buffer to chip base address + 0x100000 */
	fb_writel(0x15FF1000, regs + mmMC_FB_LOCATION);

	/* Set the external memory location */
	fb_writel(0x9FFF8000, regs + mmMC_EXT_MEM_LOCATION);
}

/******************************************************************************/

/* Initial settings for the display */
void w100fb_init_display(void)
{
	u32 temp32;

	fb_writel(0x00008003, regs + mmLCD_FORMAT);
	fb_writel(0x03cf1c06, regs + mmGRAPHIC_CTRL);
	fb_writel(0x00100000, regs + mmGRAPHIC_OFFSET);
	fb_writel(0x000001e0, regs + mmGRAPHIC_PITCH);

	/* Set the hardware to 565 (not really needed, but do it anyway) */
	temp32 = fb_readl(regs + mmDISP_DEBUG2);
	temp32 |= 0x00800000; /* Set this bit */
	fb_writel(temp32, regs + mmDISP_DEBUG2);

	fb_writel(0x0149011b, regs + mmCRTC_TOTAL);
	fb_writel(0x01050015, regs + mmACTIVE_H_DISP);
	fb_writel(0x01450005, regs + mmACTIVE_V_DISP);
	fb_writel(0x01050015, regs + mmGRAPHIC_H_DISP);
	fb_writel(0x01450005, regs + mmGRAPHIC_V_DISP);

	fb_writel(0x80150014, regs + mmCRTC_SS);
	fb_writel(0x8014000d, regs + mmCRTC_LS);
	fb_writel(0x0040010a, regs + mmCRTC_REV);
	fb_writel(0xa1700030, regs + mmCRTC_DCLK);
	fb_writel(0xc1000005, regs + mmCRTC_GS);
	fb_writel(0x00020147, regs + mmCRTC_VPOS_GS);
	fb_writel(0x80cc0015, regs + mmCRTC_GCLK);
	fb_writel(0x80cc0015, regs + mmCRTC_GOE);
	fb_writel(0x00000000, regs + mmCRTC_FRAME);
	fb_writel(0x00000000, regs + mmCRTC_FRAME_VPOS);

	fb_writel(0x00000000, regs + mmCRTC_DEFAULT_COUNT);

	fb_writel(0x00000000, regs + mmLCDD_CNTL1);
	fb_writel(0x0003ffff, regs + mmLCDD_CNTL2);
	fb_writel(0x00fff003, regs + mmGENLCD_CNTL1);
	fb_writel(0x00000003, regs + mmGENLCD_CNTL2);

	fb_writel(0x00000000, regs + mmLCD_BACKGROUND_COLOR);
	fb_writel(0x61060017, regs + mmCRTC_PS1_ACTIVE);
	fb_writel(0x000143aa, regs + mmGENLCD_CNTL3);
}

/* Rotate the lcd through 0/270 degrees. */
void w100fb_lcd_rotate(struct fb_info *info, int degrees)
{
	u32 val;
	val = fb_readl(regs + mmGRAPHIC_CTRL);

	/* We use the portrait_mode flag from the graphic control,
	   and also set the graphic pitch to width * 2. The offset
	   also seems to change for some strange reason, so we have
	   to adjust that here as well. */

	switch (degrees) {
		case 0:
			val &= 0xffffffef;
			info->var.xres = info->var.xres_virtual = 240;
			info->var.yres = info->var.yres_virtual = 320;
			info->fix.line_length = 240 * 2;
			fb_writel(0x000001e0, regs + mmGRAPHIC_PITCH);
			fb_writel(0x00100000, regs + mmGRAPHIC_OFFSET);
			break;
		case 270:
			val |= 0x00000010;
			info->var.xres = info->var.xres_virtual = 320;
			info->var.yres = info->var.yres_virtual = 240;
			info->fix.line_length = 320 * 2;
			fb_writel(0x00000280, regs + mmGRAPHIC_PITCH);
			fb_writel(0x0010027c, regs + mmGRAPHIC_OFFSET);
			break;
		default:
			printk(KERN_INFO "imageonfb: unknown rotate value\n");
	};

	/* Write the new value out */
	fb_writel(val, regs + mmGRAPHIC_CTRL);
}

/******************************************************************************/

/* We should go through and set all the registers to something reasonable
   similar to the mach64_accel code. For now this is the minimum. */
void w100fb_init_accel(void)
{
	fb_writel(0x00100000, regs + mmDST_OFFSET);
}

/* Common code, based on mach64_accel code */
void w100fb_drawrect(s16 x, s16 y, u16 width, u16 height)
{
	fb_writel((x << 16) | y, regs + mmDST_X_Y);
	fb_writel((width << 16) | height, regs + mmDST_WIDTH_HEIGHT);
}

/* The second function to be written in the accel code. Seems to work, if
   anything needs fixing it will be the tmp value. The first and last
   writel translate from the mach64 code with no problems. The second
   writel was a problem and is based on my best guess. */
void w100fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	u32 color = rect->color;

	/* We should use a calculated value, but this works for now.
	   The second writel is based on these values which are my best
	   guess. It is based on mach64 code and the w100fb stuff. */
	dp_datatype_u tmp;
	tmp.f.dp_dst_datatype = DP_DST_16BPP_1555;
	tmp.f.dp_brush_datatype = DP_BRUSH_SOLIDCOLOR;
	tmp.f.dp_src_datatype = DP_SRC_SOLID_COLOR_BLT;

	if (!rect->width || !rect->height)
		return;

	color |= (rect->color << 8);
	color |= (rect->color << 16);

	fb_writel(color, regs + mmDP_SRC_FRGD_CLR);
	fb_writel(tmp.val, regs + mmDP_DATATYPE);
	fb_writel(0x00000013, regs + mmDP_CNTL);

	w100fb_drawrect(rect->dx, rect->dy, rect->width, rect->height);
}

/* Third function written. Have no idea if it works - does not seem to
   get called during boot. Prob does not quite work yet. */
void w100fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	u32 dy = area->dy, sy = area->sy;
	u32 sx = area->sx, dx = area->dx;
	dp_cntl_u direction;

	if (!area->width || !area->height)   
		return;

	if (area->sy < area->dy) {
		dy += area->height - 1;
		sy += area->height - 1;
	} else
		direction.f.dst_y_dir = 1;

	if (sx < dx) {  
		dx += area->width - 1;
		sx += area->width - 1;
	} else
		direction.f.dst_x_dir = 1;

//	fb_writel(FRGD_SRC_BLIT, regs + mmDP_DATATYPE); ?????
	fb_writel((sy << 16) | sx, regs + mmSRC_Y_X);
	fb_writel(area->width, regs + mmSRC_WIDTH);
	fb_writel(area->height, regs + mmSRC_HEIGHT);
	fb_writel(direction.val, regs + mmDP_CNTL);

	w100fb_drawrect(dx, dy, area->width, area->height);
}

/* Fourth and final function. This one appears to be the most important.
   Have been unable to find any examples, so this one has been written 
   being more blind than the others. */
void w100fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
}

/* Based on stuff found in the "reset" part of radeonfb.c. This is a real 
   basic hack to show how it works. First of the accel functions that
   was written. */
void w100fb_drawline()
{
	dst_line_start_u linestart;
	dst_line_end_u lineend;

	linestart.f.dst_start_x = 0;
	linestart.f.dst_start_y = 0;
	lineend.f.dst_end_x = 100;
	lineend.f.dst_end_y = 100;

	fb_writel(0x00100000, regs + mmDST_OFFSET);
	fb_writel(linestart.val, regs + mmDST_LINE_START);
	fb_writel(lineend.val, regs + mmDST_LINE_END);
	fb_writel(0xffffffff, regs + mmDP_BRUSH_FRGD_CLR);
	fb_writel(0x00000000, regs + mmDP_BRUSH_BKGD_CLR);
	fb_writel(0xffffffff, regs + mmDP_WRITE_MSK);
}

/******************************************************************************/

/* Initial settings */
void w100fb_init_idct(void)
{
}

/******************************************************************************/

/* Set the default values */
void w100fb_init_hwcursor(void)
{
	cursor_offset_u offset;

	/* Write the default values to the registers */
	fb_writel(0x00ff0000, regs + CURSOR1_BASE + CURSOR_COLOR0);
	fb_writel(0x00000000, regs + CURSOR1_BASE + CURSOR_COLOR1);

	/* Hack. Not sure how all this works, but at least we get 
	   something on the lcd. */
	offset.f.offset = 0x000100;
	offset.f.x_offset = 0x0;
	offset.f.y_offset = 0x0;
	fb_writel(offset.val, regs + CURSOR1_BASE + CURSOR_OFFSET);
}

/* Based on soft cursor code */
/* FIXME: Deal with lcd in landscape mode */
/* TODO: Check to see if the chip can generate the signal to flash the
   cursor. ATM the kernel calls this function as needed. */
/* TODO: Use cursor 1 for lcd, cursor 2 for crt? */
int w100fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	static int counter = 0;
	cursor_h_pos_u hpos;
	cursor_v_pos_u vpos;

	if (cursor->set & FB_CUR_SETSIZE) {
		info->cursor.image.height = cursor->image.height;
		info->cursor.image.width = cursor->image.width;
	}

	if (cursor->set & FB_CUR_SETPOS) {
		info->cursor.image.dx = cursor->image.dx;
		info->cursor.image.dy = cursor->image.dy;
	}

	if (cursor->set & FB_CUR_SETHOT)
		info->cursor.hot = cursor->hot;

	if (info->cursor.enable) {
		/* Have no idea why there are these offsets for the start */
		hpos.f.start = info->cursor.image.dx + 21;
		hpos.f.end = hpos.f.start + info->cursor.image.width;
		hpos.f.en = 1;

		vpos.f.start = info->cursor.image.dy + 10;
		vpos.f.end = vpos.f.start + info->cursor.image.height;

		w100fb_freeze();
		fb_writel(hpos.val, regs + CURSOR1_BASE + CURSOR_H_POS);
		fb_writel(vpos.val, regs + CURSOR1_BASE + CURSOR_V_POS);
		w100fb_thaw();

	} else {
		/* Only need to set hpos.f.en to 0 */
		w100fb_freeze();
		fb_writel(0x00000000, regs + CURSOR1_BASE + CURSOR_H_POS);
		w100fb_thaw();
	}

	return 0;
}

/******************************************************************************/

/******************************************************************************/

/******************************************************************************/

static struct w100fb_par {
} par;

static struct fb_var_screeninfo w100fb_var = {
	.xres 		= 240,
	.yres 		= 320,
	.xres_virtual 	= 240,
	.yres_virtual 	= 320,
	.xoffset	= 0,
	.yoffset	= 0,
	.bits_per_pixel = 16,
	.red 		= { 11, 5, 0 },
	.green 		= {  5, 6, 0 }, 
	.blue 		= {  0, 5, 0 },
	.transp		= {  0, 0, 0 },
	.grayscale	= 0,
	.activate	= FB_ACTIVATE_NOW,
	.height		= -1,
	.width		= -1,
	.vmode		= FB_VMODE_NONINTERLACED,
	.sync		= 0,
	.pixclock	= 0x04,	// 171521
};

static struct fb_fix_screeninfo w100fb_fix = {
        .id		= "imageon w100",
        .type		= FB_TYPE_PACKED_PIXELS,
	.type_aux	= 0,
        .visual		= FB_VISUAL_TRUECOLOR,
	.smem_start	= FB_BASE,
        .xpanstep	= 0,
        .ypanstep	= 0,
        .ywrapstep	= 0,
	.line_length	= 240 * 2,
        .accel		= FB_ACCEL_NONE,
};

static struct fb_info info;
static u32 colreg[17]; // Copied from Ian's driver - but is 17 correct?

static struct fb_ops w100fb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= w100fb_setcolreg,
	.fb_fillrect	= w100fb_fillrect,
	.fb_copyarea	= w100fb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_cursor	= w100fb_cursor,
};

/******************************************************************************/

int __init w100fb_init(void)
{
	u32 chipid, chiprev;

	/* Useful for debugging */
	//create_proc_read_entry("w100fb", 0, 0, w100fb_read_proc, NULL);

	memset(&info, 0, sizeof(info));

	info.flags = FBINFO_FLAG_DEFAULT;
	info.fbops = &w100fb_ops;
	info.var = w100fb_var;
	info.fix = w100fb_fix;
	info.pseudo_palette = colreg;
	fb_alloc_cmap(&info.cmap, 16, 0);

	if (!request_mem_region(W100FB_BASE, W100FB_SIZE, "ATI Imageon 100")) {
		printk("request_mem_region(...) failed\n");
		return -EINVAL;
	}

	cfg =                   ioremap_nocache(CFG_BASE, CFG_SIZE);
	regs =                  ioremap_nocache(REGS_BASE, REGS_SIZE);
	fb = info.screen_base = ioremap_nocache(FB_BASE, FB_SIZE);

	/* Before we do anything, soft reset! Especially important if using a
	   bootloader from windows - it really stuffs around with the chip */
	w100fb_soft_reset();

	/* Check the chip id and revision */
	chipid = fb_readl(regs+mmCHIP_ID) & 0xffff;
	chiprev = fb_readl(regs+mmREVISION_ID);
	//if (!(chipid & 0x1002)) /* Imageon 100 support only so far */
	//	printk(KERN_ERR "imageonfb: Unknown chipid: %04x\n", chipid);

	/* Check the scratch register works! - Seems to fail right now */
	//if (register_test() == -1) {
	//	printk(KERN_ERR "imageonfd: Can't write to video register!\n");
	//	return -ENODEV;
	//}

	/* No go through and set up the default values */
	w100fb_freeze();
	w100fb_init_clock();
	w100fb_init_power();
	w100fb_init_memory();
	w100fb_init_display();
	w100fb_init_gpio();
	w100fb_init_hwcursor();
	w100fb_init_idct();
	w100fb_init_accel();
	w100fb_thaw();

	if (register_framebuffer(&info) < 0) {
		printk("register_framebuffer(...) failed\n");
		return -EINVAL;
	}

	/* Other (fun) working functions... */
	w100fb_freeze();
	//w100fb_lcd_rotate(&info, 270);
	//w100fb_drawline();
	w100fb_thaw();
	//w100fb_dump_registers();

	printk(KERN_INFO "fb%d: %s\n", info.node,info.fix.id);

	return 0;
}

static void __exit w100fb_cleanup(void)
{
	unregister_framebuffer(&info);

	iounmap(fb);
	iounmap(regs);
	iounmap(cfg);

	release_mem_region(W100FB_BASE, W100FB_SIZE);
}
