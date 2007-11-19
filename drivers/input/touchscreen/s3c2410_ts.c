/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (c) 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 * iPAQ H1940 touchscreen support
 *
 * ChangeLog
 *
 * 2004-09-05: Herbert Pötzl <herbert@13thfloor.at>
 *	- added clock (de-)allocation code
 *
 * 2005-03-06: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - h1940_ -> s3c2410 (this driver is now also used on the n30
 *        machines :P)
 *      - Debug messages are now enabled with the config option
 *        TOUCHSCREEN_S3C2410_DEBUG
 *      - Changed the way the value are read
 *      - Input subsystem should now work
 *      - Use ioremap and readl/writel
 *
 * 2005-03-23: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Make use of some undocumented features of the touchscreen
 *        controller
 *
 * 2006-12-05: Pierre Hebert <pierrox@pierrox.net>
 *      - Attempt to correct a problem with the ADC bad values
 *        found on s3c2440 G500. This version allows a smoother
 *        gesture and the removal of glitches but is based on a very
 *        basic condition for stylus up event (XP value)
 *
 * 2007-05-23: Harald Welte <laforge@openmoko.org>
 *     - Add proper support for S32440
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/ts.h>

/* For ts.dev.id.version */
#define S3C2410TSVERSION	0x0101

#define TSC_SLEEP  (S3C2410_ADCTSC_PULL_UP_DISABLE | S3C2410_ADCTSC_XY_PST(0))

#define WAIT4INT(x)  (((x)<<8) | \
		     S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_XY_PST(3))

#define AUTOPST	     (S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_AUTO_PST | S3C2410_ADCTSC_XY_PST(0))

#define DEBUG_LVL    KERN_DEBUG

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 touchscreen driver");
MODULE_LICENSE("GPL");

/*
 * Definitions & global arrays.
 */


static char *s3c2410ts_name = "s3c2410 TouchScreen";

/*
 * Per-touchscreen data.
 */

struct s3c2410ts {
	struct input_dev *dev;
	long xp;
	long yp;
	int count;
	int shift;
#ifdef TOUCHSCREEN_S3C2410_ALT
	int xp_threshold;
#endif
};

static struct s3c2410ts ts;
static void __iomem *base_addr;

static inline void s3c2410_ts_connect(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG12, S3C2410_GPG12_XMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG13, S3C2410_GPG13_nXPON);
	s3c2410_gpio_cfgpin(S3C2410_GPG14, S3C2410_GPG14_YMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG15, S3C2410_GPG15_nYPON);
}

#ifdef TOUCHSCREEN_S3C2410_ALT
/* 
 * Alternate loop that correct glitches on the s3c2440 G500 that make the
 * stylus unusable on this machine.
 * This version of the ADC loop is simpler than the other one but suffer from
 * a major drawback : there is no good way to know if the stylus is up after is 
 * has been put down. For the moment the only solution is to look at the XP
 * value, which is greater than a threshold when stylus is up. This can't be the
 * right way but unfortunately it is the only solution I found to get a smooth
 * movement.
 */
static irqreturn_t stylus_updown(int irq, void *dev_id)
{
  unsigned long data0;
  unsigned long data1;
  int down;

  data0=readl(base_addr+S3C2410_ADCDAT0);
  data1=readl(base_addr+S3C2410_ADCDAT1);

  down=(!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));
  if(down) {
    ts.count=0;
    ts.xp=0;
    ts.yp=0;
    ts.first=1;
    writel(AUTOPST, base_addr+S3C2410_ADCTSC);
    writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
  } 

	return IRQ_HANDLED;
}


static irqreturn_t stylus_action(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;
	int down;

	while(readl(base_addr+S3C2410_ADCCON) & S3C2410_ADCCON_ENABLE_START);
	while(!(S3C2410_ADCCON_ECFLG & readl(base_addr+S3C2410_ADCCON)));
	data0=readl(base_addr+S3C2410_ADCDAT0);
	data1=readl(base_addr+S3C2410_ADCDAT1);

	down=(data0 & S3C2410_ADCDAT0_XPDATA_MASK)<ts.xp_threshold;

	if(down) {
		ts.xp+=data0 & S3C2410_ADCDAT0_XPDATA_MASK;
		ts.yp+=data1 & S3C2410_ADCDAT1_YPDATA_MASK;
		ts.count++;

		if(ts.count==(1<<ts.shift)) {
			ts.xp>>=ts.shift;
			ts.yp>>=ts.shift;
			input_report_abs(ts.dev, ABS_X, ts.xp);
			input_report_abs(ts.dev, ABS_Y, ts.yp);
			if(ts.first) {
				input_report_key(ts.dev, BTN_TOUCH, 1);
				input_report_abs(ts.dev, ABS_PRESSURE, 1);
				ts.first=0;
			}
			input_sync(ts.dev);
			ts.count=0;
			ts.xp=0;
			ts.yp=0;
		}

		writel(AUTOPST, base_addr+S3C2410_ADCTSC);
		writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
	} else {
		writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC);
		input_report_key(ts.dev, BTN_TOUCH, 0);
		input_report_abs(ts.dev, ABS_PRESSURE, 0);
		input_sync(ts.dev);
	}

	return IRQ_HANDLED;
}

