/*
 * ac97_plugin_wm97xx.c  --  Touch screen driver for Wolfson WM9705 and WM9712
 *                           AC97 Codecs.
 *
 * Copyright 2003 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Notes:
 *
 *  Features:
 *       - supports WM9705, WM9712
 *       - polling mode
 *       - coordinate polling
 *       - continuous mode (arch-dependent)
 *       - adjustable rpu/dpp settings
 *       - adjustable pressure current
 *       - adjustable sample settle delay
 *       - 4 and 5 wire touchscreens (5 wire is WM9712 only)
 *       - pen down detection
 *       - battery monitor
 *       - sample AUX adc's
 *       - power management
 *
 *  TODO:
 *       - adjustable sample rate
 *       - AUX adc in coordinate / continous modes
 *       - Codec GPIO
 *
 *  Revision history
 *    7th May 2003   Initial version.
 *    6th June 2003  Added non module support and AC97 registration.
 *   18th June 2003  Added AUX adc sampling.
 *   23rd June 2003  Did some minimal reformatting, fixed a couple of
 *     locking bugs and noted a race to fix.
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
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <asm/io.h>

#include "wm97xx.h"

#define TS_NAME			"wm97xx"
#define TS_MINOR		16
#define WM_TS_VERSION		"0.11"
#define AC97_NUM_REG		64
#define MAX_WM97XX_DEVICES	8

/*
 * Machine specific set up.
 *
 * This is for targets that can support a PEN down interrupt and/or
 * streaming back touch data in an AC97 slot (not slot 1). The
 * streaming touch data is read back via the targets AC97 FIFO's
 */
#if defined(CONFIG_ARCH_WMMX)
#include <asm/arch/wmmx-regs.h>
#define WM97XX_FIFO_HAS_DATA	(MISR & (1 << 2))
#define WM97XX_READ_FIFO	(MODR & (0xffff))
#endif

#ifndef WM97XX_FIFO_HAS_DATA
#define WM97XX_FIFO_HAS_DATA 0
#define WM97XX_READ_FIFO 0
#endif

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

#if defined(CONFIG_PROC_FS) && !defined(CONFIG_EMBEDDED)
#  define PROC_FS_SUPPORT
#endif

/*
 * Module parameters
 */

/*
 * Set the codec sample mode.
 *
 * The WM9712 can sample touchscreen data in 3 different operating
 * modes. i.e. polling, coordinate and continous.
 *
 * Polling:      The driver polls the codec and issues 3 seperate commands
 *               over the AC97 link to read X,Y and pressure.
 *
 * Coordinate:   The driver polls the codec and only issues 1 command over
 *               the AC97 link to read X,Y and pressure. This mode has
 *               strict timing requirements and may drop samples if
 *               interrupted. However, it is less demanding on the AC97
 *               link. Note: this mode requires a larger delay than polling
 *               mode.
 *
 * Continuous:   The codec automatically samples X,Y and pressure and then
 *               sends the data over the AC97 link in slots. This is the
 *               same method used by the codec when recording audio.
 *               This mode is not selectable by this parameter because
 *               it needs separate architecture-dependent support. The
 *               following drivers will add continuouse mode support:
 *
 *               pxa-ac97-wm97xx.c - for Intel PXA built-in AC97 controller.
 */
static int mode [MAX_WM97XX_DEVICES];
module_param_array(mode, int, NULL, 0);
MODULE_PARM_DESC(mode, "WM97XX operation mode (0:polling 1:coordinate)");

/*
 * WM9712 - Set internal pull up for pen detect.
 *
 * Pull up is in the range 1.02k (least sensitive) to 64k (most sensitive)
 * i.e. pull up resistance = 64k Ohms / rpu.
 *
 * Adjust this value if you are having problems with pen detect not
 * detecting any down events.
 */
static int rpu [MAX_WM97XX_DEVICES];
module_param_array(rpu, int, NULL, 0);
MODULE_PARM_DESC(rpu, "Set internal pull up resitor for pen detect.");

/*
 * WM9705 - Pen detect comparator threshold.
 *
 * 0 to Vmid in 15 steps, 0 = use zero power comparator with Vmid threshold
 * i.e. 1 =  Vmid/15 threshold
 *      15 =  Vmid/1 threshold
 *
 * Adjust this value if you are having problems with pen detect not
 * detecting any down events.
 */
static int pdd [MAX_WM97XX_DEVICES];
module_param_array(pdd, int, NULL, 0);
MODULE_PARM_DESC(pdd, "Set pen detect comparator threshold");

/*
 * Set current used for pressure measurement.
 *
 * Set pil = 2 to use 400uA
 *     pil = 1 to use 200uA and
 *     pil = 0 to disable pressure measurement.
 *
 * This is used to increase the range of values returned by the adc
 * when measureing touchpanel pressure.
 */
static int pil [MAX_WM97XX_DEVICES];
module_param_array(pil, int, NULL, 0);
MODULE_PARM_DESC(pil, "Set current used for pressure measurement.");

/*
 * WM9712 - Set five_wire = 1 to use a 5 wire touchscreen.
 *
 * NOTE: Five wire mode does not allow for readback of pressure.
 */
static int five_wire [MAX_WM97XX_DEVICES];
module_param_array(five_wire, int, NULL, 0);
MODULE_PARM_DESC(five_wire, "Set to '1' to use 5-wire touchscreen.");

/*
 * Set adc sample delay.
 *
 * For accurate touchpanel measurements, some settling time may be
 * required between the switch matrix applying a voltage across the
 * touchpanel plate and the ADC sampling the signal.
 *
 * This delay can be set by setting delay = n, where n is the array
 * position of the delay in the array delay_table below.
 * Long delays > 1ms are supported for completeness, but are not
 * recommended.
 */
static int delay [MAX_WM97XX_DEVICES] = {4,4,4,4,4,4,4,4};
module_param_array(delay, int, NULL, 0);
MODULE_PARM_DESC(delay, "Set adc sample delay.");

/*
 * Pen down detection interrupt number
 *
 * Pen down detection can either be via an interrupt (preferred) or
 * by polling the 15th bit of sampled data. This is an option because
 * some systems may not support the pen down interrupt.
 *
 * Set pen_irq to pen irq number to enable interrupt driven pen down detection.
 */
static int pen_irq [MAX_WM97XX_DEVICES];
module_param_array(pen_irq, int, NULL, 0);
MODULE_PARM_DESC(pen_irq, "Set the pen down IRQ number");

/*
 * Pen sampling frequency in continuous mode.
 *
 * This parameter needs an additional architecture-dependent driver
 * such as pxa-ac97-wm97xx.
 *
 * The sampling frequency is given by a number in range 0-3:
 *
 * 0: 93.75Hz  1: 187.5Hz  2: 375Hz  3: 750Hz
 */
