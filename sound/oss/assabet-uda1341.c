/*
 * Glue audio driver for the SA1110 Assabet board & Philips UDA1341 codec.
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This is the machine specific part of the Assabet/UDA1341 support.
 * This driver makes use of the UDA1341 and the sa1100-audio modules.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-07-17	Nicolas Pitre	Supports 44100Hz and 22050Hz samplerate now.
 *
 * 2001-08-03	Russell King	Fix left/right channel swap.
 *				Attempt to reduce power consumption when idle.
 *
 * 2001-09-23	Russell King	Remove old L3 bus driver.
 *
 * Please note that fiddling too much with MDREFR results in oopses, so we don't
 * touch MDREFR unnecessarily, which means we don't touch it on close.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/cpufreq.h>
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/dma.h>
#include <asm/arch/assabet.h>

#include "sa1100-audio.h"

/*
 * Define this to fix the power drain on early Assabets
 */
#define FIX_POWER_DRAIN

/*
 * Debugging?
 */
#undef DEBUG


#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define AUDIO_RATE_DEFAULT	44100

/*
 * Mixer (UDA1341) interface
 */

static struct uda1341 *uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	return uda1341_mixer_ctl(uda1341, cmd, (void *)arg);
}

static struct file_operations assabet_mixer_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= mixer_ioctl,
};


/*
 * Audio interface
 */
static long audio_samplerate = AUDIO_RATE_DEFAULT;

/*
 * FIXME: what about SFRM going high when SSP is disabled?
 */
static void assabet_set_samplerate(long val)
{
	struct uda1341_cfg cfg;
	u_int clk_ref, clk_div;

	/* We don't want to mess with clocks when frames are in flight */
	Ser4SSCR0 &= ~SSCR0_SSE;
	/* wait for any frame to complete */
	udelay(125);

	/*
	 * Our clock source is derived from the CPLD on which we don't have
	 * much control unfortunately.  This was intended for a fixed 48000 Hz
	 * samplerate assuming a core clock of 221.2 MHz.  The CPLD appears
	 * to divide the memory clock so there is a ratio of 4608 between
	 * the core clock and the resulting samplerate (obtained by
	 * measurements, the CPLD equations should confirm that).
	 *
	 * Still we can play with the SA1110's clock divisor for the SSP port
	 * to get half the samplerate as well.
	 *
	 * Apparently the clock sent to the SA1110 for the SSP port is further
	 * more divided from the clock sent to the UDA1341 (some people tried
	 * to be too clever in their design, or simply failed to read the
	 * SA1110 manual).  If it was the same clock we would have been able
	 * to support a third samplerate with the UDA1341's 384FS mode.
	 *
	 * At least it would have been a minimum acceptable solution to be
	 * able to set the CPLD divisor by software.  The iPAQ design is
	 * certainly a better example to follow for a new design.
	 */
	clk_ref = cpufreq_get(0) * 1000 / 4608;
	if (val > clk_ref*4/7) {
		audio_samplerate = clk_ref;
		cfg.fs = 256;
		clk_div = SSCR0_SerClkDiv(2);
	} else {
		audio_samplerate = clk_ref/2;
		cfg.fs = 512;
		clk_div = SSCR0_SerClkDiv(4);
	}

	cfg.format = FMT_LSB16;
	uda1341_configure(uda1341, &cfg);

	Ser4SSCR0 = (Ser4SSCR0 & ~0xff00) + clk_div + SSCR0_SSE;
}

/*
 * Initialise the Assabet audio driver.
 *
 * Note that we have to be careful with the order that we do things here;
 * there is a D-type flip flop which is clocked from the SFRM line which
 * indicates whether the same is for the left or right channel to the
 * UDA1341.
 *
 * When you disable the SSP (by clearing SSCR0_SSE) it appears that the
 * SFRM signal can float high.  When you re-enable the SSP, you clock the
 * flip flop once, and end up swapping the left and right channels.
 *
 * The ASSABET_BCR_CODEC_RST line will force this flip flop into a known
 * state, but this line resets other devices as well!
 *
 * In addition to the above, it appears that powering down the UDA1341 on
 * early Assabets leaves the UDA_WS actively driving a logic '1' into the
 * chip, wasting power!  (you can tell this by D11 being half-on).  We
 * attempt to correct this by kicking the flip flop on init/open/close.
 * We should probably do this on PM resume as well.
 *
 * (Note the ordering of ASSABET_BCR_AUDIO_ON, SFRM and ASSABET_BCR_CODEC_RST
 * is important).
 */
