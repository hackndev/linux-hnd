/*
 *  linux/drivers/sound/pxa-ac97.c -- AC97 interface for the Cotula chip
 *
 *  Author:	Nicolas Pitre
 *  Created:	Aug 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *
 *  Forward ported to 2.6 by Ian Molton 15/09/2003
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ac97_codec.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>
#include <asm/arch/pxa-regs.h>

#include "pxa-audio.h"

static struct completion CAR_completion;
static int waitingForMask;
static DECLARE_MUTEX(CAR_mutex);

static u16 pxa_ac97_read(struct ac97_codec *codec, u8 reg)
{
	u16 val = -1;

	down(&CAR_mutex);
	if (!(CAR & CAR_CAIP)) {
		volatile u32 *reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);

		waitingForMask=GSR_SDONE;

		init_completion(&CAR_completion);
		(void)*reg_addr;			//start read access across the ac97 link
		wait_for_completion(&CAR_completion);

		if (GSR & GSR_RDCS) {
			GSR = GSR_RDCS;			//write a 1 to clear
			printk(KERN_CRIT "%s: read codec register timeout.\n", __FUNCTION__);
		}

		init_completion(&CAR_completion);
		val = *reg_addr;			//valid data now but we've just started another cycle...
		wait_for_completion(&CAR_completion);

	} else {
		printk(KERN_CRIT"%s: CAR_CAIP already set\n", __FUNCTION__);
	}
	up(&CAR_mutex);
	//printk("%s(0x%02x) = 0x%04x\n", __FUNCTION__, reg, val);
	return val;
}

static void pxa_ac97_write(struct ac97_codec *codec, u8 reg, u16 val)
{
	down(&CAR_mutex);
	if (!(CAR & CAR_CAIP)) {
		volatile u32 *reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);

		waitingForMask=GSR_CDONE;
		init_completion(&CAR_completion);
		*reg_addr = val;
		wait_for_completion(&CAR_completion);
	} else {
		printk(KERN_CRIT "%s: CAR_CAIP already set\n", __FUNCTION__);
	}
	up(&CAR_mutex);
	//printk("%s(0x%02x, 0x%04x)\n", __FUNCTION__, reg, val);
}

static irqreturn_t pxa_ac97_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int gsr = GSR;
	GSR = gsr & (GSR_SDONE|GSR_CDONE);		//write a 1 to clear
	if (gsr & waitingForMask)
                complete(&CAR_completion);

	return IRQ_HANDLED;
}

static struct ac97_codec pxa_ac97_codec = {
	codec_read:	pxa_ac97_read,
	codec_write:	pxa_ac97_write,
};

static DECLARE_MUTEX(pxa_ac97_mutex);
static int pxa_ac97_refcount;

int pxa_ac97_get(struct ac97_codec **codec)
{
	int ret;

	*codec = NULL;
	down(&pxa_ac97_mutex);

	if (!pxa_ac97_refcount) {
		ret = request_irq(IRQ_AC97, pxa_ac97_irq, 0, "AC97", NULL);
		if (ret)
			return ret;

		CKEN |= CKEN2_AC97;

		pxa_gpio_mode(GPIO31_SYNC_AC97_MD);
		pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
		pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
		pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);

		GCR = 0;
		udelay(10);
		GCR = GCR_COLD_RST|GCR_CDONE_IE|GCR_SDONE_IE;
		while (!(GSR & GSR_PCR)) {
			schedule();
		}

		ret = ac97_probe_codec(&pxa_ac97_codec);
		if (ret != 1) {
			free_irq(IRQ_AC97, NULL);
			GCR = GCR_ACLINK_OFF;
			CKEN &= ~CKEN2_AC97;
			return ret;
		}

		// need little hack for UCB1400 (should be moved elsewhere)
//		pxa_ac97_write(&pxa_ac97_codec,AC97_EXTENDED_STATUS,1);
		//pxa_ac97_write(&pxa_ac97_codec, 0x6a, 0x1ff7);
//		pxa_ac97_write(&pxa_ac97_codec, 0x6a, 0x0050);
//		pxa_ac97_write(&pxa_ac97_codec, 0x6c, 0x0030);
	}

	pxa_ac97_refcount++;
	up(&pxa_ac97_mutex);
	*codec = &pxa_ac97_codec;
	return 0;
}

void pxa_ac97_put(void)
{
	down(&pxa_ac97_mutex);
	pxa_ac97_refcount--;
	if (!pxa_ac97_refcount) {
		GCR = GCR_ACLINK_OFF;
		CKEN &= ~CKEN2_AC97;
		free_irq(IRQ_AC97, NULL);
	}
	up(&pxa_ac97_mutex);
}

EXPORT_SYMBOL(pxa_ac97_get);
EXPORT_SYMBOL(pxa_ac97_put);


/*
 * Audio Mixer stuff
 */

static audio_state_t ac97_audio_state;
static audio_stream_t ac97_audio_in;

