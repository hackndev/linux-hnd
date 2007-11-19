/*
 * LCD controls for h2200
 *
 * Copyright © 2004 Andrew Zabolotny
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * History:
 *
 * 2003		Andrew Zabolotny	Original code for Axim X5
 * 2004-02-22	Pattrick Hueper		Copied from aximx5_lcd.c and adapted to h2200
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/soc-old.h>
#include <linux/mfd/mq11xx.h>

#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h2200.h>
#include <asm/arch/h2200-gpio.h>

#if 0
#  define debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug(s, args...)
#endif
#define debug_func(s, args...) debug ("%s: " s, __FUNCTION__, ##args)

#define H2200_MQ_MAX_CONTRAST	7

#define MQ_BASE PXA_CS1_PHYS

static void h2200_mq_set_power (int on);

/* MediaQ 1178 init state */
static struct mediaq11xx_init_data mq1178_init_data = {
    .irq_base = H2200_MQ11XX_IRQ_BASE,
    .DC = {
	/* dc00 */		0x00000001,
	/* dc01 */		0x00000042,
	/* dc02 */		0x00000001,
	/* dc03 NOT SET */	0x0,		/* wince 0x00000008 RO */
	/* dc04 */		0x00000004,
        /* dc05 */		0x00000009,
    },
    .CC = {
	/* cc00 */		0x00000000, /* wince 0x00000002 */
	/* cc01 */		0x00201010,
	/* cc02 */		0x00000000,
	/* cc03 */		0x00000000,
	/* cc04 */		0x00000004,
    },
    .MIU = {
	/* mm00 */		0x00000001,
	/* mm01 */		0x1b676ca0,
	/* mm02 */		0x00000000,
	/* mm03 */		0x0000bb4d,
        /* mm04 */		0x3eee3e45,
        /* mm05 */		0x00170211,
        /* mm06 */		0x00000000,
    },
    .GC = {
	/* gc00 */		0x051100c8, /* powered down */
	/* gc01 */		0x00000200,
	/* gc02 */		0x00f8013b,
	/* gc03 */		0x013f0154,
	/* gc04 */		0x01370132,
	/* gc05 */		0x01550154,
	/* gc06 */		0x01350000,
	/* gc07 NOT SET */	0x01370000,
	/* gc08 */		0x00ef0000,
	/* gc09 */		0x013f0000,
	/* gc0a */		0x00f88000,
	/* gc0b */		0x006e8037,
	/* gc0c */		0x00000000,	/* wince 0x00002000 */
	/* gc0d */		0x00457ff2,
	/* gc0e */		0x000001e0,
	/* gc0f NOT SET */	0x0,
	/* gc10 */		0x010107b4,
	/* gc11 */		0x0a1900d3,
	/* gc12 NOT SET */	0x00fc3c6c,
	/* gc13 NOT SET */	0x00f8303c,
	/* gc14 */		0x02fec234,
	/* gc15 */		0x00000000,
	/* gc16 */		0x00000000,
	/* gc17 */		0x00000000,
	/* gc18 */		0x00000000,
	/* gc19 */		0x00000000,
	/* gc1a */		0x00000000, /* wince 0x004e82bb */
    },
    .FP = {
	/* fp00 */		0x00006020,
	/* fp01 */		0x000c5008, /* wince 0x000cd008 */
	/* fp02 */		0x9ffdfcfd,
	/* fp03 */		0x00000000,
	/* fp04 */		0x00bd0001,
	/* fp05 */		0x01010000,
	/* fp06 */		0x80000000,
	/* fp07 */		0x00000000,
	/* fp08 */		0xbc3e5b3f,
	/* fp09 NOT SET */	0xfffffdff,
	/* fp0a */		0x00000000,
	/* fp0b */		0x00000000,
	/* fp0c NOT SET */	0x00000003,
	/* fp0d NOT SET */	0x0,
	/* fp0e NOT SET */	0x0,
	/* fp0f */		0x25180000,
	/* fp10 */		0x553fde9e,
	/* fp11 */		0xcfb7f3fb,
	/* fp12 */		0x5ba7df29,
	/* fp13 */		0xf7787cd6,
	/* fp14 */		0xb6f7c3b9,
	/* fp15 */		0x757fffdd,
	/* fp16 */		0x3de535d0,
	/* fp17 */		0xbdf8231f,
	/* fp18 */		0xfeaee16d,
	/* fp19 */		0x9c78ceff,
	/* fp1a */		0xc8ffefac,
	/* fp1b */		0xfdfb3bff,
	/* fp1c */		0x7f2dbdf9,
	/* fp1d */		0xbf7536ea,
	/* fp1e */		0xfbdb7d65,
        /* fp1f */		0xb1dabdee,
	/* fp20 */		0xfe7a5eef,
	/* fp21 */		0x77ffefe7,
	/* fp22 */		0xdffea7ff,
	/* fp23 */		0xe7faed77,
	/* fp24 */		0xdffef735,
	/* fp25 */		0x53feefff,
	/* fp26 */		0x5f7bfdff,
	/* fp27 */		0x7dbefefc,
	/* fp28 */		0x8dfae4f6,
	/* fp29 */		0xfffbf7db,
	/* fp2a */		0xfaffdff7,
	/* fp2b */		0xdffeb6f7,
	/* fp2c */		0xe736eeff,
	/* fp2d */		0x7ef7ed7b,
	/* fp2e */		0xdf1ceef7,
	/* fp2f */		0x7bdeefb3,
	/* fp30 */		0x5ffdeff7,
	/* fp31 */		0xdfb9e7ff,
	/* fp32 */		0xdbf8c7f7,
	/* fp33 */		0x5b5bf5ff,
	/* fp34 */		0xfe696cdf,
	/* fp35 */		0x6efafff7,
	/* fp36 */		0x2cfb7fff,
        /* fp37 */		0x7ffa5cf5,
	/* fp38 */		0xcbf30000,
	/* fp39 */		0x0000e6d3,
	/* fp3a */		0x0000f2b3,
	/* fp3b */		0x0000effe,
	/* fp3c */		0xff5efffd,
	/* fp3d */		0x7ffd77b7,
	/* fp3e */		0x6ff9fef7,
	/* fp3f */		0x7ffafff7,
	/* fp40 */		0x0,
	/* fp41 */		0x0,
	/* fp42 */		0x0,
	/* fp43 */		0x0,
	/* fp44 */		0x0,
	/* fp45 */		0x0,
	/* fp46 */		0x0,
	/* fp47 */		0x0,
	/* fp48 */		0x0,
	/* fp49 */		0x0,
	/* fp4a */		0x0,
	/* fp4b */		0x0,
	/* fp4c */		0x0,
	/* fp4d */		0x0,
	/* fp4e */		0x0,
	/* fp4f */		0x0,
	/* fp50 */		0x0,
	/* fp51 */		0x0,
	/* fp52 */		0x0,
	/* fp53 */		0x0,
	/* fp54 */		0x0,
	/* fp55 */		0x0,
	/* fp56 */		0x0,
	/* fp57 */		0x0,
	/* fp58 */		0x0,
	/* fp59 */		0x0,
	/* fp5a */		0x0,
	/* fp5b */		0x0,
	/* fp5c */		0x0,
	/* fp5d */		0x0,
	/* fp5e */		0x0,
	/* fp5f */		0x0,
	/* fp60 */		0x0,
	/* fp61 */		0x0,
	/* fp62 */		0x0,
	/* fp63 */		0x0,
	/* fp64 */		0x0,
	/* fp65 */		0x0,
	/* fp66 */		0x0,
	/* fp67 */		0x0,
	/* fp68 */		0x0,
	/* fp69 */		0x0,
	/* fp6a */		0x0,
	/* fp6b */		0x0,
	/* fp6c */		0x0,
	/* fp6d */		0x0,
	/* fp6e */		0x0,
	/* fp6f */		0x0,
	/* fp70 */		0x0,
	/* fp71 */		0x0,
	/* fp72 */		0x0,
	/* fp73 */		0x0,
	/* fp74 */		0x0,
	/* fp75 */		0x0,
	/* fp76 */		0x0,
	/* fp77 */		0x0,
    },
    .GE = {
	/* ge00 NOT SET */	0x0,
	/* ge01 NOT SET */	0x0,
	/* ge02 NOT SET */	0x0,
	/* ge03 NOT SET */	0x0,
	/* ge04 NOT SET */	0x0,
	/* ge05 NOT SET */	0x0,
	/* ge06 NOT SET */	0x0,
	/* ge07 NOT SET */	0x0,
	/* ge08 NOT SET */	0x0,
	/* ge09 NOT SET */	0x0,
	/* ge0a */		0x00000000,
	/* ge0b */		0x6b6ffeae,
	/* ge0c NOT SET */	0x0,
	/* ge0d NOT SET */	0x0,
	/* ge0e NOT SET */	0x0,
	/* ge0f NOT SET */	0x0,
	/* ge10 NOT SET */	0x0,
	/* ge11 */		0x55,
	/* ge12 NOT SET */	0x0,
	/* ge13 NOT SET */	0x0,
    },
    .SPI = { },

