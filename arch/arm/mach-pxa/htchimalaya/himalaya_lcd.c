/*
 * LCD support for HTC Himalaya
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
#include <linux/mfd/asic3_base.h>

#include <video/w100fb.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch-pxa/htchimalaya-gpio.h>
#include <asm/arch-pxa/htchimalaya-asic.h>
#include <asm/hardware/ipaq-asic3.h>

static int himalaya_boardid;

static struct lcd_device *himalaya_lcd_device;
static int himalaya_lcd_power;

static int himalaya_get_boardid(void)
{

	himalaya_boardid=0;
	if( asic3_gpio_get_value(asic3_dev,GPIO_BOARDID0) ) {
		himalaya_boardid |= 1;
	}
	if( asic3_gpio_get_value(asic3_dev,GPIO_BOARDID1) ) {
		himalaya_boardid |= 2;
	}
	if( asic3_gpio_get_value(asic3_dev,GPIO_BOARDID2) ) {
		himalaya_boardid |= 4;
	}

	system_rev=himalaya_boardid;

 return himalaya_boardid;
}

static void lcd_hw_off( void )
{
	asic3_gpio_set_value(asic3_dev, GPIOB_LCD_PWR2_ON, 0);
	asic3_gpio_set_value(asic3_dev, GPIOB_LCD_PWR3_ON, 0);
	asic3_gpio_set_value(asic3_dev, GPIOB_LCD_PWR4_ON, 0);
	asic3_gpio_set_value(asic3_dev, GPIOB_LCD_PWR1_ON, 0);
	msleep(100);
}

static void lcd_hw_init( void )
{	
	lcd_hw_off();

	switch (himalaya_boardid)
	{
	 case 4:

	  /* pre */
	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR2_ON, 1);
	  msleep(40);

	  /* post */
	  msleep(40);

	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR3_ON, 1);
	  msleep(40); 

	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR1_ON, 1);


	 break;
	 case 6:

	  /* pre */
	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR2_ON, 1);
	  msleep(1); 

	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR3_ON, 1);

	  /* post */
	  msleep(10); 

	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR4_ON, 1);
	  msleep(1); 

	  asic3_gpio_set_value(asic3_dev, GPIO_LCD_PWR1_ON, 1);

	 break;
	}
}


static int
himalaya_lcd_set_power( struct lcd_device *lm, int level )
{
	switch (level) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
			lcd_hw_init();
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			break;
		case FB_BLANK_POWERDOWN:
			lcd_hw_off();
			break;
	}
	himalaya_lcd_power = level;
	return 0;
}

static int himalaya_lcd_get_power( struct lcd_device *lm )
{
	return himalaya_lcd_power;
}

static struct lcd_ops himalaya_lcd_props = {
	.get_power      = himalaya_lcd_get_power,
	.set_power      = himalaya_lcd_set_power,
};

#ifdef CONFIG_PM

unsigned long ati_gpios[4];

static void himalaya_lcd_suspend( struct w100fb_par *wfb )
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
	lcd_hw_off();
}

static void himalaya_lcd_resume( struct w100fb_par *wfb )
{
#if 0
	w100fb_gpio_write(W100_GPIO_PORT_A, ati_gpios[0] );
	w100fb_gpcntl_write(W100_GPIO_PORT_A, ati_gpios[1] );
	w100fb_gpio_write(W100_GPIO_PORT_B, ati_gpios[2] );
	w100fb_gpcntl_write(W100_GPIO_PORT_B, ati_gpios[3] );
#endif
	lcd_hw_init();
}

#else
#define himalaya_lcd_resume	NULL
#define himalaya_lcd_suspend	NULL
#endif

struct w100_tg_info himalaya_tg_info = {
	.suspend	= himalaya_lcd_suspend,
	.resume		= himalaya_lcd_resume,
};

static struct w100_gen_regs himalaya_w100_regs = {
	.lcd_format =        0x00000003,
	.lcdd_cntl1 =        0x00000000,
	.lcdd_cntl2 =        0x0003ffff,
	.genlcd_cntl1 =      0x00fff003,
	.genlcd_cntl2 =      0x00000003,	
	.genlcd_cntl3 =      0x000102aa,
};

