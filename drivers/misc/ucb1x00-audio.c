/*
 *  linux/drivers/misc/ucb1x00-audio.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/list.h>
#include <linux/device.h>

#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "ucb1x00.h"

#include "../sound/oss/sa1100-audio.h"

#define MAGIC	0x41544154

struct ucb1x00_audio {
	struct file_operations	fops;
	struct file_operations	mops;
	struct ucb1x00		*ucb;
	audio_stream_t		output_stream;
	audio_stream_t		input_stream;
	audio_state_t		state;
	unsigned int		rate;
	int			dev_id;
	int			mix_id;
	unsigned int		daa_oh_bit;
	unsigned int		telecom;
	unsigned int		magic;
	unsigned int		ctrl_a;
	unsigned int		ctrl_b;

	/* mixer info */
	unsigned int		mod_cnt;
	unsigned short		output_level;
	unsigned short		input_level;
};

#define REC_MASK	(SOUND_MASK_VOLUME | SOUND_MASK_MIC)
#define DEV_MASK	REC_MASK

static int
ucb1x00_mixer_ioctl(struct inode *ino, struct file *filp, uint cmd, ulong arg)
{
	struct ucb1x00_audio *ucba;
	unsigned int val, gain;
	int ret = 0;

	ucba = list_entry(filp->f_op, struct ucb1x00_audio, mops);

	if (_IOC_TYPE(cmd) != 'M')
		return -EINVAL;

	if (cmd == SOUND_MIXER_INFO) {
		struct mixer_info mi;

		strncpy(mi.id, "UCB1x00", sizeof(mi.id));
		strncpy(mi.name, "Philips UCB1x00", sizeof(mi.name));
		mi.modify_counter = ucba->mod_cnt;
		return copy_to_user((void *)arg, &mi, sizeof(mi)) ? -EFAULT : 0;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		unsigned int left, right;

		ret = get_user(val, (unsigned int *)arg);
		if (ret)
			goto out;

		left  = val & 255;
		right = val >> 8;

		if (left > 100)
			left = 100;
		if (right > 100)
			right = 100;

		gain = (left + right) / 2;

		ret = -EINVAL;
		if (!ucba->telecom) {
			switch(_IOC_NR(cmd)) {
			case SOUND_MIXER_VOLUME:
				ucba->output_level = gain | gain << 8;
				ucba->mod_cnt++;
				ucba->ctrl_b = (ucba->ctrl_b & 0xff00) |
					       (((100 - gain) * 31) / 100 );
				ucb1x00_reg_write(ucba->ucb, UCB_AC_B,
						  ucba->ctrl_b);
				ret = 0;
				break;

			case SOUND_MIXER_MIC:
				ucba->input_level = gain | gain << 8;
				ucba->mod_cnt++;
				ucba->ctrl_a = (ucba->ctrl_a & 0x7f) |
					       (((gain * 31) / 100) << 7);
				ucb1x00_reg_write(ucba->ucb, UCB_AC_A,
						  ucba->ctrl_a);
				ret = 0;
				break;
			}
		}
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		switch (_IOC_NR(cmd)) {
		case SOUND_MIXER_VOLUME:
			val = ucba->output_level;
			break;

		case SOUND_MIXER_MIC:
			val = ucba->input_level;
			break;

		case SOUND_MIXER_RECSRC:
		case SOUND_MIXER_RECMASK:
			val = ucba->telecom ? 0 : REC_MASK;
			break;

		case SOUND_MIXER_DEVMASK:
			val = ucba->telecom ? 0 : DEV_MASK;
			break;

		case SOUND_MIXER_CAPS:
		case SOUND_MIXER_STEREODEVS:
			val = 0;
			break;

		default:
			val = 0;
			ret = -EINVAL;
			break;
		}

		if (ret == 0)
			ret = put_user(val, (int *)arg);
	}
 out:
	return ret;
}

