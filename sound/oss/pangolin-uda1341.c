/*
 * Glue audio driver for the SA1110 Pangolin board & Philips UDA1341 codec.
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This is the machine specific part of the Pangolin/UDA1341 support.
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
 * 2001-08-06	Richard Fan		Pangolin Support
 *
 * 2001-09-23	Russell King	Update inline with Assabet driver
 *				Remove old L3 bus driver
 *
 * Note: this should probably be merged with the Assabet audio driver,
 * and become the "SDRAM-clock driven" SA1100 audio driver.
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

#include "sa1100-audio.h"

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

#define QmutePin             GPIO_GPIO(4)
#define SpeakerOffPin        GPIO_GPIO(5)

/*
 * Mixer (UDA1341) interface
 */

static struct uda1341 *uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	return uda1341_mixer_ctl(uda1341, cmd, (void *)arg);
}

static struct file_operations pangolin_mixer_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= mixer_ioctl,
};


/*
 * Audio interface
 */
static long audio_samplerate = AUDIO_RATE_DEFAULT;

static void pangolin_set_samplerate(long val)
{
	struct uda1341_cfg cfg;
	int clk_div;

	/* We don't want to mess with clocks when frames are in flight */
	Ser4SSCR0 &= ~SSCR0_SSE;
	/* wait for any frame to complete */
	udelay(125);

	/*
	 * Our clock source is derived from the CPLD on which we don't have
	 * much control unfortunately.  This was intended for a fixed 44100Hz
	 * samplerate assuming a core clock of 206 MHz.  Still we can play
	 * with the SA1110's clock divisor for the SSP port to get a 22050Hz
	 * samplerate.
	 *
	 * Apparently the clock sent to the SA1110 for the SSP port is
	 * divided from the clock sent to the UDA1341 (some people tried to
	 * be too clever in their design, or simply failed to read the SA1110
	 * manual).  If it was the same source we would have been able to
	 * support a third samplerate.
	 *
	 * At least it would have been a minimum acceptable solution to be
	 * able to set the CPLD divisor by software.  The iPAQ design is
	 * certainly a better example to follow for a new design.
	 */
	if (val >= 44100) {
		audio_samplerate = 44100;
		cfg.fs = 256;
		clk_div = SSCR0_SerClkDiv(2);
	} else {
		audio_samplerate = 22050;
		cfg.fs = 512;
		clk_div = SSCR0_SerClkDiv(4);
	}

	cfg.format = FMT_LSB16;
	uda1341_configure(uda1341, &cfg);

	Ser4SSCR0 = (Ser4SSCR0 & ~0xff00) + clk_div + SSCR0_SSE;
}

static void pangolin_audio_init(void *dummy)
{
	unsigned long flags;
	unsigned int mdrefr;

	local_irq_save(flags);

	/*
	 * Setup the SSP uart.
	 */
	PPAR |= PPAR_SPR;
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI + SSCR0_SerClkDiv(2);
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	GAFR |= GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK | GPIO_SSP_CLK |
		GPIO_SSP_SFRM;
	GPDR |= GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM;
	GPDR &= ~(GPIO_SSP_RXD | GPIO_SSP_CLK);
	Ser4SSCR0 |= SSCR0_SSE;

	GAFR &= ~(SpeakerOffPin | QmutePin);
	GPDR |=  (SpeakerOffPin | QmutePin);
	GPCR = SpeakerOffPin;

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
	mdelay(100);

	uda1341_open(uda1341);

	pangolin_set_samplerate(audio_samplerate);

	GPCR = QmutePin;
}

static void pangolin_audio_shutdown(void *dummy)
{
	GPSR = QmutePin;

	uda1341_close(uda1341);

	Ser4SSCR0 = 0;
	MDREFR &= ~(MDREFR_K2DB2 | MDREFR_K2RUN);
}

static int pangolin_audio_ioctl( struct inode *inode, struct file *file,
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
		pangolin_set_samplerate(val);
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
	.hw_init	= pangolin_audio_init,
	.hw_shutdown	= pangolin_audio_shutdown,
	.client_ioctl	= pangolin_audio_ioctl,
	.sem		= __MUTEX_INITIALIZER(audio_state.sem),
};

static int pangolin_audio_open(struct inode *inode, struct file *file)
{
	return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations pangolin_audio_fops = {
	.owner	= THIS_MODULE,
	.open	= pangolin_audio_open,
};


static int audio_dev_id, mixer_dev_id;

#ifdef CONFIG_PM
static int pangolin_audio_suspend(struct device *_dev, u32 state, u32 level)
{
	int ret = 0;
	if (level == SUSPEND_DISABLE)
		ret = sa1100_audio_suspend(&audio_state, state);
	return ret;
}

static int pangolin_audio_resume(struct device *_dev, u32 level)
{
	int ret = 0;
	if (level == RESUME_ENABLE)
		ret = sa1100_audio_resume(&audio_state);
	return ret;
}
#else
#define pangolin_audio_suspend	NULL
#define pangolin_audio_resume	NULL
#endif

static int pangolin_audio_probe(struct device *_dev)
{
	unsigned long flags;

	uda1341 = uda1341_attach("l3-bit-sa1100-gpio");
	if (IS_ERR(uda1341))
		return PTR_ERR(uda1341);

	/* register devices */
	audio_dev_id = register_sound_dsp(&pangolin_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&pangolin_mixer_fops, -1);

	local_irq_save(flags);
	GAFR &= ~(SpeakerOffPin | QmutePin);
	GPDR |=  (SpeakerOffPin | QmutePin);
	local_irq_restore(flags);

	printk(KERN_INFO "Pangolin UDA1341 audio driver initialized\n");

	return 0;
}

static int pangolin_audio_remove(struct device *_dev)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	uda1341_detach(uda1341);
	return 0;
}

static struct device_driver pangolin_audio_driver = {
	.name		= "sa11x0-ssp",
	.bus		= &platform_bus_type,
	.probe		= pangolin_audio_probe,
	.remove		= pangolin_audio_remove,
	.suspend	= pangolin_audio_suspend,
	.resume		= pangolin_audio_resume,
};

static int __init pangolin_uda1341_init(void)
{
	int ret = -ENODEV;

	if (machine_is_pangolin())
		ret = driver_register(&pangolin_audio_driver);

	return ret;
}

static void __exit pangolin_uda1341_exit(void)
{
	sys_device_unregister(&pangolin_audio_device);
}

module_init(pangolin_uda1341_init);
module_exit(pangolin_uda1341_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Glue audio driver for the SA1110 Pangolin board & Philips UDA1341 codec.");