static int cont_rate [MAX_WM97XX_DEVICES] = {3,3,3,3,3,3,3,3};
module_param_array(cont_rate, int, NULL, 0);
MODULE_PARM_DESC(cont_rate, "Sampling rate in continuous mode (0-7, default 2)");

/*
 * AC97 slot number for continuous mode (5-11).
 *
 * This parameter needs an additional architecture-dependent driver
 * such as pxa-ac97-wm97xx.
 */
static int cont_slot [MAX_WM97XX_DEVICES] = {5,5,5,5,5,5,5,5};
module_param_array(cont_slot, int, NULL, 0);
MODULE_PARM_DESC(cont_slot, "AC97 slot number for continuous mode (5-11, default 5)");

/*
 * Coordinate inversion control.
 *
 * On some hardware the coordinate system of the touchscreen and the
 * coordinate system of the graphics controller are different. A well-written
 * calibration tool won't depend on that; however for compatibility with
 * some old apps sometimes it is required to bring both coordinate
 * systems together. That's what this parameter is for.
 *
 * Bit 0: if 1, the X coordinate is inverted
 * Bit 1: if 1, the Y coordinate is inverted
 */
static int invert [MAX_WM97XX_DEVICES];
module_param_array(invert, int, NULL, 0);
MODULE_PARM_DESC(invert, "Set bit 0 to invert X coordinate, set bit 1 to invert Y coordinate");

static char rate_desc [4][6] =
{
	"93.75",
	"187.5",
	"375",
	"750"
};

static u16 wm97xx_adcsel [2][4] =
{
	/* WM9705 */
	{ WM9705_ADCSEL_BMON, WM9705_ADCSEL_AUX, WM9705_ADCSEL_PHONE, WM9705_ADCSEL_PCBEEP },
	/* WM9712 */
	{ WM9712_ADCSEL_BMON, WM9712_ADCSEL_COMP1, WM9712_ADCSEL_COMP2, WM9712_ADCSEL_WIPER },
};

typedef struct {
	unsigned adcsel;		/* ADC select mask; !=0 -> enabled */
	unsigned next_jiffies;		/* Next time mark to collect data at */
	unsigned period;		/* Interval in jiffies to collect data */
	wm97xx_ac_prepare func;		/* Before/after conversion callback */
	int value;			/* Last value sampled (-1 = no data yet) */
} wm97xx_auxconv_data;

typedef struct {
	u16 dig1, dig2;			/* Cached digitizer registers */
	spinlock_t lock;		/* Hardware lock */
	struct input_dev *idev;
	wm97xx_public pub;		/* Public structure exposed to clients */
	struct completion ts_comp;	/* Touchscreen thread completion */
	struct task_struct *ts_task;	/* Touchscreen thread task pointer */
	wm97xx_auxconv_data aux_conv [4];
	struct completion aux_comp;
	struct task_struct *aux_task;
	wait_queue_head_t aux_wait;	/* AUX pen-up wait queue */
	int irq;			/* IRQ number in use */
	volatile int irq_count;
	wait_queue_head_t irq_wait;	/* IRQ wait queue */
	int adc_errs;			/* sample read back errors */
	int use_count;
#if defined(PROC_FS_SUPPORT)
	struct proc_dir_entry *wm97xx_ts_ps;
#endif
	/* Bitfields grouped here to get some space gain */
	unsigned is_wm9712:1;		/* are we a WM9705/12 */
	unsigned pen_is_down:1;
	unsigned pen_probably_down:1;	/* used in polling and coordinate modes */
	unsigned invert_x:1;
	unsigned invert_y:1;
	unsigned aux_waiting:1;
#if defined(CONFIG_PM)
	unsigned phone_pga:5;
	unsigned line_pga:16;
	unsigned mic_pga:16;
#endif
} wm97xx_ts_t;

#define to_wm97xx_ts(n) container_of(n, wm97xx_ts_t, pub)

/* Shortcut since we have a lot of such references */
#define CODEC ts->pub.codec

/*
 * Since in pil=0 mode we can't measure the actual pressure,
 * if the pen is down we'll say the pressure is, say, 80.
 * This is because some clients can have some arbitrary pressure
 * thresholds (say 20), besides with pil=1 we get values around
 * 100 and with pil=2 we get values around 200.
 */
#define DEFAULT_PRESSURE	0xb064

#if defined (PROC_FS_SUPPORT)
static struct proc_dir_entry *proc_wm97xx;
#endif
static wm97xx_public *wm97xx_devices [MAX_WM97XX_DEVICES];

/*
 * ADC sample delay times in uS
 */
static const int delay_table[16] = {
	21,// 1 AC97 Link frames
	42,// 2
	84,// 4
	167,// 8
	333,// 16
	667,// 32
	1000,// 48
	1333,// 64
	2000,// 96
	2667,// 128
	3333,// 160
	4000,// 192
	4667,// 224
	5333,// 256
	6000,// 288
	0 // No delay, switch matrix always on
};

/*
 * Delay after issuing a POLL command.
 *
 * The delay is 3 AC97 link frames + the touchpanel settling delay
 */
static inline void poll_delay(int d)
{
	udelay (3 * AC97_LINK_FRAME + delay_table [d]);
}

static inline int pden (wm97xx_ts_t *ts)
{
	u16 pden_mask;

	if (ts->is_wm9712)
		pden_mask = WM9712_PDEN;
	else
		pden_mask = WM9705_PDEN;

	return ts->dig2 & pden_mask;
}

/*
 * Read a sample from the adc in polling mode.
 */
static int wm97xx_poll_read_adc (wm97xx_ts_t *ts, int adcsel, int *sample)
{
	u16 dig1;
	int d = delay [CODEC->id];
	int timeout = 5 * d;

	if (!ts->pen_probably_down) {
		u16 data = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
		if (!(data & WM97XX_PEN_DOWN))
			return RC_PENUP;
		ts->pen_probably_down = 1;
	}

	/* set up digitiser */
	dig1 = (ts->dig1 & ~WM97XX_ADCSEL_MASK) | adcsel;
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1,
			   (ts->dig1 = dig1) | WM97XX_POLL);

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay (d);

	/* wait for POLL to go low */
	while ((CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1) & WM97XX_POLL) && timeout) {
		udelay(AC97_LINK_FRAME);
		timeout--;
	}

	if (timeout <= 0) {
		/* If PDEN is set, we can get a timeout when pen goes up */
		if (pden (ts))
			ts->pen_probably_down = 0;
		else {
			ts->adc_errs++;
			dbg ("adc sample timeout");
		}
		return 0;
	}

	*sample = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* check we have correct sample */
	if ((*sample & WM97XX_ADCSEL_MASK) != adcsel) {
		ts->adc_errs++;
		dbg ("adc wrong sample, read %x got %x", adcsel,
		     *sample & WM97XX_ADCSEL_MASK);
		return 0;
	}

	if (!(*sample & WM97XX_PEN_DOWN)) {
		ts->pen_probably_down = 0;
		return RC_PENUP;
	}

	return RC_VALID;
}

