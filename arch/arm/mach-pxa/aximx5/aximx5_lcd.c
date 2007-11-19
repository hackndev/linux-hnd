/*
 * MediaQ 1132 LCD controller initialization for Dell Axim X5.
 *
 * Copyright © 2003 Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/soc-old.h>
#include <linux/err.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/aximx5-gpio.h>
#include <linux/mfd/mq11xx.h>

#if 0
#  define debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug(s, args...)
#endif
#define debug_func(s, args...) debug ("%s: " s, __FUNCTION__, ##args)

static int aximx5_lcd_set_power (struct lcd_device *ld, int state)
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
		mq_base->set_GPIO (mq_base, GPIO_NR_AXIMX5_MQ1132_FP_ENABLE, MQ_GPIO_OUT1);
	else
		/* Output '0' to MQ1132 GPIO 1 */
		mq_base->set_GPIO (mq_base, GPIO_NR_AXIMX5_MQ1132_FP_ENABLE, MQ_GPIO_OUT0);

	return 0;
}

static int aximx5_lcd_get_power (struct lcd_device *ld)
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

static int
aximx5_lcd_set_contrast (struct lcd_device *ld, int contrast)
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
aximx5_lcd_get_contrast (struct lcd_device *ld)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	u32 c, x;

	debug_func ("\n");

	x = (mq_base->regs->FP.pin_output_select_1 & 0x7f);
	for (c = 7; x; x >>= 1, c--)
		;
	return c;
}

/*-------------------// Flat panel driver for SoC device //------------------*/

static struct lcd_ops mq11xx_fb0_lcd = {
	.get_power      = aximx5_lcd_get_power,
	.set_power      = aximx5_lcd_set_power,
//	.max_contrast   = 7,
	.get_contrast   = aximx5_lcd_get_contrast,
	.set_contrast   = aximx5_lcd_set_contrast,
};

static struct lcd_device *mqfb_lcd_device;

static int aximx5_lcd_probe(struct platform_device *dev)
{
	struct mediaq11xx_base *mq_base = dev->dev.platform_data;

	mqfb_lcd_device = lcd_device_register ("mq11xx_fb0", mq_base,
		&mq11xx_fb0_lcd);
	if (IS_ERR (mqfb_lcd_device)) {
		return PTR_ERR (mqfb_lcd_device);
	}

	return 0;
}

static int aximx5_lcd_remove(struct platform_device *dev)
{
	debug_func ("\n");

	lcd_device_unregister (mqfb_lcd_device);

	return 0;
}

static int aximx5_lcd_suspend(struct platform_device *dev, pm_message_t state)
{
	debug_func ("\n");
	return 0;
}

static int aximx5_lcd_resume(struct platform_device *dev)
{
	debug_func ("\n");
	return 0;
}

//static platform_device_id aximx5_lcd_device_ids[] = { MEDIAQ_11XX_FP_DEVICE_ID, 0 };

struct platform_driver aximx5_lcd_driver = {
	.driver	= {
		.name     = "mq11xx_lcd",
	},
	.probe    = aximx5_lcd_probe,
	.remove   = aximx5_lcd_remove,
	.suspend  = aximx5_lcd_suspend,
	.resume   = aximx5_lcd_resume
};

static int __init
aximx5_lcd_init(void)
{
	debug_func ("\n");
	return platform_driver_register(&aximx5_lcd_driver);
}

static void __exit
aximx5_lcd_exit(void)
{
	debug_func ("\n");
	platform_driver_unregister(&aximx5_lcd_driver);
}

module_init(aximx5_lcd_init);
module_exit(aximx5_lcd_exit);

MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("LCD driver for Dell Axim X5");
MODULE_LICENSE("GPL");
