/*
 * "Alternative" (SSP-based) ads8746 touchscreen driver.
 *
 * Copyright (c) 2007 Paul Sokolovsky
 * Copyright (c) 2007 Pierre Gaufillet
 * Copyright (c) 2006 Kevin O'Connor <kevin@koconnor.net>
 * Copyright (C) 2005-2006 Pawel Kolodziejski
 * Copyright (c) 2005 SDG Systems, LLC
 * Copyright (C) 2003 Joshua Wise
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

//#define DEBUG
//#define DEBUG_SAMPLES

#define DRIVER_NAME "ts-adc"

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/adc.h>
#include <linux/touchscreen-adc.h>
#include <asm/arch/pxa-regs.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#define BIG_VALUE 999999

#define SAMPLE_TIMEOUT 20	/* sample every 20ms */

static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-2)");

#define pr_debug(fmt,arg...) \
        if (debug) printk(KERN_DEBUG fmt,##arg) 


enum touchscreen_state {
	STATE_WAIT_FOR_TOUCH,   /* Waiting for a PEN interrupt */
	STATE_SAMPLING          /* Actively sampling ADC */
};

struct adc_work {
	struct delayed_work work;
	struct platform_device *pdev;
};

struct ts_pos {
	unsigned long xd;
	unsigned long yd;
	unsigned int pressure;
	int err;
};

struct ts_adc {
	unsigned int pen_irq;
	struct input_dev *input;
	// Is touchscreen active.
	int state;
	int sample_no;
	struct ts_pos last_pos;

	struct adc_request req;
	struct adc_sense *pins;

	struct workqueue_struct *wq;
	struct adc_work work;
};

#define delta_within_bounds(x,y,d) ( ((int)((x) - (y)) < (int)(d)) && ((int)((y) - (x)) < (int)(d)) )

static void report_touchpanel(struct ts_adc *ts, int x, int y, int pressure)
{
	input_report_key(ts->input, BTN_TOUCH, pressure != 0);
	input_report_abs(ts->input, ABS_PRESSURE, pressure);
	if (pressure) {
		input_report_abs(ts->input, ABS_X, x);
		input_report_abs(ts->input, ABS_Y, y);
	}
	input_sync(ts->input);
}

void debounce_samples(struct tsadc_platform_data *params, struct adc_sense *pins, struct ts_pos *tp)
{
	unsigned int t, x, y, z1, z2, pressure = 0;
	int i, ptr;
	int d = params->max_jitter;

	if (debug > 1) {
		pr_debug(DRIVER_NAME ": ");
		ptr = 0;
		for (i = 0; i < params->num_xy_samples; i++) {
			printk("%04d ", pins[ptr++].value);
		}
		printk("| ");
		for (i = 0; i < params->num_xy_samples; i++) {
			printk("%04d ", pins[ptr++].value);
		}
		printk("| ");
		for (i = 0; i < params->num_z_samples; i++) {
			printk("%04d ", pins[ptr++].value);
		}
		printk("| ");
		for (i = 0; i < params->num_z_samples; i++) {
			printk("%04d ", pins[ptr++].value);
		}
		printk("\n");
	}

	/* X-axis */
	ptr = 0;
	x = pins[ptr++].value;
	/* Do real debouncing only if multiple xy samples requested. */
	if (params->num_xy_samples > 1) {
		/* Run thru samples and see if they converge */
		for (i = params->num_xy_samples - 1; i > 0; i--) {
			t = x;
			x = pins[ptr++].value;
			if (delta_within_bounds(t, x, d))
				break;
		}
		/* If we exhausted loop, it means the samples unconverged */
		if (i == 0)
			tp->err = 1;
	}

	/* Y-axis */
	ptr = params->num_xy_samples;
	y = pins[ptr++].value;
	/* Do real debouncing only if multiple xy samples requested. */
	if (params->num_xy_samples > 1) {
		for (i = params->num_xy_samples - 1; i > 0; i--) {
			t = y;
			y = pins[ptr++].value;
			if (delta_within_bounds(t, y, d))
				break;
		}
		if (i == 0)
			tp->err = 1;
	}

	/* Pressure */
	ptr = params->num_xy_samples * 2;
	/* Discard first sample as unstable if more than one z reading requested */ 
	if (params->num_z_samples > 1) 
		ptr++;
	z1 = pins[ptr++].value;

	if (params->num_z_samples > 1) 
		ptr++;
	z2 = pins[ptr++].value;

	// RTOUCH = (RXPlate) x (XPOSITION /4096) x [(Z2/Z1) - 1]
	// pressure is proportional to 1 / RTOUCH
	/* Allow for some noise in readings still. Too low z1 value means 0, 
	   and zero means pen up. */
	if (z1 < 20) {
		pressure = 0;
	} else {
		pressure = abs(z2 - z1) * (x ? x : 1); // X just shouldn't be 0
		if (pressure != 0)
			pressure = params->pressure_factor * z1 / pressure;
		else
			//pressure = BIG_VALUE; // this regresses h5000. workaround is below. needs more investigation
			pressure = z1;
	}
	
	pr_debug(DRIVER_NAME ": x=%d y=%d z1=%d z2=%d P=%d\n", x, y, z1, z2, pressure);

	tp->xd = x;
	tp->yd = y;
	tp->pressure = pressure;
}

static irqreturn_t ts_adc_debounce_isr(int irq, void* data)
{
	struct platform_device *pdev = data;
	struct tsadc_platform_data *params = pdev->dev.platform_data;
	struct ts_adc *ts = platform_get_drvdata(pdev);

	int pen_up = 0;

	if (params->pen_gpio)
		pen_up = GPLR(params->pen_gpio) & GPIO_bit(params->pen_gpio);

	/* Don't even bother to do anything for spurious bounce */
	if (pen_up) {
		pr_debug(DRIVER_NAME ": Bouncing TS IRQ\n");
		return IRQ_HANDLED;
	}

	disable_irq_nosync(ts->pen_irq);

	ts->state = STATE_SAMPLING;
	ts->sample_no = 0;
	pr_debug(DRIVER_NAME ": Started to sample\n");
	cancel_delayed_work(&ts->work.work);
	queue_delayed_work(ts->wq, &ts->work.work, 0);

	return IRQ_HANDLED;
}

static void ts_adc_debounce_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = container_of(work,
	                                         struct delayed_work, work);
	struct adc_work *adc_work = container_of(delayed_work,
	                                         struct adc_work, work);
	struct platform_device *pdev = adc_work->pdev;
	struct tsadc_platform_data *params = pdev->dev.platform_data;
	struct ts_adc *ts = platform_get_drvdata(pdev);

	struct ts_pos ts_pos;
	int pen_up = 0;
	int finish_sample = 0;

	ts_pos.err = 0;
	ts_pos.pressure = -1;

	/* Sample actual state of pen up/down */
	if (params->pen_gpio)
		pen_up = GPLR(params->pen_gpio) & GPIO_bit(params->pen_gpio);

	if (!pen_up) {
		adc_request_sample(&ts->req);
		debounce_samples(params, ts->req.senses, &ts_pos);
	}

	/* Finish sampling when either pen is up or too low pressure was detected */
	finish_sample = pen_up || (ts_pos.pressure <= params->min_pressure);

	if (finish_sample) {
		report_touchpanel(ts, 0, 0, 0);
		ts->state = STATE_WAIT_FOR_TOUCH;
		pr_debug(DRIVER_NAME ": Finished to sample, reason: pen_up=%d, pressure=%d <= %u\n", !!pen_up, ts_pos.pressure, params->min_pressure);
		enable_irq(ts->pen_irq);
	} else {
		//printk("%04d %04d\n", (int)ts_pos.xd, (int)ts_pos.yd);
		if (ts_pos.err)
			printk(DRIVER_NAME ": Sample with too much jitter - ignored\n");
		else {
			if (params->delayed_pressure) {
				if (ts->sample_no == 0)
					report_touchpanel(ts, ts_pos.xd, ts_pos.yd, 1);
				else
					report_touchpanel(ts, ts->last_pos.xd, ts->last_pos.yd, 1);
				ts->last_pos = ts_pos;
			} else {
				report_touchpanel(ts, ts_pos.xd, ts_pos.yd, 1);
			}
			ts->sample_no++;
		}

		queue_delayed_work(ts->wq, &ts->work.work, (SAMPLE_TIMEOUT * HZ) / 1000);
	}

	return;
};

