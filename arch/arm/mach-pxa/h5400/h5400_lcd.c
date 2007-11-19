/*
 * LCD support for iPAQ H5400
 *
 * Copyright © 2003 Keith Packard
 * Copyright © 2003, 2005 Phil Blundell
 * Copyright © 2007 Anton Vorontsov <cbou@mail.ru>
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
#include <linux/platform_device.h>
#include <linux/soc-old.h>
#include <linux/err.h>
#include <linux/mfd/mq11xx.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/h5400.h>
#include <asm/arch/h5400-asic.h>
#include <linux/mfd/samcop_base.h>

static struct lcd_device *mqfb_lcd_device;
struct mediaq11xx_base *mq_base;
EXPORT_SYMBOL(mq_base);
/* protects FP regs during RMW sequences */
static spinlock_t fp_regs_lock;

#define MQ_BASE	0x04000000

/*
 * Some H5400 devices have different LCDs installed that require this
 * other timing.  Phil Blundell was blessed with one of these
 * This is marked in the ENVEE bit (fp[9] & 2), the sharp
 * LCD version has ENVEE bit 0, the phillips has that bit 1.
 */
static struct mediaq11xx_init_data mq1100_init_phillips = {
    .irq_base = H5000_MQ11XX_IRQ_BASE,
    .DC = {
	/* dc00 */		0x00000001,
	/* dc01 */		0x00000003,
	/* dc02 */		0x00000001,
	/* dc03 NOT SET */	0x0,
	/* dc04 */		0x00000004,
	/* dc05 */		0x00000003,
    },
    .CC = {
	/* cc00 */		0x00000000,
	/* cc01 */		0x00001010,
	/* cc02 */		0x00000a22,
	/* cc03 */		0x00000000,
	/* cc04 */		0x00000004,
    },
    .MIU = {
	/* mm00 */		0x00000001,
	/* mm01 */		0x1b676ca8,
	/* mm02 */		0x00000000,
	/* mm03 */		0x00001479,
	/* mm04 */		0x6bfc2d76,
    },
    .GC = {
	/* gc00 */		0x051100c8, /* powered down */
	/* gc01 */		0x00000000,
	/* gc02 */		0x00f8013b,
	/* gc03 */		0x013f0150,
	/* gc04 */		0x01370132,
	/* gc05 */		0x01510150,
	/* gc06 */		0x00000000,
	/* gc07 NOT SET */	0x0,
	/* gc08 */		0x00ef0000,
	/* gc09 */		0x013f0000,
	/* gc0a */		0x00000000,
	/* gc0b */		0x006e0037,
	/* gc0c */		0x00000000,
	/* gc0d */		0x00066373,
	/* gc0e */		0x000001e0,
	/* gc0f NOT SET */	0x0,
	/* gc10 */		0x02ff07ff,
	/* gc11 */		0x000000ff,
	/* gc12 NOT SET */	0x0,
	/* gc13 NOT SET */	0x0,
	/* gc14 */		0x00000000,
	/* gc15 */		0x00000000,
	/* gc16 */		0x00000000,
	/* gc17 */		0x00000000,
	/* gc18 */		0x00000000,
	/* gc19 */		0x00000000,
	/* gc1a */		0x00000000,
    },
    .FP = {
	/* fp00 */		0x00006120,
	/* fp01 */		0x00bcc02c,
	/* fp02 */		0xfffcfcfd,
	/* fp03 */		0x00000002,
	/* fp04 */		0x80bd0001,
	/* fp05 */		0xc3000000,
	/* fp06 */		0xa0000000,
	/* fp07 */		0x00040005,
	/* fp08 */		0x00040005,
	/* fp09 NOT SET */	0x0,
	/* fp0a */		0x00000000,
	/* fp0b */		0x00000000,
	/* fp0c NOT SET */	0x0,
	/* fp0d NOT SET */	0x0,
	/* fp0e NOT SET */	0x0,
	/* fp0f */		0x00000120,
	/* fp10 */		0x2645a216,
	/* fp11 */		0x1941d898,
	/* fp12 */		0xc2f5b84c,
	/* fp13 */		0x411585dd,
	/* fp14 */		0xad1d0a85,
	/* fp15 */		0xd7928b26,
	/* fp16 */		0x4848e722,
	/* fp17 */		0x696d0932,
	/* fp18 */		0xf2d20075,
	/* fp19 */		0xd547235e,
	/* fp1a */		0x310304f6,
	/* fp1b */		0x3cef4474,
	/* fp1c */		0x22a3d351,
	/* fp1d */		0x01825700,
	/* fp1e */		0xb7d793d1,
	/* fp1f */		0x10906408,
	/* fp20 */		0x0,
	/* fp21 */		0x0,
	/* fp22 */		0x0,
	/* fp23 */		0x0,
	/* fp24 */		0x0,
	/* fp25 */		0x0,
	/* fp26 */		0x0,
	/* fp27 */		0x0,
	/* fp28 */		0x0,
	/* fp29 */		0x0,
	/* fp2a */		0x0,
	/* fp2b */		0x0,
	/* fp2c */		0x0,
	/* fp2d */		0x0,
	/* fp2e */		0x0,
	/* fp2f */		0x0,
	/* fp30 */		0x7ba9b45b,
	/* fp31 */		0xb56738c3,
	/* fp32 */		0xde47a2ba,
	/* fp33 */		0x88241cf0,
	/* fp34 */		0x35c661b2,
	/* fp35 */		0x7ae04b8d,
	/* fp36 */		0x7acb010d,
	/* fp37 */		0xa216c122,
	/* fp38 */		0xd64fb56a,
	/* fp39 */		0xe2b4dea9,
	/* fp3a */		0x4267c3f7,
	/* fp3b */		0x9b75c799,
	/* fp3c */		0x01aee179,
	/* fp3d */		0x586fd0a8,
	/* fp3e */		0x4eeb130e,
	/* fp3f */		0x42604a40,
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
	/* fp70 */		0x405,
	/* fp71 */		0xced6a94e,
	/* fp72 */		0x2,
	/* fp73 */		0x0,
	/* fp74 */		0x0,
	/* fp75 */		0x0,
	/* fp76 */		0x95f9c859,
	/* fp77 */		0x30388dc9,
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
	/* ge0a */		0x40000280,
	/* ge0b */		0x00000000,
    },
};