static struct w100_mode himalaya4_w100_modes[] = {
{
	.xres 		= 240,
	.yres 		= 320,
	.left_margin 	= 0,
	.right_margin 	= 31,
	.upper_margin 	= 15,
	.lower_margin 	= 0,
	.crtc_ss	= 0x80150014,
	.crtc_ls	= 0xa0fb00f7,
	.crtc_gs	= 0xc0080007,
	.crtc_vpos_gs	= 0x00080007,
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

static struct w100_mode himalaya6_w100_modes[] = {
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
	.crtc_dclk	= 0xa1700030,
	.crtc_gclk	= 0x8015010f,
	.crtc_goe	= 0x00000000,
	.pll_freq 	= 95,
	.pixclk_divider = 0xb,
	.pixclk_divider_rotated = 4,
	.pixclk_src     = CLK_SRC_PLL,
	.sysclk_divider = 1,
	.sysclk_src     = CLK_SRC_PLL,
},
};

static struct w100_gpio_regs himalaya_w100_gpio_info = {
	.init_data1 = 0xffff0000,	// GPIO_DATA
	.gpio_dir1  = 0x00000000,	// GPIO_CNTL1
	.gpio_oe1   = 0x003c0000,	// GPIO_CNTL2
	.init_data2 = 0x00000000,	// GPIO_DATA2
	.gpio_dir2  = 0x00000000,	// GPIO_CNTL3
	.gpio_oe2   = 0x00000000,	// GPIO_CNTL4
};

static struct w100fb_mach_info himalaya_fb_info = {
	.tg = &himalaya_tg_info,
	.gpio = &himalaya_w100_gpio_info,
	.regs = &himalaya_w100_regs,
	.num_modes = 1,
	.xtal_freq = 16000000,
};


static struct resource himalaya_fb_resources[] = {
	[0] = {
		.start	= HIMALAYA_ATI_W3220_PHYS,
		.end	= HIMALAYA_ATI_W3220_PHYS + 0x00ffffff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device himalaya_fb_device = {
	.name	= "w100fb",
	.id	= -1,
	.dev	= {
		.platform_data = &himalaya_fb_info,
	},
	.num_resources	= ARRAY_SIZE( himalaya_fb_resources ),
	.resource	= himalaya_fb_resources,
};

static int himalaya_lcd_probe( struct platform_device *dev )
{
 int ret;

	himalaya_boardid=himalaya_get_boardid();

	printk("Himalaya Board ID 0x%x\n", himalaya_boardid);

	himalaya_lcd_device = lcd_device_register("w100fb", (void *)&himalaya_fb_info, &himalaya_lcd_props);
	if (IS_ERR(himalaya_lcd_device)) {
		return PTR_ERR(himalaya_lcd_device);
	}

	ret = platform_device_register( &himalaya_fb_device );
	// TODO:

 return ret;
}

static int himalaya_lcd_remove( struct platform_device *dev )
{
	return 0;
}

static struct platform_driver himalaya_lcd_driver = {
	.driver = {
		.name     = "himalaya-lcd",
	},
	.probe    = himalaya_lcd_probe,
	.remove   = himalaya_lcd_remove,
};

static int __init himalaya_lcd_init( void )
{

	himalaya_boardid=himalaya_get_boardid();
	printk( KERN_INFO "himalaya LCD Driver init. boardid=%d\n",himalaya_boardid );

	switch (himalaya_boardid)
	{
	 case 0x4:
		himalaya_fb_info.modelist=himalaya4_w100_modes;
	 break;
	 case 0x6:
		himalaya_fb_info.modelist=himalaya6_w100_modes;
	 break;
	 default:
	 	printk("himalaya lcd_init: unknown boardid=%d. Using 0x4\n",himalaya_boardid);
		himalaya_fb_info.modelist=himalaya4_w100_modes;
	}

	return platform_driver_register( &himalaya_lcd_driver );
}


static void __exit himalaya_lcd_exit( void )
{
	platform_driver_unregister( &himalaya_lcd_driver );
}

module_init( himalaya_lcd_init );
module_exit( himalaya_lcd_exit );

MODULE_AUTHOR( "Todd Blumer, SDG Systems, LLC; Luke Kenneth Casson Leighton" );
MODULE_DESCRIPTION( "Framebuffer driver for HTC Himalaya" );
MODULE_LICENSE( "GPL" );

/* vim600: set noexpandtab sw=8 ts=8 :*/

