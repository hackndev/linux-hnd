/*
* Driver interface to the SAMCOP Companion chip on the iPAQ H5400
*
* Copyright (c) 2007 Anton Vorontsov <cbou@mail.ru>
* Copyright (c) Phil Blundell <pb@handhelds.org>
* Copyright (c) 2003 Keith Packard <keith.packard@hp.com>
* Copyright (c) 2003 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Keith Packard <keith.packard@hp.com>
*          May 2003
*/

#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/adc.h>
#include <linux/mfd/samcop_adc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <asm/hardware/samcop-adc.h>

#define ADC_FREQ 2000000  /* target ADC clock frequency */

/* Jamey says:
 * 
 * initialization:
 * adc control: enable prescaler, use prescaler value 19, sel=3 (ain3?)
 * use adc delay=1000
 * touchscreen control: initialize to 0xd3
 *  SAMCOP_ADC_TSC_XYPST_INTERRUPT, SAMCOP_ADC_TSC_XP_SEN, 
 *  SAMCOP_ADC_TSC_YP_SEN, SAMCOP_ADC_TSC_YM_SEN
 * 
 * when getting touch coordinage,
 *  - set adctsc to ((1<<3)|(1<<2)) -- pull up disable, auto mode
 *  - set bit 0 of adc control
 *  - set back to 0xd3 after getting coordinate
 */

#define SAMCOP_ADC_PRESCALER       (((clk_get_rate(adc->clk) / ADC_FREQ) - 1) \
                                       << SAMCOP_ADC_CONTROL_PRESCALER_SHIFT)

#define SAMCOP_ADC_CONTROL_INIT    (SAMCOP_ADC_CONTROL_PRESCALER_ENABLE | \
                                    SAMCOP_ADC_PRESCALER)

#define SAMCOP_ADC_CONTROL_SAMPLE  (SAMCOP_ADC_CONTROL_INIT        | \
                                    SAMCOP_ADC_CONTROL_FORCE_START)

#define SAMCOP_ADC_TSC_WAIT        (SAMCOP_ADC_TSC_XYPST_INTERRUPT | \
                                    SAMCOP_ADC_TSC_XP_SEN          | \
                                    SAMCOP_ADC_TSC_YP_SEN          | \
                                    SAMCOP_ADC_TSC_YM_SEN)

#define SAMCOP_ADC_TSC_SAMPLE_TS   (SAMCOP_ADC_TSC_AUTO_MODE | \
                                    SAMCOP_ADC_TSC_PULL_UP   | \
                                    SAMCOP_ADC_TSC_XP_SEN    | \
                                    SAMCOP_ADC_TSC_YP_SEN    | \
                                    SAMCOP_ADC_TSC_YM_SEN)

#define SAMCOP_ADC_TSC_SAMPLE_AX   (SAMCOP_ADC_TSC_XYPST_NONE)

#define SAMCOP_ADC_TSC_SUSPEND     (SAMCOP_ADC_TSC_XYPST_NONE | \
                                    SAMCOP_ADC_TSC_PULL_UP    | \
                                    SAMCOP_ADC_TSC_XP_SEN     | \
                                    SAMCOP_ADC_TSC_YP_SEN)

static void samcop_adc_write_register(struct samcop_adc *adc, int reg,
                                      u_int16_t val)
{
	__raw_writew (val, reg + adc->map);
	return;
}

static u_int16_t samcop_adc_read_register(struct samcop_adc *adc, int reg)
{
	return __raw_readw (reg + adc->map);
}

static irqreturn_t adc_isr(int irq, void *data)
{
	struct samcop_adc *adc = data;

	complete(&adc->comp);

	return IRQ_HANDLED;
}

static int samcop_adc_start_sample(struct samcop_adc *adc, int pin)
{
	u_int16_t adcc;
	u_int8_t tsc;
	int timeout = 50;

	if (pin >= SAMCOP_ADC_PIN_X && pin <= SAMCOP_ADC_PIN_Z2) {
		adcc = SAMCOP_ADC_CONTROL_SEL_AIN3;
		tsc = SAMCOP_ADC_TSC_SAMPLE_TS;
	}
	else if (pin >= SAMCOP_ADC_PIN_AIN0 && pin <= SAMCOP_ADC_PIN_AIN2) {
		adcc = (pin - SAMCOP_ADC_PIN_AIN0) <<
		       SAMCOP_ADC_CONTROL_SEL_SHIFT;
		tsc = SAMCOP_ADC_TSC_SAMPLE_AX;
	}
	else
		return -ENODEV;

	if (adc->pdata->set_power)
		adc->pdata->set_power(pin, 1);

	adcc |= SAMCOP_ADC_CONTROL_SAMPLE;

	/* start sampling */
	samcop_adc_write_register(adc, _SAMCOP_ADC_Delay, adc->pdata->delay);
	samcop_adc_write_register(adc, _SAMCOP_ADC_TouchScreenControl, tsc);
	samcop_adc_write_register(adc, _SAMCOP_ADC_Control, adcc);

	/* wait for irq, it will tell when sampling complete */
	wait_for_completion(&adc->comp);

	while (!(samcop_adc_read_register(adc, _SAMCOP_ADC_Control)
				& SAMCOP_ADC_CONTROL_CONVERSION_END)) {
		if (!--timeout) {
			printk(KERN_WARNING "samcop_adc: Got interrupt but conversion not ended\n");
			break;
		}
		udelay(10);
	}
	//printk(KERN_DEBUG "samcop_adc: timeout=%d\n", timeout);

	return 0;
}