/*
 * Read a sample from the adc in coordinate mode.
 */
static int wm97xx_coord_read_adc(wm97xx_ts_t *ts, wm97xx_data *data)
{
	u16 dig1;
	int d = delay [CODEC->id];
	int timeout = 5 * d;

	if (!ts->pen_probably_down) {
		u16 data = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
		if (!(data & WM97XX_PEN_DOWN))
			return RC_PENUP;
		ts->pen_probably_down = 1;
	}

	/* set up digitiser */
	dig1 = (ts->dig1 & ~WM97XX_ADCSEL_MASK) | WM97XX_ADCSEL_PRES;
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1,
			   (ts->dig1 = dig1) | WM97XX_POLL);

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay(d);

	/* read X then wait for 1 AC97 link frame + settling delay */
	data->x = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
	udelay (AC97_LINK_FRAME + delay_table[d]);

	/* read Y */
	data->y = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* wait for POLL to go low and then read pressure */
	while ((CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1) & WM97XX_POLL)&& timeout) {
			udelay(AC97_LINK_FRAME);
			timeout--;
	}

	if (timeout <= 0) {
		if (pden (ts))
			ts->pen_probably_down = 0;
		else {
			ts->adc_errs++;
			dbg ("adc sample timeout");
		}
		return 0;
	}

        /* read pressure */
	data->p = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* check we have correct samples */
	if (((data->x & WM97XX_ADCSEL_MASK) != 0x1000) ||
	    ((data->y & WM97XX_ADCSEL_MASK) != 0x2000) ||
	    ((data->p & WM97XX_ADCSEL_MASK) != 0x3000)) {
		ts->adc_errs++;
		dbg ("wrong sample order");
		return 0;
	}

	if (!(data->p & WM97XX_PEN_DOWN)) {
		ts->pen_probably_down = 0;
		return RC_PENUP;
	}

	return RC_VALID;
}

/*
 * Sample the touchscreen in polling mode
 */
static int wm97xx_poll_touch(wm97xx_ts_t *ts, wm97xx_data *data)
{
	int rc;

	if ((rc = wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_X, &data->x)) != RC_VALID)
		return rc;
	if ((rc = wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_Y, &data->y)) != RC_VALID)
		return rc;
	if (pil [CODEC->id] && !five_wire [CODEC->id]) {
		if ((rc = wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_PRES, &data->p)) != RC_VALID)
			return rc;
	} else
		data->p = DEFAULT_PRESSURE;

	return RC_VALID;
}

/*
 * Sample the touchscreen in polling coordinate mode
 */
static int wm97xx_poll_coord_touch(wm97xx_ts_t *ts, wm97xx_data *data)
{
	return wm97xx_coord_read_adc(ts, data);
}

/*---//---//---//---//--/ Auxiliary ADC sampling mechanism /--//---//---//---*/

static int wm97xx_check_aux (wm97xx_ts_t *ts)
{
	int i, restore_state = 0, min_delta = 0x7fffffff;
	u16 adcsel, old_dig1 = 0;
	unsigned jiff = jiffies;

	spin_lock (&ts->lock);
	if (ts->pen_is_down) {
		/* If pen is down, delay auxiliary sampling. The touchscreen
		 * thread will wake us up when the pen will be released.
		 */
		spin_unlock (&ts->lock);
		ts->aux_waiting = 1;
		return 10*HZ;
	}

	ts->aux_waiting = 0;

	for (i = 0; i < ARRAY_SIZE (ts->aux_conv); i++)
		if ((adcsel = ts->aux_conv [i].adcsel)) {
			int delta = ts->aux_conv [i].next_jiffies - jiff;

			/* Loose the check here to syncronize events that
			 * have close periods. This allows less timer
			 * interrupts to be generated, and better performance
			 * because of less codec writes.
			 */
			if (delta < (int)(ts->aux_conv [i].period >> 5)) {
				int auxval = 0;

				if (!restore_state) {
					/* This is the first AUX sample we
					 * read, prepare the chip.
					 */
					restore_state = 1;
					old_dig1 = ts->dig1;
					ts->dig1 = 0;
					CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER2, WM97XX_PRP_DET_DIG);
				}

				/* Fool wm97xx_poll_read_adc() */
				ts->pen_probably_down = 1;

				if (ts->aux_conv [i].func)
					ts->aux_conv [i].func (&ts->pub, i, 0);
				wm97xx_poll_read_adc (ts, adcsel, &auxval);
				if (ts->aux_conv [i].func)
					ts->aux_conv [i].func (&ts->pub, i, 1);
				if ((auxval & WM97XX_ADCSEL_MASK) == adcsel) {
					ts->aux_conv [i].value = auxval & 0xfff;
					ts->aux_conv [i].next_jiffies = jiff + ts->aux_conv [i].period;
				}
			}

			delta = ts->aux_conv [i].next_jiffies - jiff;
			if (delta < min_delta)
				min_delta = delta;
		}

	if (restore_state) {
		ts->dig1 = old_dig1;
		/* We know for sure pen is not down */
		ts->pen_probably_down = 0;
		/* Restore previous state of digitizer registers */
		CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER1, ts->dig1);
		CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER2, ts->dig2);
	}

	spin_unlock (&ts->lock);

	if (min_delta < 1)
		min_delta = 1;

        return min_delta;
}

/* Auxiliary ADC reading thread.
 */
static int wm97xx_aux_thread (void *_ts)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)_ts;

	ts->aux_task = current;

	/* set up thread context */
	daemonize("kwm97xx_adc");

	/* This thread is low priority */
	set_user_nice (current, 5);

	/* we will die when we receive SIGKILL */
	allow_signal (SIGKILL);

	/* init is complete */
	complete (&ts->aux_comp);

	/* touch reader loop */
	while (!signal_pending (current)) {
		int sleep_time = wm97xx_check_aux (ts);

		if (ts->aux_waiting) {
			wait_event_interruptible_timeout (ts->aux_wait,
							  ts->aux_waiting == 0,
							  sleep_time);
		} else {
			set_task_state (current, TASK_INTERRUPTIBLE);
			schedule_timeout (sleep_time);
		}
	}

	ts->aux_task = NULL;
	complete_and_exit (&ts->aux_comp, 0);
}

/*---/ Public interface exposed to custom architecture-dependent drivers /---*/

static irqreturn_t wm97xx_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)dev_id;
	ts->irq_count++;
	wake_up_interruptible (&ts->irq_wait);
	return IRQ_HANDLED;
}

