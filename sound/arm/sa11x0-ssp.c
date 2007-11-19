/*
 *  linux/sound/arm/sa11x0-ssp.c
 *
 *  Copyright (C) 2003 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include <asm/dma.h>

#include "sa11x0-ssp.h"

struct sa11x0_ssp_runtime {
	int		use;
	dma_regs_t	*dma;
	dma_addr_t	dma_addr;
	unsigned int	dma_period;
	unsigned int	dma_size;
};

struct sa11x0_ssp_data {
	struct device			*dev;
	struct sa11x0_snd_ops		*ops;
	spinlock_t			lock;
	struct semaphore		sem;
	struct sa11x0_ssp_runtime	playback, capture;
};

static snd_pcm_hardware_t sa11x0_ssp_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= 0,
	.rate_min		= 0,
	.rate_max		= 0,
	.channels_min		= 2,
	.channels_max		= 2,
	.buffer_bytes_max	= 0,
	.period_bytes_min	= 32,
	.period_bytes_max	= MAX_DMA_SIZE,
	.periods_min		= 4,
	.periods_max		= 1024,
};

static void callback(void *data)
{
	struct sa11x0_ssp_data *ssd = data;
}

static int sa11x0_ssp_open(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	struct sa11x0_ssp_data *ssd = substream->private_data;
	struct sa11x0_ssp_runtime *ssr;
	int ret;

	memcpy(&runtime->hw, &sa11x0_ssp_hardware, sizeof(sa11x0_ssp_hardware));

	down(&ssd->sem);

	if (ssd->playback.use++ == 0) {
		ret = sa1100_request_dma(DMA_Ser4SSPWr, "SSP Tx", callback, ssd, &ssd->playback.dma);
		if (ret)
			goto err_play;
	}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    ssd->capture.use++ == 0) {
		ret = sa1100_request_dma(DMA_Ser4SSPRd, "SSP Rx", callback, ssd, &ssd->capture.dma);
		if (ret)
			goto err_cap;
	}

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ssr = &ssd->playback;
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		ssr = &ssd->capture;
		break;
	}

	runtime->private_data = ssr;

	ret = ssd->ops->open(ssd->dev, runtime);
	if (ret)
		goto err_cap;

	up(&ssd->sem);

	return 0;

 err_cap:
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    --ssd->capture.use == 0 && ssd->capture.dma) {
		sa1100_free_dma(ssd->capture.dma);
		ssd->capture.dma = NULL;
	}
 err_play:
	if (--ssd->playback.use == 0 && ssd->playback.dma) {
		sa1100_free_dma(ssd->playback.dma);
		ssd->playback.dma = NULL;
	}
	up(&ssd->sem);
	return ret;
}

static int sa11x0_ssp_close(snd_pcm_substream_t *substream)
{
	struct sa11x0_ssp_data *ssd = substream->private_data;

	down(&ssd->sem);

	ssd->ops->close(ssd->dev);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    --ssd->capture.use == 0 && ssd->capture.dma) {
		sa1100_free_dma(ssd->capture.dma);
		ssd->capture.dma = NULL;
	}
	if (--ssd->playback.use == 0 && ssd->playback.dma) {
		sa1100_free_dma(ssd->playback.dma);
		ssd->playback.dma = NULL;
	}

	up(&ssd->sem);

	return 0;
}

static int
sa11x0_ssp_hw_params(snd_pcm_substream_t *substream, snd_pcm_hw_params_t *hw)
{
	struct sa11x0_ssp_data *ssd = substream->private_data;
	unsigned int rate = params_rate(hw);

	ssd->ops->samplerate(ssd->dev, rate);

	/* allocate memory */

	return 0;
}

static int sa11x0_ssp_playback_prepare(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	struct sa11x0_ssp_runtime *ssr = substream->runtime->private_data;

	ssr->dma_period = snd_pcm_lib_period_bytes(substream);
	ssr->dma_addr = runtime->dma_addr;
	ssr->dma_size = snd_pcm_lib_buffer_bytes(substream);

	return 0;
}

static int sa11x0_ssp_playback_trigger(snd_pcm_substream_t *substream, int cmd)
{
}

static snd_pcm_uframes_t sa11x0_ssp_pointer(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	struct sa11x0_ssp_data *ssd = substream->private_data;
	struct sa11x0_ssp_runtime *ssr = runtime->private_data;
	unsigned long flags;
	dma_addr_t off;

	spin_lock_irqsave(&ssd->lock, flags);
	off = sa1100_get_dma_pos(ssr->dma) - runtime->dma_addr;
	spin_unlock_irqrestore(&ssd->lock, flags);

	return bytes_to_frames(runtime, off);
}

static snd_pcm_ops_t sa11x0_ssp_capture_ops = {
	.open		= sa11x0_ssp_open,
	.close		= sa11x0_ssp_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= sa11x0_ssp_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.prepare	= sa11x0_ssp_playback_prepare,
	.trigger	= sa11x0_ssp_playback_trigger,
	.pointer	= sa11x0_ssp_pointer,
};

static snd_pcm_ops_t sa11x0_ssp_playback_ops = {
	.open		= sa11x0_ssp_open,
	.close		= sa11x0_ssp_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= sa11x0_ssp_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.prepare	= sa11x0_ssp_playback_prepare,
	.trigger	= sa11x0_ssp_playback_trigger,
	.pointer	= sa11x0_ssp_pointer,
};

snd_pcm_t *sa11x0_ssp_create(snd_card_t *card)
{
	snd_pcm_t *pcm;
	int err;

	err = snd_pcm_new(card, "SA11x0-SSP", 0, 1, 1, &pcm);
	if (err) {
		pcm = ERR_PTR(err);
		goto out;
	}

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &sa11x0_ssp_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &sa11x0_ssp_capture_ops);

 out:
	return pcm;
}
