/*
 * Machine initialization for Dell Axim X3
 *
 * Authors: Andrew Zabolotny <zap@homelink.ru>
 *
 * For now this is mostly a placeholder file; since I don't own an Axim X3
 * it is supposed the project to be overtaken by somebody else. The code
 * in here is *supposed* to work so that you can at least boot to the command
 * line, but there is no guarantee.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/err.h>

#include <asm/arch/aximx3-init.h>
#include <asm/arch/aximx3-gpio.h>
#include <linux/fb.h>
#include "asm/arch/pxafb.h"

static int aximx3_lcd_set_power (struct lcd_device *lm, int setp)
{
	return 0;
}

static int aximx3_lcd_get_power (struct lcd_device *lm)
{
	return 0;
}

static int aximx3_lcd_set_enable (struct lcd_device *lm, int setp)
{
	return -EINVAL;
}

static int aximx3_lcd_get_enable (struct lcd_device *lm)
{
	return -EINVAL;
}

/*  LCCR0: 0x3008f9	-- ENB | LDM | SFM | IUM | EFM | PAS | QDM | BM | OUM
    LCCR1: 0x1a0a10ef	-- PPL=0xef, HSW=0x4, ELW=0xA, BLW=0x1A
    LCCR2: 0x303013f	-- BFW=0x0, EFW=0x30, VSW=0xC, LPP=0x13F
    LCCR3: 0x4700008	-- PCD=0x8, ACB=0x0, API=0x0, VSP, HSP, PCP, BPP=0x4 */
          
static struct pxafb_mach_info aximx3_fb_info =
{
	.pixclock =	0,
	.bpp =		16,
	.xres =		240,
	.yres =		320,
	.hsync_len =	5,
	.vsync_len =	13,
	.left_margin =	27,
	.upper_margin =	1,
	.right_margin =	11,
	.lower_margin =	49,
	.sync =		0,
	.lccr0 =	(LCCR0_LDM | LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_PAS | LCCR0_QDM | LCCR0_BM | LCCR0_OUM),
	.lccr3 =	(LCCR3_HorSnchL | LCCR3_VrtSnchL | LCCR3_16BPP | LCCR3_PCP | /*PCD */ 0x8)
};

struct lcd_ops aximx3_lcd_properties =
{
	.set_power = aximx3_lcd_set_power,
	.get_power = aximx3_lcd_get_power,
	.set_enable = aximx3_lcd_set_enable,
	.get_enable = aximx3_lcd_get_enable
};

static int aximx3_backlight_set_power (struct backlight_device *bm, int on)
{
	return 0;
}

static int aximx3_backlight_get_power (struct backlight_device *bm)
{
	return 0;
}

static struct backlight_properties aximx3_backlight_properties =
{
	.owner         = THIS_MODULE,
	.set_power     = aximx3_backlight_set_power,
	.get_power     = aximx3_backlight_get_power,
};

static struct lcd_device *pxafb_lcd_device;
static struct backlight_device *pxafb_backlight_device;

static int
aximx3_lcd_init (void)
{
	if (! machine_is_aximx3 ())
		return -ENODEV;

	set_pxa_fb_info(&aximx3_fb_info);
	pxafb_lcd_device = lcd_device_register("pxafb", NULL, &aximx3_lcd_properties);
	if (IS_ERR (pxafb_lcd_device))
		return PTR_ERR (pxafb_lcd_device);
	pxafb_backlight_device = backlight_device_register("pxafb", NULL,
		&aximx3_backlight_properties);
	if (IS_ERR (pxafb_backlight_device)) {
		lcd_device_unregister (pxafb_lcd_device);
		return PTR_ERR (pxafb_backlight_device);
	}

	return 0;
}

static void
aximx3_lcd_exit (void)
{
	lcd_device_unregister (pxafb_lcd_device);
	backlight_device_unregister (pxafb_backlight_device);
}

module_init (aximx3_lcd_init);
module_exit (aximx3_lcd_exit);