static int samcop_adc_end_sample(struct samcop_adc *adc, int pin)
{
	if (adc->pdata->set_power)
		adc->pdata->set_power(pin, 0);

	/* 
	 * Set touch screen control back to 0xd3 so that the next
	 * sample will have the PEN_UP bit set correctly
	 */
	samcop_adc_write_register(adc, _SAMCOP_ADC_Control,
	                          SAMCOP_ADC_CONTROL_INIT);
	samcop_adc_write_register(adc, _SAMCOP_ADC_TouchScreenControl,
	                          SAMCOP_ADC_TSC_WAIT);

	return 0;
}

static int samcop_adc_read_sense(struct samcop_adc *adc,
                                 struct adc_sense *sen)
{
	int pin = sen->pin_id;
	int data[2];

	data[0] = samcop_adc_read_register(adc, _SAMCOP_ADC_Data0);
	data[1] = samcop_adc_read_register(adc, _SAMCOP_ADC_Data1);

	if (pin == SAMCOP_ADC_PIN_X || pin == SAMCOP_ADC_PIN_Y)
		sen->value = data[pin-1] & SAMCOP_ADC_DATA_DATA_MASK;
	else if (pin == SAMCOP_ADC_PIN_Z1 || pin == SAMCOP_ADC_PIN_Z2) {
		if (data[0] & SAMCOP_ADC_DATA_PEN_UP ||
		    data[1] & SAMCOP_ADC_DATA_PEN_UP)
			sen->value = 0;
		else
			sen->value = 100;
	}
	else
		sen->value = data[0] & SAMCOP_ADC_DATA_DATA_MASK;

	pr_debug("%s (%d): 0x%x 0x%x (%d)\n", sen->name, sen->pin_id,
	                                      data[0], data[1], sen->value);
	return 0;
}

static int samcop_adc_sense(struct device *dev, struct adc_request *req)
{
	struct samcop_adc *adc = platform_get_drvdata(to_platform_device(dev));
	int i = 0;
	int mask;

	/* Here we assume that we'll not be asked for senses from different
	 * channels by one request. We're passing any (first) pin_id to
	 * start_sample & end_sample functions to determine channel. */

	down(&dev->sem);

restart:
	mask = 0;
	samcop_adc_start_sample(adc, req->senses[0].pin_id);
	for (; i < req->num_senses; i++) {
		if (mask & (1 << req->senses[i].pin_id))
			goto restart;
		mask |= 1 << req->senses[i].pin_id;
		samcop_adc_read_sense(adc, &req->senses[i]);
	}
	samcop_adc_end_sample(adc, req->senses[0].pin_id);

	up(&dev->sem);
	return 0;
}

static void samcop_adc_up(struct samcop_adc *adc)
{
	clk_enable(adc->clk);
	samcop_adc_write_register(adc, _SAMCOP_ADC_TouchScreenControl,
	                          SAMCOP_ADC_TSC_WAIT);
	return;
}

static void samcop_adc_down(struct samcop_adc *adc)
{
	u_int16_t val;

	val = samcop_adc_read_register(adc, _SAMCOP_ADC_Control);
	samcop_adc_write_register(adc, _SAMCOP_ADC_Control, val |
	                          SAMCOP_ADC_CONTROL_STANDBY);
	samcop_adc_write_register(adc, _SAMCOP_ADC_TouchScreenControl,
	                          SAMCOP_ADC_TSC_SUSPEND);
	clk_disable(adc->clk);

	return;
}

static int samcop_adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct samcop_adc *adc = platform_get_drvdata(pdev);
	samcop_adc_down(adc);
	return 0;
}

static int samcop_adc_resume(struct platform_device *pdev)
{
	struct samcop_adc *adc = platform_get_drvdata(pdev);
	samcop_adc_up(adc);
	return 0;
}

