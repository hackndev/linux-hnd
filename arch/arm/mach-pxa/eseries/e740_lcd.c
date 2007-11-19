/* e740_lcd.c
 *
 * (c) Ian Molton 2005
 *
 * This file contains the definitions for the LCD timings and functions
 * to control the LCD power / frontlighting via the w100fb driver.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include  <linux/device.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/err.h>


#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-gpio.h>
#include <linux/platform_device.h>

#include <video/w100fb.h>

/*
This sequence brings up display on the e740
mmDISP_DB_BUF_CNTL 0x00000078		0x04d8
mmCLK_PIN_CNTL     0x0003f		0x0080
mmPLL_REF_FB_DIV   0x50500d04		0x0084
mmPLL_CNTL         0x4b000200		0x0088
mmSCLK_CNTL        0x00000b11		0x008C
mmPCLK_CNTL        0x00000000		0x0090
mmCLK_TEST_CNTL    0x00000000		0x0094
mmPWRMGT_CNTL      0x00000004		0x0098
mmMC_FB_LOCATION   0x15ff1000		0x0188
mmLCD_FORMAT       0x00008023		0x0410
mmGRAPHIC_CTRL     0x03cf1c06		0x0414
mmGRAPHIC_OFFSET   0x00100000		0x0418
mmGRAPHIC_PITCH    0x000001e0		0x041C
mmCRTC_TOTAL       0x01510120		0x0420
mmACTIVE_H_DISP    0x01040014		0x0424
mmACTIVE_V_DISP    0x01490009		0x0428
mmGRAPHIC_H_DISP   0x01040014		0x042C
mmGRAPHIC_V_DISP   0x01490009		0x0430
mmCRTC_SS	   0x80140013		0x048C
mmCRTC_LS          0x81150110		0x0490
mmCRTC_REV         0x0040010a		0x0494
mmCRTC_DCLK        0xa906000a		0x049C
mmCRTC_GS          0x80050005		0x04A0
mmCRTC_VPOS_GS     0x000a0009		0x04A4
mmCRTC_GCLK        0x80050108		0x04A8
mmCRTC_GOE         0x80050108		0x04AC
mmCRTC_FRAME       0x00000000		0x04B0
mmCRTC_FRAME_VPOS  0x00000000		0x04B4
mmGPIO_DATA        0x21bd21bf		0x04B8
mmGPIO_CNTL1       0xffffff00		0x04BC
mmGPIO_CNTL2       0x03c00643		0x04C0
mmLCDD_CNTL1       0x00000000		0x04C4
mmLCDD_CNTL2       0x0003ffff		0x04C8
mmGENLCD_CNTL1     0x00fff003		0x04CC
mmGENLCD_CNTL2     0x00000003		0x04D0
mmCRTC_DEFAULT_COUNT   0x00000000		0x04E0
mmLCD_BACKGROUND_COLOR 0x0000ff00		0x04E4
mmCRTC_PS1_ACTIVE      0x41060010		0x04F0
mmGENLCD_CNTL3         0x000143aa		0x0524
mmGPIO_DATA2           0xff3fff3f		0x0528
mmGPIO_CNTL3           0xffff0000		0x052C
mmGPIO_CNTL4           0x000000ff		0x0530
mmDISP_DEBUG2          0x00800000		0x0538
.long 0x0c011004  == 0x00100000   @ enable accel?
mmDISP_DB_BUF_CNTL 0x0000007b

devmem2 0x0c0104d8 w 0x00000078
devmem2 0x0c010080 w 0x0000003f
devmem2 0x0c010084 w 0x50500d04
devmem2 0x0c010088 w 0x4b000200
devmem2 0x0c01008C w 0x00000b11
devmem2 0x0c010090 w 0x00000000
devmem2 0x0c010094 w 0x00000000
devmem2 0x0c010098 w 0x00000004
devmem2 0x0c010188 w 0x15ff1000
devmem2 0x0c010410 w 0x00008023
devmem2 0x0c010414 w 0x03cf1c06
devmem2 0x0c010418 w 0x00100000
devmem2 0x0c01041C w 0x000001e0
devmem2 0x0c010420 w 0x01510120
devmem2 0x0c010424 w 0x01040014
devmem2 0x0c010428 w 0x01490009
devmem2 0x0c01042C w 0x01040014
devmem2 0x0c010430 w 0x01490009
devmem2 0x0c01048C w 0x80140013
devmem2 0x0c010490 w 0x81150110
devmem2 0x0c010494 w 0x0040010a
devmem2 0x0c01049C w 0xa906000a
devmem2 0x0c0104A0 w 0x80050005
devmem2 0x0c0104A4 w 0x000a0009
devmem2 0x0c0104A8 w 0x80050108
devmem2 0x0c0104AC w 0x80050108
devmem2 0x0c0104B0 w 0x00000000
devmem2 0x0c0104B4 w 0x00000000
devmem2 0x0c0104B8 w 0x21bd21bf
devmem2 0x0c0104BC w 0xffffff00
devmem2 0x0c0104C0 w 0x03c00643
devmem2 0x0c0104C4 w 0x00000000
devmem2 0x0c0104C8 w 0x0003ffff
devmem2 0x0c0104CC w 0x00fff003
devmem2 0x0c0104D0 w 0x00000003
devmem2 0x0c0104E0 w 0x00000000
devmem2 0x0c0104E4 w 0x0000ff00
devmem2 0x0c0104F0 w 0x41060010
devmem2 0x0c010524 w 0x000143aa
devmem2 0x0c010528 w 0xff3fff3f
devmem2 0x0c01052C w 0xffff0000
devmem2 0x0c010530 w 0x000000ff
devmem2 0x0c010538 w 0x00800000  # 0xc0800000
devmem2 0x0c0104d8 w 0x0000007b

**potential** shutdown routine
devmem2 0x0c010528 w 0xff3fff00
devmem2 0x0c010190 w 0x7FFF8000
devmem2 0x0c0101b0 w 0x00FF0000
devmem2 0x0c01008c w 0x00000000
devmem2 0x0c010080 w 0x000000bf
devmem2 0x0c010098 w 0x00000015
devmem2 0x0c010088 w 0x4b000204
devmem2 0x0c010098 w 0x0000001d

*/

