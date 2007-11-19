/*
 * LCD controls for roverp5p
 *
 * Copyright ? 2004 Andrew Zabolotny
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *
 * 2003		Andrew Zabolotny		Original code for Axim X5
 * 2004		Konstantine A. Beklemishev	Copied from aximx5_lcd.c and adapted to Rover P5+
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
#include <linux/device.h>
#include <linux/soc-old.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/roverp5p-gpio.h>
#include "../drivers/soc/mq11xx.h"

#if 0
#  define debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug(s, args...)
#endif
#define debug_func(s, args...) debug ("%s: " s, __FUNCTION__, ##args)

static int roverp5p_lcd_set_power (struct lcd_device *ld, int state)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	int lcdfp_state = mq_base->get_power (mq_base, MEDIAQ_11XX_FP_DEVICE_ID) ? 1 : 0;
	int fpctl_state = mq_base->get_power (mq_base, MEDIAQ_11XX_FB_DEVICE_ID) ? 1 : 0;

	int new_lcdfp_state = (state < 1) ? 1 : 0;
	int new_fpctl_state = (state < 4) ? 1 : 0;

	debug_func ("state:%d\n", state);

	if (new_lcdfp_state != lcdfp_state)
		mq_base->set_power (mq_base, MEDIAQ_11XX_FP_DEVICE_ID, new_lcdfp_state);
	if (new_fpctl_state != fpctl_state)
		mq_base->set_power (mq_base, MEDIAQ_11XX_FB_DEVICE_ID, new_fpctl_state);

	if (new_lcdfp_state)
		/* MediaQ GPIO 1 seems connected to flat-panel power */
		mq_base->set_GPIO (mq_base, 1, MQ_GPIO_OUT1);
	else
		/* Output '0' to MQ1132 GPIO 1 */
		mq_base->set_GPIO (mq_base, 1, MQ_GPIO_OUT0);

	return 0;
}

static int roverp5p_lcd_get_power (struct lcd_device *ld)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	int lcdfp_state = mq_base->get_power (mq_base, MEDIAQ_11XX_FP_DEVICE_ID);
	int fpctl_state = mq_base->get_power (mq_base, MEDIAQ_11XX_FB_DEVICE_ID);

	debug_func ("\n");

	if (lcdfp_state == 0) {
		if (fpctl_state == 0)
			return 4;
		else
			return 2;
	}
	return 0;
}

/* This routine is generic for any MediaQ 1100/1132 chips */
static int
roverp5p_lcd_set_contrast (struct lcd_device *ld, int contrast)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	u32 x;

	debug_func ("contrast:%d\n", contrast);

	/* Well... this is kind of tricky but here's how it works:
	 * On 24-bit TFT panels the R,G,B channels are output via
	 * the FD0..FD23 MQ1132' outputs. There are separate enable
	 * bits for these pins in FP07R, which we will use.
	 * Basically we just mask out (setting them to '1')
	 * the lowest 1..8 bits of every color component thus
	 * effectively reducing the number of bits for them.
	 */
	if (contrast > 7)
		contrast = 7;
	contrast = 7 ^ contrast;

	x = (1 << contrast) - 1;

	mq_base->regs->FP.pin_output_data |= 0x00ffffff;
	mq_base->regs->FP.pin_output_select_1 =
		(mq_base->regs->FP.pin_output_select_1 & 0xff000000) |
		x | (x << 8) | (x << 16);

	return 0;
}

static int
roverp5p_lcd_get_contrast (struct lcd_device *ld)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	u32 c, x;

	debug_func ("\n");

	x = (mq_base->regs->FP.pin_output_select_1 & 0x7f);
	for (c = 7; x; x >>= 1, c--)
		;
	return c;
}

/*---* Backlight *---*/

static int 
roverp5p_backlight_set_power (struct backlight_device *bd, int state)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&bd->class_dev);

	debug_func ("state:%d\n", state);

	/* Turn off backlight power */
	if (state == 0) {
		mq_base->set_GPIO (mq_base, 64, MQ_GPIO_IN | MQ_GPIO_OUT1);
		if (PWM_PWDUTY0 & 0x3ff)
			CKEN |= CKEN0_PWM0;
		else
			CKEN &= ~CKEN0_PWM0;
	} else {
		CKEN &= ~CKEN0_PWM0;
		mq_base->set_GPIO (mq_base, 64, MQ_GPIO_IN | MQ_GPIO_OUT0);
	}
	return 0;
}