#else

static void touch_timer_fire(unsigned long data)
{
  	unsigned long data0;
  	unsigned long data1;
	int updown;

  	data0 = readl(base_addr+S3C2410_ADCDAT0);
  	data1 = readl(base_addr+S3C2410_ADCDAT1);

 	updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

 	if (updown) {
 		if (ts.count != 0) {
 			ts.xp >>= ts.shift;
 			ts.yp >>= ts.shift;

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
 			{
 				struct timeval tv;
 				do_gettimeofday(&tv);
 				printk(DEBUG_LVL "T: %06d, X: %03ld, Y: %03ld\n", (int)tv.tv_usec, ts.xp, ts.yp);
 			}
#endif

 			input_report_abs(ts.dev, ABS_X, ts.xp);
 			input_report_abs(ts.dev, ABS_Y, ts.yp);

 			input_report_key(ts.dev, BTN_TOUCH, 1);
 			input_report_abs(ts.dev, ABS_PRESSURE, 1);
 			input_sync(ts.dev);
 		}

 		ts.xp = 0;
 		ts.yp = 0;
 		ts.count = 0;

 		writel(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
 		writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
 	} else {
 		ts.count = 0;

 		input_report_key(ts.dev, BTN_TOUCH, 0);
 		input_report_abs(ts.dev, ABS_PRESSURE, 0);
 		input_sync(ts.dev);

 		writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC);
 	}
}

static struct timer_list touch_timer =
		TIMER_INITIALIZER(touch_timer_fire, 0, 0);

static irqreturn_t stylus_updown(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;
	int updown;

	data0 = readl(base_addr+S3C2410_ADCDAT0);
	data1 = readl(base_addr+S3C2410_ADCDAT1);

	updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

	/* TODO we should never get an interrupt with updown set while
	 * the timer is running, but maybe we ought to verify that the
	 * timer isn't running anyways. */

	if (updown)
		touch_timer_fire(0);

	return IRQ_HANDLED;
}