static int ucb1x00_audio_setrate(struct ucb1x00_audio *ucba, int rate)
{
	unsigned int div_rate = ucb1x00_clkrate(ucba->ucb) / 32;
	unsigned int div;

	div = (div_rate + (rate / 2)) / rate;
	if (div < 17) /* simpad change  */
		div = 17;
	if (div > 127)
		div = 127;

	ucba->ctrl_a = (ucba->ctrl_a & ~0x7f) | div;

	if (ucba->telecom) {
		ucb1x00_reg_write(ucba->ucb, UCB_TC_B, 0);
		ucb1x00_set_telecom_divisor(ucba->ucb, div * 32);
		ucb1x00_reg_write(ucba->ucb, UCB_TC_A, ucba->ctrl_a);
		ucb1x00_reg_write(ucba->ucb, UCB_TC_B, ucba->ctrl_b);
	} else {
		ucb1x00_reg_write(ucba->ucb, UCB_AC_B, 0);
		ucb1x00_set_audio_divisor(ucba->ucb, div * 32);
		ucb1x00_reg_write(ucba->ucb, UCB_AC_A, ucba->ctrl_a);
		ucb1x00_reg_write(ucba->ucb, UCB_AC_B, ucba->ctrl_b);
	}

	ucba->rate = div_rate / div;
	if (rate >= 22050 )
		ucba->rate = 20050;

	return ucba->rate;
}

static int ucb1x00_audio_getrate(struct ucb1x00_audio *ucba)
{
	return ucba->rate;
}

static void ucb1x00_audio_startup(void *data)
{
	struct ucb1x00_audio *ucba = data;

	ucb1x00_enable(ucba->ucb);
	ucb1x00_audio_setrate(ucba, ucba->rate);

	ucb1x00_reg_write(ucba->ucb, UCB_MODE, UCB_MODE_DYN_VFLAG_ENA);

	/*
	 * Take off-hook
	 */
	if (ucba->daa_oh_bit)
		ucb1x00_io_write(ucba->ucb, 0, ucba->daa_oh_bit);
}

static void ucb1x00_audio_shutdown(void *data)
{
	struct ucb1x00_audio *ucba = data;

	/*
	 * Place on-hook
	 */
	if (ucba->daa_oh_bit)
		ucb1x00_io_write(ucba->ucb, ucba->daa_oh_bit, 0);

	ucb1x00_reg_write(ucba->ucb, ucba->telecom ? UCB_TC_B : UCB_AC_B, 0);
	ucb1x00_disable(ucba->ucb);
}

static int
ucb1x00_audio_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	struct ucb1x00_audio *ucba;
	int val, ret = 0;

	ucba = list_entry(file->f_op, struct ucb1x00_audio, fops);

	/*
	 * Make sure we have our magic number
	 */
	if (ucba->magic != MAGIC)
		return -ENODEV;

	switch (cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *)arg);
		if (ret)
			return ret;
		if (val != 0)
			return -EINVAL;
		val = 0;
		break;

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		val = 1;
		break;

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (int *)arg);
		if (ret)
			return ret;
		val = ucb1x00_audio_setrate(ucba, val);
		break;

	case SOUND_PCM_READ_RATE:
		val = ucb1x00_audio_getrate(ucba);
		break;

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		val = AFMT_S16_LE;
		break;

	default:
		return ucb1x00_mixer_ioctl(inode, file, cmd, arg);
	}

	return put_user(val, (int *)arg);
}

static int ucb1x00_audio_open(struct inode *inode, struct file *file)
{
	struct ucb1x00_audio *ucba;

	ucba = list_entry(file->f_op, struct ucb1x00_audio, fops);

	return sa1100_audio_attach(inode, file, &ucba->state);
}

static struct ucb1x00_audio *ucb1x00_audio_alloc(struct ucb1x00 *ucb)
{
	struct ucb1x00_audio *ucba;

	ucba = kmalloc(sizeof(*ucba), GFP_KERNEL);
	if (ucba) {
		memset(ucba, 0, sizeof(*ucba));
		ucb->audio_data = ucba;

		ucba->magic = MAGIC;
		ucba->ucb = ucb;
		ucba->fops.owner = THIS_MODULE;
		ucba->fops.open  = ucb1x00_audio_open;
		ucba->mops.owner = THIS_MODULE;
		ucba->mops.ioctl = ucb1x00_mixer_ioctl;
		ucba->state.output_stream = &ucba->output_stream;
		ucba->state.input_stream = &ucba->input_stream;
		ucba->state.data = ucba;
		ucba->state.hw_init = ucb1x00_audio_startup;
		ucba->state.hw_shutdown = ucb1x00_audio_shutdown;
		ucba->state.client_ioctl = ucb1x00_audio_ioctl;

		/* There is a bug in the StrongARM causes corrupt MCP data to be sent to
		 * the codec when the FIFOs are empty and writes are made to the OS timer
		 * match register 0. To avoid this we must make sure that data is always
		 * sent to the codec.
		 */
		ucba->state.need_tx_for_rx = 1;

		init_MUTEX(&ucba->state.sem);
		ucba->rate = 8000;
	}
	return ucba;
}

