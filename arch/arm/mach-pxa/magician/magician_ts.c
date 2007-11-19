/* Touch screen driver for the TI something-or-other
 *
 * Copyright © 2005 SDG Systems, LLC
 *
 * Based on code that was based on the SAMCOP driver.
 *     Copyright © 2003, 2004 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author:  Keith Packard <keith.packard@hp.com>
 *          May 2003
 *
 * Updates:
 *
 * 2004-02-11   Michael Opdenacker      Renamed names from samcop to shamcop,
 *                                      Goal:support HAMCOP and SAMCOP.
 * 2004-02-14   Michael Opdenacker      Temporary fix for device id handling
 *
 * 2005-02-18   Aric Blumer             Converted  basic structure to support hx4700
 *
 * 2005-06-07   Aric Blumer             Added tssim device handling so we can
 *                                      hook in the fbvncserver.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#include <asm/io.h>
#include <asm/gpio.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/magician.h>

enum touchscreen_state {
	STATE_WAIT_FOR_TOUCH,	/* Waiting for a PEN interrupt */
	STATE_SAMPLING_X,	/* Actively sampling ADC */
	STATE_SAMPLING_Y,	/* Actively sampling ADC */
	STATE_SAMPLING_Z,	/* Actively sampling ADC */
};

struct touchscreen_data {
	enum touchscreen_state state;
	struct timer_list timer;
	int irq;
	struct input_dev *input;
	struct work_struct serial_work;
};

static unsigned long poll_sample_time = 10;	/* Sample every 10 milliseconds */

static struct touchscreen_data *ts_data;

#define TS_SAMPLES 5

module_param(poll_sample_time, ulong, 0644);
MODULE_PARM_DESC(poll_sample_time, "Poll sample time");

static inline void
report_touchpanel(struct touchscreen_data *ts, int pressure, int x, int y)
{
	input_report_abs(ts->input, ABS_PRESSURE, pressure);
	input_report_abs(ts->input, ABS_X, x);
	input_report_abs(ts->input, ABS_Y, y);
	input_sync(ts->input);
}

static irqreturn_t pen_isr(int irq, void *irq_desc)
{
	/* struct touchscreen_data *ts = dev_id->data; */
	struct touchscreen_data *ts = ts_data;

	if (irq == gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ)) {

		if (ts->state == STATE_WAIT_FOR_TOUCH) {
			/*
			 * There is ground bounce or noise or something going on here:
			 * when you write to the SSP port to get the X and Y values, it
			 * causes a TOUCHPANEL_IRQ_N interrupt to occur.  So if that
			 * happens, we can check to make sure the pen is actually down and
			 * disregard the interrupt if it's not.
			 */
			if (gpio_get_value(GPIO115_MAGICIAN_nPEN_IRQ) == 0) {
				/*
				 * Disable the pen interrupt.  It's reenabled when the user lifts the
				 * pen.
				 */
				disable_irq(irq);

				input_report_key(ts->input, BTN_TOUCH, 1);
				ts->state = STATE_SAMPLING_X;
				schedule_work(&ts->serial_work);
			}
		} else {
			/* Shouldn't happen */
			printk(KERN_ERR "Unexpected ts interrupt\n");
		}

	}
	return IRQ_HANDLED;
}

static void ssp_init(void)
{

	pxa_set_cken(CKEN3_SSP2, 1);

	/* *** Set up the SPI Registers *** */
	SSCR0_P2 = SSCR0_EDSS	/* Extended Data Size Select */
	    | SSCR0_SerClkDiv(7)	/* Serial Clock Rate */
	    |(0 << 7)		/* Synchronous Serial Enable (Disable for now) */
	    | SSCR0_Motorola	/* Motorola SPI */
	    | SSCR0_DataSize(8)	/* Data Size Select  (24-bit) */
	    ;
	SSCR1_P2 = 0;
	SSPSP_P2 = 0;

	/* Clear the Status */
	SSSR_P2 = SSSR_P2 & 0x00fcfffc;

	/* Now enable it */
	SSCR0_P2 = SSCR0_EDSS	/* Extended Data Size Select */
	    |SSCR0_SerClkDiv(7)	/* Serial Clock Rate */
	    |SSCR0_SSE		/* Synchronous Serial Enable */
	    |SSCR0_Motorola	/* Motorola SPI Interface */
	    |SSCR0_DataSize(8)	/* Data Size Select  (24-bit) */
	    ;
	/* enable_irq(gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ)); */
}