static void wm97xx_get_params (struct _wm97xx_public *tspub, wm97xx_params *params)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	int idx = CODEC->id;

	params->mode = mode [idx];
	params->pen_irq = pen_irq [idx];
	params->cont_slot = cont_slot [idx];
	params->cont_rate = cont_rate [idx];
	params->delay = delay [idx];
	params->rpu = rpu [idx];
	params->pil = pil [idx];
	params->pdd = pdd [idx];
	params->five_wire = five_wire [idx];
	params->invert = invert [idx];
}

static void wm97xx_set_params (struct _wm97xx_public *tspub, wm97xx_params *params)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	int idx = CODEC->id;

	if (params->mode >= 0 && params->mode <= 1) {
		dbg("Sampling pen in %s mode",
		    params->mode ? "coordinate" : "polling");
		mode [idx] = params->mode;
	}

	if (params->pen_irq >= 0) {
                dbg("Using IRQ %d for pen-down detection", params->pen_irq);
		pen_irq [idx] = params->pen_irq;
	}

	if (params->cont_slot >= 5 && params->cont_slot <= 11) {
		dbg("Using AC97 slot %d for touchscreen data", params->cont_slot);
		cont_slot [idx] = params->cont_slot;
	}

	if (params->cont_rate >= 0 && params->cont_rate <= 3) {
		(void)rate_desc;
		dbg("Sampling touchscreen data at a rate of %sHz",
		    rate_desc [params->cont_rate]);
		cont_rate [idx] = params->cont_rate;
	}

	if (params->delay >= 0 && params->delay <= 15) {
		dbg("setting adc sample delay to %d uSecs.", delay_table[params->delay]);
		delay [idx] = params->delay;
	}

	if (params->rpu >= 0 && params->rpu <= 63) {
		dbg("setting pen detect pull-up to %d Ohms",
		    params->rpu ? 64000 / params->rpu : 9999999);
		rpu [idx] = params->rpu;
	}

	if (params->pil >= 0 && params->pil <= 2) {
		dbg("setting pressure measurement current to %c00uA.",
		    params->pil == 2 ? '4' : '2');
		pil [idx] = params->pil;
	}

	if (ts->is_wm9712 && params->five_wire >= 0) {
		five_wire [idx] = params->five_wire;
		dbg("setting %c-wire touchscreen mode.",
		    params->five_wire ? '5' : '4');
	}

	if (!ts->is_wm9712 && params->pdd >= 0 && params->pdd <= 15) {
		dbg("setting pdd to %d/15 Vmid", params->pdd);
		pdd [idx] = params->pdd;
	}

	invert [idx] = params->invert & 3;

	wm97xx_get_params (tspub, params);
}

static void wm97xx_apply_params (struct _wm97xx_public *tspub)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	int idx = CODEC->id;
        u16 dig1, dig2, mask;

	spin_lock (&ts->lock);

	dig1 = ts->dig1;
	dig2 = ts->dig2;

	if (ts->irq != pen_irq [idx]) {
		if (ts->irq)
			free_irq(ts->irq, ts);

		if ((ts->irq = pen_irq [idx]) &&
		    request_irq (ts->irq, wm97xx_interrupt, 0, "AC97-touchscreen", ts))
			err("Can't get IRQ %d, falling back to pendown polling", ts->irq);
		/* No matter if we use/have used an IRQ, wake up the thread
		 * so that it can fall asleep again the Only Correct Way.
		 */
		ts->irq_count++;
		wake_up_interruptible(&ts->irq_wait);
	}

	dig1 &= ~(WM97XX_SLT_MASK | WM97XX_CM_RATE_MASK | WM97XX_DELAY_MASK);
	/* We have to enable X or Y to support pen-down detection */
	dig1 |= WM97XX_ADCSEL_X |
		WM97XX_SLT (cont_slot [idx]) |
		WM97XX_DELAY (delay [idx]) |
		WM97XX_RATE (cont_rate [idx]);

	/* WM9712 rpu */
	if (ts->is_wm9712 && rpu [idx]) {
		dig2 &= ~WM9712_RPU(63);
		dig2 |= WM9712_RPU(rpu [idx]);
	}

	/* touchpanel pressure */
	mask = ts->is_wm9712 ? WM9712_PIL : WM9705_PIL;
	if (pil [idx] == 2)
		dig2 |= mask;
	else
		dig2 &= ~mask;

	/* WM9712 five wire */
	if (ts->is_wm9712) {
		if (five_wire [idx])
			dig2 |= WM9712_45W;
		else
			dig2 &= ~WM9712_45W;
	}

	/* WM9705 pdd */
	if (!ts->is_wm9712) {
		dig2 &= ~WM9705_PDD (15);
		dig2 |= WM9705_PDD (pdd [idx]);
	}

	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, ts->dig1 = dig1);
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts->dig2 = dig2);

	ts->invert_x = (invert [idx] & 1) ? 1 : 0;
	ts->invert_y = (invert [idx] & 2) ? 1 : 0;

	spin_unlock (&ts->lock);
}

static void wm97xx_set_pden (wm97xx_public *tspub, int enable)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	u16 dig2, pden_mask;

	spin_lock (&ts->lock);

	dig2 = ts->dig2;

	if (ts->is_wm9712)
		pden_mask = WM9712_PDEN;
	else
		pden_mask = WM9705_PDEN;

	if (enable)
		dig2 |= pden_mask;
	else
		dig2 &= ~pden_mask;

	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts->dig2 = dig2);

	spin_unlock (&ts->lock);
	dbg ("PDEN bit set to %d", enable);
}

static void wm97xx_set_continuous_mode (wm97xx_public *tspub, int enable)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	int idx = CODEC->id;
        u16 dig1;

	spin_lock (&ts->lock);

	dig1 = ts->dig1;

	if (enable) {
		/* continous mode */
		dbg("Enabling continous mode");
		dig1 &= ~(WM97XX_CM_RATE_MASK | WM97XX_ADCSEL_MASK |
			  WM97XX_DELAY_MASK | WM97XX_SLT_MASK);
                dig1 |= WM97XX_ADCSEL_PRES | WM97XX_CTC |
                        WM97XX_COO | WM97XX_SLEN |
			WM97XX_DELAY (delay [idx]) |
			WM97XX_SLT (cont_slot [idx]) |
			WM97XX_RATE (cont_rate [idx]);
	} else {
		dig1 &= ~(WM97XX_CTC | WM97XX_COO | WM97XX_SLEN);
		if (mode [idx] == 1)
			/* coordinate mode */
			dig1 |= WM97XX_COO;
	}

	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, ts->dig1 = dig1);

	spin_unlock (&ts->lock);
}