static void assabet_audio_init(void *dummy)
{
	unsigned long flags;
	unsigned int mdrefr;

	local_irq_save(flags);

	/*
	 * Enable the power for the UDA1341 before driving any signals.
	 * We leave the audio amp (LM4880) disabled for now.
	 */
	ASSABET_BCR_set(ASSABET_BCR_AUDIO_ON);

#ifdef FIX_POWER_DRAIN
	GPSR = GPIO_SSP_SFRM;
	GPCR = GPIO_SSP_SFRM;
#endif

	ASSABET_BCR_set(ASSABET_BCR_CODEC_RST);
	ASSABET_BCR_clear(ASSABET_BCR_STEREO_LB);

	/*
	 * Setup the SSP uart.
	 */
	PPAR |= PPAR_SPR;
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI + SSCR0_SerClkDiv(2);
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	GAFR |= GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK | GPIO_SSP_CLK;
	GPDR |= GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM;
	GPDR &= ~(GPIO_SSP_RXD | GPIO_SSP_CLK);
	Ser4SSCR0 |= SSCR0_SSE;

	/*
	 * Only give SFRM to the SSP after it has been enabled.
	 */
	GAFR |= GPIO_SSP_SFRM;

	/*
	 * The assabet board uses the SDRAM clock as the source clock for
	 * audio. This is supplied to the SA11x0 from the CPLD on pin 19.
	 * At 206MHz we need to run the audio clock (SDRAM bank 2)
	 * at half speed. This clock will scale with core frequency so
	 * the audio sample rate will also scale. The CPLD on Assabet
	 * will need to be programmed to match the core frequency.
	 */
	mdrefr = MDREFR;
	if ((mdrefr & (MDREFR_K2DB2 | MDREFR_K2RUN | MDREFR_EAPD |
		       MDREFR_KAPD)) != (MDREFR_K2DB2 | MDREFR_K2RUN)) {
		mdrefr |= MDREFR_K2DB2 | MDREFR_K2RUN;
		mdrefr &= ~(MDREFR_EAPD | MDREFR_KAPD);
		MDREFR = mdrefr;
		(void) MDREFR;
	}
	local_irq_restore(flags);

	/* Wait for the UDA1341 to wake up */
	mdelay(1);

	uda1341_open(uda1341);

	assabet_set_samplerate(audio_samplerate);

	/* Enable the audio power */
	ASSABET_BCR_clear(ASSABET_BCR_QMUTE | ASSABET_BCR_SPK_OFF);
}

/*
 * Shutdown the Assabet audio driver.
 *
 * We have to be careful about the SFRM line here for the same reasons
 * described in the initialisation comments above.  This basically means
 * that we must hand the SSP pins back to the GPIO module before disabling
 * the SSP.
 *
 * In addition, to reduce power drain, we toggle the SFRM line once so
 * that the UDA_WS line is at logic 0.
 *
 * We can't clear ASSABET_BCR_CODEC_RST without knowing if the UCB1300 or
 * ADV7171 driver is still active.  If it is, then we still need to play
 * games, so we might as well leave ASSABET_BCR_CODEC_RST set.
 */
static void assabet_audio_shutdown(void *dummy)
{
	ASSABET_BCR_set(ASSABET_BCR_STEREO_LB | ASSABET_BCR_QMUTE |
			ASSABET_BCR_SPK_OFF);

	uda1341_close(uda1341);

	GAFR &= ~(GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM);
	Ser4SSCR0 = 0;

#ifdef FIX_POWER_DRAIN
	GPSR = GPIO_SSP_SFRM;
	GPCR = GPIO_SSP_SFRM;
#endif

	/* disable the audio power */
	ASSABET_BCR_clear(ASSABET_BCR_AUDIO_ON);
}

