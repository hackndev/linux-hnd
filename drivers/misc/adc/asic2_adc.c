/*
 * Driver interface to HTC "ASIC2"
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2003 Hewlett-Packard Company.
 * Copyright 2003, 2004, 2005 Phil Blundell
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
 * Author:  Andrew Christian
 *          <Andrew.Christian@compaq.com>
 *          October 2001
 *
 * Restrutured June 2002
 * 
 * Jamey Hicks <jamey.hicks@hp.com>
 * Re-Restrutured September 2003
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>

#include <asm/hardware/ipaq-asic2.h>
#include <linux/mfd/asic2_base.h>
#include <linux/mfd/asic2_adc.h>
#include <linux/mfd/asic2_ts.h>

#define PDEBUG(format,arg...) printk(KERN_DEBUG __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )

/***********************************************************************************
 *      ADC - Shared resource
 *
 *   Resources used:     ADC 3 & 4
 *                       Clock: ADC (CX4)
 *                       ADC Interrupt on KPIO
 *
 *   Shared resources:   Clock: 24.576MHz crystal (EX1)
 *
 *   The ADC is multiplexed between the touchscreen, battery charger, and light sensor
 ***********************************************************************************/

static void 
asic2_start_adc_sample (struct adc_data *adc, int mux)
{
	unsigned long adcsr;

	adcsr = ASIC2_ADCSR_ADPS(4) | ASIC2_ADCSR_INT_ENABLE | ASIC2_ADCSR_ENABLE;

	asic2_write_register (adc->asic, IPAQ_ASIC2_ADMUX, mux | ASIC2_ADMUX_CLKEN);
	asic2_write_register (adc->asic, IPAQ_ASIC2_ADCSR, adcsr);
	enable_irq (adc->adc_irq);
	asic2_write_register (adc->asic, IPAQ_ASIC2_ADCSR, adcsr | ASIC2_ADCSR_START);
}

static void 
asic2_adc_select_next_sample (struct adc_data *adc)
{
	if (adc->ts_mux > 0) {
		adc->state = ADC_STATE_TOUCHSCREEN;
		asic2_start_adc_sample (adc, adc->ts_mux);
	}
	else if (adc->user_mux >= 0) {
		adc->state = ADC_STATE_USER;
		asic2_start_adc_sample (adc, adc->user_mux);
	} else {
		adc->state = ADC_STATE_IDLE;
	}
}

static irqreturn_t 
asic2_adc_isr (int irq, void *dev_id)
{
	struct adc_data *adc = dev_id;
	int data;
	unsigned long adcsr;

	data = asic2_read_register (adc->asic, IPAQ_ASIC2_ADCDR);
	adcsr = asic2_read_register (adc->asic, IPAQ_ASIC2_ADCSR);
	adcsr &= ~ASIC2_ADCSR_INT_ENABLE;   /* Disable the interrupt */
	asic2_write_register (adc->asic, IPAQ_ASIC2_ADCSR, adcsr);
	disable_irq (adc->adc_irq);

	switch (adc->state) {
	case ADC_STATE_IDLE:
		PDEBUG("Error --- called when no outstanding requests!");
		break;
	case ADC_STATE_TOUCHSCREEN:
		adc->ts_mux = asic2_touchscreen_sample (adc, data);
		break;
	case ADC_STATE_USER:
		if (adc->user_mux >= 0) {
			adc->last     = data;
			adc->user_mux = -1;
			wake_up_interruptible (&adc->waitq);
		}
		break;
	}
	
	asic2_adc_select_next_sample (adc);

	return IRQ_HANDLED;
}

/* Call this from touchscreen code (running in interrupt context) */
void 
asic2_adc_start_touchscreen (struct adc_data *adc, int mux)
{
	adc->ts_mux = mux;
	if (adc->state == ADC_STATE_IDLE)
		asic2_adc_select_next_sample (adc);
}