static struct adc_classdev acdevs[] = {
	{
		.name = "x",
		.units = "coordinate",
		.pin_id = SAMCOP_ADC_PIN_X,
	},
	{
		.name = "y",
		.units = "coordinate",
		.pin_id = SAMCOP_ADC_PIN_Y,
	},
	{
		.name = "z1",
		.units = "coordinate",
		.pin_id = SAMCOP_ADC_PIN_Z1,
	},
	{
		.name = "z2",
		.units = "coordinate",
		.pin_id = SAMCOP_ADC_PIN_Z2,
	},
};

static int samcop_adc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct samcop_adc *adc;
	struct resource *res;
	struct samcop_adc_platform_data *adc_pdata = pdev->dev.platform_data;
	size_t i, j, k, m;

	adc = kzalloc(sizeof(*adc), GFP_KERNEL);
	if (!adc) {
		ret = -ENOMEM;
		goto adc_alloc_failed;
	}

	platform_set_drvdata(pdev, adc);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adc->map = ioremap(res->start, res->end - res->start + 1);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		ret = -EINVAL;
		goto noirq;
	}
	adc->irq = res->start;

	adc->clk = clk_get(&pdev->dev, "adc");
	if (IS_ERR(adc->clk)) {
		printk(KERN_ERR "samcop_adc: failed to get adc clock\n");
		ret = PTR_ERR(adc->clk);
		goto clk_get_failed;
	}

	adc->pdata = adc_pdata;

	if (!adc_pdata->delay)
		adc_pdata->delay = 500;

	init_completion(&adc->comp);

	samcop_adc_up(adc);

	ret = request_irq(adc->irq, adc_isr, IRQF_DISABLED,
	                  pdev->dev.bus_id, adc);
	if (ret)
		goto irqfailed;

	for (i = 0; i < ARRAY_SIZE(acdevs); i++) {
		acdevs[i].min_sense = 0;
		acdevs[i].max_sense = SAMCOP_ADC_DATA_DATA_MASK;
		acdevs[i].sense = samcop_adc_sense;

		ret = adc_classdev_register(&pdev->dev, &acdevs[i]);
		if (ret) {
			printk("samcop_adc: failed to register adc class "
			       "device %s\n", acdevs[i].name);
			goto adc_cdev_register_failed;
		}
	}

	for (j = 0; j < adc->pdata->num_acdevs; j++) {
		adc->pdata->acdevs[j].min_sense = 0;
		adc->pdata->acdevs[j].max_sense = SAMCOP_ADC_DATA_DATA_MASK;
		adc->pdata->acdevs[j].sense = samcop_adc_sense;

		ret = adc_classdev_register(&pdev->dev,
		                            &adc->pdata->acdevs[j]);
		if (ret) {
			printk("samcop_adc: failed to register adc class "
			       "device %s\n", adc->pdata->acdevs[j].name);
			goto plat_adc_cdev_register_failed;
		}
	}

	goto success;

plat_adc_cdev_register_failed:
	for (m = 0; m <= j; m++)
		adc_classdev_unregister(&adc->pdata->acdevs[j - m]);
adc_cdev_register_failed:
	for (k = 0; k <= i; k++)
		adc_classdev_unregister(&acdevs[i - k]);
	free_irq(adc->irq, adc);
irqfailed:
	clk_put(adc->clk);
clk_get_failed:
	kfree(adc);
noirq:
adc_alloc_failed:
success:
	return ret;
}

static int samcop_adc_remove(struct platform_device *pdev)
{
	struct samcop_adc *adc = platform_get_drvdata(pdev);
	size_t i;

	for (i = 0; i < adc->pdata->num_acdevs; i++)
		adc_classdev_unregister(&adc->pdata->acdevs[i]);
	for (i = 0; i < ARRAY_SIZE(acdevs); i++)
		adc_classdev_unregister(&acdevs[i]);

	free_irq(adc->irq, adc);
	samcop_adc_down(adc);
	clk_put(adc->clk);
	iounmap(adc->map);
	kfree(adc);

	return 0;
}

static struct platform_driver samcop_adc_driver = {
	.driver   = {
		.name = "samcop adc",
	},
	.probe    = samcop_adc_probe,
	.remove   = samcop_adc_remove,
	.suspend  = samcop_adc_suspend,
	.resume   = samcop_adc_resume,
};

static int __init samcop_adc_init(void)
{
	return platform_driver_register(&samcop_adc_driver);
}

static void __exit samcop_adc_exit(void)
{
	platform_driver_unregister(&samcop_adc_driver);
}

module_init(samcop_adc_init);
module_exit(samcop_adc_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>, "
              "Keith Packard <keith.packard@hp.com>, "
              "Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("ADC driver for HAMCOP/SAMCOP");