static void wm97xx_setup_auxconv (wm97xx_public *tspub, wm97xx_aux_conv ac,
				 int period, wm97xx_ac_prepare func)
{
	int i;
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);

	ts->aux_conv [ac].value = -1;
	ts->aux_conv [ac].period = period;
	ts->aux_conv [ac].next_jiffies = jiffies + period;
	ts->aux_conv [ac].func = func;
	ts->aux_conv [ac].adcsel = period ?
		wm97xx_adcsel [ts->is_wm9712 ? 1 : 0] [ac] : 0;

	for (i = 0; i < ARRAY_SIZE (ts->aux_conv); i++)
		if (ts->aux_conv [ac].adcsel) {
			i = 13;
			break;
		}

	if (i == 13) {
		/* Start the aux thread, if not started already */
		if (!ts->aux_task) {
			init_completion (&ts->aux_comp);
			if (kernel_thread (wm97xx_aux_thread, ts, 0) >= 0)
				wait_for_completion (&ts->aux_comp);
		}
	} else if (ts->aux_task) {
		/* kill thread */
		init_completion (&ts->aux_comp);
		send_sig (SIGKILL, ts->aux_task, 1);
		wait_for_completion (&ts->aux_comp);
	}
}

static int wm97xx_get_auxconv (wm97xx_public *tspub, wm97xx_aux_conv ac)
{
	wm97xx_ts_t *ts = to_wm97xx_ts (tspub);
	return ts->aux_conv [ac].adcsel ? ts->aux_conv [ac].value : -1;
}

EXPORT_SYMBOL (wm97xx_get_device);
wm97xx_public *wm97xx_get_device (int index)
{
	if (index < 0 || index > MAX_WM97XX_DEVICES)
		return NULL;
	return wm97xx_devices [index];
}

/*---------------------------------------------------------------------------*/

#if defined(PROC_FS_SUPPORT)
static int wm97xx_read_proc (char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	int len = 0, prpu;
	u16 dig1, dig2, digrd, adcsel, adcsrc, slt, prp, rev;
	char srev = ' ';
	wm97xx_ts_t* ts;

	if ((ts = data) == NULL)
		return -ENODEV;

	spin_lock (&ts->lock);

	dig1 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1);
	dig2 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER2);

	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, dig1 | WM97XX_POLL);
	poll_delay(delay [CODEC->id]);

	digrd = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
	rev = (CODEC->codec_read(CODEC, AC97_WM9712_REV) & 0x000c) >> 2;

	spin_unlock (&ts->lock);

	adcsel = dig1 & WM97XX_ADCSEL_MASK;
	adcsrc = digrd & WM97XX_ADCSEL_MASK;
	slt = (dig1 & 0x7) + 5;
	prp = dig2 & 0xc000;
	prpu = dig2 & 0x003f;

	len += sprintf (page+len, "Wolfson WM97xx Version %s\n", WM_TS_VERSION);

	len += sprintf (page+len, "Using %s", ts->is_wm9712 ? "WM9712" : "WM9705");
	if (ts->is_wm9712) {
		switch (rev) {
		case 0x0:
			srev = 'A';
			break;
		case 0x1:
			srev = 'B';
			break;
		case 0x2:
			srev = 'D';
			break;
		case 0x3:
			srev = 'E';
			break;
		}
		len += sprintf (page+len, " silicon rev %c\n",srev);
	} else
		len += sprintf (page+len, "\n");

	len += sprintf (page+len, "Settings:\n%s%s%s%s",
			dig1 & WM97XX_POLL ? " -sampling adc data(poll)\n" : "",
			adcsel ==  WM97XX_ADCSEL_X ? " -adc set to X coordinate\n" : "",
			adcsel ==  WM97XX_ADCSEL_Y ? " -adc set to Y coordinate\n" : "",
			adcsel ==  WM97XX_ADCSEL_PRES ? " -adc set to pressure\n" : "");
	if (ts->is_wm9712) {
		len += sprintf (page+len, "%s%s%s%s",
				adcsel ==  WM9712_ADCSEL_COMP1 ? " -adc set to COMP1/AUX1\n" : "",
				adcsel ==  WM9712_ADCSEL_COMP2 ? " -adc set to COMP2/AUX2\n" : "",
				adcsel ==  WM9712_ADCSEL_BMON ? " -adc set to BMON\n" : "",
				adcsel ==  WM9712_ADCSEL_WIPER ? " -adc set to WIPER\n" : "");
	} else {
		len += sprintf (page+len, "%s%s%s%s",
				adcsel ==  WM9705_ADCSEL_PCBEEP ? " -adc set to PCBEEP\n" : "",
				adcsel ==  WM9705_ADCSEL_PHONE ? " -adc set to PHONE\n" : "",
				adcsel ==  WM9705_ADCSEL_BMON ? " -adc set to BMON\n" : "",
				adcsel ==  WM9705_ADCSEL_AUX ? " -adc set to AUX\n" : "");
	}

	len += sprintf (page+len, "%s%s%s%s%s%s",
			dig1 & WM97XX_COO ? " -coordinate sampling\n" : " -individual sampling\n",
			dig1 & WM97XX_CTC ? " -continuous mode\n" : " -polling mode\n",
			prp == WM97XX_PRP_DET ? " -pen detect enabled, no wake up\n" : "",
			prp == WM97XX_PRP_DETW ? " -pen detect enabled, wake up\n" : "",
			prp == WM97XX_PRP_DET_DIG ? " -pen digitiser and pen detect enabled\n" : "",
			dig1 & WM97XX_SLEN ? " -read back using slot " : " -read back using AC97\n");

	if ((dig1 & WM97XX_SLEN) && slt !=12)
		len += sprintf(page+len, "%d\n", slt);
	len += sprintf (page+len, " -adc sample delay %d uSecs\n", delay_table[(dig1 & 0x00f0) >> 4]);

	if (ts->is_wm9712) {
		if (prpu)
			len += sprintf (page+len, " -rpu %d Ohms\n", 64000/ prpu);
		len += sprintf (page+len, " -pressure current %s uA\n", dig2 & WM9712_PIL ? "400" : "200");
		len += sprintf (page+len, " -using %s wire touchscreen mode", dig2 & WM9712_45W ? "5" : "4");
	} else {
		len += sprintf (page+len, " -pressure current %s uA\n", dig2 & WM9705_PIL ? "400" : "200");
		len += sprintf (page+len, " -%s impedance for PHONE and PCBEEP\n", dig2 & WM9705_PHIZ ? "high" : "low");
	}
	if (ts->irq)
		len += sprintf (page+len, " -Pen down detection using IRQ %d\n", ts->irq);
	else
		len += sprintf (page+len, " -Pen down detection using polling\n");

	len += sprintf(page+len, "\nADC data:\n%s%d\n%s%s\n",
		       " -adc value (decimal) : ", digrd & 0x0fff,
		       " -pen ", digrd & 0x8000 ? "Down" : "Up");
	if (ts->is_wm9712) {
		len += sprintf (page+len, "%s%s%s%s",
				adcsrc ==  WM9712_ADCSEL_COMP1 ? " -adc value is COMP1/AUX1\n" : "",
				adcsrc ==  WM9712_ADCSEL_COMP2 ? " -adc value is COMP2/AUX2\n" : "",
				adcsrc ==  WM9712_ADCSEL_BMON ? " -adc value is BMON\n" : "",
				adcsrc ==  WM9712_ADCSEL_WIPER ? " -adc value is WIPER\n" : "");
	} else {
		len += sprintf (page+len, "%s%s%s%s",
				adcsrc ==  WM9705_ADCSEL_PCBEEP ? " -adc value is PCBEEP\n" : "",
				adcsrc ==  WM9705_ADCSEL_PHONE ? " -adc value is PHONE\n" : "",
				adcsrc ==  WM9705_ADCSEL_BMON ? " -adc value is BMON\n" : "",
				adcsrc ==  WM9705_ADCSEL_AUX ? " -adc value is AUX\n" : "");
	}

	len += sprintf(page+len, "\nRegisters:\n%s%x\n%s%x\n%s%x\n",
		       " -digitiser 1    (0x76) : 0x", dig1,
		       " -digitiser 2    (0x78) : 0x", dig2,
		       " -digitiser read (0x7a) : 0x", digrd);

	len += sprintf(page+len, "\nErrors:\n%s%d\n",
		       " -coordinate errors ", ts->adc_errs);

	return len;
}
#endif

