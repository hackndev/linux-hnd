/*
 *  ov96xx.c - Omnivision 9640, 9650 CMOS sensor driver
 *
 *  Copyright (C) 2003 Intel Corporation
 *  Copyright (C) 2004 Motorola Inc.
 *  Copyright (C) 2007 Philipp Zabel <philipp.zabel@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <media/ov96xx.h>	/* ov96xx_platform_data */
#include <media/v4l2-dev.h>

#include <asm/arch/gpio.h>

#include <asm/ioctl.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

#include "pxa_ci_types.h"
#include "ov96xx_hw.h"

extern int i2c_ov96xx_init(void);
extern int i2c_ov96xx_cleanup(void);
extern int i2c_ov96xx_read(u8 addr, u8 *pvalue);
extern int i2c_ov96xx_write(u8 addr, u8 value);

#define DEBUG 1
#define DPRINTK(fmt,args...) \
	do { if (DEBUG) printk("in function %s "fmt,__FUNCTION__,##args);} while(0)

struct ov96xx_data {
	int		version;
	struct clk	*clk;
};

void ov96xx_soft_reset(struct platform_device *pdev)
{
	struct ov96xx_data *ov96xx = platform_get_drvdata(pdev);

	i2c_ov96xx_write(OV96xx_COM7, 0x80);		/* COM7 = 0x12 */
	mdelay(10);

	/* enable digital BLC */
	switch (ov96xx->version) {
	case REV_OV9640:
	case REV_OV9640_v3:
		break;
	case REV_OV9650_0:
		i2c_ov96xx_write(OV96xx_TSLB, 0x0c);	/* TSLB = 0x3a */
		i2c_ov96xx_write(OV96xx_COM6, 0x4a);	/* COM6 = 0x0f */
		break;
	case REV_OV9650:
		i2c_ov96xx_write(OV96xx_TSLB, 0x0d);	/* TSLB = 0x3a */
		i2c_ov96xx_write(OV96xx_COM6, 0x42);	/* COM6 = 0x0f */
		break;
	}

	dbg_print("end initial register");
}

int ov96xx_check_revision(struct platform_device *pdev)
{
	struct ov96xx_data *ov96xx = platform_get_drvdata(pdev);
	u8 val;

	i2c_ov96xx_read(OV96xx_PID, &val);
	DPRINTK("PID = %x\n", val);

	if (val != PID_OV96xx)
		return -ENODEV;

	mdelay(2000);
	i2c_ov96xx_read(OV96xx_REV, &val);
	DPRINTK("REV = %x\n", val);

	switch (val) {
	case REV_OV9640:
		printk("detected OV9640\n");
		break;
	case REV_OV9640_v3:
		printk("detected OV9640 v3\n");
		break;
	case REV_OV9650_0:
		printk("detected OV9650-0\n");
		break;
	case REV_OV9650:
		printk("detected OV9650\n");
		break;
	default:
		printk("couldn't detect OV96xx\n");
		return -ENODEV;
	}
	ov96xx->version = val;
	return val;
}

void ov96xx_power(struct platform_device *pdev, int powerup)
{
	struct ov96xx_platform_data *pdata = pdev->dev.platform_data;

	if (powerup) {
		gpio_set_value(pdata->power_gpio, pdata->power_gpio_active_high);
		if (pdata->reset_gpio) {
			gpio_set_value(pdata->reset_gpio, 1);
			mdelay(20);
			gpio_set_value(pdata->reset_gpio, 0);
		}
	} else {
		gpio_set_value(pdata->power_gpio, !pdata->power_gpio_active_high);
	}
	mdelay(100);
}

static int ov96xx_probe(struct platform_device *pdev)
{
	struct ov96xx_data *ov96xx;
	struct ov96xx_platform_data *pdata;
	int ret;
	int rate, i;
	u8 val;

	if (!pdev)
		return -ENODEV;

	pdata = pdev->dev.platform_data;
	if (!pdata->power_gpio)
		return -EINVAL;

	ov96xx = kzalloc(sizeof (struct ov96xx_data), GFP_KERNEL);
	if (!ov96xx)
		return -ENOMEM;

	platform_set_drvdata(pdev, ov96xx);

	ov96xx->clk = clk_get(&pdev->dev, "CI_MCLK");
	if (IS_ERR(ov96xx->clk)) {
		DPRINTK("couldn't get CI master clock\n");
		ret = -ENOENT;
		goto err;
	}

	rate = clk_get_rate(ov96xx->clk);
	printk("clock rate = %d\n", rate);

	/* enable clock, camera */
	clk_enable(ov96xx->clk);
	ov96xx_power(pdev, 1);

	/* enable i2c */
	i2c_ov96xx_init();

	/* check revision */
	//mdelay(2000);
	mdelay(100);
	ov96xx_check_revision(pdev);

	/* dump all registers */
	for (i=0; i<0xff; i++) {
		if ((i%16) == 0) {
			printk("\n%02x: ", i);
		}
		i2c_ov96xx_read(i, &val);
		printk("%02x ", val);
	}
	printk("\n");

	ov96xx_power(pdev, 0);

	/* disable clock */
	clk_disable(ov96xx->clk);

	/* disable i2c */
	i2c_ov96xx_cleanup();

	return 0;
err:
	kfree(ov96xx);
	return ret;
}

#ifdef CONFIG_PM
static int ov96xx_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	 * stopping DMA transfers and disabling interrupts / clocks
	 * is the business of the capture interface driver
	 */

	return 0;
}

static int ov96xx_resume(struct platform_device *pdev)
{
/*	struct ov96xx_data *ov96xx = platform_get_drvdata(pdev); */

	/*
	 * enabling interrupts / clocks and resuming DMA transfers
	 * is the business of the capture interface driver
	 */
#if 0
	camera_init(ov96xx->cam_ctx);
	//ov96xx_restore_property(cam_ctx, 0);
	if (resume_dma == 1) {
		camera_start_video_capture(cam_ctx, 0);
		resume_dma = 0;
	}
#endif
	return 0;
}
#else
#define ov96xx_suspend NULL
#define ov96xx_resume NULL
#endif

static int ov96xx_remove(struct platform_device *pdev)
{
	struct ov96xx_data *ov96xx = platform_get_drvdata(pdev);

	kfree(ov96xx);
	return 0;
}

static struct platform_driver ov96xx_driver = {
	.driver   = {
		.name = "ov96xx",
	},
	.probe    = ov96xx_probe,
	.remove   = ov96xx_remove,
	.suspend  = ov96xx_suspend,
	.resume   = ov96xx_resume
};

static int __init ov96xx_init(void)
{
	return platform_driver_register(&ov96xx_driver);
}

static void __exit ov96xx_exit(void)
{
	platform_driver_unregister(&ov96xx_driver);
}

module_init(ov96xx_init);
module_exit(ov96xx_exit);

MODULE_DESCRIPTION("OV96xx CMOS sensor driver");
MODULE_LICENSE("GPL");

