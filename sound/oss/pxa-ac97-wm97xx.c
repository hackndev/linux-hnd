/*
 * WM97XX AC97 codec continuous mode touchscreen implementation.
 *
 * Copyright © 2004 Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/spinlock.h>

#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/arch/pxa-regs.h>

#include "wm97xx.h"
#include "pxa-audio.h"

/* Define to 100 to disable the built-in noise filter. This value defines
 * the maximal delta X/Y between two successive reads (which happen with
 * an interval of 1.33 msec, thus given a typical PDA LCD a threshold of
 * 10 evaluates to a pen movement speed of ~12.8cm/second. Don't set this
 * too low as it is possible the analog signal is noisy and you will filter
 * out a lot of valid samples.
 */
#define TS_NOISE_THRESHOLD	15
/* Number of samples to average */
#define TS_AVERAGE_SAMPLES	4

extern int pxa_ac97_get(struct ac97_codec **codec);
extern void pxa_ac97_put(void);

#define DATA (*(wm97xx_ac97_private *)tspub->private)
static wm97xx_public *tspub;

typedef struct {
	/* A dummy file object to fool pxa-audio.c */
	struct file file;
	struct file_operations f_op;
	/* pen-is-down flag */
	int pen_is_down;
	/* x, y and p */
	int x, y, p;
	/* Count of samples acquired so far */
	int samples;
	/* The accumulated X, Y and pressure values */
	int sum_x, sum_y, sum_p;
	/* The amount of samples summed in sum_? variables */
	int avg_count;
#if TS_NOISE_THRESHOLD < 100
	int last_x, last_y;
	int skip_noise;
#endif
} wm97xx_ac97_private;

/* Use 8-byte bursts of 16 bit widths */
#define DCMD_RXMODR	(DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_BURST8|DCMD_WIDTH2)

static audio_stream_t ac97_ts_in = {
	name:		"AC97 touchscreen in",
	dcmd:		DCMD_RXMODR,
	drcmr:		&DRCMRRXMODR,
	dev_addr:	__PREG(MODR),
};

static audio_state_t ac97_ts_state = {
	input_stream:	&ac97_ts_in,
	sem:		 __SEMAPHORE_INIT(ac97_ts_state.sem,1),
};

static int pxa_wm97xx_read (wm97xx_public *tspub, void *d, int count)
{
	int ret;
	mm_segment_t old_fs = get_fs ();
	set_fs (KERNEL_DS);
	ret = DATA.f_op.read (&DATA.file, (char *)d, count, &DATA.file.f_pos);
	set_fs (old_fs);
	return ret;
}

static int pxa_wm97xx_read_sample (wm97xx_public *tspub, wm97xx_data *data)
{
	int count = 0;
        int xx, yy, pp;
	u16 d, t;

	if (DATA.pen_is_down)
		DATA.file.f_flags |= O_NONBLOCK;
	else
		DATA.file.f_flags &= ~O_NONBLOCK;

	while (count != 0x55) {
		count = pxa_wm97xx_read (tspub, &d, sizeof (d));

		if (count < (int)sizeof (d))
			return 0;

		if (!(d & WM97XX_PEN_DOWN)) {
			int pen_was_down = DATA.pen_is_down;

			DATA.file.f_flags |= O_NONBLOCK;
			DATA.samples = DATA.avg_count = 0;

			if (DATA.pen_is_down) {
				DATA.pen_is_down = 0;
				/* Enable pen detection (stop the DMA flow) */
				tspub->set_pden (tspub, 1);
			}

			do {
				if (pxa_wm97xx_read (tspub, &d, sizeof (d)) != sizeof (d))
					/* Call the function again to flush DMA buffers */
					return RC_PENUP | RC_AGAIN;
			} while ((d & WM97XX_PEN_DOWN) == 0);

			/* Hmm, found a pendown event again ... */
			if (pen_was_down) {
				DATA.pen_is_down = 1;
				tspub->set_pden (tspub, 0);
			}
		}

		t = d & 0x7000;
		switch (DATA.samples) {
		case 0:
			if (t == WM97XX_ADCSEL_X) {
				DATA.samples++;
				DATA.x = d;
			}
			break;
		case 1:
			if (t == WM97XX_ADCSEL_Y) {
				DATA.samples++;
				DATA.y = d;
			} else
				DATA.samples = 0;
			break;
		case 2:
			/* Pressure >0x400 means something is wrong */
			if (((d & 0xfff) < 0x400)
			 && (t == WM97XX_ADCSEL_PRES))
				DATA.p = d;
			DATA.samples = 0;
			count = 0x55;
			break;
		}
	}

	xx = DATA.x & 0xfff;
	yy = DATA.y & 0xfff;
	pp = DATA.p & 0xfff;

#if TS_NOISE_THRESHOLD < 100
	if (DATA.pen_is_down) {
		if ((abs (DATA.last_x - xx) > TS_NOISE_THRESHOLD)
		 || (abs (DATA.last_y - yy) > TS_NOISE_THRESHOLD)) {
			/* If we have more than one "noise" in sequence,
			 * don't skip it since noises usually come by one.
			 */
			if (!DATA.skip_noise) {
				DATA.skip_noise = 1;
				return RC_PENDOWN | RC_AGAIN;
			}
		} else
			DATA.skip_noise = 0;
	}

	DATA.last_x = xx;
	DATA.last_y = yy;
#endif

	if (!DATA.pen_is_down) {
		DATA.pen_is_down = 1;
		/* Disable pen detection (so that we can see pen up events) */
		tspub->set_pden (tspub, 0);
		/* Reset state machine */
		DATA.avg_count = 0;
		DATA.skip_noise = 0;
		/* Skip the first sample after pendown since it is way too
		   often just plain garbage */
		return RC_PENDOWN | RC_AGAIN;
	}

	if (!DATA.avg_count++) {
		DATA.sum_x = xx;
		DATA.sum_y = yy;
		DATA.sum_p = pp;
	} else {
		DATA.sum_x += xx;
		DATA.sum_y += yy;
		DATA.sum_p += pp;
	}

	/* If we don't have enough samples, return */
	if (DATA.avg_count < TS_AVERAGE_SAMPLES)
		return RC_PENDOWN | RC_AGAIN;

	data->x = (DATA.sum_x / TS_AVERAGE_SAMPLES) |
		  WM97XX_PEN_DOWN | WM97XX_ADCSEL_X;
	data->y = (DATA.sum_y / TS_AVERAGE_SAMPLES) |
		  WM97XX_PEN_DOWN | WM97XX_ADCSEL_Y;
	data->p = (DATA.sum_p / TS_AVERAGE_SAMPLES) |
		  WM97XX_PEN_DOWN | WM97XX_ADCSEL_PRES;
	DATA.avg_count = 0;

	return RC_VALID | RC_AGAIN;
}