#if defined(CONFIG_PM)
/* save state of wm9712 PGA's */
static void wm9712_pga_save(wm97xx_ts_t* ts)
{
	ts->phone_pga = CODEC->codec_read(CODEC, AC97_PHONE_VOL) & 0x001f;
	ts->line_pga = CODEC->codec_read(CODEC, AC97_LINEIN_VOL) & 0x1f1f;
	ts->mic_pga = CODEC->codec_read(CODEC, AC97_MIC_VOL) & 0x1f1f;
}

/* restore state of wm9712 PGA's */
static void wm9712_pga_restore(wm97xx_ts_t* ts)
{
	u16 reg;

	reg = CODEC->codec_read(CODEC, AC97_PHONE_VOL);
	CODEC->codec_write(CODEC, AC97_PHONE_VOL, (reg & ~0x1f) | ts->phone_pga);

	reg = CODEC->codec_read(CODEC, AC97_LINEIN_VOL);
	CODEC->codec_write(CODEC, AC97_LINEIN_VOL, (reg & ~0x1f1f) | ts->line_pga);

	reg = CODEC->codec_read(CODEC, AC97_MIC_VOL);
	CODEC->codec_write(CODEC, AC97_MIC_VOL, (reg & ~0x1f1f) | ts->mic_pga);
}

/*
 * Power down the codec
 */
static void wm97xx_suspend(wm97xx_ts_t* ts)
{
	u16 reg;

        /* wm9705 does not have extra PM */
	if (!ts->is_wm9712)
		return;

	spin_lock (&ts->lock);

	/* save and mute the PGA's */
	wm9712_pga_save(ts);

	reg = CODEC->codec_read(CODEC, AC97_PHONE_VOL);
	CODEC->codec_write(CODEC, AC97_PHONE_VOL, reg | 0x001f);

	reg = CODEC->codec_read(CODEC, AC97_MIC_VOL);
	CODEC->codec_write(CODEC, AC97_MIC_VOL, reg | 0x1f1f);

	reg = CODEC->codec_read(CODEC, AC97_LINEIN_VOL);
	CODEC->codec_write(CODEC, AC97_LINEIN_VOL, reg | 0x1f1f);

	/* power down, dont disable the AC link */
	CODEC->codec_write(CODEC, AC97_WM9712_POWER, WM9712_PD(14) |
			       WM9712_PD(13) | WM9712_PD(12) | WM9712_PD(11) |
			       WM9712_PD(10) | WM9712_PD(9) | WM9712_PD(8) |
			       WM9712_PD(7) | WM9712_PD(6) | WM9712_PD(5) |
			       WM9712_PD(4) | WM9712_PD(3) | WM9712_PD(2) |
			       WM9712_PD(1) | WM9712_PD(0));

	spin_unlock (&ts->lock);
}

/*
 * Power up the Codec
 */
static void wm97xx_resume(wm97xx_ts_t* ts)
{
        /* wm9705 does not have extra PM */
	if (!ts->is_wm9712)
		return;

	/* are we registered */
	spin_lock (&ts->lock);

	/* power up */
	CODEC->codec_write(CODEC, AC97_WM9712_POWER, 0x0);

	/* restore PGA state */
	wm9712_pga_restore(ts);

	spin_unlock (&ts->lock);
}

/* WM97xx Power Management
 * The WM9712 has extra powerdown states that are controlled in
 * seperate registers from the AC97 power management.
 * We will only power down into the extra WM9712 states and leave
 * the AC97 power management to the sound driver.
 */
static int wm97xx_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)dev->data;

	switch(rqst) {
	case PM_SUSPEND:
		wm97xx_suspend(ts);
		break;
	case PM_RESUME:
		wm97xx_resume(ts);
		break;
	}
	return 0;
}
#endif

/*
 * set up the physical settings of the device
 */
static void init_wm97xx_phy(wm97xx_ts_t *ts)
{
	u16 aux, vid;
	int idx = CODEC->id;

	ts->dig1 = 0;

	if (ts->is_wm9712)
		ts->dig2 = WM97XX_RPR | WM9712_RPU(1);
	else {
		ts->dig2 = WM97XX_RPR;

		/*
		 * mute VIDEO and AUX as they share X and Y touchscreen
		 * inputs on the WM9705
		 */
		aux = CODEC->codec_read(CODEC, AC97_AUX_VOL);
		if (!(aux & 0x8000)) {
			dbg("muting AUX mixer as it shares X touchscreen coordinate");
			CODEC->codec_write(CODEC, AC97_AUX_VOL, 0x8000 | aux);
		}

		vid = CODEC->codec_read(CODEC, AC97_VIDEO_VOL);
		if (!(vid & 0x8000)) {
			dbg("muting VIDEO mixer as it shares Y touchscreen coordinate");
			CODEC->codec_write(CODEC, AC97_VIDEO_VOL, 0x8000 | vid);
		}
	}

	/* Sanity check for driver parameters */
	if (rpu [idx] < 0 || rpu [idx] > 63)
		rpu [idx] = 0;
	if (pil [idx] < 0 || pil [idx] > 2)
		pil [idx] = 0;
	if (five_wire [idx] < 0 || five_wire [idx] > 1)
		five_wire [idx] = 0;
	if (delay [idx] < 0 || delay [idx] > 15)
		delay [idx] = 4;
	if (ts->is_wm9712 || pdd [idx] < 0 || pdd [idx] > 15)
		pdd [idx] = 0;
	if (cont_rate [idx] < 0 || cont_rate [idx] > 3)
		cont_rate [idx] = 3;
	if (cont_slot [idx] < 5 || cont_slot [idx] > 11)
		cont_slot [idx] = 5;

	wm97xx_set_continuous_mode (&ts->pub, 0);
	wm97xx_set_pden (&ts->pub, 1);
	wm97xx_apply_params (&ts->pub);
}