static int ucb1x00_audio_add(struct class_device *_dev)
{
	struct mcp            *mcp;
	struct ucb1x00        *ucb;
	struct ucb1x00_audio  *a;

	ucb = classdev_to_ucb1x00(_dev);
	mcp = ucb->mcp;

	a = ucb1x00_audio_alloc(ucb);
	if (a) {
		a->telecom = 1; /* dev->id == UCBDEVID_TELECOM; */

		a->input_stream.dev = _dev->dev;
		a->output_stream.dev = _dev->dev;
		a->ctrl_a = 0;

		if (a->telecom) {
			a->input_stream.dma_dev = mcp->dma_telco_rd;
			a->input_stream.id = "UCB1x00 telco in";
			a->output_stream.dma_dev = mcp->dma_telco_wr;
			a->output_stream.id = "UCB1x00 telco out";
			a->ctrl_b = UCB_TC_B_IN_ENA|UCB_TC_B_OUT_ENA;
#if 0
			a->daa_oh_bit = UCB_IO_8;

			ucb1x00_enable(_dev);
			ucb1x00_io_write(_dev, a->daa_oh_bit, 0);
			ucb1x00_io_set_dir(_dev, UCB_IO_7 | UCB_IO_6, a->daa_oh_bit);
			ucb1x00_disable(_dev);
#endif
		} else {
			a->input_stream.dma_dev  = mcp->dma_audio_rd;
			a->input_stream.id = "UCB1x00 audio in";
			a->output_stream.dma_dev = mcp->dma_audio_wr;
			a->output_stream.id = "UCB1x00 audio out";
			a->ctrl_b = UCB_AC_B_IN_ENA|UCB_AC_B_OUT_ENA;
		}

		a->dev_id = register_sound_dsp(&a->fops, -1);
		a->mix_id = register_sound_mixer(&a->mops, -1);

		printk("Sound: UCB1x00 %s: dsp id %d mixer id %d\n",
			a->telecom ? "telecom" : "audio",
			a->dev_id, a->mix_id);
	}

	return a ? 0 : -ENOMEM;
}

static void ucb1x00_audio_remove(struct  class_device *_dev)
{
	struct ucb1x00     *ucb = classdev_to_ucb1x00(_dev);
	struct ucb1x00_audio *a = ucb->audio_data;

	unregister_sound_dsp(a->dev_id);
	unregister_sound_mixer(a->mix_id);

	kfree(a);
}

static int ucb1x00_audio_suspend(struct ucb1x00 *ucb, u32 level)
{
	struct ucb1x00_audio *a = ucb->audio_data;

	return sa1100_audio_suspend(&a->state, 0, level);
}

static int ucb1x00_audio_resume(struct ucb1x00 *ucb)
{
	struct ucb1x00_audio *a = ucb->audio_data;

	return sa1100_audio_resume(&a->state, RESUME_ENABLE);
}

static struct ucb1x00_class_interface ucb1x00_audio_interface = {
	.interface  = {
		.add     = ucb1x00_audio_add,
		.remove  = ucb1x00_audio_remove,
	},
	.suspend    = ucb1x00_audio_suspend,
	.resume     = ucb1x00_audio_resume,

};


static int __init ucb1x00_audio_init(void)
{
	return ucb1x00_register_interface(&ucb1x00_audio_interface);
}

static void __exit ucb1x00_audio_exit(void)
{
	ucb1x00_unregister_interface(&ucb1x00_audio_interface);
}

module_init(ucb1x00_audio_init);
module_exit(ucb1x00_audio_exit);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 telecom/audio driver");
MODULE_LICENSE("GPL");
