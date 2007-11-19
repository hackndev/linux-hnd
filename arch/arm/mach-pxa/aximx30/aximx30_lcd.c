/*
 * LCD driver for Dell Axim X30
 * 
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 * 
 * based on previous work, see below:
 * 
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
#include <linux/err.h>

#include <asm/arch/aximx3-init.h>
#include <asm/arch/aximx3-gpio.h>
#include <linux/fb.h>
#include <asm/mach-types.h>
#include "asm/arch/pxa-regs.h"
#include "asm/arch/pxafb.h"

static int aximx30_lcd_set_power (struct lcd_device *lm, int setp)
{
	return 0;
}

static int aximx30_lcd_get_power (struct lcd_device *lm)
{
	return 0;
}

static struct pxafb_mode_info aximx30_lcd_modes[] = {
{
	.pixclock =     0,   // --
	.bpp =          16,  // ??
	.xres =         240, // PPL + 1
	.yres =         320, // LPP + 1
	.hsync_len =    20,  // HSW + 1
	.vsync_len =    4,   // VSW + 1
	.left_margin =  59,  // BLW + 1
	.upper_margin = 4,   // BFW
	.right_margin = 16,  // ELW + 1
	.lower_margin = 0,   // EFW
	.sync =         0,   // --
}
};

static struct pxafb_mach_info aximx30_fb_info =
{
        .modes		= aximx30_lcd_modes,
        .num_modes	= ARRAY_SIZE(aximx30_lcd_modes), 

	.lccr0 = 0x003008f9, // yes that's ugly, but it's due a limitation of the
	.lccr3 = 0x04900008 // pxafb driver assuming pxa25x fbs...
};

struct lcd_ops aximx30_lcd_properties =
{
	.set_power = aximx30_lcd_set_power,
	.get_power = aximx30_lcd_get_power,
};

static struct lcd_device *pxafb_lcd_device;

static int __init
aximx30_lcd_init (void)
{
//	if (! machine_is_x30 ())
//		return -ENODEV;

	set_pxa_fb_info(&aximx30_fb_info);
	pxafb_lcd_device = lcd_device_register("pxafb", NULL, &aximx30_lcd_properties);
//	if (IS_ERR (pxafb_lcd_device))
//		return PTR_ERR (pxafb_lcd_device);

	return 0;
}

static void __exit
aximx30_lcd_exit (void)
{
	lcd_device_unregister (pxafb_lcd_device);
}

module_init (aximx30_lcd_init);
module_exit (aximx30_lcd_exit);

MODULE_AUTHOR("Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>");
MODULE_DESCRIPTION("Dell Axim X30 Core frambuffer driver");
MODULE_LICENSE("GPL");