/* Private struct for communication between wm97xx_ts_thread and wm97xx_proceed */
struct ts_state {
	int sleep_time;
	int min_sleep_time;
};

static int wm97xx_proceed (wm97xx_ts_t *ts, struct ts_state *state)
{
	wm97xx_data data;
	int rc = RC_PENUP;

	if (ts->pub.read_sample) {
		spin_unlock (&ts->lock);
		rc = ts->pub.read_sample (&ts->pub, &data);
		spin_lock (&ts->lock);
	} else
		switch (mode [CODEC->id]) {
		case 0:
			rc = wm97xx_poll_touch (ts, &data);
			break;
		case 1:
			rc = wm97xx_poll_coord_touch (ts, &data);
			break;
		}

	if (rc & RC_PENUP) {
		if (ts->pen_is_down) {
			ts->pen_is_down = 0;
			dbg("pen up");
			input_report_abs(ts->idev, ABS_PRESSURE, 0);
                        input_sync(ts->idev);
			if (ts->aux_waiting) {
				ts->aux_waiting = 0;
				wake_up_interruptible (&ts->aux_wait);
			}
		} else if (!(rc & RC_AGAIN)) {
			/* We need high frequency updates only while pen is down,
			 * the user never will be able to touch screen faster than
			 * a few times per second... On the other hand, when the
			 * user is actively working with the touchscreen we don't
			 * want to lose the quick response. So we will slowly
			 * increase sleep time after the pen is up and quicky
			 * restore it to ~one task switch when pen is down again.
			 */
			if (state->sleep_time < HZ/10)
				state->sleep_time++;
		}
	} else if (rc & RC_VALID) {
		dbg("pen down: x=%x:%d, y=%x:%d, pressure=%x:%d",
		    data.x >> 12, data.x & 0xfff,
		    data.y >> 12, data.y & 0xfff,
		    data.p >> 12, data.p & 0xfff);
		if (ts->invert_x)
			data.x ^= 0xfff;
		if (ts->invert_y)
			data.y ^= 0xfff;
		input_report_abs (ts->idev, ABS_X, data.x & 0xfff);
		input_report_abs (ts->idev, ABS_Y, data.y & 0xfff);
		input_report_abs (ts->idev, ABS_PRESSURE, data.p & 0xfff);
		input_sync (ts->idev);
		ts->pen_is_down = 1;
		state->sleep_time = state->min_sleep_time;
	} else if (rc & RC_PENDOWN) {
		dbg("pen down");
		ts->pen_is_down = 1;
		state->sleep_time = state->min_sleep_time;
	}

	return rc;
}

/*
 * The touchscreen sample reader thread.
 * Also once in 30 seconds we'll update the battery status.
 */
static int wm97xx_ts_thread(void *_ts)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)_ts;
	struct ts_state state;
	int rc;

	ts->ts_task = current;

	/* set up thread context */
	daemonize("kwm97xx_ts");

	/* This thread is low priority */
	set_user_nice (current, 5);

	/* we will die when we receive SIGKILL */
	allow_signal(SIGKILL);

	/* init is complete */
	complete(&ts->ts_comp);

	ts->pen_is_down = 0;
	state.min_sleep_time = HZ/100;
	if (state.min_sleep_time < 1)
		state.min_sleep_time = 1;
	state.sleep_time = state.min_sleep_time;

	/* touch reader loop */
	while (!signal_pending (current)) {

		do {
			spin_lock (&ts->lock);
			rc = wm97xx_proceed (ts, &state);
			spin_unlock (&ts->lock);
		} while (rc & RC_AGAIN);

		if (!ts->pen_is_down && ts->irq) {
			/* Nice, we don't have to poll for pen down event */
			wait_event_interruptible (ts->irq_wait, ts->irq_count);
			ts->irq_count = 0;
		} else {
			set_task_state(current, TASK_INTERRUPTIBLE);
			schedule_timeout(state.sleep_time);
		}
	}
	ts->ts_task = NULL;
	complete_and_exit(&ts->ts_comp, 0);
}

/*
 * Start the touchscreen thread and
 * the touch digitiser.
 */
static int wm97xx_ts_input_open(struct input_dev *idev)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)idev->private;
	int ret, dig2;

	if (ts->use_count++ != 0)
		return 0;

	spin_lock (&ts->lock);
	/* start digitiser */
	dig2 = ts->dig2 | WM97XX_PRP_DET_DIG;
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts->dig2 = dig2);
	/* Do a dummy read of digitizer data register */
	CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
	spin_unlock (&ts->lock);

	/* start touchscreen thread */
	init_completion(&ts->ts_comp);
	ret = kernel_thread(wm97xx_ts_thread, ts, 0);
	if (ret >= 0)
		wait_for_completion(&ts->ts_comp);

	return 0;
}

/*
 * Kill the touchscreen thread and stop
 * the touch digitiser.
 */
static void wm97xx_ts_input_close(struct input_dev *idev)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)idev->private;
	int val;

	if (--ts->use_count == 0) {
		/* kill thread */
		if (ts->ts_task) {
			init_completion(&ts->ts_comp);
			send_sig(SIGKILL, ts->ts_task, 1);
			wait_for_completion(&ts->ts_comp);
		}

		/* stop digitiser */
		spin_lock (&ts->lock);
		val = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER2);
		CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2,
				       val & ~WM97XX_PRP_DET_DIG);
		spin_unlock (&ts->lock);
	}
}

/*
 * Called by the audio codec initialisation to register
 * the touchscreen driver.
 */