static int 
roverp5p_backlight_get_power (struct backlight_device *bd)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&bd->class_dev);

	debug_func ("\n");

	return mq_base->get_GPIO (mq_base, 64) ? 0 : 4;
}

static int
roverp5p_backlight_set_brightness (struct backlight_device *bd, int brightness)
{
	debug_func ("brightness:%d\n", brightness);

	if (brightness > 0x3ff)
		brightness = 0x3ff;

	/* LCD brightness on Dell Axim is driven by PWM0.
	 * We'll set the pre-scaler to 8, and the period to 1024, this
	 * means the backlight refresh rate will be 3686400/(8*1024) =
	 * 450 Hz which is quite enough.
	 */
	PWM_CTRL0 = 7; /* pre-scaler */
	PWM_PWDUTY0 = brightness; /* duty cycle */
	PWM_PERVAL0 = 0x3ff; /* period */

	if (brightness)
		CKEN |= CKEN0_PWM0;
	else
		CKEN &= ~CKEN0_PWM0;

        return 0;
}

static int
roverp5p_backlight_get_brightness (struct backlight_device *bd)
{
        u32 x, y;

	debug_func ("\n");

	x = PWM_PWDUTY0 & 0x3ff;
	y = PWM_PERVAL0 & 0x3ff;
	// To avoid division, we'll approximate y to nearest 2^n-1.
	// PocketPC is using 0xfe as PERVAL0 and we're using 0x3ff.
	x <<= (10 - fls (y));
	return x;
}

/*-------------------// Flat panel driver for SoC device //------------------*/

static struct lcd_ops mq11xx_fb0_lcd = {
	.get_power      = roverp5p_lcd_get_power,
	.set_power      = roverp5p_lcd_set_power,
	.max_contrast   = 7,
	.get_contrast   = roverp5p_lcd_get_contrast,
	.set_contrast   = roverp5p_lcd_set_contrast,
};

static struct backlight_properties mq11xx_fb0_bl = {
	.owner		= THIS_MODULE,
	.get_power      = roverp5p_backlight_get_power,
	.set_power      = roverp5p_backlight_set_power,
	.max_brightness = 0x3ff,
	.get_brightness = roverp5p_backlight_get_brightness,
	.set_brightness = roverp5p_backlight_set_brightness,
};

static struct lcd_device *mqfb_lcd_device;
static struct backlight_device *mqfb_backlight_device;

static int roverp5p_fp_probe (struct device *dev)
{
	struct mediaq11xx_base *mq_base =
		(struct mediaq11xx_base *)dev->platform_data;

	mqfb_backlight_device = backlight_device_register ("mq11xx_fb0",
		mq_base, &mq11xx_fb0_bl);
	if (IS_ERR (mqfb_backlight_device))
		return PTR_ERR (mqfb_backlight_device);
	mqfb_lcd_device = lcd_device_register ("mq11xx_fb0", mq_base,
		&mq11xx_fb0_lcd);
	if (IS_ERR (mqfb_lcd_device)) {
		backlight_device_unregister (mqfb_backlight_device);
		return PTR_ERR (mqfb_lcd_device);
	}

	return 0;
}

static int roverp5p_fp_remove (struct device *dev)
{
	debug_func ("\n");

	backlight_device_unregister (mqfb_backlight_device);
	lcd_device_unregister (mqfb_lcd_device);

        return 0;
}

static int roverp5p_fp_suspend (struct device *dev, u32 state, u32 level)
{
	debug_func ("\n");
        return 0;
}

static int roverp5p_fp_resume (struct device *dev, u32 level)
{
	debug_func ("\n");
	return 0;
}

static platform_device_id roverp5p_fp_device_ids[] = { MEDIAQ_11XX_FP_DEVICE_ID, 0 };

struct device_driver roverp5p_fp_device_driver = {
	.name     = "mq11xx_lcd",
	.probe    = roverp5p_fp_probe,
	.remove   = roverp5p_fp_remove,
	.suspend  = roverp5p_fp_suspend,
	.resume   = roverp5p_fp_resume
};

static int __init
roverp5p_fp_init(void)
{
	debug_func ("\n");
	return driver_register (&roverp5p_fp_device_driver);
}

static void __exit
roverp5p_fp_exit(void)
{
	debug_func ("\n");
	driver_unregister (&roverp5p_fp_device_driver);
}

module_init(roverp5p_fp_init);
module_exit(roverp5p_fp_exit);

MODULE_AUTHOR("Konstantine A. Bekelmishev <konstantine@r66.ru>");
MODULE_DESCRIPTION("LCD driver for Rover P5+");
MODULE_LICENSE("GPL");
