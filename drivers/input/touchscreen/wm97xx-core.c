/*
 * wm97xx-core.c  --  Touch screen driver core for Wolfson WM9705, WM9712
 *                    and WM9713 AC97 Codecs.
 *
 * Copyright 2003, 2004, 2005, 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 * Parts Copyright : Ian Molton <spyro@f2s.com>
 *                   Andrew Zabolotny <zap@homelink.ru>
 *                   Russell King <rmk@arm.linux.org.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * Notes:
 *
 *  Features:
 *       - supports WM9705, WM9712, WM9713
 *       - polling mode
 *       - continuous mode (arch-dependent)
 *       - adjustable rpu/dpp settings
 *       - adjustable pressure current
 *       - adjustable sample settle delay
 *       - 4 and 5 wire touchscreens (5 wire is WM9712 only)
 *       - pen down detection
 *       - battery monitor
 *       - sample AUX adc's
 *       - power management
 *       - codec GPIO
 *       - codec event notification
 * Todo
 *       - Support for async sampling control for noisy LCD's.
 *
 *  Revision history
 *    7th May 2003   Initial version.
 *    6th June 2003  Added non module support and AC97 registration.
 *   18th June 2003  Added AUX adc sampling.
 *   23rd June 2003  Did some minimal reformatting, fixed a couple of
 *                   codec_mutexing bugs and noted a race to fix.
 *   24th June 2003  Added power management and fixed race condition.
 *   10th July 2003  Changed to a misc device.
 *   31st July 2003  Moved TS_EVENT and TS_CAL to wm97xx.h
 *    8th Aug  2003  Added option for read() calling wm97xx_sample_touch()
 *                   because some ac97_read/ac_97_write call schedule()
 *    7th Nov  2003  Added Input touch event interface, stanley.cai@intel.com
 *   13th Nov  2003  Removed h3600 touch interface, added interrupt based
 *                   pen down notification and implemented continous mode
 *                   on XScale arch.
 *   16th Nov  2003  Ian Molton <spyro@f2s.com>
 *                   Modified so that it suits the new 2.6 driver model.
 *   25th Jan  2004  Andrew Zabolotny <zap@homelink.ru>
 *                   Implemented IRQ-driven pen down detection, implemented
 *                   the private API meant to be exposed to platform-specific
 *                   drivers, reorganized the driver so that it supports
 *                   an arbitrary number of devices.
 *    1st Feb  2004  Moved continuous mode handling to a separate
 *                   architecture-dependent file. For now only PXA
 *                   built-in AC97 controller is supported (pxa-ac97-wm97xx.c).
 *    11th Feb 2004  Reduced CPU usage by keeping a cached copy of both
 *                   digitizer registers instead of reading them every time.
 *                   A reorganization of the whole code for better
 *                   error handling.
 *    17th Apr 2004  Added BMON support.
 *    17th Nov 2004  Added codec GPIO, codec event handling (real and virtual
 *                   GPIOs) and 2.6 power management.
 *    29th Nov 2004  Added WM9713 support.
 *     4th Jul 2005  Moved codec specific code out to seperate files.
 *     6th Sep 2006  Mike Arthur <linux@wolfsonmicro.com>
 *                   Added bus interface.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wm97xx.h>
#include <linux/freezer.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TS_NAME			"wm97xx"
#define WM_CORE_VERSION		"0.64"
#define DEFAULT_PRESSURE	0xb0c0


/*
 * Touchscreen absolute values
 *
 * These parameters are used to help the input layer discard out of
 * range readings and reduce jitter etc.
 *
 *   o min, max:- indicate the min and max values your touch screen returns
 *   o fuzz:- use a higher number to reduce jitter
 *
 * The default values correspond to Mainstone II in QVGA mode
 *
 * Please read
 * Documentation/input/input-programming.txt for more details.
 */

