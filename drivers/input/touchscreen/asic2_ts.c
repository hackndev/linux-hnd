/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
* Copyright 2003 Hewlett-Packard Company.
* Copyright 2003, 2004, 2005 Phil Blundell
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/soc-old.h>
#include <linux/input.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>

#include <asm/hardware/ipaq-asic2.h>

#include <linux/mfd/asic2_base.h>
#include <linux/mfd/asic2_adc.h>
#include <linux/mfd/asic2_ts.h>

#define PERROR(format,arg...) printk(KERN_ERR __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )

/***********************************************************************************
 *      Touchscreen
 *
 *   Resources           ADC stuff
 *                       Pen interrupt on GPIO2
 *
 ***********************************************************************************/

#define TSMASK  (GPIO2_IN_Y1_N | GPIO2_IN_X0 | GPIO2_IN_Y0 | GPIO2_IN_X1_N)

struct ts_sample {
	unsigned int   mask;
	unsigned short mux;
	char *         name;
};

/* TODO:  This is ugly, but good for debugging.
   In the future, we'd prefer to use one of the ASIC2 timers
   to toggle ADC sampling, rather than queueing a timer.
   We also should look at settling times for the screen.
*/

const struct ts_sample g_samples[] = {
	{ GPIO2_IN_X1_N | GPIO2_IN_Y0, ASIC2_ADMUX_3_TP_X0, "X" },    /* Measure X */
	{ GPIO2_IN_X1_N | GPIO2_IN_Y0, ASIC2_ADMUX_3_TP_X0, "X" },    /* Measure X */
	{ GPIO2_IN_X1_N | GPIO2_IN_Y0, ASIC2_ADMUX_3_TP_X0, "X" },    /* Measure X */
	{ GPIO2_IN_X1_N | GPIO2_IN_Y0, ASIC2_ADMUX_3_TP_X0, "X" },    /* Measure X */
	{ GPIO2_IN_X1_N | GPIO2_IN_Y0, ASIC2_ADMUX_4_TP_Y1, "XY" },  
	{ GPIO2_IN_Y1_N | GPIO2_IN_X0, ASIC2_ADMUX_4_TP_Y1, "Y" },    /* Measure Y */
	{ GPIO2_IN_Y1_N | GPIO2_IN_X0, ASIC2_ADMUX_4_TP_Y1, "Y" },    /* Measure Y */
	{ GPIO2_IN_Y1_N | GPIO2_IN_X0, ASIC2_ADMUX_4_TP_Y1, "Y" },    /* Measure Y */
	{ GPIO2_IN_Y1_N | GPIO2_IN_X0, ASIC2_ADMUX_4_TP_Y1, "Y" },    /* Measure Y */
	{ GPIO2_IN_Y1_N | GPIO2_IN_X0, ASIC2_ADMUX_3_TP_X0, "YX" },    /* Measure Y */
	{ GPIO2_IN_Y1_N | GPIO2_IN_Y0, ASIC2_ADMUX_3_TP_X0, "CX" },
	{ GPIO2_IN_Y1_N | GPIO2_IN_Y0, ASIC2_ADMUX_4_TP_Y1, "CY" },

	{ GPIO2_IN_Y1_N | GPIO2_IN_X0 | GPIO2_IN_X1_N, 0,   "End" },      /* Go to PEN_IRQ mode */
};

#define TS_SAMPLE_COUNT (sizeof(g_samples)/sizeof(struct ts_sample))

enum touchscreen_state {
	TS_STATE_WAIT_PEN_DOWN,   // Waiting for a PEN interrupt
	TS_STATE_ACTIVE_SAMPLING  // Actively sampling ADC
};

struct touchscreen_data {
	enum touchscreen_state state;
	unsigned long          shared;   // Shared resources
	int                    samples[TS_SAMPLE_COUNT];
	int                    index;
	struct timer_list      timer;
	unsigned long	       pen_irq;
	struct input_dev       *input;
};

#define ASIC_ADC_DELAY  10  /* Delay 10 milliseconds */

static void
report_touchpanel (struct touchscreen_data *ts, int x, int y, int pressure)
{
	input_report_abs(ts->input, ABS_X, x);
	input_report_abs(ts->input, ABS_Y, y);
	input_report_abs(ts->input, ABS_PRESSURE, pressure);
	input_sync(ts->input);
}

/* Called by ADC ISR routine */
/* Return the number of the next mux to read, or 0 to stop */
int 
asic2_touchscreen_sample (struct adc_data *adc, int data)
{
	struct touchscreen_data *ts = adc->ts;
	const struct ts_sample  *s;

	ts->samples[ ts->index++ ] = data;
	s = &g_samples[ ts->index ];

	if ( ts->state == TS_STATE_ACTIVE_SAMPLING ) {
		asic2_set_gpiod (adc->asic, TSMASK, s->mask);   // Set the output pins
		if ( !s->mux )
			mod_timer(&ts->timer, jiffies + (ASIC_ADC_DELAY * HZ) / 1000);
	}

	return s->mux;
}