DECLARE_MUTEX(serial_mutex);

static void start_read(struct work_struct *work)
{
	struct touchscreen_data *touch = container_of(work, struct touchscreen_data, serial_work);
	unsigned long inc = (poll_sample_time * HZ) / 1000;
	int i;
	int command, samples;

	down(&serial_mutex);

	/* Write here to the serial port.
	 * Then we have to wait for poll_sample_time before we read out the serial
	 * port.  Then, when we read it out, we check to see if the pen is still
	 * down.  If so, then we issue another request here.
	 */

	switch (touch->state) {
	case STATE_SAMPLING_X:
		command = 0xd10000;
		samples = 14;
		break;
	case STATE_SAMPLING_Y:
		command = 0x910000;
		samples = 12;
		break;
	case STATE_SAMPLING_Z:
		command = 0xc00000;
		samples = 1;
	}

	for (i = 0; i < (samples - 1); i++) {
		while (!(SSSR_P2 & (1 << 2))) ;
		/* It's not full. Write the command for X/Y/Z */
		SSDR_P2 = command;
	}
	while (!(SSSR_P2 & (1 << 2))) ;
	/* Write the command for X/Y/Z, turn off ADC */
	SSDR_P2 = command & ~(0x10000);

	/*
	 * Enable the timer. We should get an interrupt, but we want keep a timer
	 * to ensure that we can detect missing data
	 */
	mod_timer(&touch->timer, jiffies + inc);
}

static void ts_timer_callback(unsigned long data)
{
	struct touchscreen_data *ts = (struct touchscreen_data *)data;
	static int x, y;
	int ssrval;
	int samples;

	switch (ts->state) {
	case STATE_SAMPLING_X:
		samples = 14;
		break;
	case STATE_SAMPLING_Y:
		samples = 12;
		break;
	case STATE_SAMPLING_Z:
		samples = 1;
	}

	/*
	 * Check here to see if there is anything in the SPI FIFO.  If so,
	 * return it if there has been a change.  If not, then we have a
	 * timeout.  Generate an erro somehow.
	 */
	ssrval = SSSR_P2;
	if (ssrval & (1 << 3)) {	/* Look at Rx Not Empty bit */
		int number_of_entries_in_fifo;

		/* The FIFO is not emtpy. Good! Now make sure there are at least two
		 * entries. */

		number_of_entries_in_fifo = ((ssrval >> 12) & 0xf) + 1;

		if (number_of_entries_in_fifo < samples) {
			/* Not ready yet. Come back later. */
			unsigned long inc = (poll_sample_time * HZ) / 1000;
			mod_timer(&ts->timer, jiffies + inc);
			return;
		}

		if (number_of_entries_in_fifo == samples) {
			int i, keep;
			int X[14], Y[12], Z2;

			switch (ts->state) {
			case STATE_SAMPLING_X:
				for (i = 0; i < 14; i++)
					X[13 - i] = SSDR_P2 & 0xffff;
				x = (2 * X[0] + X[1] + X[2]) / 4;
				ts->state = STATE_SAMPLING_Y;
				break;
			case STATE_SAMPLING_Y:
				for (i = 0; i < 12; i++)
					Y[11 - i] = SSDR_P2 & 0xffff;
				y = (2 * Y[0] + Y[1] + Y[2]) / 4;
				ts->state = STATE_SAMPLING_Z;
				break;
			case STATE_SAMPLING_Z:
				Z2 = SSDR_P2 & 0xffff;
				ts->state = STATE_SAMPLING_X;
			}

			up(&serial_mutex);

			keep = 1;
#if 0
			keep = 0;
			x = y = 0;

			result = 0;
			if (do_delta_calc(X[0], Y[0], X[1], Y[1], X[2], Y[2])) {
				result |= (1 << 2);
			}
			if (do_delta_calc(X[1], Y[1], X[2], Y[2], X[3], Y[3])) {
				result |= (1 << 1);
			}
			if (do_delta_calc(X[2], Y[2], X[3], Y[3], X[4], Y[4])) {
				result |= (1 << 0);
			}
			switch (result) {
			case 0:
				/* Take average of point 0 and point 3 */
				X[2] = (X[1] + X[3]) / 2;
				Y[2] = (Y[1] + Y[3]) / 2;
				/* don't keep */
				break;
			case 1:
				/* Just ignore this one */
				break;
			case 2:
			case 3:
			case 6:
				/* keep this sample */
				x = (X[1] + (2 * X[2]) + X[3]) / 4;
				y = (Y[1] + (2 * Y[2]) + Y[3]) / 4;
				keep = 1;
				break;
			case 4:
				X[1] = (X[0] + X[2]) / 2;
				Y[1] = (Y[0] + Y[2]) / 2;
				/* don't keep */
				break;
			case 5:
			case 7:
				x = (X[0] + (4 * X[1]) + (6 * X[2]) +
				     (4 * X[3]) + X[4]) >> 4;
				y = (Y[0] + (4 * Y[1]) + (6 * Y[2]) +
				     (4 * Y[3]) + Y[4]) >> 4;
				keep = 1;
				break;
			}
#endif
			if (gpio_get_value(GPIO115_MAGICIAN_nPEN_IRQ) == 0) {
				/* Still down */
				if (keep) {
					report_touchpanel(ts, 1, x, y);
				}
				start_read(&ts->serial_work);
			} else {
				/* Up */
				input_report_key(ts->input, BTN_TOUCH, 0);
				report_touchpanel(ts, 0, 0, 0);
				ts->state = STATE_WAIT_FOR_TOUCH;
				/* Re-enable pen down interrupt */
				enable_irq(gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ));
			}

		} else {
			/* We have an error! Too many entries. */
			printk(KERN_ERR "TS: Expected %d entries. Got %d\n", 2,
			       number_of_entries_in_fifo);
			/* Try to clear the FIFO */
			while (number_of_entries_in_fifo--) {
				(void)SSDR_P2;
			}
			up(&serial_mutex);
			if (gpio_get_value(GPIO115_MAGICIAN_nPEN_IRQ) == 0) {
				start_read(&ts->serial_work);
			}
			return;
		}
	} else {
		/* Not ready yet. Come back later. */
		unsigned long inc = (poll_sample_time * HZ) / 1000;
		printk("wait some more\n");
		mod_timer(&ts->timer, jiffies + inc);
		return;
	}
}