#if 0
/* Call this from user-mode programs */
int 
ipaq_asic2_adc_read_channel (int mux)
{
	struct adc_data *adc = &g_adcdev;
	int              result;
	unsigned long    flags;

	if ( down_interruptible(&adc->lock) )
		return -ERESTARTSYS;

	// Kick start if we aren't currently running
	local_irq_save( flags );
	adc->user_mux = mux;
	if ( adc->state == ADC_STATE_IDLE ) 
		asic2_adc_select_next_sample( adc );
	local_irq_restore( flags );

	result = wait_event_interruptible( adc->waitq, adc->user_mux < 0 );

	adc->user_mux = -1;  // May not be -1 if we received a signal
	if ( result >= 0 ) 
		result = adc->last;
	
	up(&adc->lock);
	return result;
}
#endif

static void 
asic2_adc_up (struct adc_data *adc)
{
	asic2_shared_add (adc->asic, &adc->shared, ASIC_SHARED_CLOCK_EX1);
	asic2_clock_enable (adc->asic, ASIC2_CLOCK_ADC, 1);
}

static void 
asic2_adc_down (struct adc_data *adc)
{
	asic2_clock_enable (adc->asic, ASIC2_CLOCK_ADC, 0);
	asic2_shared_release (adc->asic, &adc->shared, ASIC_SHARED_CLOCK_EX1);

	if (adc->ts_mux != 0) {
		// Clear any current touchscreen requests
		adc->ts_mux = 0;
	}
	adc->state  = ADC_STATE_IDLE;
}


static int 
asic2_adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct adc_data *adc = pdev->dev.driver_data;

	asic2_touchscreen_suspend(adc, state);

	down (&adc->lock);  // No interruptions
	asic2_adc_down (adc);
	disable_irq (adc->adc_irq);
	return 0;
}

static int
asic2_adc_resume(struct platform_device *pdev)
{
	struct adc_data *adc = pdev->dev.driver_data;

	enable_irq (adc->adc_irq);
	asic2_adc_up (adc);
	up (&adc->lock);
	asic2_touchscreen_resume(adc);
	return 0;
}

static int 
asic2_adc_probe(struct platform_device *pdev)
{
	int result;
	struct adc_data *adc;
	
	adc = kmalloc (sizeof (*adc), GFP_KERNEL);
	if (!adc)
		return -ENOMEM;
	memset (adc, 0, sizeof (*adc));

	adc->adc_irq = pdev->resource[1].start;
	adc->asic = pdev->dev.parent;

	pdev->dev.driver_data = adc;

	result = request_irq (adc->adc_irq, asic2_adc_isr, SA_SAMPLE_RANDOM, "asic2 adc", adc);
	if (result) {
		printk (KERN_ERR "asic2_adc: unable to claim irq %d; error %d\n", adc->adc_irq, result);
		kfree (adc);
		return result;
	}

	result = asic2_touchscreen_attach (adc);
	if (result) {
		free_irq (adc->adc_irq, adc);
		kfree (adc);
		return result;
	}

	init_MUTEX (&adc->lock);
	init_waitqueue_head (&adc->waitq);
	asic2_adc_up (adc);

	return result;
}

static int
asic2_adc_remove (struct platform_device *pdev)
{
	struct adc_data *adc = pdev->dev.driver_data;

	free_irq (adc->adc_irq, adc);

	asic2_touchscreen_detach (adc);
	asic2_adc_down (adc);

	return 0;
}

//static platform_device_id asic2_adc_device_ids[] = { { IPAQ_ASIC2_ADC_DEVICE_ID }, { 0 } };

struct platform_driver asic2_adc_device_driver = {
	.driver = {
	    .name     = "asic2-adc",
	},
	.probe    = asic2_adc_probe,
	.remove   = asic2_adc_remove,
	.suspend  = asic2_adc_suspend,
	.resume   = asic2_adc_resume
};

static int
asic2_adc_init (void)
{
	return platform_driver_register(&asic2_adc_device_driver);
}

static void
asic2_adc_cleanup (void)
{
	platform_driver_unregister(&asic2_adc_device_driver);
}

module_init(asic2_adc_init);
module_exit(asic2_adc_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org> and others");
MODULE_DESCRIPTION("ADC driver for iPAQ ASIC2");
MODULE_SUPPORTED_DEVICE("asic2 adc");
//MODULE_DEVICE_TABLE(soc, asic2_adc_device_ids);