static void 
asic2_touchscreen_start_record (struct adc_data *adc, struct touchscreen_data *ts)
{
	ts->index = 0;
	asic2_set_gpiod (adc->asic, TSMASK, g_samples[0].mask);
	asic2_adc_start_touchscreen (adc, g_samples[0].mux);
}

/* Invoked after a complete series of ADC samples has been taken  */
static void 
asic2_timer_callback (unsigned long nr)
{
	struct adc_data *adc = (struct adc_data *)nr;
	struct touchscreen_data *ts = adc->ts;

	// The last ADC sample sets us up for "pen" mode
	if (asic2_read_gpiod (adc->asic) & GPIO2_PEN_IRQ ) {
		report_touchpanel (ts, 0, 0, 0);
		ts->state = TS_STATE_WAIT_PEN_DOWN;
		enable_irq (ts->pen_irq);
	} else {
		unsigned long flags;
		report_touchpanel (ts, ts->samples[3], ts->samples[8], 1 );
		local_irq_save (flags);
		asic2_touchscreen_start_record (adc, ts);
		local_irq_restore (flags);
	}
}

static irqreturn_t 
asic2_pen_isr (int irq, void *dev_id)
{
	struct adc_data *adc = dev_id;
	struct touchscreen_data *ts = adc->ts;

	disable_irq (ts->pen_irq);
	ts->state = TS_STATE_ACTIVE_SAMPLING;
	asic2_touchscreen_start_record (adc, ts);
	return IRQ_HANDLED;
}

int
asic2_touchscreen_suspend (struct adc_data *adc, pm_message_t state)
{
	unsigned long flags;
	struct touchscreen_data *ts = adc->ts;

	local_irq_save (flags);
	
	switch (ts->state) {
	case TS_STATE_WAIT_PEN_DOWN:
		disable_irq (ts->pen_irq);
		break;
	case TS_STATE_ACTIVE_SAMPLING:
		ts->state = TS_STATE_WAIT_PEN_DOWN;   // Prevents the timers from sending messages 
		del_timer_sync (&ts->timer);
		report_touchpanel (ts, 0, 0, 0);
		break;
	}

	asic2_set_gpiod (adc->asic, TSMASK, GPIO2_IN_Y1_N | GPIO2_IN_X1_N); /* Normal off state */

	local_irq_restore (flags);

	return 0;
}

int
asic2_touchscreen_resume(struct adc_data *adc)
{
	struct touchscreen_data *ts = adc->ts;

	ts->state = TS_STATE_WAIT_PEN_DOWN;
	asic2_set_gpiod (adc->asic, TSMASK, GPIO2_IN_Y1_N | GPIO2_IN_X0 | GPIO2_IN_X1_N);
	
	enable_irq (ts->pen_irq);

	return 0;
}

int 
asic2_touchscreen_attach (struct adc_data *adc)
{
	int result;
	struct touchscreen_data *ts;

	ts = kmalloc (sizeof (*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	memset (ts, 0, sizeof (*ts));
	ts->input = input_allocate_device();

	init_timer (&ts->timer);
	ts->timer.function = asic2_timer_callback;
	ts->timer.data     = (unsigned long)adc;
	ts->state          = TS_STATE_WAIT_PEN_DOWN;
	ts->pen_irq        = asic2_irq_base (adc->asic) + IRQ_IPAQ_ASIC2_PEN;

	set_bit(EV_ABS, ts->input->evbit);
	ts->input->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);

	ts->input->absmin[ABS_X] = 0;   ts->input->absmin[ABS_Y] = 0; ts->input->absmin[ABS_PRESSURE] = 0;
	ts->input->absmax[ABS_X] = 1023; ts->input->absmax[ABS_Y] = 1023; ts->input->absmax[ABS_PRESSURE] = 1;

	ts->input->name = "asic2 ts";
	ts->input->private = ts;

	input_register_device(ts->input);

	asic2_set_gpiod (adc->asic, TSMASK, GPIO2_IN_Y1_N | GPIO2_IN_X0 | GPIO2_IN_X1_N);
	asic2_set_gpint (adc->asic, GPIO2_PEN_IRQ, 1, 0);

	adc->ts = ts;

	result = request_irq (ts->pen_irq, asic2_pen_isr, SA_SAMPLE_RANDOM, "asic2 touchscreen", adc);
	if (result) {
		PERROR("unable to grab touchscreen virtual IRQ");
		input_unregister_device(ts->input);
		kfree (ts);
		return result;
	}

	return result;
}

void 
asic2_touchscreen_detach (struct adc_data *adc)
{
	struct touchscreen_data *ts = adc->ts;

	free_irq (ts->pen_irq, adc);

	if (del_timer_sync (&ts->timer)) {
		report_touchpanel (ts, 0, 0, 0);
	}

	input_unregister_device(ts->input);

	asic2_set_gpiod (adc->asic, TSMASK, GPIO2_IN_Y1_N | GPIO2_IN_X1_N); /* Normal off state */

	kfree (ts);
}
