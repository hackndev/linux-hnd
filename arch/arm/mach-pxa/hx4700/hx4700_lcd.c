/*
 * LCD support for iPAQ HX4700
 *
 * Copyright (c) 2005 Ian Molton
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 04-Jan-2005          Todd Blumer <todd@sdgsystems.com>
 *                      Developed from iPAQ H5400
 * 11-Mar-2005		Ian Molton <spyro@f2s.com>
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <video/w100fb.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/pxa-regs.h>

/* ATI Imageon 3220 Graphics */
#define HX4700_ATI_W3220_PHYS	PXA_CS2_PHYS

static int lcd_power;

static void
lcd_hw_init( void )
{
	// printk( KERN_NOTICE "LCD init: clear values\n" );
	SET_HX4700_GPIO( LCD_SQN, 1 );
	SET_HX4700_GPIO( LCD_LVDD_3V3_ON, 0 );
	SET_HX4700_GPIO( LCD_AVDD_3V3_ON, 0 );
	SET_HX4700_GPIO( LCD_SLIN1, 0 );
	SET_HX4700_GPIO( LCD_RESET_N, 0 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_PC1, 0 );
	SET_HX4700_GPIO( LCD_LVDD_3V3_ON, 0 );
	mdelay(20);

	// printk( KERN_NOTICE "LCD init: set values\n" );
	SET_HX4700_GPIO( LCD_LVDD_3V3_ON, 1 );
	mdelay(5);
	SET_HX4700_GPIO( LCD_AVDD_3V3_ON, 1 );
	// init w3220 regs?

	mdelay(5);
	SET_HX4700_GPIO( LCD_SLIN1, 1 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_RESET_N, 1 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_PC1, 1 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_N2V7_7V3_ON, 1 );
}

static void
lcd_hw_off( void )
{
	SET_HX4700_GPIO( LCD_PC1, 0 );
	SET_HX4700_GPIO( LCD_RESET_N, 0 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_N2V7_7V3_ON, 0 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_AVDD_3V3_ON, 0 );
	mdelay(10);
	SET_HX4700_GPIO( LCD_LVDD_3V3_ON, 0 );
}

static int
hx4700_lcd_set_power( struct lcd_device *lm, int level )
{
	switch (level) {
		case FB_BLANK_UNBLANK:
			lcd_hw_init();
			break;
		case FB_BLANK_NORMAL:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_POWERDOWN:
			lcd_hw_off();
			break;
	}
	lcd_power = level;
	return 0;
}

static int
hx4700_lcd_get_power( struct lcd_device *lm )
{
	return lcd_power;
}

static struct lcd_ops w3220_fb0_lcd = {
	.get_power      = hx4700_lcd_get_power,
	.set_power      = hx4700_lcd_set_power,
};

static struct lcd_device *atifb_lcd_dev;

#ifdef CONFIG_PM

unsigned long ati_gpios[4];

static void
hx4700_lcd_suspend( struct w100fb_par *wfb )
{
	ati_gpios[0] = w100fb_gpio_read(W100_GPIO_PORT_A);
	ati_gpios[1] = w100fb_gpcntl_read(W100_GPIO_PORT_A);
	ati_gpios[2] = w100fb_gpio_read(W100_GPIO_PORT_B);
	ati_gpios[3] = w100fb_gpcntl_read(W100_GPIO_PORT_B);
	w100fb_gpio_write(W100_GPIO_PORT_A, 0xDFE00000 );
	w100fb_gpcntl_write(W100_GPIO_PORT_A, 0xFFFF0000 );
	w100fb_gpio_write(W100_GPIO_PORT_B, 0x00000000 );
	w100fb_gpcntl_write(W100_GPIO_PORT_B, 0xFFFFFFFF );
	lcd_hw_off();
}

static void
hx4700_lcd_resume( struct w100fb_par *wfb )
{
	w100fb_gpio_write(W100_GPIO_PORT_A, ati_gpios[0] );
	w100fb_gpcntl_write(W100_GPIO_PORT_A, ati_gpios[1] );
	w100fb_gpio_write(W100_GPIO_PORT_B, ati_gpios[2] );
	w100fb_gpcntl_write(W100_GPIO_PORT_B, ati_gpios[3] );
	lcd_hw_init();
}

#else
#define hx4700_lcd_resume	NULL
#define hx4700_lcd_suspend	NULL
#endif

struct w100_tg_info hx4700_tg_info = {
	.suspend	= hx4700_lcd_suspend,
	.resume		= hx4700_lcd_resume,
};

//  				 W3220_VGA		QVGA
static struct w100_gen_regs hx4700_w100_regs = {
	.lcd_format =        0x00000003,
	.lcdd_cntl1 =        0x00000000,
	.lcdd_cntl2 =        0x0003ffff,
	.genlcd_cntl1 =      0x00abf003,	//  0x00fff003
	.genlcd_cntl2 =      0x00000003,	
	.genlcd_cntl3 =      0x000102aa,
};