/*
 * According to the PXA250 spec, mic-in should use different
 * DRCMR and different AC97 FIFO.
 * Unfortunately current UCB1400 versions (up to ver 2A) don't
 * produce slot 6 for the audio input frame, therefore the PXA
 * AC97 mic-in FIFO is always starved.
 * But since UCB1400 is not the only audio CODEC out there,
 * this is still enabled by default.
 */
static void update_audio_in (void)
{
#if 1
	long val;

	/* Use the value stuffed by ac97_recmask_io()
	 * into recording select register
	 */
	val = pxa_ac97_codec.codec_read(&pxa_ac97_codec, AC97_RECORD_SELECT);
	pxa_audio_clear_buf(&ac97_audio_in);
	*ac97_audio_in.drcmr = 0;
	if (val == 0) {
		ac97_audio_in.dcmd = DCMD_RXMCDR;
		ac97_audio_in.drcmr = &DRCMRRXMCDR;
		ac97_audio_in.dev_addr = __PREG(MCDR);
	} else {
		ac97_audio_in.dcmd = DCMD_RXPCDR;
		ac97_audio_in.drcmr = &DRCMRRXPCDR;
		ac97_audio_in.dev_addr = __PREG(PCDR);
	}
	if (ac97_audio_state.rd_ref)
		*ac97_audio_in.drcmr =
			ac97_audio_in.dma_ch | DRCMR_MAPVLD;
#endif
}

static int mixer_ioctl( struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret;

	ret = pxa_ac97_codec.mixer_ioctl(&pxa_ac97_codec, cmd, arg);
	if (ret)
		return ret;

	/* We must snoop for some commands to provide our own extra processing */
	switch (cmd) {
	case SOUND_MIXER_WRITE_RECSRC:
		update_audio_in ();
		break;
	}
	return 0;
}

static struct file_operations mixer_fops = {
	ioctl:		mixer_ioctl,
	llseek:		no_llseek,
	owner:		THIS_MODULE
};

/*
 * AC97 codec ioctls
 */

static int codec_adc_rate = 48000;
static int codec_dac_rate = 48000;

static int ac97_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	int ret;
	long val = 0;

	switch(cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *) arg);
		if (ret)
			return ret;
		/* FIXME: do we support mono? */
		ret = (val == 0) ? -EINVAL : 1;
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		/* FIXME: do we support mono? */
		return put_user(2, (long *) arg);

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		if (file->f_mode & FMODE_READ)
			codec_adc_rate = ac97_set_adc_rate(&pxa_ac97_codec, val);
		if (file->f_mode & FMODE_WRITE)
			codec_dac_rate = ac97_set_dac_rate(&pxa_ac97_codec, val);
		/* fall through */
	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_READ)
			val = codec_adc_rate;
		if (file->f_mode & FMODE_WRITE)
			val = codec_dac_rate;
		return put_user(val, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* FIXME: can we do other fmts? */
		return put_user(AFMT_S16_LE, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (As per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return 0;
}


/*
 * Audio stuff
 */

static audio_stream_t ac97_audio_out = {
	name:			"AC97 audio out",
	dcmd:			DCMD_TXPCDR,
	drcmr:			&DRCMRTXPCDR,
	dev_addr:		__PREG(PCDR),
};

static audio_stream_t ac97_audio_in = {
	name:			"AC97 audio in",
	dcmd:			DCMD_RXPCDR,
	drcmr:			&DRCMRRXPCDR,
	dev_addr:		__PREG(PCDR),
};

static audio_state_t ac97_audio_state = {
	output_stream:		&ac97_audio_out,
	input_stream:		&ac97_audio_in,
	client_ioctl:		ac97_ioctl,
	sem:			 __SEMAPHORE_INIT(ac97_audio_state.sem,1),
};

static int ac97_audio_open(struct inode *inode, struct file *file)
{
	return pxa_audio_attach(inode, file, &ac97_audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to pxa_audio_attach().
 */

static struct file_operations ac97_audio_fops = {
	open:		ac97_audio_open,
	owner:		THIS_MODULE
};


static int __init pxa_ac97_init(void)
{
	int ret;
	struct ac97_codec *dummy;

	ret = pxa_ac97_get(&dummy);
	if (ret)
		return ret;

	update_audio_in ();

	ac97_audio_state.dev_dsp = register_sound_dsp(&ac97_audio_fops, -1);
	pxa_ac97_codec.dev_mixer = register_sound_mixer(&mixer_fops, -1);

	return 0;
}

static void __exit pxa_ac97_exit(void)
{
	unregister_sound_dsp(ac97_audio_state.dev_dsp);
	unregister_sound_mixer(pxa_ac97_codec.dev_mixer);
	pxa_ac97_put();
}


module_init(pxa_ac97_init);
module_exit(pxa_ac97_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("AC97 interface for the Cotula chip");
MODULE_LICENSE("GPL");