static struct mediaq11xx_init_data mq1100_init_sharp = {
    .irq_base = H5000_MQ11XX_IRQ_BASE,
    .DC = {
	/* dc00 */		0x00000001,
	/* dc01 */		0x00000003,
	/* dc02 */		0x00000001,
	/* dc03 NOT SET */	0x0,
	/* dc04 */		0x00000004,
	/* dc05 */		0x00000003,
    },
    .CC = {
	/* cc00 */		0x00000000,
	/* cc01 */		0x00001010,
	/* cc02 */		0x000002a2,
	/* cc03 */		0x00000000,
	/* cc04 */		0x00000004,
    },
    .MIU = {
	/* mm00 */		0x00000001,
	/* mm01 */		0x1b676ca8,
	/* mm02 */		0x00000000,
	/* mm03 */		0x00001479,
	/* mm04 */		0x6bfc2d76,
    },
    .GC = {
	/* gc00 */		0x080100c8, /* powered down */
	/* gc01 */		0x00000000,
	/* gc02 */		0x00f0011a,
	/* gc03 */		0x013f015f,
	/* gc04 */		0x011100fa,
	/* gc05 */		0x015a0158,
	/* gc06 */		0x00000000,
	/* gc07 NOT SET */	0x0,
	/* gc08 */		0x00ef0000,
	/* gc09 */		0x013f0000,
	/* gc0a */		0x00000000,
	/* gc0b */		0x011700f2,
	/* gc0c */		0x00000000,
	/* gc0d */		0x00066373,
	/* gc0e */		0x000001e0,
	/* gc0f NOT SET */	0x0,
	/* gc10 */		0x02ff07ff,
	/* gc11 */		0x000000ff,
	/* gc12 NOT SET */	0x0,
	/* gc13 NOT SET */	0x0,
	/* gc14 */		0x00000000,
	/* gc15 */		0x00000000,
	/* gc16 */		0x00000000,
	/* gc17 */		0x00000000,
	/* gc18 */		0x00000000,
	/* gc19 */		0x00000000,
	/* gc1a */		0x00000000,
    },
    .FP = {
	/* fp00 */		0x00006120,
	/* fp01 */		0x003d5008,
	/* fp02 */		0xfffcfcfd,
	/* fp03 */		0x00000002,
	/* fp04 */		0x80bd0001,
	/* fp05 */		0xc9000000,
	/* fp06 */		0x80000000,
	/* fp07 */		0x00040005,
	/* fp08 */		0x00040004,
	/* fp09 NOT SET */	0x0,
	/* fp0a */		0x00000000,
	/* fp0b */		0x00000000,
	/* fp0c NOT SET */	0x0,
	/* fp0d NOT SET */	0x0,
	/* fp0e NOT SET */	0x0,
	/* fp0f */		0x00000120,
	/* fp10 */		0x2645a216,
	/* fp11 */		0x1941d898,
	/* fp12 */		0xc2f5b84c,
	/* fp13 */		0x411585dd,
	/* fp14 */		0xad1d0a85,
	/* fp15 */		0xd7928b26,
	/* fp16 */		0x4848e722,
	/* fp17 */		0x696d0932,
	/* fp18 */		0xf2d20075,
	/* fp19 */		0xd547235e,
	/* fp1a */		0x310304f6,
	/* fp1b */		0x3cef4474,
	/* fp1c */		0x22a3d351,
	/* fp1d */		0x01825700,
	/* fp1e */		0xb7d793d1,
	/* fp1f */		0x10906408,
	/* fp20 */		0x0,
	/* fp21 */		0x0,
	/* fp22 */		0x0,
	/* fp23 */		0x0,
	/* fp24 */		0x0,
	/* fp25 */		0x0,
	/* fp26 */		0x0,
	/* fp27 */		0x0,
	/* fp28 */		0x0,
	/* fp29 */		0x0,
	/* fp2a */		0x0,
	/* fp2b */		0x0,
	/* fp2c */		0x0,
	/* fp2d */		0x0,
	/* fp2e */		0x0,
	/* fp2f */		0x0,
	/* fp30 */		0x7ba9b45b,
	/* fp31 */		0xb56738c3,
	/* fp32 */		0xde47a2ba,
	/* fp33 */		0x88241cf0,
	/* fp34 */		0x35c661b2,
	/* fp35 */		0x7ae04b8d,
	/* fp36 */		0x7acb010d,
	/* fp37 */		0xa216c122,
	/* fp38 */		0xd64fb56a,
	/* fp39 */		0xe2b4dea9,
	/* fp3a */		0x4267c3f7,
	/* fp3b */		0x9b75c799,
	/* fp3c */		0x01aee179,
	/* fp3d */		0x586fd0a8,
	/* fp3e */		0x4eeb130e,
	/* fp3f */		0x42604a40,
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
	/* fp70 */		0x405,
	/* fp71 */		0xced6a94e,
	/* fp72 */		0x2,
	/* fp73 */		0x0,
	/* fp74 */		0x0,
	/* fp75 */		0x0,
	/* fp76 */		0x95f9c859,
	/* fp77 */		0x30388dc9,
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
	/* ge0a */		0x40000280,
	/* ge0b */		0x00000000,
    },
};