static int wm97xx_probe(struct ac97_codec *codec, struct ac97_driver *driver)
{
	u16 id1, id2;
        wm97xx_ts_t *ts;
#if defined (PROC_FS_SUPPORT)
        char proc_str[64];
#endif

	if (codec->id > MAX_WM97XX_DEVICES)
		return -ENOMEM;

	ts = kmalloc (sizeof (wm97xx_ts_t), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	memset(ts, 0, sizeof(wm97xx_ts_t));

	spin_lock_init (&ts->lock);
	init_waitqueue_head (&ts->irq_wait);
	init_waitqueue_head (&ts->aux_wait);

	/* set up AC97 codec interface */
	CODEC = codec;
	codec->driver_private = ts;
if (!(	ts->idev=input_allocate_device())){
	printk(KERN_ERR "touch:not enough memory\n");
}

	/* tell input system what we events we accept and register */
	ts->idev->name = "wm97xx touchscreen";
	ts->idev->open = wm97xx_ts_input_open;
	ts->idev->close = wm97xx_ts_input_close;
	set_bit(EV_ABS, ts->idev->evbit);
	set_bit(ABS_X, ts->idev->absbit);
	set_bit(ABS_Y, ts->idev->absbit);
	set_bit(ABS_PRESSURE, ts->idev->absbit);
	ts->idev->absmax[ABS_X] = 0xfff;
	ts->idev->absmax[ABS_Y] = 0xfff;
	ts->idev->absmax[ABS_PRESSURE] = 0xfff;
	ts->idev->absfuzz[ABS_X] = 8;
	ts->idev->absfuzz[ABS_Y] = 8;
	ts->idev->absfuzz[ABS_PRESSURE] = 4;
	ts->idev->private = ts;
	input_register_device(ts->idev);

	/*
	 * We can only use a WM9705 or WM9712 that has been *first* initialised
	 * by the AC97 audio driver. This is because we have to use the audio
	 * drivers codec read() and write() functions to sample the touchscreen
	 *
	 * If an initialsed WM97xx is found then get the codec read and write
	 * functions.
	 */
	spin_lock (&ts->lock);

	/* test for a WM9712 or a WM9705 */
	id1 = codec->codec_read(codec, AC97_VENDOR_ID1);
	id2 = codec->codec_read(codec, AC97_VENDOR_ID2);
	if (id1 == WM97XX_ID1 && id2 == WM9712_ID2)
		ts->is_wm9712 = 1;
	else if (id1 == WM97XX_ID1 && id2 == WM9705_ID2)
		ts->is_wm9712 = 0;
	else {
		err("could not find a WM97xx codec. Found a 0x%4x:0x%4x instead",
		    id1, id2);
		spin_unlock (&ts->lock);
		input_unregister_device(ts->idev);
		kfree (ts);
		return -1;
	}

	info("Detected a WM97%s codec connected to AC97 controller %d", ts->is_wm9712 ? "12" : "05", codec->id);

	/* set up physical characteristics */
	init_wm97xx_phy(ts);

	spin_unlock (&ts->lock);

#if defined (PROC_FS_SUPPORT)
	if (!proc_wm97xx) {
		sprintf(proc_str, "driver/%s", TS_NAME);
		proc_wm97xx = proc_mkdir(proc_str, 0);
		if (!proc_wm97xx)
			err("could not create directory /proc/%s\n", proc_str);
	}
	if (proc_wm97xx) {
		sprintf(proc_str, "ts%d", codec->id);
		ts->wm97xx_ts_ps =
			create_proc_read_entry (proc_str, 0, proc_wm97xx,
						wm97xx_read_proc, ts);
		if (!ts->wm97xx_ts_ps)
			err("could not register proc interface /proc/%s", proc_str);
	}
#endif
#if defined (CONFIG_PM)
//	if ((ts->idev.pm_dev = pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, wm97xx_pm_event)) == 0)
//		err("could not register with power management");
//	ts->idev.pm_dev->data = ts;
#endif

	ts->pub.get_params = wm97xx_get_params;
	ts->pub.set_params = wm97xx_set_params;
	ts->pub.apply_params = wm97xx_apply_params;
	ts->pub.set_continuous_mode = wm97xx_set_continuous_mode;
	ts->pub.set_pden = wm97xx_set_pden;
	ts->pub.setup_auxconv = wm97xx_setup_auxconv;
	ts->pub.get_auxconv = wm97xx_get_auxconv;
	wm97xx_devices [codec->id] = &ts->pub;

	return 0;
}

/*
 * Called by ac97_codec when it is unloaded.
 */
static void wm97xx_remove(struct ac97_codec *codec, struct ac97_driver *driver)
{
	int i;
	u16 dig1, dig2;
	wm97xx_ts_t *ts = codec->driver_private;

        /* Sanity check */
	if (!ts)
		return;

	dbg("Unloading AC97 codec %d", codec->id);

	input_unregister_device(ts->idev);

	/* Stop all AUX sampling threads */
	for (i = 0; i < ARRAY_SIZE (ts->aux_conv); i++)
		wm97xx_setup_auxconv (&ts->pub, i, 0, NULL);

	spin_lock (&ts->lock);

	if (pen_irq [codec->id]) {
		pen_irq [codec->id] = 0;
		wm97xx_apply_params (&ts->pub);
	}

	wm97xx_devices [codec->id] = NULL;
	codec->driver_private = NULL;

	/* restore default digitiser values */
	dig1 = WM97XX_DELAY(4) | WM97XX_SLT(6);
	if (ts->is_wm9712)
		dig2 = WM9712_RPU(1);
	else
		dig2 = 0x0;

	codec->codec_write(codec, AC97_WM97XX_DIGITISER1, dig1);
	codec->codec_write(codec, AC97_WM97XX_DIGITISER2, dig2);

	spin_unlock (&ts->lock);

	kfree (ts);
}

/* AC97 registration info */
static struct ac97_driver wm9705_driver = {
	codec_id: (WM97XX_ID1 << 16) | WM9705_ID2,
	codec_mask: 0xFFFFFFFF,
	name: "Wolfson WM9705 Touchscreen/Battery Monitor",
	probe:	wm97xx_probe,
	remove: __devexit_p(wm97xx_remove),
};

static struct ac97_driver wm9712_driver = {
	codec_id: (WM97XX_ID1 << 16) | WM9712_ID2,
	codec_mask: 0xFFFFFFFF,
	name: "Wolfson WM9712 Touchscreen/Battery Monitor",
	probe:	wm97xx_probe,
	remove: __devexit_p(wm97xx_remove),
};

static int __init
wm97xx_ts_init(void)
{
	info("Wolfson WM9705/WM9712 Touchscreen Controller driver");
	info("Version %s  liam.girdwood@wolfsonmicro.com", WM_TS_VERSION);

	/* register with the AC97 layer */
	ac97_register_driver(&wm9705_driver);
        ac97_register_driver(&wm9712_driver);

	return 0;
}

static void __exit
wm97xx_ts_exit(void)
{
	/* Unregister from the AC97 layer */
	ac97_unregister_driver(&wm9712_driver);
	ac97_unregister_driver(&wm9705_driver);
}

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("WM9705/WM9712 Touch Screen / BMON Driver");
MODULE_LICENSE("GPL");

module_init(wm97xx_ts_init);
module_exit(wm97xx_ts_exit);