inline static void fill_pins(struct adc_sense *pins, int from, int num, char *with)
{
	int i;
	for (i = from + num - 1; i >= from; i--)
		pins[i].name = with;
}

static int ts_adc_debounce_probe(struct platform_device *pdev)
{
	struct tsadc_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct ts_adc *ts;
	int ret, i;

	// Initialize ts data structure.
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	platform_set_drvdata(pdev, ts);

	pdata->num_xy_samples = pdata->num_xy_samples ?: 10;
	pdata->num_z_samples  = pdata->num_z_samples ?: 2;

	ts->pins = kmalloc(sizeof(*ts->pins) * (pdata->num_xy_samples*2 + pdata->num_z_samples*2), GFP_KERNEL);
	ts->req.senses = ts->pins;
	ts->req.num_senses = (pdata->num_xy_samples*2 + pdata->num_z_samples*2);
	i = 0;
	fill_pins(ts->pins, i, pdata->num_xy_samples, pdata->x_pin);
	fill_pins(ts->pins, i += pdata->num_xy_samples, 
		pdata->num_xy_samples, pdata->y_pin);
	fill_pins(ts->pins, i += pdata->num_xy_samples,  
		pdata->num_z_samples, pdata->z1_pin);
	fill_pins(ts->pins, i +=  pdata->num_z_samples,  pdata->num_z_samples, pdata->z2_pin);
	adc_request_register(&ts->req);

	ts->work.pdev = pdev;
	INIT_DELAYED_WORK(&ts->work.work, ts_adc_debounce_work);
	ts->wq = create_workqueue(pdev->dev.bus_id);
	if (!ts->wq)
		return -ESRCH;

        ts->input = input_allocate_device();
        if (!ts->input)
                return -ENOMEM;

	ts->input->name = DRIVER_NAME;
	ts->input->phys = "touchscreen/adc";

	set_bit(EV_ABS, ts->input->evbit);
	set_bit(EV_KEY, ts->input->evbit);
	set_bit(ABS_X, ts->input->absbit);
	set_bit(ABS_Y, ts->input->absbit);
	set_bit(ABS_PRESSURE, ts->input->absbit);
	set_bit(BTN_TOUCH, ts->input->keybit);
	ts->input->absmin[ABS_PRESSURE] = 0;
	ts->input->absmax[ABS_PRESSURE] = 1;
	ts->input->absmin[ABS_X] = 0;
	ts->input->absmax[ABS_X] = 0;
	ts->input->absmin[ABS_Y] = 32767;
	ts->input->absmax[ABS_Y] = 32767;

	ts->state = STATE_WAIT_FOR_TOUCH;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		ret = -EINVAL;
		goto noirq;
	}

	ts->pen_irq = res->start;
	set_irq_type(ts->pen_irq, IRQT_FALLING);
	ret = request_irq(ts->pen_irq, ts_adc_debounce_isr, SA_INTERRUPT, DRIVER_NAME, pdev);
	if (ret < 0)
		goto irqfail;

	ret = input_register_device(ts->input);
	if (ret < 0)
		goto regfail;

	return 0;