    .set_power = h2200_mq_set_power,
};

static int save_lcd_power;
static int save_lcd_contrast;


/* controls power to the MediaQ chip */
static void
h2200_mq_set_power(int on)
{
	if (on) {
		/* XXX Toggle MQ1178_RESET, and set MQ1178_VDD = 0? */
		SET_H2200_GPIO(MQ1178_POWER_ON, 1);
		mdelay(10);
	} else {
		SET_H2200_GPIO(MQ1178_POWER_ON, 0);
		/* XXX Set MQ1178_VDD = 1? */
	}
}

static int h2200_lcd_set_power(struct lcd_device *ld, int state)
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

	return 0;
}

static int h2200_lcd_get_power(struct lcd_device *ld)
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
h2200_lcd_set_contrast(struct lcd_device *ld, int contrast)
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
	if (contrast > H2200_MQ_MAX_CONTRAST)
		contrast = H2200_MQ_MAX_CONTRAST;
	contrast = H2200_MQ_MAX_CONTRAST ^ contrast;

	x = (1 << contrast) - 1;

	mq_base->regs->FP.pin_output_data |= 0x00ffffff;
	mq_base->regs->FP.pin_output_select_1 =
		(mq_base->regs->FP.pin_output_select_1 & 0xff000000) |
		x | (x << 8) | (x << 16);

	return 0;
}