static int ts_probe(struct platform_device *dev)
{
	int retval;
	struct touchscreen_data *ts;

	ts = ts_data = kmalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_NOTICE "magician_ts: unable to allocate memory\n");
		free_irq(ts->irq, ts);
		return -ENOMEM;
	}
	memset(ts, 0, sizeof(*ts));

	ts->irq = gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ);
	retval = request_irq(ts->irq, pen_isr, IRQF_DISABLED, "magician_ts", ts);
	if (retval < 0) {
		printk("Unable to get interrupt\n");
		kfree(ts);
		return -ENODEV;
	}
	set_irq_type(ts->irq, IRQ_TYPE_EDGE_FALLING);


	/* *** Set up the input subsystem stuff *** */
	// memset(ts->input, 0, sizeof(struct input_dev));
	ts->input = input_allocate_device();
	if (ts->input == NULL) {
		printk(KERN_NOTICE
		       "magician_ts: unable to allocation touchscreen input\n");
		free_irq(ts->irq, ts);
		kfree(ts);
		return -ENOMEM;
	}
	ts->input->evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
	ts->input->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);
	ts->input->keybit[LONG(BTN_TOUCH)] |= BIT(BTN_TOUCH);
	ts->input->absmin[ABS_X] = 0;
	ts->input->absmax[ABS_X] = 32767;
	ts->input->absmin[ABS_Y] = 0;
	ts->input->absmax[ABS_Y] = 32767;
	ts->input->absmin[ABS_PRESSURE] = 0;
	ts->input->absmax[ABS_PRESSURE] = 1;

	ts->input->name = "magician_ts";
	ts->input->private = ts;

	input_register_device(ts->input);

	ts->timer.function = ts_timer_callback;
	ts->timer.data = (unsigned long)ts;
	ts->state = STATE_WAIT_FOR_TOUCH;
	init_timer(&ts->timer);

	INIT_WORK(&ts->serial_work, start_read);

	platform_set_drvdata(dev, ts);

	/* *** Initialize the SSP interface *** */
	ssp_init();

	down(&serial_mutex);
	/* Make sure the device is in such a state that it can give pen
	 * interrupts. */
	while (!(SSSR_P2 & (1 << 2))) ;
	SSDR_P2 = 0xd00000;

	for (retval = 0; retval < 100; retval++) {
		if (SSSR_P2 & (1 << 3)) {
			while (SSSR_P2 & (1 << 3)) {
				(void)SSDR_P2;
			}
			break;
		}
		mdelay(1);
	}
	up(&serial_mutex);

	return 0;
}

