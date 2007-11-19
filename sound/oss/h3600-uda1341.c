/*
 * Glue audio driver for the Compaq iPAQ H3600 & Philips UDA1341 codec.
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This is the machine specific part of the Compaq iPAQ (aka Bitsy) support.
 * This driver makes use of the UDA1341 and the sa1100-audio modules.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial UDA1341 driver release.
 *
 * 2000-07-??	George France	Bitsy support.
 *
 * 2000-12-13	Deborah Wallach Fixed power handling for iPAQ/h3600
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 *				the former sa1100-uda1341.c driver.
 *
 * 2001-07-13	Nicolas Pitre	Fixes for all supported samplerates.
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

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/dma.h>
#include <asm/arch/h3600.h>

#include "sa1100-audio.h"


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define AUDIO_NAME		"Bitsy_UDA1341"

#define AUDIO_RATE_DEFAULT	44100


static struct l3_client uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	/*
	 * We only accept mixer (type 'M') ioctls.
	 */
	if (_IOC_TYPE(cmd) != 'M')
		return -EINVAL;

	return l3_command(&uda1341, cmd, (void *)arg);
}

static struct file_operations h3600_mixer_fops = {
	ioctl:		mixer_ioctl,
	owner:		THIS_MODULE
};


/*
 * Audio interface
 */

static long audio_samplerate = AUDIO_RATE_DEFAULT;

/*
 * Stop-gap solution until rest of hh.org HAL stuff is merged.
 */
#define GPIO_H3600_CLK_SET0		GPIO_GPIO (12)
#define GPIO_H3600_CLK_SET1		GPIO_GPIO (13)
static void h3600_set_audio_clock(long val)
{
	switch (val) {
	case 24000: case 32000: case 48000:	/* 00: 12.288 MHz */
		GPCR = GPIO_H3600_CLK_SET0 | GPIO_H3600_CLK_SET1;
		break;

	case 22050: case 29400: case 44100:	/* 01: 11.2896 MHz */
		GPSR = GPIO_H3600_CLK_SET0;
		GPCR = GPIO_H3600_CLK_SET1;
		break;

	case 8000: case 10666: case 16000:	/* 10: 4.096 MHz */
		GPCR = GPIO_H3600_CLK_SET0;
		GPSR = GPIO_H3600_CLK_SET1;
		break;

	case 10985: case 14647: case 21970:	/* 11: 5.6245 MHz */
		GPSR = GPIO_H3600_CLK_SET0 | GPIO_H3600_CLK_SET1;
		break;
	}
}

static void h3600_set_samplerate(long val)
{
	struct uda1341_cfg cfg;
	int clk_div = 0;

	/* We don't want to mess with clocks when frames are in flight */
	Ser4SSCR0 &= ~SSCR0_SSE;
	/* wait for any frame to complete */
	udelay(125);

	/*
	 * We have the following clock sources:
	 * 4.096 MHz, 5.6245 MHz, 11.2896 MHz, 12.288 MHz
	 * Those can be divided either by 256, 384 or 512.
	 * This makes up 12 combinations for the following samplerates...
	 */
	if (val >= 48000)
		val = 48000;
	else if (val >= 44100)
		val = 44100;
	else if (val >= 32000)
		val = 32000;
	else if (val >= 29400)
		val = 29400;
	else if (val >= 24000)
		val = 24000;
	else if (val >= 22050)
		val = 22050;
	else if (val >= 21970)
		val = 21970;
	else if (val >= 16000)
		val = 16000;
	else if (val >= 14647)
		val = 14647;
	else if (val >= 10985)
		val = 10985;
	else if (val >= 10666)
		val = 10666;
	else
		val = 8000;

	/* Set the external clock generator */
	h3600_set_audio_clock(val);

	/* Select the clock divisor */
	switch (val) {
	case 8000:
	case 10985:
	case 22050:
	case 24000:
		cfg.fs = 512;
		clk_div = SSCR0_SerClkDiv(16);
		break;
	case 16000:
	case 21970:
	case 44100:
	case 48000:
		cfg.fs = 256;
		clk_div = SSCR0_SerClkDiv(8);
		break;
	case 10666:
	case 14647:
	case 29400:
	case 32000:
		cfg.fs = 384;
		clk_div = SSCR0_SerClkDiv(12);
		break;
	}

	cfg.format = FMT_LSB16;
	l3_command(&uda1341, L3_UDA1341_CONFIGURE, &cfg);
	Ser4SSCR0 = (Ser4SSCR0 & ~0xff00) + clk_div + SSCR0_SSE;
	audio_samplerate = val;
}

