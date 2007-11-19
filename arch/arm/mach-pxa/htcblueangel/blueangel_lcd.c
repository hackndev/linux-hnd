/*
 * LCD support for HTC Blueangel
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2003-05-14	Joshua Wise        Adapted for the HP iPAQ H1900
 * 2002-08-23   Jamey Hicks        Adapted for use with PXA250-based iPAQs
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 *                                 and abstracted EGPIO interface.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <video/w100fb.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>

#include <linux/mfd/asic3_base.h>

#define BLUEANGEL_ATI_W3200_PHYS      PXA_CS2_PHYS

static int blueangel_boardid;

static struct lcd_device *blueangel_lcd_device;
static int blueangel_lcd_power;

static int blueangel_get_boardid(void)
{

	blueangel_boardid=0;
	if (GPLR(GPIO_NR_BLUEANGEL_BOARDID0) & GPIO_bit(GPIO_NR_BLUEANGEL_BOARDID0))
		blueangel_boardid |= 1;
	if (GPLR(GPIO_NR_BLUEANGEL_BOARDID1) & GPIO_bit(GPIO_NR_BLUEANGEL_BOARDID1))
		blueangel_boardid |= 2;
	if (GPLR(GPIO_NR_BLUEANGEL_BOARDID2) & GPIO_bit(GPIO_NR_BLUEANGEL_BOARDID2))
		blueangel_boardid |= 4;

	printk("Blue Angel Board ID 0x%x\n", blueangel_boardid);
	system_rev=blueangel_boardid;

	return blueangel_boardid;
}

static void
blueangel_lcd_hw_init_pre(void)
{
	printk("blueangel_lcd_hw_init_pre");

	switch (blueangel_boardid) 
	{
	 case 4:
	 case 5:
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR2_ON, 1<<GPIOB_LCD_PWR2_ON);
		mdelay(1);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 1<<GPIOB_LCD_PWR3_ON);
	 break;
	 case 6:
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR2_ON, 1<<GPIOB_LCD_PWR2_ON);
		asic3_set_gpio_out_c(&blueangel_asic3.dev, 1<<GPIOC_LCD_PWR5_ON, 1<<GPIOC_LCD_PWR5_ON);
		udelay(600);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR1_ON, 1<<GPIOB_LCD_PWR1_ON);
		udelay(100);
	 break;
	}
}

static void
blueangel_lcd_hw_init_post(void)
{
	printk("blueangel_lcd_hw_init_post");

	switch (blueangel_boardid) 
	{
	 case 4:
	 case 5:
		mdelay(10);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR1_ON, 1<<GPIOB_LCD_PWR1_ON);
		mdelay(1);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR6_ON, 1<<GPIOB_LCD_PWR6_ON);
		mdelay(42);
	 break;
	 case 6:
	 	mdelay(1);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 1<<GPIOB_LCD_PWR3_ON);
		mdelay(5);
		asic3_set_gpio_out_c(&blueangel_asic3.dev, 1<<GPIOC_LCD_PWR4_ON, 1<<GPIOC_LCD_PWR4_ON);
		mdelay(20);
	 break;
	}
}

static void
blueangel_lcd_hw_off(void)
{
	printk("blueangel_lcd_hw_off\n");

	switch (blueangel_boardid) 
	{
	 case 6:
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR1_ON, 0);
		asic3_set_gpio_out_c(&blueangel_asic3.dev, 1<<GPIOC_LCD_PWR4_ON, 0);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 0);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR2_ON, 0);
		asic3_set_gpio_out_c(&blueangel_asic3.dev, 1<<GPIOC_LCD_PWR5_ON, 0);
	 break;
	 case 4:
	 case 5:
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR6_ON, 0);
		mdelay(5);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR1_ON, 0);
		mdelay(2);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR3_ON, 0);
		mdelay(2);
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_LCD_PWR2_ON, 0);
	 break;
	}
}

static int blueangel_lcd_set_power( struct lcd_device *ld, int level)
{
	switch (level) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
#if 1
		blueangel_lcd_hw_init_pre();
		/* you need to re-initialise the w100
		 * chip. as well.  sorry.*/
		/* boardid6: send 
		    0x75 0x12
		    0x01 0xbf
		    0x78 0x30
		    0x79 0x67
		    0x7a 0x73
		    0x7b 0x33
		    0x7c 0x30
		    0x7d 0x67
		    0x7e 0x03
		*/

		blueangel_lcd_hw_init_post();
#endif
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		blueangel_lcd_hw_off();
		break;
	}
	blueangel_lcd_power=level;
	return 0;
}

static int blueangel_lcd_get_power(struct lcd_device *ld)
{
	printk("blueangel_lcd_get_power\n");
	return blueangel_lcd_power;
}