static int h5400_lcd_set_power (struct lcd_device *lm, int level)
{
	int new_pwr, fp_pwr, lcd_pwr;

	new_pwr = (level < 1) ? 1 : 0;

	fp_pwr = mq_base->get_power(mq_base, MEDIAQ_11XX_FP_DEVICE_ID) ? 1 : 0;
	lcd_pwr = samcop_get_gpio_b(&h5400_samcop.dev) & SAMCOP_GPIO_GPB_LCD_EN;

	if (new_pwr != lcd_pwr) {
		samcop_set_gpio_b(&h5400_samcop.dev,
		                  SAMCOP_GPIO_GPB_LCD_EN, new_pwr ?
		                  SAMCOP_GPIO_GPB_LCD_EN : 0);
	}

	if (new_pwr != fp_pwr)
		mq_base->set_power(mq_base, MEDIAQ_11XX_FP_DEVICE_ID, new_pwr);

	return 0;
}

static int h5400_lcd_get_power (struct lcd_device *lm)
{
	if (samcop_get_gpio_b (&h5400_samcop.dev) & SAMCOP_GPIO_GPB_MQ_POWER_ON) {
		if (samcop_get_gpio_b (&h5400_samcop.dev) & SAMCOP_GPIO_GPB_LCD_EN)
			return 0;
		else
			return 2;
	} else
		return 4;
}