static int __init
pxa_wm97xx_init(void)
{
	wm97xx_params params;
	struct ac97_codec *pxa_ac97;
	int i, ret = pxa_ac97_get (&pxa_ac97);
	mm_segment_t old_fs;

	if (ret)
		return ret;

	/* For now we suppose 1st wm9705 is pxa-ac97 */
	for (i = 0; ; i++) {
		tspub = wm97xx_get_device (i);
		if (!tspub)
			break;
		if (tspub->codec == pxa_ac97)
			break;
	}

	pxa_ac97_put ();

	if (!tspub)
		return -ENODEV;

	tspub->private = kmalloc (sizeof (wm97xx_ac97_private), GFP_KERNEL);
	memset (tspub->private, 0, sizeof (wm97xx_ac97_private));

	DATA.file.f_mode = FMODE_READ;
        DATA.file.f_op = &DATA.f_op;

	if ((ret = pxa_audio_attach (NULL, &DATA.file, &ac97_ts_state))) {
		printk (KERN_ERR "error %d: cannot open AC97 input stream\n", ret);
		kfree (tspub->private);
		tspub->private = NULL;
		return ret;
	}

	/* Use 16 buffers of 32 bytes each (~0.7 seconds) for DMA data */
	old_fs = get_fs ();
	set_fs (KERNEL_DS);
	i = 0x00100005;
	DATA.f_op.ioctl (NULL, &DATA.file, SNDCTL_DSP_SETFRAGMENT, (ulong)&i);
	set_fs (old_fs);

	tspub->read_sample = pxa_wm97xx_read_sample;

	/* Set up the driver as we like it */
	params.mode = params.rpu = params.pdd = params.pil =
	params.five_wire = params.delay = params.pen_irq =
	params.cont_rate = -1;
	/* On XScale the only option is to use fifth AC97 slot (modem) */
	params.cont_slot = 5;
	tspub->set_params (tspub, &params);
	tspub->apply_params (tspub);

	/* Initially don't fetch data when the pen is not down */
	tspub->set_pden (tspub, 1);
	/* Enable continuous mode */
	tspub->set_continuous_mode (tspub, 1);

	return 0;
}

static void __exit
pxa_wm97xx_exit(void)
{
	if (!tspub)
		return;

	tspub->read_sample = NULL;
	tspub->set_continuous_mode (tspub, 0);
	tspub->set_pden (tspub, 1);
	pxa_audio_clear_buf(&ac97_ts_in);
	kfree (tspub->private);
	tspub->private = NULL;
}

/* This is a dummy symbol so that platform drivers can depend on this one */
int driver_pxa_ac97_wm97xx;
EXPORT_SYMBOL (driver_pxa_ac97_wm97xx);

/* Module information */
MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("AC97 controller driver for Dell Axim X5");
MODULE_LICENSE("GPL");

module_init(pxa_wm97xx_init);
module_exit(pxa_wm97xx_exit);
