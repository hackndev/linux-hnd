/*
 * Glue audio driver for the Stork & Philips UDA1341 codec.
 *
 * Based on bitsy-uda1341.c 99% the same in fact!
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This driver makes use of the UDA1341 and the sa1100-audio modules.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial UDA1341 driver release.
 *
 * 2000-07-??	George France	Bitsy support.
 *
 * 2000-12-13	Deborah Wallach	Fixed power handling for iPAQ/bitsy
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-07-13	Nicolas Pitre	Fixes for all supported samplerates.
 *
 * 2001-09-18   Ken Gordon	Support Stork
 *
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
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/dma.h>

#include "sa1100-audio.h"


#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define AUDIO_NAME		"Stork_UDA1341"

#define AUDIO_FMT_MASK		(AFMT_S16_LE)
#define AUDIO_FMT_DEFAULT	(AFMT_S16_LE)
#define AUDIO_CHANNELS_DEFAULT	2
#define AUDIO_RATE_DEFAULT	44100



static struct uda1341 *uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	return uda1341_mixer_ctl(uda1341, cmd, (void *)arg);
}


static struct file_operations stork_mixer_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= mixer_ioctl,
};


/*
 * Audio interface
 */

static long audio_samplerate = AUDIO_RATE_DEFAULT;

static void stork_set_samplerate(long val)
{
	struct uda1341_cfg cfg;
	int clk_div = 0;

	/* We don't want to mess with clocks when frames are in flight */
	Ser4SSCR0 &= ~SSCR0_SSE;
	/* wait for any frame to complete */
	udelay(125);


        val = 44100;
        storkSetLatchB(STORK_AUDIO_CLOCK_SEL0);
        storkClearLatchB(STORK_AUDIO_CLOCK_SEL1);
	clk_div = SSCR0_SerClkDiv(4);
 	cfg.fs = 256;
	cfg.format = FMT_LSB16;
	uda1341_configure(uda1341, &cfg);

	Ser4SSCR0 = (Ser4SSCR0 & ~0xff00) + clk_div + SSCR0_SSE;
	audio_samplerate = val;
}

static void stork_audio_init(void *data)
{
	storkSetLatchA(STORK_AUDIO_POWER_ON);
        storkClearLatchA(STORK_AUDIO_AMP_ON);
	storkClearLatchB(STORK_CODEC_QMUTE);

	/* Setup the uarts */
	GAFR |= (GPIO_SSP_CLK);
	GPDR &= ~(GPIO_SSP_CLK);
	Ser4SSCR0 = 0;
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI + SSCR0_SerClkDiv(8);
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	Ser4SSCR0 |= SSCR0_SSE;

	/* Setup L3 bus */
/* 	L3_init(); - not any more */

/* this is not really a reset it puts the cpld into a wild mode - watch out in case the
   channels end up the wrong way round */

	storkSetLatchB(STORK_AUDIO_CODEC_RESET);

	stork_set_samplerate(audio_samplerate);

	/* Wait for the UDA1341 to wake up */
	mdelay(1);

	uda1341_open(uda1341);
}

static void stork_audio_shutdown(void *data)
{
	uda1341_close(uda1341);
	/* disable the audio power and all signals leading to the audio chip */
	Ser4SSCR0 = 0;
	storkSetLatchB(STORK_CODEC_QMUTE);
	storkClearLatchA(STORK_AUDIO_POWER_ON);
        storkSetLatchA(STORK_AUDIO_AMP_ON);
}

static int stork_audio_ioctl(struct inode *inode, struct file *file,
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
		stork_set_samplerate(val);
		/* fall through */

	case SOUND_PCM_READ_RATE:
		return put_user(audio_samplerate, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* we can do 16-bit only */
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
	.hw_init	= stork_audio_init,
	.hw_shutdown	= stork_audio_shutdown,
	.client_ioctl	= stork_audio_ioctl,
	.sem		= __MUTEX_INITIALIZER(audio_state.sem),
};

static int stork_audio_open(struct inode *inode, struct file *file)
{
	return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_instance()
 */
static struct file_operations stork_audio_fops = {
	.owner	= THIS_MODULE,
	.open	= stork_audio_open,
};


static int audio_dev_id, mixer_dev_id;

#ifdef CONFIG_PM
static int stork_audio_suspend(struct device *_dev, u32 state, u32 level)
{
	int ret = 0;
	if (level == SUSPEND_DISABLE)
		ret = sa1100_audio_suspend(&audio_state, state);
	return ret;
}

static int stork_audio_resume(struct device *_dev, u32 level)
{
	int ret = 0;
	if (level == RESUME_ENABLE)
		ret = sa1100_audio_resume(&audio_state);
	return ret;
}
#else
#define stork_audio_suspend	NULL
#define stork_audio_resume	NULL
#endif

static int stork_audio_probe(struct device *_dev)
{
	int ret;

	printk("Stork audio driver\n" );

	uda1341 = uda1341_attach("l3-bit-sa1100-gpio");
	if (IS_ERR(uda1341)) {
		ret = PTR_ERR(uda1341);
		goto out;
	}

	/* register devices */
	audio_dev_id = register_sound_dsp(&stork_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&stork_mixer_fops, -1);

	printk("Stork audio support initialized\n" );

 out:
	return ret;
}

static int stork_audio_remove(struct device *_dev)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	uda1341_detach(uda1341);
	return 0;
}

static struct device_driver stork_audio_driver = {
	.name		= "stork-uda1341",
	.bus		= &system_bus_type,
	.probe		= stork_audio_probe,
	.remove		= stork_audio_remove,
	.suspend	= stork_audio_suspend,
	.resume		= stork_audio_resume,
};

static int __init stork_uda1341_init(void)
{
	int ret = -ENODEV;

	if (machine_is_stork())
		ret = driver_register(&stork_audio_driver);

	return ret;
}

static void __exit stork_uda1341_exit(void)
{
	driver_unregister(&stork_audio_driver);
}

module_init(stork_uda1341_init);
module_exit(stork_uda1341_exit);

MODULE_AUTHOR("Nicolas Pitre, George France");
MODULE_DESCRIPTION("Glue audio driver for the Stork & Philips UDA1341 codec.");