#if 0
static int
h5400_lcd_set_contrast (struct lcd_device *ld, int contrast)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	u32 x;

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
h5400_lcd_get_contrast (struct lcd_device *ld)
{
	struct mediaq11xx_base *mq_base = class_get_devdata (&ld->class_dev);
	u32 c, x;

	x = (mq_base->regs->FP.pin_output_select_1 & 0x7f);
	for (c = 7; x; x >>= 1, c--)
		;
	return c;
}
#endif

static struct lcd_ops mq11xx_fb0_lcd = {
	.get_power      = h5400_lcd_get_power,
	.set_power      = h5400_lcd_set_power,
#if 0
	.max_contrast   = 7,
	.get_contrast   = h5400_lcd_get_contrast,
	.set_contrast   = h5400_lcd_set_contrast,
#endif
};

static int 
h5400_fp_probe (struct platform_device *pdev)
{
	mq_base = (struct mediaq11xx_base *)pdev->dev.platform_data;

	spin_lock_init (&fp_regs_lock);

	mqfb_lcd_device = lcd_device_register ("mq11xx_fb0", mq_base,
		&mq11xx_fb0_lcd);
	if (IS_ERR (mqfb_lcd_device)) {
		return PTR_ERR (mqfb_lcd_device);
	}

	/* Power up the LCD. */
	h5400_lcd_set_power (mqfb_lcd_device, 0);

	return 0;
}

static void
h5400_fp_shutdown (struct platform_device *pdev)
{
	h5400_lcd_set_power (NULL, 4);
	return;
}

static int 
h5400_fp_remove (struct platform_device *pdev)
{
	lcd_device_unregister (mqfb_lcd_device);
	h5400_fp_shutdown (pdev);
	return 0;
}

#ifdef CONFIG_PM

static int
h5400_fp_suspend(struct platform_device *pdev, pm_message_t pm)
{
	h5400_fp_shutdown (pdev);
	return 0;
}

static int
h5400_fp_resume(struct platform_device *pdev)
{
	h5400_lcd_set_power (mqfb_lcd_device, 0);
	return 0;
}

#endif /* CONFIG_PM */

//static platform_device_id h5400_fp_device_ids[] = { { MEDIAQ_11XX_FP_DEVICE_ID }, { 0 } };

static struct platform_driver h5400_fp_device_driver = {
	.driver = {
		.name     = "mq11xx_lcd",
	},
	.probe    = h5400_fp_probe,
	.shutdown = h5400_fp_shutdown,
	.remove   = h5400_fp_remove,
#ifdef CONFIG_PM
	.suspend  = h5400_fp_suspend,
	.resume   = h5400_fp_resume,
#endif /* CONFIG_PM */
};