static int abs_x[3] = {350,3900,5};
module_param_array(abs_x, int, NULL, 0);
MODULE_PARM_DESC(abs_x, "Touchscreen absolute X min, max, fuzz");

static int abs_y[3] = {320,3750,40};
module_param_array(abs_y, int, NULL, 0);
MODULE_PARM_DESC(abs_y, "Touchscreen absolute Y min, max, fuzz");

static int abs_p[3] = {0,150,4};
module_param_array(abs_p, int, NULL, 0);
MODULE_PARM_DESC(abs_p, "Touchscreen absolute Pressure min, max, fuzz");

/*
 * Debug
 */
#if 0
#define dbg(format, arg...) printk(KERN_DEBUG TS_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...)
#endif
#define err(format, arg...) printk(KERN_ERR TS_NAME ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO TS_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING TS_NAME ": " format "\n" , ## arg)

/*
 * wm97xx IO access, all IO locking done by AC97 layer
 */
int wm97xx_reg_read(struct wm97xx *wm, u16 reg)
{
	if (wm->ac97)
		return wm->ac97->bus->ops->read(wm->ac97, reg);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(wm97xx_reg_read);

void wm97xx_reg_write(struct wm97xx *wm, u16 reg, u16 val)
{
	/* cache digitiser registers */
	if(reg >= AC97_WM9713_DIG1 && reg <= AC97_WM9713_DIG3)
		wm->dig[(reg - AC97_WM9713_DIG1) >> 1] = val;

	/* cache gpio regs */
	if(reg >= AC97_GPIO_CFG && reg <= AC97_MISC_AFE)
		wm->gpio[(reg - AC97_GPIO_CFG) >> 1] = val;

	/* wm9713 irq reg */
	if(reg == 0x5a)
		wm->misc = val;

	if (wm->ac97)
		wm->ac97->bus->ops->write(wm->ac97, reg, val);
}
EXPORT_SYMBOL_GPL(wm97xx_reg_write);

/**
 *	wm97xx_read_aux_adc - Read the aux adc.
 *	@wm: wm97xx device.
 *  @adcsel: codec ADC to be read
 *
 *	Reads the selected AUX ADC.
 */

int wm97xx_read_aux_adc(struct wm97xx *wm, u16 adcsel)
{
	int power_adc = 0, auxval;
	u16 power = 0;

	/* get codec */
	mutex_lock(&wm->codec_mutex);

	/* When the touchscreen is not in use, we may have to power up the AUX ADC
	 * before we can use sample the AUX inputs->
	 */
	if (wm->id == WM9713_ID2 &&
	    (power = wm97xx_reg_read(wm, AC97_EXTENDED_MID)) & 0x8000) {
		power_adc = 1;
		wm97xx_reg_write(wm, AC97_EXTENDED_MID, power & 0x7fff);
	}

	/* Prepare the codec for AUX reading */
	wm->codec->digitiser_ioctl(wm, WM97XX_AUX_PREPARE);

	/* Turn polling mode on to read AUX ADC */
	wm->pen_probably_down = 1;
	wm->codec->poll_sample(wm, adcsel, &auxval);

	if (power_adc)
		wm97xx_reg_write(wm, AC97_EXTENDED_MID, power | 0x8000);

	wm->codec->digitiser_ioctl(wm, WM97XX_DIG_RESTORE);

	wm->pen_probably_down = 0;

	mutex_unlock(&wm->codec_mutex);
	return auxval & 0xfff;
}
EXPORT_SYMBOL_GPL(wm97xx_read_aux_adc);

/**
 *	wm97xx_get_gpio - Get the status of a codec GPIO.
 *	@wm: wm97xx device.
 *  @gpio: gpio
 *
 *	Get the status of a codec GPIO pin
 */

wm97xx_gpio_status_t wm97xx_get_gpio(struct wm97xx *wm, u32 gpio)
{
	u16 status;
	wm97xx_gpio_status_t ret;

	mutex_lock(&wm->codec_mutex);
	status = wm97xx_reg_read(wm, AC97_GPIO_STATUS);

	if (status & gpio)
		ret = WM97XX_GPIO_HIGH;
	else
		ret = WM97XX_GPIO_LOW;

	mutex_unlock(&wm->codec_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(wm97xx_get_gpio);

/**
 *	wm97xx_set_gpio - Set the status of a codec GPIO.
 *	@wm: wm97xx device.
 *  @gpio: gpio
 *
 *
 *	Set the status of a codec GPIO pin
 */

void wm97xx_set_gpio(struct wm97xx *wm, u32 gpio,
				wm97xx_gpio_status_t status)
{
	u16 reg;

	mutex_lock(&wm->codec_mutex);
	reg = wm97xx_reg_read(wm, AC97_GPIO_STATUS);

	if (status & WM97XX_GPIO_HIGH)
		reg |= gpio;
	else
		reg &= ~gpio;

	if (wm->id == WM9712_ID2)
		wm97xx_reg_write(wm, AC97_GPIO_STATUS, reg << 1);
	else
		wm97xx_reg_write(wm, AC97_GPIO_STATUS, reg);
	mutex_unlock(&wm->codec_mutex);
}
EXPORT_SYMBOL_GPL(wm97xx_set_gpio);

/*
 * Codec GPIO pin configuration, this set's pin direction, polarity,
 * stickyness and wake up.
 */
void wm97xx_config_gpio(struct wm97xx *wm, u32 gpio, wm97xx_gpio_dir_t dir,
		   wm97xx_gpio_pol_t pol, wm97xx_gpio_sticky_t sticky,
		   wm97xx_gpio_wake_t wake)
{
	u16 reg;

	mutex_lock(&wm->codec_mutex);
	reg = wm97xx_reg_read(wm, AC97_GPIO_POLARITY);

	if (pol == WM97XX_GPIO_POL_HIGH)
		reg |= gpio;
	else
		reg &= ~gpio;

	wm97xx_reg_write(wm, AC97_GPIO_POLARITY, reg);
	reg = wm97xx_reg_read(wm, AC97_GPIO_STICKY);

	if (sticky == WM97XX_GPIO_STICKY)
		reg |= gpio;
	else
		reg &= ~gpio;

	wm97xx_reg_write(wm, AC97_GPIO_STICKY, reg);
	reg = wm97xx_reg_read(wm, AC97_GPIO_WAKEUP);

	if (wake == WM97XX_GPIO_WAKE)
		reg |= gpio;
	else
		reg &= ~gpio;

	wm97xx_reg_write(wm, AC97_GPIO_WAKEUP, reg);
	reg = wm97xx_reg_read(wm, AC97_GPIO_CFG);

	if (dir == WM97XX_GPIO_IN)
		reg |= gpio;
	else
		reg &= ~gpio;

	wm97xx_reg_write(wm, AC97_GPIO_CFG, reg);
	mutex_unlock(&wm->codec_mutex);
}
EXPORT_SYMBOL_GPL(wm97xx_config_gpio);

/*
 * Handle a pen down interrupt.
 */
static void wm97xx_pen_irq_worker(struct work_struct *work)
{
	struct wm97xx *wm = container_of(work, struct wm97xx, pen_event_work);

	/* do we need to enable the touch panel reader */
	if (wm->id == WM9705_ID2) {
		if (wm97xx_reg_read(wm, AC97_WM97XX_DIGITISER_RD) & WM97XX_PEN_DOWN)
			wm->pen_is_down = 1;
		else
			wm->pen_is_down = 0;
		wake_up_interruptible(&wm->pen_irq_wait);
	} else {
		u16 status, pol;
		mutex_lock(&wm->codec_mutex);
		status = wm97xx_reg_read(wm, AC97_GPIO_STATUS);
		pol = wm97xx_reg_read(wm, AC97_GPIO_POLARITY);

		if (WM97XX_GPIO_13 & pol & status) {
			wm->pen_is_down = 1;
			wm97xx_reg_write(wm, AC97_GPIO_POLARITY, pol & ~WM97XX_GPIO_13);
		} else {
			wm->pen_is_down = 0;
		    wm97xx_reg_write(wm, AC97_GPIO_POLARITY, pol | WM97XX_GPIO_13);
		}

		if (wm->id == WM9712_ID2)
			wm97xx_reg_write(wm, AC97_GPIO_STATUS, (status & ~WM97XX_GPIO_13) << 1);
		else
			wm97xx_reg_write(wm, AC97_GPIO_STATUS, status & ~WM97XX_GPIO_13);
		mutex_unlock(&wm->codec_mutex);
		wake_up_interruptible(&wm->pen_irq_wait);
	}

	if (!wm->pen_is_down && wm->mach_ops && wm->mach_ops->acc_enabled)
		wm->mach_ops->acc_pen_up(wm);
	enable_irq(wm->pen_irq);
}

/*
 * Codec PENDOWN irq handler
 *
 * We have to disable the codec interrupt in the handler because it can
 * take upto 1ms to clear the interrupt source. The interrupt is then enabled
 * again in the slow handler when the source has been cleared.
 */
static irqreturn_t wm97xx_pen_interrupt(int irq, void *dev_id)
{
	struct wm97xx *wm = (struct wm97xx *) dev_id;
	disable_irq(wm->pen_irq);
	queue_work(wm->pen_irq_workq, &wm->pen_event_work);
	return IRQ_HANDLED;
}

/*
 * initialise pen IRQ handler and workqueue
 */
static int wm97xx_init_pen_irq(struct wm97xx *wm)
{
	u16 reg;

	INIT_WORK(&wm->pen_event_work, wm97xx_pen_irq_worker);
	if ((wm->pen_irq_workq =
		create_singlethread_workqueue("kwm97pen")) == NULL) {
		err("could not create pen irq work queue");
		wm->pen_irq = 0;
		return -EINVAL;
	}

	if (request_irq (wm->pen_irq, wm97xx_pen_interrupt, SA_SHIRQ, "wm97xx-pen", wm)) {
		err("could not register codec pen down interrupt, will poll for pen down");
		destroy_workqueue(wm->pen_irq_workq);
		wm->pen_irq = 0;
		return -EINVAL;
	}

	/* enable PEN down on wm9712/13 */
	if (wm->id != WM9705_ID2) {
		reg = wm97xx_reg_read(wm, AC97_MISC_AFE);
		wm97xx_reg_write(wm, AC97_MISC_AFE, reg & 0xfffb);
		reg = wm97xx_reg_read(wm, 0x5a);
		wm97xx_reg_write(wm, 0x5a, reg & ~0x0001);
	}

	return 0;
}

/* Private struct for communication between struct wm97xx_tshread
 * and wm97xx_read_samples */
struct ts_state {
	int sleep_time;
	int min_sleep_time;
};

static int wm97xx_read_samples(struct wm97xx *wm, struct ts_state *state)
{
	struct wm97xx_data data;
	int rc;

	mutex_lock(&wm->codec_mutex);

    if (wm->mach_ops && wm->mach_ops->acc_enabled)
	   rc = wm->mach_ops->acc_pen_down(wm);
    else
        rc = wm->codec->poll_touch(wm, &data);

	if (rc & RC_PENUP) {
		if (wm->pen_is_down) {
			wm->pen_is_down = 0;
			dbg("pen up");
			input_report_abs(wm->input_dev, ABS_PRESSURE, 0);
			input_sync(wm->input_dev);
		} else if (!(rc & RC_AGAIN)) {
			/* We need high frequency updates only while pen is down,
			* the user never will be able to touch screen faster than
			* a few times per second... On the other hand, when the
			* user is actively working with the touchscreen we don't
			* want to lose the quick response. So we will slowly
			* increase sleep time after the pen is up and quicky
			* restore it to ~one task switch when pen is down again.
			*/
			if (state->sleep_time < HZ / 10)
				state->sleep_time++;
		}

	} else if (rc & RC_VALID) {
		dbg("pen down: x=%x:%d, y=%x:%d, pressure=%x:%d\n",
			data.x >> 12, data.x & 0xfff, data.y >> 12,
			data.y & 0xfff, data.p >> 12, data.p & 0xfff);
		input_report_abs(wm->input_dev, ABS_X, data.x & 0xfff);
		input_report_abs(wm->input_dev, ABS_Y, data.y & 0xfff);
		input_report_abs(wm->input_dev, ABS_PRESSURE, data.p & 0xfff);
		input_sync(wm->input_dev);
		wm->pen_is_down = 1;
		state->sleep_time = state->min_sleep_time;
	} else if (rc & RC_PENDOWN) {
		dbg("pen down");
		wm->pen_is_down = 1;
		state->sleep_time = state->min_sleep_time;
	}

	mutex_unlock(&wm->codec_mutex);
	return rc;
}

/*
* The touchscreen sample reader thread.
*/
static int wm97xx_ts_read(void *data)
{
	int rc;
	struct ts_state state;
	struct wm97xx *wm = (struct wm97xx *) data;

	/* set up thread context */
	wm->ts_task = current;
	daemonize("kwm97xxts");

	if (wm->codec == NULL) {
		wm->ts_task = NULL;
		printk(KERN_ERR "codec is NULL, bailing\n");
	}

	complete(&wm->ts_init);
	wm->pen_is_down = 0;
	state.min_sleep_time = HZ >= 100 ? HZ / 100 : 1;
	if (state.min_sleep_time < 1)
		state.min_sleep_time = 1;
	state.sleep_time = state.min_sleep_time;

	/* touch reader loop */
	while (wm->ts_task) {
		do {
			try_to_freeze();
			rc = wm97xx_read_samples(wm, &state);
		} while (rc & RC_AGAIN);
		if (!wm->pen_is_down && wm->pen_irq) {
			/* Nice, we don't have to poll for pen down event */
			wait_event_interruptible(wm->pen_irq_wait, wm->pen_is_down);
		} else {
			set_task_state(current, TASK_INTERRUPTIBLE);
			schedule_timeout(state.sleep_time);
		}
	}
	complete_and_exit(&wm->ts_exit, 0);
}

/**
 *	wm97xx_ts_input_open - Open the touch screen input device.
 *	@idev:	Input device to be opened.
 *
 *	Called by the input sub system to open a wm97xx touchscreen device.
 *  Starts the touchscreen thread and touch digitiser.
 */
static int wm97xx_ts_input_open(struct input_dev *idev)
{
	int ret = 0;
	struct wm97xx *wm = (struct wm97xx *) idev->private;

	mutex_lock(&wm->codec_mutex);
	/* first time opened ? */
	if (wm->ts_use_count++ == 0) {
		/* start touchscreen thread */
		init_completion(&wm->ts_init);
		init_completion(&wm->ts_exit);
		ret = kernel_thread(wm97xx_ts_read, wm, CLONE_KERNEL);

		if (ret >= 0) {
			wait_for_completion(&wm->ts_init);
			if (wm->ts_task == NULL)
				ret = -EINVAL;
		} else {
			mutex_unlock(&wm->codec_mutex);
			return ret;
		}

		/* start digitiser */
        if (wm->mach_ops && wm->mach_ops->acc_enabled)
            wm->codec->acc_enable(wm, 1);
		wm->codec->digitiser_ioctl(wm, WM97XX_DIG_START);

		/* init pen down/up irq handling */
		if (wm->pen_irq) {
			wm97xx_init_pen_irq(wm);

			if (wm->pen_irq == 0) {
				/* we failed to get an irq for pen down events,
				 * so we resort to polling. kickstart the reader */
				wm->pen_is_down = 1;
				wake_up_interruptible(&wm->pen_irq_wait);
			}
		}
	}

	mutex_unlock(&wm->codec_mutex);
	return 0;
}

/**
 *	wm97xx_ts_input_close - Close the touch screen input device.
 *	@idev:	Input device to be closed.
 *
 *	Called by the input sub system to close a wm97xx touchscreen device.
 *  Kills the touchscreen thread and stops the touch digitiser.
 */

static void wm97xx_ts_input_close(struct input_dev *idev)
{
	struct wm97xx *wm = (struct wm97xx *) idev->private;

	mutex_lock(&wm->codec_mutex);
	if (--wm->ts_use_count == 0) {
		/* destroy workqueues and free irqs */
		if (wm->pen_irq) {
			free_irq(wm->pen_irq, wm);
			destroy_workqueue(wm->pen_irq_workq);
		}

		/* kill thread */
		if (wm->ts_task) {
			wm->ts_task = NULL;
			wm->pen_is_down = 1;
			mutex_unlock(&wm->codec_mutex);
			wake_up_interruptible(&wm->pen_irq_wait);
			wait_for_completion(&wm->ts_exit);
			wm->pen_is_down = 0;
			mutex_lock(&wm->codec_mutex);
		}

		/* stop digitiser */
		wm->codec->digitiser_ioctl(wm, WM97XX_DIG_STOP);
		if (wm->mach_ops && wm->mach_ops->acc_enabled)
			wm->codec->acc_enable(wm, 0);
	}
	mutex_unlock(&wm->codec_mutex);
}

/*
 * Bus interface to allow for client drivers codec access
 * e.g. battery monitor
 */
static int wm97xx_bus_match(struct device *dev, struct device_driver *drv)
{
    return !(strcmp(dev->bus_id,drv->name));
}

/*
 * The AC97 audio driver will do all the Codec suspend and resume
 * tasks. This is just for anything machine specific or extra.
 */
static int wm97xx_bus_suspend(struct device *dev, pm_message_t state)
{
    int ret = 0;

    if (dev->driver && dev->driver->suspend)
        ret = dev->driver->suspend(dev, state);

    return ret;
}

static int wm97xx_bus_resume(struct device *dev)
{
    int ret = 0;

    if (dev->driver && dev->driver->resume)
        ret = dev->driver->resume(dev);

    return ret;
}

struct bus_type wm97xx_bus_type = {
    .name       = "wm97xx",
    .match      = wm97xx_bus_match,
    .suspend    = wm97xx_bus_suspend,
    .resume     = wm97xx_bus_resume,
};
EXPORT_SYMBOL_GPL(wm97xx_bus_type);

static void  wm97xx_release(struct device *dev)
{
    kfree(dev);
}

static int wm97xx_probe(struct device *dev)
{
	struct wm97xx* wm;
	int ret = 0, id = 0;

	if (!(wm = kzalloc(sizeof(struct wm97xx), GFP_KERNEL)))
		return -ENOMEM;
    mutex_init(&wm->codec_mutex);

	init_waitqueue_head(&wm->pen_irq_wait);
	wm->dev = dev;
	dev->driver_data = wm;
	wm->ac97 = to_ac97_t(dev);

	/* check that we have a supported codec */
	if ((id = wm97xx_reg_read(wm, AC97_VENDOR_ID1)) != WM97XX_ID1) {
		err("could not find a wm97xx, found a %x instead\n", id);
		kfree(wm);
		return -ENODEV;
	}

	wm->id = wm97xx_reg_read(wm, AC97_VENDOR_ID2);
	if(wm->id != wm97xx_codec.id) {
		err("could not find a the selected codec, please build for wm97%2x", wm->id & 0xff);
		kfree(wm);
		return -ENODEV;
	}

	if((wm->input_dev = input_allocate_device()) == NULL) {
        kfree(wm);
		return -ENOMEM;
    }

	/* set up touch configuration */
	info("detected a wm97%2x codec", wm->id & 0xff);
	wm->input_dev->name = "wm97xx touchscreen";
	wm->input_dev->open = wm97xx_ts_input_open;
	wm->input_dev->close = wm97xx_ts_input_close;
	set_bit(EV_ABS, wm->input_dev->evbit);
	set_bit(ABS_X, wm->input_dev->absbit);
	set_bit(ABS_Y, wm->input_dev->absbit);
	set_bit(ABS_PRESSURE, wm->input_dev->absbit);
	wm->input_dev->absmax[ABS_X] = abs_x[1];
	wm->input_dev->absmax[ABS_Y] = abs_y[1];
	wm->input_dev->absmax[ABS_PRESSURE] = abs_p[1];
	wm->input_dev->absmin[ABS_X] = abs_x[0];
	wm->input_dev->absmin[ABS_Y] = abs_y[0];
	wm->input_dev->absmin[ABS_PRESSURE] = abs_p[0];
	wm->input_dev->absfuzz[ABS_X] = abs_x[2];
	wm->input_dev->absfuzz[ABS_Y] = abs_y[2];
	wm->input_dev->absfuzz[ABS_PRESSURE] = abs_p[2];
	wm->input_dev->private = wm;
	wm->codec = &wm97xx_codec;
	if((ret = input_register_device(wm->input_dev)) < 0) {
		kfree(wm);
		return -ENOMEM;
    }

	/* set up physical characteristics */
	wm->codec->digitiser_ioctl(wm, WM97XX_PHY_INIT);

	/* load gpio cache */
	wm->gpio[0] = wm97xx_reg_read(wm, AC97_GPIO_CFG);
	wm->gpio[1] = wm97xx_reg_read(wm, AC97_GPIO_POLARITY);
	wm->gpio[2] = wm97xx_reg_read(wm, AC97_GPIO_STICKY);
	wm->gpio[3] = wm97xx_reg_read(wm, AC97_GPIO_WAKEUP);
	wm->gpio[4] = wm97xx_reg_read(wm, AC97_GPIO_STATUS);
	wm->gpio[5] = wm97xx_reg_read(wm, AC97_MISC_AFE);

	/* register our battery device */
    if (!(wm->battery_dev = kzalloc(sizeof(struct device), GFP_KERNEL))) {
    	ret = -ENOMEM;
        goto batt_err;
    }
    wm->battery_dev->bus = &wm97xx_bus_type;
    strcpy(wm->battery_dev->bus_id,"wm97xx-battery");
    wm->battery_dev->driver_data = wm;
    wm->battery_dev->parent = dev;
    wm->battery_dev->release = wm97xx_release;
    if((ret = device_register(wm->battery_dev)) < 0)
    	goto batt_reg_err;

	/* register our extended touch device (for machine specific extensions) */
    if (!(wm->touch_dev = kzalloc(sizeof(struct device), GFP_KERNEL))) {
    	ret = -ENOMEM;
        goto touch_err;
    }
    wm->touch_dev->bus = &wm97xx_bus_type;
    strcpy(wm->touch_dev->bus_id,"wm97xx-touchscreen");
    wm->touch_dev->driver_data = wm;
    wm->touch_dev->parent = dev;
    wm->touch_dev->release = wm97xx_release;
    if((ret = device_register(wm->touch_dev)) < 0)
    	goto touch_reg_err;

    return ret;

touch_reg_err:
	kfree(wm->touch_dev);
touch_err:
    device_unregister(wm->battery_dev);
batt_reg_err:
	kfree(wm->battery_dev);
batt_err:
    input_unregister_device(wm->input_dev);
    kfree(wm);
    return ret;
}

static int wm97xx_remove(struct device *dev)
{
	struct wm97xx *wm = dev_get_drvdata(dev);

	/* Stop touch reader thread */
	if (wm->ts_task) {
		wm->ts_task = NULL;
		wm->pen_is_down = 1;
		wake_up_interruptible(&wm->pen_irq_wait);
		wait_for_completion(&wm->ts_exit);
	}
	device_unregister(wm->battery_dev);
	device_unregister(wm->touch_dev);
    input_unregister_device(wm->input_dev);

	kfree(wm);
	return 0;
}

#ifdef CONFIG_PM
int wm97xx_resume(struct device* dev)
{
	struct wm97xx *wm = dev_get_drvdata(dev);

	/* restore digitiser and gpio's */
	if(wm->id == WM9713_ID2) {
		wm97xx_reg_write(wm, AC97_WM9713_DIG1, wm->dig[0]);
		wm97xx_reg_write(wm, 0x5a, wm->misc);
		if(wm->ts_use_count) {
			u16 reg = wm97xx_reg_read(wm, AC97_EXTENDED_MID) & 0x7fff;
			wm97xx_reg_write(wm, AC97_EXTENDED_MID, reg);
		}
	}

	wm97xx_reg_write(wm, AC97_WM9713_DIG2, wm->dig[1]);
	wm97xx_reg_write(wm, AC97_WM9713_DIG3, wm->dig[2]);

	wm97xx_reg_write(wm, AC97_GPIO_CFG, wm->gpio[0]);
	wm97xx_reg_write(wm, AC97_GPIO_POLARITY, wm->gpio[1]);
	wm97xx_reg_write(wm, AC97_GPIO_STICKY, wm->gpio[2]);
	wm97xx_reg_write(wm, AC97_GPIO_WAKEUP, wm->gpio[3]);
	wm97xx_reg_write(wm, AC97_GPIO_STATUS, wm->gpio[4]);
	wm97xx_reg_write(wm, AC97_MISC_AFE, wm->gpio[5]);

	return 0;
}

#else
#define wm97xx_resume		NULL
#endif

/*
 * Machine specific operations
 */
int wm97xx_register_mach_ops(struct wm97xx *wm, struct wm97xx_mach_ops *mach_ops)
{
	mutex_lock(&wm->codec_mutex);
    if(wm->mach_ops) {
    	mutex_unlock(&wm->codec_mutex);
    	return -EINVAL;
    }
    wm->mach_ops = mach_ops;
    mutex_unlock(&wm->codec_mutex);
    return 0;
}
EXPORT_SYMBOL_GPL(wm97xx_register_mach_ops);

void wm97xx_unregister_mach_ops(struct wm97xx *wm)
{
	mutex_lock(&wm->codec_mutex);
    wm->mach_ops = NULL;
    mutex_unlock(&wm->codec_mutex);
}
EXPORT_SYMBOL_GPL(wm97xx_unregister_mach_ops);

static struct device_driver wm97xx_driver = {
	.name = 	"ac97",
	.bus = 		&ac97_bus_type,
	.owner = 	THIS_MODULE,
	.probe = 	wm97xx_probe,
	.remove = 	wm97xx_remove,
	.resume =	wm97xx_resume,
};

static int __init wm97xx_init(void)
{
	int ret;

	info("version %s liam.girdwood@wolfsonmicro.com", WM_CORE_VERSION);
    if((ret = bus_register(&wm97xx_bus_type)) < 0)
    	return ret;
	return driver_register(&wm97xx_driver);
}

static void __exit wm97xx_exit(void)
{
	driver_unregister(&wm97xx_driver);
    bus_unregister(&wm97xx_bus_type);
}

module_init(wm97xx_init);
module_exit(wm97xx_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("WM97xx Core - Touch Screen / AUX ADC / GPIO Driver");
MODULE_LICENSE("GPL");