static int ts_remove(struct platform_device *dev)
{
	struct touchscreen_data *ts = platform_get_drvdata(dev);

	input_unregister_device(ts->input);
	del_timer_sync(&ts->timer);
	free_irq(ts->irq, ts);

	kfree(ts);
	pxa_set_cken(CKEN3_SSP2, 0);
	return 0;
}

static int ts_suspend(struct platform_device *dev, pm_message_t state)
{
	disable_irq(gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ));
	return 0;
}

static int ts_resume(struct platform_device *dev)
{
	struct touchscreen_data *ts = platform_get_drvdata(dev);

	ts->state = STATE_WAIT_FOR_TOUCH;
	ssp_init();
	enable_irq(gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ));

	return 0;
}

static struct platform_driver ts_driver = {
	.probe = ts_probe,
	.remove = ts_remove,
	.suspend = ts_suspend,
	.resume = ts_resume,
	.driver = {
		.name = "magician-ts",
	},
};

static int tssim_init(void);

static int ts_module_init(void)
{
	printk(KERN_NOTICE "HTC Magician Touch Screen Driver\n");
	if (tssim_init()) {
		printk(KERN_NOTICE "  TS Simulator Not Installed\n");
	} else {
		printk(KERN_NOTICE "  TS Simulator Installed\n");
	}
	return platform_driver_register(&ts_driver);
}

static void tssim_exit(void);

static void ts_module_cleanup(void)
{
	tssim_exit();
	platform_driver_unregister(&ts_driver);
}

/************* Code for Touch Screen Simulation for FBVNC Server **********/
static dev_t dev;
static struct cdev *cdev;

static long a0 = -1122, a2 = 33588528, a4 = 1452, a5 = -2970720, a6 = 65536;

/* The input into the input subsystem is prior to correction from calibration.
 * So we have to undo the effects of the calibration.  It's actually a
 * complicated equation where the calibrated value of X depends on the
 * uncalibrated values of X and Y.  Fortunately, at least on the magician, the
 * multiplier for the Y value is zero, so I assume that here.  It is a shame
 * that the tslib does not allow multiple inputs.  Then we could do another
 * driver for this (as it was originally) that give input that does not
 * require calibration.
 */