static irqreturn_t stylus_action(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;

	data0 = readl(base_addr+S3C2410_ADCDAT0);
	data1 = readl(base_addr+S3C2410_ADCDAT1);

	ts.xp += data0 & S3C2410_ADCDAT0_XPDATA_MASK;
	ts.yp += data1 & S3C2410_ADCDAT1_YPDATA_MASK;
	ts.count++;

        if (ts.count < (1<<ts.shift)) {
		writel(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
		writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
	} else {
		mod_timer(&touch_timer, jiffies+1);
		writel(WAIT4INT(1), base_addr+S3C2410_ADCTSC);
	}

	return IRQ_HANDLED;
}
#endif

static struct clk	*adc_clock;

/*
 * The functions for inserting/removing us as a module.
 */

static int __init s3c2410ts_probe(struct platform_device *pdev)
{
	struct s3c2410_ts_mach_info *info;
	struct input_dev *input_dev;

	info = ( struct s3c2410_ts_mach_info *)pdev->dev.platform_data;

	if (!info)
	{
		printk(KERN_ERR "Hm... too bad : no platform data for ts\n");
		return -EINVAL;
	}

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
	printk(DEBUG_LVL "Entering s3c2410ts_init\n");
#endif

	adc_clock = clk_get(NULL, "adc");
	if (!adc_clock) {
		printk(KERN_ERR "failed to get adc clock source\n");
		return -ENOENT;
	}
	clk_enable(adc_clock);

#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
	printk(DEBUG_LVL "got and enabled clock\n");
#endif

	base_addr=ioremap(S3C2410_PA_ADC,0x20);
	if (base_addr == NULL) {
		printk(KERN_ERR "Failed to remap register block\n");
		return -ENOMEM;
	}


	/* If we acutally are a S3C2410: Configure GPIOs */
	if (!strcmp(pdev->name, "s3c2410-ts"))
		s3c2410_ts_connect();

	if ((info->presc&0xff) > 0)
		writel(S3C2410_ADCCON_PRSCEN | S3C2410_ADCCON_PRSCVL(info->presc&0xFF),\
			     base_addr+S3C2410_ADCCON);
	else
		writel(0,base_addr+S3C2410_ADCCON);


	/* Initialise registers */
	if ((info->delay&0xffff) > 0)
		writel(info->delay & 0xffff,  base_addr+S3C2410_ADCDLY);

	writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC);

	/* Initialise input stuff */
	memset(&ts, 0, sizeof(struct s3c2410ts));
	input_dev = input_allocate_device();

	if (!input_dev) {
		printk(KERN_ERR "Unable to allocate the input device !!\n");
		return -ENOMEM;
	}

	ts.dev = input_dev;
	ts.dev->evbit[0] = BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	ts.dev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_set_abs_params(ts.dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(ts.dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(ts.dev, ABS_PRESSURE, 0, 1, 0, 0);

	ts.dev->private = &ts;
	ts.dev->name = s3c2410ts_name;
	ts.dev->id.bustype = BUS_RS232;
	ts.dev->id.vendor = 0xDEAD;
	ts.dev->id.product = 0xBEEF;
	ts.dev->id.version = S3C2410TSVERSION;

	ts.shift = info->oversampling_shift;

	/* Get irqs */
	if (request_irq(IRQ_ADC, stylus_action, IRQF_SAMPLE_RANDOM,
		"s3c2410_action", ts.dev)) {
		printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_ADC !\n");
		iounmap(base_addr);
		return -EIO;
	}
	if (request_irq(IRQ_TC, stylus_updown, IRQF_SAMPLE_RANDOM,
			"s3c2410_action", ts.dev)) {
		printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_TC !\n");
		iounmap(base_addr);
		return -EIO;
	}

	printk(KERN_INFO "%s successfully loaded\n", s3c2410ts_name);

	/* All went ok, so register to the input system */
	input_register_device(ts.dev);

	return 0;
}

static int s3c2410ts_remove(struct platform_device *pdev)
{
	disable_irq(IRQ_ADC);
	disable_irq(IRQ_TC);
	free_irq(IRQ_TC,ts.dev);
	free_irq(IRQ_ADC,ts.dev);

	if (adc_clock) {
		clk_disable(adc_clock);
		clk_put(adc_clock);
		adc_clock = NULL;
	}

	input_unregister_device(ts.dev);
	iounmap(base_addr);

	return 0;
}

#ifdef CONFIG_PM
static int s3c2410ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	writel(TSC_SLEEP, base_addr+S3C2410_ADCTSC);
	writel(readl(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_STDBM,
	       base_addr+S3C2410_ADCCON);

	disable_irq(IRQ_ADC);
	disable_irq(IRQ_TC);

	clk_disable(adc_clock);

	return 0;
}

static int s3c2410ts_resume(struct platform_device *pdev)
{
	struct s3c2410_ts_mach_info *info =
		( struct s3c2410_ts_mach_info *)pdev->dev.platform_data;

	clk_enable(adc_clock);
	msleep(1);

	enable_irq(IRQ_ADC);
	enable_irq(IRQ_TC);

	if ((info->presc&0xff) > 0)
		writel(S3C2410_ADCCON_PRSCEN | S3C2410_ADCCON_PRSCVL(info->presc&0xFF),\
			     base_addr+S3C2410_ADCCON);
	else
		writel(0,base_addr+S3C2410_ADCCON);

	/* Initialise registers */
	if ((info->delay&0xffff) > 0)
		writel(info->delay & 0xffff,  base_addr+S3C2410_ADCDLY);

	writel(WAIT4INT(0), base_addr+S3C2410_ADCTSC);

	return 0;
}

#else
#define s3c2410ts_suspend NULL
#define s3c2410ts_resume  NULL
#endif

static struct platform_driver s3c2410ts_driver = {
       .driver         = {
	       .name   = "s3c2410-ts",
	       .owner  = THIS_MODULE,
       },
       .probe          = s3c2410ts_probe,
       .remove         = s3c2410ts_remove,
       .suspend        = s3c2410ts_suspend,
       .resume         = s3c2410ts_resume,

};

static struct platform_driver s3c2440ts_driver = {
       .driver         = {
	       .name   = "s3c2440-ts",
	       .owner  = THIS_MODULE,
       },
       .probe          = s3c2410ts_probe,
       .remove         = s3c2410ts_remove,
       .suspend        = s3c2410ts_suspend,
       .resume         = s3c2410ts_resume,

};

static int __init s3c2410ts_init(void)
{
	int rc;

	rc = platform_driver_register(&s3c2410ts_driver);
	if (rc < 0)
		return rc;

	rc = platform_driver_register(&s3c2440ts_driver);
	if (rc < 0)
		platform_driver_unregister(&s3c2410ts_driver);

	return rc;
}

static void __exit s3c2410ts_exit(void)
{
	platform_driver_unregister(&s3c2440ts_driver);
	platform_driver_unregister(&s3c2410ts_driver);
}

module_init(s3c2410ts_init);
module_exit(s3c2410ts_exit);

/*
    Local variables:
        compile-command: "make ARCH=arm CROSS_COMPILE=/usr/local/arm/3.3.2/bin/arm-linux- -k -C ../../.."
        c-basic-offset: 8
    End:
*/