regfail:
	free_irq(ts->pen_irq, pdev);
irqfail:
	input_free_device(ts->input);
noirq:
	return ret;
}

static int ts_adc_debounce_remove(struct platform_device *pdev)
{
	struct ts_adc *ts = platform_get_drvdata(pdev);

	free_irq(ts->pen_irq, pdev);
	cancel_delayed_work(&ts->work.work);
	destroy_workqueue(ts->wq);
	adc_request_unregister(&ts->req);
	input_unregister_device(ts->input);
	input_free_device(ts->input);

	return 0;
}

#ifdef CONFIG_PM
static int ts_adc_debounce_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ts_adc *ts = platform_get_drvdata(pdev);

	if (ts->state != STATE_SAMPLING)
		disable_irq(ts->pen_irq);

	return 0;
}

static int ts_adc_debounce_resume(struct platform_device *pdev)
{
	struct ts_adc *ts = platform_get_drvdata(pdev);

	ts->state = STATE_WAIT_FOR_TOUCH;
	enable_irq(ts->pen_irq);

	return 0;
}
#endif

static struct platform_driver ts_adc_debounce_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
	},
	.probe		= ts_adc_debounce_probe,
	.remove         = ts_adc_debounce_remove,
#ifdef CONFIG_PM
	.suspend	= ts_adc_debounce_suspend,
	.resume		= ts_adc_debounce_resume,
#endif
};

static int __init ts_adc_debounce_init(void)
{
	return platform_driver_register(&ts_adc_debounce_driver);
}

static void __exit ts_adc_debounce_exit(void)
{
	platform_driver_unregister(&ts_adc_debounce_driver);
}

module_init(ts_adc_debounce_init)
module_exit(ts_adc_debounce_exit)

MODULE_AUTHOR("Joshua Wise, Pawel Kolodziejski, Paul Sokolovsky, Aric Blumer");
MODULE_DESCRIPTION("Lightweight touchscreen driver with debouncing for ADC chip");
MODULE_LICENSE("GPL");