static int
tssim_ioctl(struct inode *inode, struct file *fp, unsigned int ioctlnum,
	    unsigned long parm)
{
	switch (ioctlnum) {
	case 0:
		a0 = parm;
		break;
	case 1:
		break;
	case 2:
		a2 = parm;
		break;
	case 3:
		break;
	case 4:
		a4 = parm;
		break;
	case 5:
		a5 = parm;
		break;
	case 6:
		a6 = parm;
		printk(KERN_DEBUG
		       "a0 = %ld, a2 = %ld, a4 = %ld, a5 = %ld, a6 = %ld\n", a0,
		       a2, a4, a5, a6);
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static int tssim_open(struct inode *inode, struct file *fp)
{
	/* Nothing to do here */
	return 0;
}

static ssize_t
tssim_write(struct file *fp, const char __user * data, size_t bytes,
	    loff_t * offset)
{
	unsigned long pressure;
	long y;
	long x;

	x = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	data += 4;
	y = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	data += 4;
	pressure = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	data += 4;

	input_report_abs(ts_data->input, ABS_PRESSURE, pressure ? 1 : 0);
	input_report_abs(ts_data->input, ABS_X, ((x * a6) - a2) / a0);
	input_report_abs(ts_data->input, ABS_Y, ((y * a6) - a5) / a4);
	input_sync(ts_data->input);

	return bytes;
}

int tssim_close(struct inode *inode, struct file *fp)
{
	return 0;
}

struct file_operations fops = {
	THIS_MODULE,
	.write = tssim_write,
	.open = tssim_open,
	.release = tssim_close,
	.ioctl = tssim_ioctl,
};

#ifdef CONFIG_POWER_SUPPLY
static int battery_registered;

static int tsc2046_single_readout(int cmd)
{
	int sample = -1;

	if (!down_interruptible(&serial_mutex)) {
		int i;
		int ssrval;

		while (!(SSSR_P2 & (1 << 2))) ;
		SSDR_P2 = (cmd & 0xff) << 16;
		while (!(SSSR_P2 & (1 << 2))) ;
		SSDR_P2 = (cmd & 0xff) << 16;
		while (!(SSSR_P2 & (1 << 2))) ;
		SSDR_P2 = 0xd00000;	/* Dummy command to allow pen interrupts again */

		for (i = 0; i < 10; i++) {
			ssrval = SSSR_P2;
			if (ssrval & (1 << 3)) {	/* Look at Rx Not Empty bit */
				int number_of_entries_in_fifo;

				number_of_entries_in_fifo =
				    ((ssrval >> 12) & 0xf) + 1;
				if (number_of_entries_in_fifo == 3) {
					break;
				}
			}
			msleep(1);
		}

		if (i < 1000) {
			(void)SSDR_P2;
			sample = SSDR_P2 & 0xfff;
			(void)SSDR_P2;
		} else {
			/* Make sure the FIFO is empty */
			while (SSSR_P2 & (1 << 3)) {
				(void)SSDR_P2;
			}
			sample = -1;
		}
		up(&serial_mutex);
	}

	return sample;

}

static int get_capacity(struct power_supply *b)
{
//	return 100;
	static int auxin_sample;
	static int vbat_sample;
	static int temp0_sample;
	static int temp1_sample;

	// S A2 A1 A0  MODE SER/DFR PD1 PD0
	auxin_sample = tsc2046_single_readout(0xe7); // e is AUX_IN 1(110)
	vbat_sample = tsc2046_single_readout(0xa7); // a is V_BAT 1(010)
	temp0_sample = tsc2046_single_readout(0x87); // 8 is TEMP0 1(000)
	temp1_sample = tsc2046_single_readout(0xf7); // f is TEMP1 1(111) ser/dfr high

	printk("AUXIN: %d\n", auxin_sample);
	printk("VBAT: %d\n", vbat_sample);
	printk("TEMP0: %d\n", temp0_sample);
	printk("TEMP1: %d\n", temp1_sample);

	return 100;
}

static int get_property(struct power_supply *b,
                        enum power_supply_property psp,
                        union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 1400000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 1000000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = get_capacity(b);
		break; 
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = tsc2046_single_readout(0xe7); // e is AUX 1(110)
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING; /* check GPIO30 here? */
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static enum power_supply_property props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_STATUS,
};

static struct power_supply magician_power = {
	.name = "backup-battery",
	.get_property = get_property,
	.properties = props,
	.num_properties = ARRAY_SIZE(props),
};
#endif

static int tssim_init(void)
{
	int retval;

	retval = alloc_chrdev_region(&dev, 0, 1, "tssim");
	if (retval) {
		printk(KERN_ERR "TSSIM Unable to allocate device numbers\n");
		return retval;
	}

	cdev = cdev_alloc();
	cdev->owner = THIS_MODULE;
	cdev->ops = &fops;
	retval = cdev_add(cdev, dev, 1);
	if (retval) {
		printk(KERN_ERR "Unable to add cdev\n");
		unregister_chrdev_region(dev, 1);
		return retval;
	}

#ifdef CONFIG_POWER_SUPPLY
	battery_registered = 0;
	if (power_supply_register(NULL, &magician_power)) {
		printk(KERN_ERR
		       "magician_ts: Could not register battery class\n");
	} else {
		battery_registered = 1;
	}
#endif

	return 0;
}

static void tssim_exit(void)
{
	cdev_del(cdev);
	unregister_chrdev_region(dev, 1);
#ifdef CONFIG_POWER_SUPPLY
	if (battery_registered) {
		power_supply_unregister(&magician_power);
	}
#endif
	return;
}

module_init(ts_module_init);
module_exit(ts_module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aric Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Magician Touch Screen Driver");