static int
h2200_lcd_get_contrast(struct lcd_device *ld)
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
	.get_power      = h2200_lcd_get_power,
	.set_power      = h2200_lcd_set_power,
//	.max_contrast   = H2200_MQ_MAX_CONTRAST,
	.get_contrast   = h2200_lcd_get_contrast,
	.set_contrast   = h2200_lcd_set_contrast,
};

static struct lcd_device *mqfb_lcd_device;

static int h2200_fp_probe(struct platform_device *pdev)
{
	struct mediaq11xx_base *mq_base =
		(struct mediaq11xx_base *)pdev->dev.platform_data;

	mqfb_lcd_device = lcd_device_register ("mq11xx_fb0", mq_base,
		&mq11xx_fb0_lcd);
	if (IS_ERR (mqfb_lcd_device)) {
		return PTR_ERR (mqfb_lcd_device);
	}

	/* Power up the LCD. */
	h2200_lcd_set_power(mqfb_lcd_device, 0);
	h2200_lcd_set_contrast(mqfb_lcd_device, H2200_MQ_MAX_CONTRAST);

	return 0;
}

static int h2200_fp_remove(struct platform_device *pdev)
{
	debug_func ("\n");

	lcd_device_unregister(mqfb_lcd_device);
        return 0;
}

static int h2200_fp_suspend(struct platform_device *pdev, pm_message_t state)
{
	debug_func ("\n");

	if (state.event == PM_EVENT_SUSPEND) {
		save_lcd_power = h2200_lcd_get_power(mqfb_lcd_device);
		save_lcd_contrast = h2200_lcd_get_contrast(mqfb_lcd_device);
		h2200_lcd_set_power(mqfb_lcd_device, 4);
	}
	return 0;
}

static int h2200_fp_resume(struct platform_device *pdev)
{
	debug_func ("\n");

	h2200_lcd_set_power(mqfb_lcd_device, save_lcd_power);
	h2200_lcd_set_contrast(mqfb_lcd_device, save_lcd_contrast);

	return 0;
}

struct platform_driver h2200_fp_driver = {
	.probe    = h2200_fp_probe,
	.remove   = h2200_fp_remove,
	.suspend  = h2200_fp_suspend,
	.resume   = h2200_fp_resume,
	.driver	  = {
		.name = "mq11xx_lcd",
	},
};

/*-------------------------------------------------------------------------*/


static struct resource mq1178_resources[] = {
	/* Synchronous memory */
	[0] = {
		.start	= MQ_BASE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
        /* Non-synchronous memory */
	[1] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE * 2 - 1,
		.flags	= IORESOURCE_MEM,
	},
        /* MediaQ registers */
	[2] = {
		.start	= MQ_BASE + MQ11xx_FB_SIZE,
		.end	= MQ_BASE + MQ11xx_FB_SIZE + MQ11xx_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
        },
        /* MediaQ interrupt number */
        [3] = {
		.start	= IRQ_GPIO (GPIO_NR_H2200_MQ1178_IRQ_N),
		.flags	= IORESOURCE_IRQ,
        }
};

struct platform_device h2200_mq1178 = {
	.name		= "mq11xx",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mq1178_resources),
	.resource	= mq1178_resources,
	.dev		= {
		.platform_data = &mq1178_init_data,
	}
};

static struct platform_device *pdev;

static int __init
h2200_fp_init(void)
{
	int rc;

	debug_func ("\n");

	if (!machine_is_h2200())
		return -ENODEV;

	rc = platform_driver_register(&h2200_fp_driver);
	if (rc)
		return rc;

	rc = platform_device_register(&h2200_mq1178);
	if (rc)
		platform_driver_unregister(&h2200_fp_driver);

	return rc;
}

static void __exit
h2200_fp_exit(void)
{
	debug_func ("\n");
	platform_driver_unregister(&h2200_fp_driver);
	platform_device_unregister(pdev);
}


module_init(h2200_fp_init);
module_exit(h2200_fp_exit);

MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("HP iPAQ h2200 LCD driver");
MODULE_LICENSE("GPL");