static void h3600_audio_init(void *dummy)
{
	unsigned long flags;

	/* Setup the uarts */
	local_irq_save(flags);
	GAFR |= (GPIO_SSP_CLK);
	GPDR &= ~(GPIO_SSP_CLK);
	Ser4SSCR0 = 0;
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI + SSCR0_SerClkDiv(8);
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	Ser4SSCR0 |= SSCR0_SSE;

	/* Enable the audio power */

	clr_h3600_egpio(IPAQ_EGPIO_CODEC_NRESET);
	set_h3600_egpio(IPAQ_EGPIO_AUDIO_ON);
	set_h3600_egpio(IPAQ_EGPIO_QMUTE);
	local_irq_restore(flags);

	/* external clock configuration */
	h3600_set_samplerate(audio_samplerate);

	/* Wait for the UDA1341 to wake up */
	set_h3600_egpio(IPAQ_EGPIO_CODEC_NRESET);
	mdelay(1);

	/* make the left and right channels unswapped (flip the WS latch ) */
	Ser4SSDR = 0;

	/* Initialize the UDA1341 internal state */
	l3_open(&uda1341);

	clr_h3600_egpio(IPAQ_EGPIO_QMUTE);
}

static void h3600_audio_shutdown(void *dummy)
{
	/* disable the audio power and all signals leading to the audio chip */
	l3_close(&uda1341);
	Ser4SSCR0 = 0;
	clr_h3600_egpio(IPAQ_EGPIO_CODEC_NRESET);
	clr_h3600_egpio(IPAQ_EGPIO_AUDIO_ON);
	clr_h3600_egpio(IPAQ_EGPIO_QMUTE);
}

static int h3600_audio_ioctl(struct inode *inode, struct file *file,
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
		h3600_set_samplerate(val);
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
	id:		"UDA1341 out",
	dma_dev:	DMA_Ser4SSPWr,
};

static audio_stream_t input_stream = {
	id:		"UDA1341 in",
	dma_dev:	DMA_Ser4SSPRd,
};

static audio_state_t audio_state = {
	output_stream:	&output_stream,
	input_stream:	&input_stream,
	need_tx_for_rx: 1,
	hw_init:	h3600_audio_init,
	hw_shutdown:	h3600_audio_shutdown,
	client_ioctl:	h3600_audio_ioctl,
	sem:		__MUTEX_INITIALIZER(audio_state.sem),
};

static int h3600_audio_open(struct inode *inode, struct file *file)
{
	return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations h3600_audio_fops = {
	open:		h3600_audio_open,
	owner:		THIS_MODULE
};


static int audio_dev_id, mixer_dev_id;

#ifdef CONFIG_PM
static int h3x00_audio_suspend(struct device *_dev, u32 state, u32 level)
{
	return sa1100_audio_suspend(&audio_state, state, level);
}

static int h3x00_audio_resume(struct device *_dev, u32 level)
{
	return sa1100_audio_resume(&audio_state, level);
}
#else
#define h3x00_audio_suspend	NULL
#define h3x00_audio_resume	NULL
#endif

static int h3x00_audio_probe(struct device *_dev)
{
	int ret;

	ret = l3_attach_client(&uda1341, "l3-bit-sa1100-gpio", "uda1341");
	if (ret)
		goto out;

	/* register devices */
	audio_dev_id = register_sound_dsp(&h3600_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&h3600_mixer_fops, -1);

	printk( KERN_INFO "iPAQ audio support initialized\n" );

 out:
	return ret;
}

static int h3x00_audio_remove(struct device *_dev)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	l3_detach_client(&uda1341);

	return 0;
}

static struct device_driver h3x00_audio_driver = {
	.name		= "sa11x0-ssp",
	.bus		= &platform_bus_type,
	.probe		= h3x00_audio_probe,
	.remove		= h3x00_audio_remove,
	.suspend	= h3x00_audio_suspend,
	.resume		= h3x00_audio_resume,
};

static int __init h3600_uda1341_init(void)
{
	int ret = -ENODEV;

	if (!machine_is_h3600() || machine_is_h3100() || machine_is_h3800())
		ret = driver_register(&h3x00_audio_driver);

	return ret;
}

static void __exit h3600_uda1341_exit(void)
{
	driver_unregister(&h3x00_audio_driver);
}

module_init(h3600_uda1341_init);
module_exit(h3600_uda1341_exit);

MODULE_AUTHOR("Nicolas Pitre, George France");
MODULE_DESCRIPTION("Glue audio driver for the Compaq iPAQ H3600 & Philips UDA1341 codec.");