static int assabet_audio_ioctl( struct inode *inode, struct file *file,
				uint cmd, ulong arg)
{
	long val;
	int ret = 0;

	/*
	 * These are platform dependent ioctls which are not handled by the
	 * generic sa1100-audio module.
	 */
	switch (cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *) arg);
		if (ret)
			return ret;
		/* the UDA1341 is stereo only */
		ret = (val == 0) ? -EINVAL : 1;
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		/* the UDA1341 is stereo only */
		return put_user(2, (long *) arg);

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret) break;
		assabet_set_samplerate(val);
		/* fall through */

	case SOUND_PCM_READ_RATE:
		return put_user(audio_samplerate, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* we can do signed 16-bit only */
		return put_user(AFMT_S16_LE, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (As per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}

	return ret;
}

static audio_stream_t output_stream = {
	.id		= "UDA1341 out",
	.dma_dev	= DMA_Ser4SSPWr,
};

static audio_stream_t input_stream = {
	.id		= "UDA1341 in",
	.dma_dev	= DMA_Ser4SSPRd,
};

static audio_state_t audio_state = {
	.output_stream	= &output_stream,
	.input_stream	= &input_stream,
	.need_tx_for_rx	= 1,
	.hw_init	= assabet_audio_init,
	.hw_shutdown	= assabet_audio_shutdown,
	.client_ioctl	= assabet_audio_ioctl,
	.sem		= __MUTEX_INITIALIZER(audio_state.sem),
};

static int assabet_audio_open(struct inode *inode, struct file *file)
{
	return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations assabet_audio_fops = {
	.owner	= THIS_MODULE,
	.open	= assabet_audio_open,
};


static int audio_dev_id, mixer_dev_id;

#ifdef CONFIG_PM
static int assabet_audio_suspend(struct device *_dev, u32 state, u32 level)
{
	int ret = 0;
	if (level == SUSPEND_DISABLE)
		ret = sa1100_audio_suspend(&audio_state, state);
	return ret;
}

static int assabet_audio_resume(struct device *_dev, u32 level)
{
	int ret = 0;
	if (level == RESUME_ENABLE)
		ret = sa1100_audio_resume(&audio_state);
	return ret;
}
#else
#define assabet_audio_suspend	NULL
#define assabet_audio_resume	NULL
#endif

static int assabet_audio_probe(struct device *_dev)
{
	uda1341 = uda1341_attach("l3-bit-sa1100-gpio");
	if (IS_ERR(uda1341))
		return PTR_ERR(uda1341);

	output_stream.dev = _dev;
	input_stream.dev = _dev;

	/* register devices */
	audio_dev_id = register_sound_dsp(&assabet_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&assabet_mixer_fops, -1);

#ifdef FIX_POWER_DRAIN
	{
		unsigned long flags;
		local_irq_save(flags);
		ASSABET_BCR_set(ASSABET_BCR_CODEC_RST);
		GPSR = GPIO_SSP_SFRM;
		GPDR |= GPIO_SSP_SFRM;
		GPCR = GPIO_SSP_SFRM;
		local_irq_restore(flags);
	}
#endif

	printk(KERN_INFO "Sound: Assabet UDA1341: dsp id %d mixer id %d\n",
		audio_dev_id, mixer_dev_id);
	return 0;
}

static int assabet_audio_remove(struct device *_dev)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	uda1341_detach(uda1341);
	return 0;
}

static struct device_driver assabet_audio_driver = {
	.name		= "sa11x0-ssp",
	.bus		= &platform_bus_type,
	.probe		= assabet_audio_probe,
	.remove		= assabet_audio_remove,
	.suspend	= assabet_audio_suspend,
	.resume		= assabet_audio_resume,
};

static int __init assabet_uda1341_init(void)
{
	int ret = -ENODEV;

	if (machine_is_assabet())
		ret = driver_register(&assabet_audio_driver);

	return ret;
}

static void __exit assabet_uda1341_exit(void)
{
	driver_unregister(&assabet_audio_driver);
}

module_init(assabet_uda1341_init);
module_exit(assabet_uda1341_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Glue audio driver for the SA1110 Assabet board & Philips UDA1341 codec.");