static struct w100_mode hx4700_w100_modes[] = {
{
	.xres 		= 480,
	.yres 		= 640,
	.left_margin 	= 15,
	.right_margin 	= 16,
	.upper_margin 	= 8,
	.lower_margin 	= 7,
	.crtc_ss	= 0x00000000,
	.crtc_ls	= 0xa1ff01f9,	// 0x21ff01f9,
	.crtc_gs	= 0xc0000000,	// 0x40000000,
	.crtc_vpos_gs	= 0x0000028f,
	.crtc_ps1_active = 0x00000000,	// 0x41060010
	.crtc_rev	= 0,
	.crtc_dclk	= 0x80000000,
	.crtc_gclk	= 0x040a0104,
	.crtc_goe	= 0,
	.pll_freq 	= 95,
	.pixclk_divider = 4,
	.pixclk_divider_rotated = 4,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 0,
	.sysclk_src     = CLK_SRC_PLL,
},
{
	.xres 		= 240,
	.yres 		= 320,
	.left_margin 	= 9,
	.right_margin 	= 8,
	.upper_margin 	= 5,
	.lower_margin 	= 4,
	.crtc_ss	= 0x80150014,
	.crtc_ls	= 0xa0fb00f7,
	.crtc_gs	= 0xc0080007,
	.crtc_vpos_gs	= 0x00080007,
	.crtc_rev	= 0x0000000a,
	.crtc_dclk	= 0x81700030,
	.crtc_gclk	= 0x8015010f,
	.crtc_goe	= 0x00000000,
	.pll_freq 	= 95,
	.pixclk_divider = 4,
	.pixclk_divider_rotated = 4,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 0,
	.sysclk_src     = CLK_SRC_PLL,
},
};

struct w100_mem_info hx4700_mem_info = {
	.ext_cntl = 0x09640011,
	.sdram_mode_reg = 0x00600021,
	.ext_timing_cntl = 0x1a001545,	// 0x15001545,
	.io_cntl = 0x7ddd7333,
	.size = 0x1fffff,
};

struct w100_bm_mem_info hx4700_bm_mem_info = {
	.ext_mem_bw = 0x50413e01,
	.offset = 0,
	.ext_timing_ctl = 0x00043f7f,
	.ext_cntl = 0x00000010,
	.mode_reg = 0x00250000,
	.io_cntl = 0x0fff0000,
	.config = 0x08301480,
};

static struct w100_gpio_regs hx4700_w100_gpio_info = {
	.init_data1 = 0xdfe00100,	// GPIO_DATA
	.gpio_dir1  = 0xffff0000,	// GPIO_CNTL1
	.gpio_oe1   = 0x00000000,	// GPIO_CNTL2
	.init_data2 = 0x00000000,	// GPIO_DATA2
	.gpio_dir2  = 0x00000000,	// GPIO_CNTL3
	.gpio_oe2   = 0x00000000,	// GPIO_CNTL4
};

static struct w100fb_mach_info hx4700_fb_info = {
	.tg = &hx4700_tg_info,
	.mem = &hx4700_mem_info, 
	.bm_mem = &hx4700_bm_mem_info, 
	.gpio = &hx4700_w100_gpio_info,
	.regs = &hx4700_w100_regs,
	.modelist = hx4700_w100_modes,
	.num_modes = 2,
	.xtal_freq = 16000000,
};


static struct resource hx4700_fb_resources[] = {
	[0] = {
		.start	= HX4700_ATI_W3220_PHYS,
		.end	= HX4700_ATI_W3220_PHYS + 0x00ffffff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device hx4700_fb_device = {
	.name	= "w100fb",
	.id	= -1,
	.dev	= {
		.platform_data = &hx4700_fb_info,
	},
	.num_resources	= ARRAY_SIZE( hx4700_fb_resources ),
	.resource	= hx4700_fb_resources,
};

static int
hx4700_lcd_probe( struct platform_device *dev )
{
	lcd_hw_init();
	return 0;
}

static int
hx4700_lcd_remove( struct platform_device *dev )
{
	return 0;
}

static struct platform_driver hx4700_lcd_driver = {
	.driver = {
		.name     = "hx4700-lcd",
	},
	.probe    = hx4700_lcd_probe,
	.remove   = hx4700_lcd_remove,
};


static int __init
hx4700_lcd_init( void )
{
	int ret;

	printk( KERN_INFO "hx4700 LCD Driver\n" );
	if (!machine_is_h4700())
		return -ENODEV;
	ret = platform_device_register( &hx4700_fb_device );
	if (ret != 0)
		return ret;

	atifb_lcd_dev = lcd_device_register( "w100fb", (void *)&hx4700_fb_info,
		&w3220_fb0_lcd );
	if (IS_ERR( atifb_lcd_dev ))
		return PTR_ERR( atifb_lcd_dev );

	return platform_driver_register( &hx4700_lcd_driver );
}


static void __exit
hx4700_lcd_exit( void )
{
	lcd_device_unregister( atifb_lcd_dev );
	platform_device_unregister( &hx4700_fb_device );
	platform_driver_unregister( &hx4700_lcd_driver );
}

module_init( hx4700_lcd_init );
module_exit( hx4700_lcd_exit );

MODULE_AUTHOR( "Todd Blumer, SDG Systems, LLC" );
MODULE_DESCRIPTION( "Framebuffer driver for iPAQ hx4700" );
MODULE_LICENSE( "GPL" );

/* vim600: set noexpandtab sw=8 ts=8 :*/