static struct resource mq1100_resources[] = {
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
        /* MediaQ interrupt number -- @@@FIXME */
        [3] = {
		/* The MediaQ IRQ pin is unconnected on the h5xxx.
		 */
		.start	= (unsigned long)-1,
		.flags	= IORESOURCE_IRQ,
        }
};

static void
h5400_mq_release (struct device *dev)
{
	struct platform_device *pdev = to_platform_device (dev);
	kfree (pdev);
}

static const struct platform_device h5400_mq1100 = {
	.name		= "mq11xx",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(mq1100_resources),
	.resource	= mq1100_resources,		/* XXX FIXME */
	.dev		= {
		.release = h5400_mq_release
	},
};

static void
h5400_mq_set_power(int on)
{
	samcop_set_gpio_b (&h5400_samcop.dev, SAMCOP_GPIO_GPB_MQ_POWER_ON,
	                   on ? SAMCOP_GPIO_GPB_MQ_POWER_ON : 0);
	if (on) mdelay (10);
	return;
}

static struct mediaq11xx_init_data *
h5400_init_lcd_info (void)
{
	struct mediaq11xx_init_data *ret;
	u32 pin_input_data;
	struct mediaq11xx_regs *regs;

	regs = (struct mediaq11xx_regs *)ioremap (MQ_BASE + MQ11xx_FB_SIZE, MQ_BASE + MQ11xx_FB_SIZE + 0x1fff);
	if (!regs)
		return NULL;

	h5400_mq_set_power(1);

	/*
	 * Turn on the config module
	 */
	regs->DC.config_1 = MQ_CONFIG_18_OSCILLATOR_INTERNAL;
	mdelay (1);		/* wait for the oscillator to stabilize */
	regs->DC.config_2 = MQ_CONFIG_CC_MODULE_ENABLE;
	mdelay (1);		/* probably not needed... */
	regs->FP.input_control |= MQ_FP_ENVEE;
	pin_input_data = regs->FP.pin_input_data;
	iounmap (regs);

	if (pin_input_data & MQ_FP_ENVEE) {
		printk ("REMOVEME: detected Philips LCD\n");
		ret = &mq1100_init_phillips;
	} else {
		printk ("REMOVEME: detected Sharp LCD\n");
		ret = &mq1100_init_sharp;
	}

	ret->set_power = h5400_mq_set_power;

	return ret;
}

static struct platform_device *pdev;

static int __init 
h5400_lcd_init(void)
{
	int rc;

	if (! machine_is_h5400 ())
		return -ENODEV;

	pdev = kmalloc (sizeof (*pdev), GFP_KERNEL);
	if (!pdev)
		return -ENOMEM;

	rc = platform_driver_register (&h5400_fp_device_driver);
	if (rc)
		return rc;

	*pdev = h5400_mq1100;
	pdev->dev.platform_data = h5400_init_lcd_info ();
	rc = platform_device_register (pdev);
	if (rc)
		platform_driver_unregister (&h5400_fp_device_driver);

	return rc;
}

static void __exit 
h5400_lcd_exit(void)
{
	platform_driver_unregister (&h5400_fp_device_driver);
	platform_device_unregister (pdev);
}

module_init (h5400_lcd_init);
module_exit (h5400_lcd_exit);

MODULE_AUTHOR ("Keith Packard <keithp@keithp.com>, Phil Blundell <pb@nexus.co.uk>");
MODULE_DESCRIPTION ("Framebuffer driver for iPAQ H5400");
MODULE_LICENSE ("GPL");
MODULE_SUPPORTED_DEVICE ("h5400_lcd");
//MODULE_DEVICE_TABLE (soc, h5400_fp_device_ids);