static struct w100_gen_regs e740_lcd_regs = {
	.lcd_format =            0x00008023,
	.lcdd_cntl1 =            0x0f000000,
	.lcdd_cntl2 =            0x0003ffff,
	.genlcd_cntl1 =          0x00ffff03,
	.genlcd_cntl2 =          0x003c0f03,
	.genlcd_cntl3 =          0x000143aa,
};

static struct w100_mode e740_lcd_mode = {
	.xres            = 240,
	.yres            = 320,
//	.bpp = 16,
	.left_margin     = 20,
	.right_margin    = 28,
	.upper_margin    = 9,
	.lower_margin    = 8,
	.crtc_ss         = 0x80140013,
	.crtc_ls         = 0x81150110,
	.crtc_gs         = 0x80050005,
	.crtc_vpos_gs    = 0x000a0009,
	.crtc_rev        = 0x0040010a,
	.crtc_dclk       = 0xa906000a,
	.crtc_gclk       = 0x80050108,
	.crtc_goe        = 0x80050108,
	.pll_freq        = 57,
	.pixclk_divider         = 4,
	.pixclk_divider_rotated = 4,
	.pixclk_src     = CLK_SRC_XTAL,
	.sysclk_divider  = 1,
	.sysclk_src     = CLK_SRC_PLL,
	.crtc_ps1_active =       0x41060010,
};


static struct w100_gpio_regs e740_w100_gpio_info = {
	.init_data1 = 0x21002103,
	.gpio_dir1  = 0xffffdeff,
	.gpio_oe1   = 0x03c00643,
	.init_data2 = 0x003f003f,
	.gpio_dir2  = 0xffffffff,
	.gpio_oe2   = 0x000000ff,
};

static struct w100fb_mach_info e740_fb_info = {
	.modelist   = &e740_lcd_mode,
	.num_modes  = 1,
	.regs       = &e740_lcd_regs,
	.gpio       = &e740_w100_gpio_info,
	.xtal_freq = 14318000,
	.xtal_dbl   = 1,
};

static struct resource e740_fb_resources[] = {
	[0] = {
		.start          = 0x0c000000,
		.end            = 0x0cffffff,
		.flags          = IORESOURCE_MEM,
	},
};

/* ----------------------- device declarations -------------------------- */


static struct platform_device e740_fb_device = {
	.name           = "w100fb",
	.id             = -1,
	.dev            = {
		.platform_data  = &e740_fb_info,
	},
	.num_resources  = ARRAY_SIZE(e740_fb_resources),
	.resource       = e740_fb_resources,
};


static struct platform_device e740_lcd_hook_device = {
	.name = "e740-lcd-hook",
	.dev = {
		.parent = &e740_fb_device.dev,
	},
	.id = -1,
};

static int e740_lcd_init (void) {
	int ret;

	if (!machine_is_e740 ())
		return -ENODEV;

	if((ret = platform_device_register(&e740_fb_device)))
		return ret;

	platform_device_register(&e740_lcd_hook_device);

	return 0;
}

static void e740_lcd_exit (void)
{
	//FIXME - free the platform dev for the w100 (imageon)
}

module_init (e740_lcd_init);
module_exit (e740_lcd_exit);

MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("e740 lcd driver");
MODULE_LICENSE("GPLv2");