static struct lcd_ops blueangel_lcd_props = {
	.set_power     = blueangel_lcd_set_power,
	.get_power     = blueangel_lcd_get_power,
};

static void
blueangel_lcd_suspend(struct w100fb_par *wfb)
{
#if 0
	ati_gpios[0] = w100fb_gpio_read(W100_GPIO_PORT_A);
	ati_gpios[1] = w100fb_gpcntl_read(W100_GPIO_PORT_A);
	ati_gpios[2] = w100fb_gpio_read(W100_GPIO_PORT_B);
	ati_gpios[3] = w100fb_gpcntl_read(W100_GPIO_PORT_B);
	w100fb_gpio_write(W100_GPIO_PORT_A, 0xDFE00000 );
	w100fb_gpcntl_write(W100_GPIO_PORT_A, 0xFFFF0000 );
	w100fb_gpio_write(W100_GPIO_PORT_B, 0x00000000 );
	w100fb_gpcntl_write(W100_GPIO_PORT_B, 0xFFFFFFFF );
#endif

	blueangel_lcd_hw_off();
}

static void
blueangel_lcd_resume_pre(struct w100fb_par *wfb)
{
#if 0
	w100fb_gpio_write(W100_GPIO_PORT_A, ati_gpios[0] );
	w100fb_gpcntl_write(W100_GPIO_PORT_A, ati_gpios[1] );
	w100fb_gpio_write(W100_GPIO_PORT_B, ati_gpios[2] );
	w100fb_gpcntl_write(W100_GPIO_PORT_B, ati_gpios[3] );
	lcd_hw_init();
#endif
	blueangel_lcd_hw_init_pre();
}

static void
blueangel_lcd_resume_post(struct w100fb_par *wfb)
{
	blueangel_lcd_hw_init_post();
}

static void
blueangel_lcd_w100_resume(struct w100fb_par *wfb) {
	blueangel_lcd_resume_pre(wfb);
	msleep(30);
	blueangel_lcd_resume_post(wfb);
}


struct w100_tg_info blueangel_tg_info = {
	.suspend	= blueangel_lcd_suspend,
	.resume		= blueangel_lcd_w100_resume,
};

static struct w100_gen_regs blueangel_w100_regs = {
	.lcd_format =        0x00000003,
	.lcdd_cntl1 =        0x00000000,
	.lcdd_cntl2 =        0x0003ffff,
	.genlcd_cntl1 =      0x00fff003,
	.genlcd_cntl2 =      0x00000003,	
	.genlcd_cntl3 =      0x000102aa,
};

static struct w100_mode blueangel_w100_modes_4[] = {
{
	.xres 		= 240,
	.yres 		= 320,
	.left_margin	= 0,
	.right_margin	= 31,
	.upper_margin	= 15,
	.lower_margin	= 0,
	.crtc_ss	= 0x80150014,
	.crtc_ls        = 0xa0fb00f7,
	.crtc_gs	= 0xc0080007,
	.crtc_vpos_gs	= 0xc0080007,
	.crtc_rev	= 0x0000000a,
	.crtc_dclk	= 0x81700030,
	.crtc_gclk	= 0x8015010f,
	.crtc_goe	= 0x00000000,
	.pll_freq 	= 80,
	.pixclk_divider = 15,
	.pixclk_divider_rotated = 15,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 0,
	.sysclk_src     = CLK_SRC_PLL,
},
};

static struct w100_mode blueangel_w100_modes_5[] = {
{
	.xres 		= 240,
	.yres 		= 320,
	.left_margin	= 0,
	.right_margin	= 31,
	.upper_margin	= 15,
	.lower_margin	= 0,
	.crtc_ss	= 0x80150014,
	.crtc_ls        = 0xa0fb00f7,
	.crtc_gs	= 0xc0080007,
	.crtc_vpos_gs	= 0xc0080007,
	.crtc_rev	= 0x0000000a,
	.crtc_dclk	= 0x81700030,
	.crtc_gclk	= 0x8015010f,
	.crtc_goe	= 0x00000000,
	.pll_freq 	= 80,
	.pixclk_divider = 15,
	.pixclk_divider_rotated = 15,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 0,
	.sysclk_src     = CLK_SRC_PLL,
},
};

static struct w100_mode blueangel_w100_modes_6[] = {
{
	.xres 		= 240,
	.yres 		= 320,
	.left_margin 	= 20,
	.right_margin 	= 19,
	.upper_margin 	= 3,
	.lower_margin 	= 2,
	.crtc_ss	= 0x80150014,
	.crtc_ls	= 0xa0020110,
	.crtc_gs	= 0xc0890088,
	.crtc_vpos_gs	= 0x01450144,
	.crtc_rev	= 0x0000000a,
	.crtc_dclk	= 0xa1700030,
	.crtc_gclk	= 0x8015010f,
	.crtc_goe	= 0x00000000,
	.pll_freq 	= 80,
	.pixclk_divider = 14,
	.pixclk_divider_rotated = 14,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 0,
	.sysclk_src     = CLK_SRC_PLL,
},
};

struct w100_mem_info blueangel_mem_info = {
	.ext_cntl	= 0x01040010,
	.sdram_mode_reg	= 0x00250000,
	.ext_timing_cntl= 0x00001545,
	.io_cntl	= 0x7ddd7333,
	.size		= 0x3fffff,
};

struct w100_bm_mem_info blueangel_bm_mem_info = {
	.ext_mem_bw	= 0xfbfd2d07,
	.offset		= 0x000c0000,
	.ext_timing_ctl	= 0x00043f7f,
	.ext_cntl	= 0x00000010,
	.mode_reg	= 0x006c0000,
	.io_cntl	= 0x000e0fff,
	.config		= 0x08300562,
};

static struct w100_gpio_regs blueangel_w100_gpio_info = {
	.init_data1	= 0x00000000,	// GPIO_DATA
	.gpio_dir1	= 0xe0000000,	// GPIO_CNTL1
	.gpio_oe1	= 0x003c2000,	// GPIO_CNTL2
	.init_data2	= 0x00000000,	// GPIO_DATA2
	.gpio_dir2	= 0x00000000,	// GPIO_CNTL3
	.gpio_oe2	= 0x00000000,	// GPIO_CNTL4
};

static struct w100fb_mach_info blueangel_fb_info = {
	.tg = &blueangel_tg_info,
	.mem = &blueangel_mem_info, 
	.bm_mem = &blueangel_bm_mem_info, 
	.gpio = &blueangel_w100_gpio_info,
	.regs = &blueangel_w100_regs,
	.num_modes = 1,
	.xtal_freq = 16000000,
};


static struct resource blueangel_fb_resources[] = {
	[0] = {
		.start	= BLUEANGEL_ATI_W3200_PHYS,
		.end	= BLUEANGEL_ATI_W3200_PHYS + 0x00ffffff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device blueangel_fb_device = {
	.name	= "w100fb",
	.id	= -1,
	.dev	= {
		.platform_data = &blueangel_fb_info,
	},
	.num_resources	= ARRAY_SIZE( blueangel_fb_resources ),
	.resource	= blueangel_fb_resources,
};

static int
blueangel_lcd_probe(struct platform_device *dev)
{
	int ret;
	
	printk("in blueangel_lcd_probe\n");

	blueangel_lcd_device = lcd_device_register("w100fb", (void *)&blueangel_fb_info, &blueangel_lcd_props);
	if (IS_ERR(blueangel_lcd_device)) {
		return PTR_ERR(blueangel_lcd_device);
	}

	ret = platform_device_register( &blueangel_fb_device );
	// TODO:
	
	return ret;
}

static int
blueangel_lcd_remove(struct platform_device *dev)
{
	lcd_device_unregister (blueangel_lcd_device);
	platform_device_unregister(&blueangel_fb_device);
	return 0;
}

static int
blueangel_lcd_resume(struct platform_device *dev) {
	blueangel_lcd_hw_init_pre();
	msleep(30);
	blueangel_lcd_hw_init_post();
	return 0;
}


struct platform_driver blueangel_lcd_driver = {
	.driver = {
		.name     = "blueangel-lcd",
	},
	.probe    = blueangel_lcd_probe,
	.remove   = blueangel_lcd_remove,
	.resume   = blueangel_lcd_resume,
};


int __init
blueangel_lcd_init (void)
{
	printk("blueangel_lcd_init\n");
	if (! machine_is_blueangel())
		return -ENODEV;

	blueangel_boardid=blueangel_get_boardid();

	switch (blueangel_boardid)
	{
	 case 0x4:
		blueangel_fb_info.modelist=blueangel_w100_modes_4;
	 break;
	 case 0x5:
		blueangel_fb_info.modelist=blueangel_w100_modes_5;
	 break;
	 case 0x6:
		blueangel_fb_info.modelist=blueangel_w100_modes_6;
	 break;
	 default:
	 	printk("blueangel lcd_init: unknown boardid=%d. Using 0x6\n",blueangel_boardid);
		blueangel_fb_info.modelist=blueangel_w100_modes_6;
	}

	return platform_driver_register( &blueangel_lcd_driver );
	
	
}

void __exit
blueangel_lcd_exit (void)
{
	platform_driver_unregister( &blueangel_lcd_driver );
}

module_init (blueangel_lcd_init);
module_exit (blueangel_lcd_exit);
MODULE_LICENSE("GPL");
