/* 
 *
 * Copyright (C) 2003 Hewlett-Packard Company
 *
 * This code is released under GPL (GNU Public License) with
 * absolutely no warranty. Please see http://www.gnu.org/ for a
 * complete discussion of the GPL.
 *
 * based on code by:
 *
 * Copyright (C) 2003 Christian Pellegrin
 * Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 * Copyright (c) 2002 Tomas Kasparek <tomas.kasparek@seznam.cz>
 * Copyright (c) 2002 Hewlett-Packard Company
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 */

#include <linux/module.h>
#include <sound/driver.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/i2c.h>
#include <linux/kmod.h>
#include <linux/dma-mapping.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch-pxa/h5400-asic.h>
#include "pxa-i2s.h"
#include <asm/arch/pxa-regs.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/driver.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>
#include <sound/ak4535.h>

#include "../i2c/ak4535.h"

#undef DEBUG
#define DEBUG 0         /* 1 enables, 0 disables debug output */
#if DEBUG
#define DPRINTK( format, x... )  printk( format, ## x )
#else
#define DPRINTK( format, x... ) while(0)
#endif

#define chip_t snd_card_h5400_ak4535_t

#define H5400_AK4535_BSIZE 65536
#define H5400_AK4535_MAX_DMA_SIZE 4096

extern ak4535_device_controls_t ak4535_device_controls;

static char *id = NULL;	/* ID for this card */
static int max_periods_allowed = 255; /* @@@@ check on this */

MODULE_PARM(id, "s");
MODULE_PARM_DESC(id, "ID string for PXA + AK4535 soundcard.");


static struct snd_card_h5400_ak4535 *h5400_ak4535 = NULL;

#define AUDIO_RATE_DEFAULT	44100

static unsigned int period_sizes[] = { 128, 256, 512, 1024, 2048, 4096 };
 
#define PERIOD_SIZES sizeof(period_sizes) / sizeof(period_sizes[0])
 
static snd_pcm_hw_constraint_list_t hw_constraints_period_sizes = {
        .count = PERIOD_SIZES,
        .list = period_sizes,
        .mask = 0
};


static void h5400_ak4535_set_samplerate(snd_card_h5400_ak4535_t *h5400_ak4535, long val, int force)
{
	struct i2c_client *ak4535 = h5400_ak4535->ak4535;
	struct ak4535_cfg cfg;

	DPRINTK("set_samplerate rate: %ld\n", val);

	if (val == h5400_ak4535->samplerate && !force)
		return;

	cfg.fs = pxa_i2s_set_samplerate (val);
	cfg.format = FMT_I2S;
	
	i2c_clients_command(ak4535->adapter, I2C_AK4535_CONFIGURE, &cfg);
	h5400_ak4535->samplerate = val;

	DPRINTK("set sample rate done\n");
}

static void h5400_ak4535_audio_init(snd_card_h5400_ak4535_t *h5400_ak4535)
{
	DPRINTK("%s\n", __FUNCTION__);
	
	ak4535_device_controls.set_codec_power(0);
	ak4535_device_controls.set_codec_power(0);

	pxa_i2s_init ();
	mdelay(1);

	ak4535_device_controls.set_codec_power(1);
	ak4535_device_controls.set_codec_power(1);

	/* Wait for the AK4535 to wake up */
	mdelay(1);

	/* Initialize the AK4535 internal state */
	i2c_clients_command(h5400_ak4535->ak4535->adapter, I2C_AK4535_OPEN, 0);

	ak4535_device_controls.set_audio_mute(0);

	h5400_ak4535_set_samplerate(h5400_ak4535, h5400_ak4535->samplerate, 1);

	DPRINTK("%s done\n", __FUNCTION__);
}

static void h5400_ak4535_audio_shutdown(snd_card_h5400_ak4535_t *h5400_ak4535)
{
	DPRINTK("%s\n", __FUNCTION__);
	

	/* disable the audio power and all signals leading to the audio chip */
	ak4535_device_controls.set_audio_mute(1);
	i2c_clients_command(h5400_ak4535->ak4535->adapter, I2C_AK4535_CLOSE, 0);

	pxa_i2s_shutdown ();

	ak4535_device_controls.set_codec_power(0);
	ak4535_device_controls.set_codec_power(0);

	i2c_use_client(h5400_ak4535->ak4535);

	DPRINTK("%s done\n", __FUNCTION__);
}

static void audio_dma_setup_descriptors_ring(h5400_audio_stream_t *s)
{
	int i, j;
	unsigned int p_size, d_size, di;
	dma_addr_t p, d_buf;
	d_size = sizeof(pxa_dma_desc);

	/* set up the number of descriptors for this device */
	s->dma_size = H5400_AK4535_MAX_DMA_SIZE;
	p_size = s->buffer_size / s->periods; /* determine period size */
	s->dma_dpp = (p_size -1) / s->dma_size + 1;

	/* allocate storage space for descriptors ring,
	 * include 15 bytes extra for alignment issue */
	s->dma_dring_store = dma_alloc_coherent(NULL, d_size * s->periods * s->dma_dpp + 15,
						&s->dma_dring_store_p, GFP_KERNEL);

	/* setup the descriptors ring pointer, aligned on a 16 bytes boundary */
	s->dma_dring = ((dma_addr_t) s->dma_dring_store & 0xf) ?
			 (pxa_dma_desc *) (((dma_addr_t) s->dma_dring_store & ~0xf) + 16) :
			 (pxa_dma_desc *) s->dma_dring_store;
	s->dma_dring_p = s->dma_dring_store_p +
			  (dma_addr_t) s->dma_dring - (dma_addr_t) s->dma_dring_store;
	
	/* fill the descriptors ring */
	di = 0; /* current descriptor index */
	p = s->buffer; /* p: current period (address in buffer) */
	d_buf = s->buffer; /* d_buf: address in buffer
			             for current dma descriptor */
	 /* iterate over periods */
	for (i = 0; i < s->periods; i++, p += p_size) { 
		/* iterate over dma descriptors in a period */
		for (j = 0; j < s->dma_dpp; j++, di++, d_buf += s->dma_size) {
			/* link to next descriptor */
			s->dma_dring[di].ddadr = s->dma_dring_p + (di + 1) * d_size;
			if (s->stream_id == PLAYBACK) {
				s->dma_dring[di].dsadr = d_buf;
				s->dma_dring[di].dtadr = __PREG(SADR);
				s->dma_dring[di].dcmd = DCMD_TXPCDR;
			}
			else {
				s->dma_dring[di].dsadr = __PREG(SADR);
				s->dma_dring[di].dtadr = d_buf;
				s->dma_dring[di].dcmd = DCMD_RXPCDR;
			}
			s->dma_dring[di].dcmd |=
			  ((p + p_size - d_buf) >= s->dma_size) ?
			  s->dma_size : p + p_size - d_buf; /* transfer length */
		}
		s->dma_dring[di].dcmd |= DCMD_ENDIRQEN; /* period irq */
	}
	s->dma_dring[di - 1].ddadr = s->dma_dring_p; /* close the ring */
}

static void audio_dma_free_descriptors_ring(h5400_audio_stream_t *s)
{
	unsigned int d_size = sizeof(pxa_dma_desc);
	if (s->dma_dring_store) {
		dma_free_coherent(NULL, d_size * s->periods * s->dma_dpp + 15,
				  s->dma_dring_store, s->dma_dring_store_p);
		s->dma_dring_store = NULL;
		s->dma_dring = NULL;
		s->dma_dring_store_p = 0;
		s->dma_dring_p = 0;
	}	
}

static void audio_dma_irq(int chan, void *dev_id, struct pt_regs *regs)
{
	h5400_audio_stream_t *s = (h5400_audio_stream_t *) dev_id;
	int ch = s->dma_ch;
	u32 dcsr;

	dcsr = DCSR(ch);

	DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;

	if (dcsr & DCSR_BUSERR) {
		printk("bus error\n");
		return;
	}

	if (dcsr & DCSR_ENDINTR) {
		snd_pcm_period_elapsed(s->stream);
		return;
	}
}

static void audio_dma_request(h5400_audio_stream_t *s, void (*callback)(int , void *, struct pt_regs *))
{
	int err;

	DPRINTK("%s\n", __FUNCTION__);

	err = pxa_request_dma(s->id, DMA_PRIO_LOW, 
			      audio_dma_irq, s);
	if (err < 0) {
		printk("panic: cannot allocate DMA for %s\n", s->id);
		/* CHRI-TODO: handle this error condition gracefully */
	}

	s->dma_ch = err;
	if (s->stream_id == CAPTURE) {
		DPRINTK("%s capture\n", __FUNCTION__);
		s->drcmr = &DRCMRRXSADR;
		*(s->drcmr) = s->dma_ch | DRCMR_MAPVLD;
		printk ("drcmr capture: %x set to %x\n", (u32) s->drcmr, *(s->drcmr));
	}
	else {
		DPRINTK("%s playback\n", __FUNCTION__);
		s->drcmr = &DRCMRTXSADR;
		*(s->drcmr) = s->dma_ch | DRCMR_MAPVLD;
		printk ("drcmr playback: %x set to %x\n", (u32) s->drcmr, *(s->drcmr));

	}
	
	DPRINTK("%s done\n", __FUNCTION__);
}

static void audio_dma_free(h5400_audio_stream_t *s)
{
	DPRINTK("%s\n", __FUNCTION__);

	pxa_free_dma(s->dma_ch);

	DPRINTK("%s done\n", __FUNCTION__);
}


static u_int audio_get_dma_pos(h5400_audio_stream_t *s)
{
	snd_pcm_substream_t * substream = s->stream;
	snd_pcm_runtime_t *runtime = substream->runtime;
	unsigned int offset_bytes;
	unsigned int offset;
	int ch = s->dma_ch;
	u32 pos;

	DPRINTK("%s\n", __FUNCTION__);

	if (s->stream_id == CAPTURE)
		pos = DTADR(ch);
	else
		pos = DSADR(ch);
	offset_bytes =  pos - s->buffer; 
	offset = bytes_to_frames(runtime, offset_bytes);

	DPRINTK("%s done, dma position is %d (in bytes is %d, we are in buffer %d)\n",
		__FUNCTION__, offset , offset_bytes, s->cur_dma_buf);

	return offset;
}

static void audio_stop_dma(h5400_audio_stream_t *s)
{
	DPRINTK("%s\n", __FUNCTION__);

	DCSR(s->dma_ch) = DCSR_STOPIRQEN;

	while (! (DCSR(s->dma_ch) & DCSR_STOPSTATE)) {
		if (!in_interrupt())
			schedule();
	}
	DPRINTK("%s finished waiting\n", __FUNCTION__);
}

static void audio_start_dma(h5400_audio_stream_t *s, int restart)
{
	DPRINTK("%s\n", __FUNCTION__);

	/* kick the DMA */
	DDADR(s->dma_ch) = s->dma_dring_p;
	DCSR(s->dma_ch) = DCSR_RUN;	
	s->dma_running = 1;
}

static int snd_card_h5400_ak4535_pcm_trigger(stream_id_t stream_id,
	snd_pcm_substream_t * substream, int cmd)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	h5400_audio_stream_t *s = chip->s[stream_id];
#if DEBUG
	snd_pcm_runtime_t *runtime = substream->runtime; 	
#endif
	int ret = 0;
	
	DPRINTK("pcm_trigger id: %d cmd: %d\n", stream_id, cmd);

	DPRINTK("  sound: %d x %d [Hz]\n", runtime->channels, runtime->rate);
	DPRINTK("  periods: %ld x %ld [fr]\n", (unsigned long)runtime->periods,
		(unsigned long) runtime->period_size);
	DPRINTK("  buffer_size: %ld [fr]\n", (unsigned long)runtime->buffer_size);
	DPRINTK("  dma_addr %p\n", (char *)runtime->dma_addr);
	DPRINTK("  dma_running PLAYBACK %d CAPTURE %d\n", 
		chip->s[PLAYBACK]->dma_running, chip->s[CAPTURE]->dma_running);


	spin_lock(&s->dma_lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		DPRINTK("trigger START\n");
		if (s->dma_running) {
			audio_stop_dma(s);
		}
		s->active = 1;
		audio_start_dma(s, 0);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* requested stream suspend */
		audio_stop_dma(s);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		DPRINTK("trigger STOP\n");
		s->active = 0;
		audio_stop_dma(s);
		break;
	default:
		DPRINTK("trigger UNKNOWN\n");
		ret = -EINVAL;
		break;
	}
	spin_unlock(&s->dma_lock);
	return 0;

}

static snd_pcm_hardware_t snd_h5400_ak4535_capture =
{
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
				   SNDRV_PCM_INFO_BLOCK_TRANSFER |
				   SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID),
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
				   SNDRV_PCM_RATE_22050 | \
				   SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
				   SNDRV_PCM_RATE_KNOT),
	.rate_min		= 8000,
	.rate_max		= 48000,
	.channels_min		= 2,
	.channels_max		= 2,
	.buffer_bytes_max	= H5400_AK4535_BSIZE, /* was 16380 */
	.period_bytes_min	= 64,    /* was 128 but this is what
					    sa11xx-uda1341 uses */
	.period_bytes_max	= H5400_AK4535_MAX_DMA_SIZE,  /* <= H5400_AK4535_MAX_DMA_SIZE from
					    asm/arch-sa1100/dma.h */
	.periods_min		= 2,
	.periods_max		= 32,
	.fifo_size		= 0,
};

static snd_pcm_hardware_t snd_h5400_ak4535_playback =
{
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
				   SNDRV_PCM_INFO_BLOCK_TRANSFER |
				   SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
				   SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
                                   SNDRV_PCM_RATE_22050 | 
				   SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |
				   SNDRV_PCM_RATE_KNOT),
	.rate_min		= 8000,
	.rate_max		= 48000,
	.channels_min		= 2,
	.channels_max		= 2,
	.buffer_bytes_max	= H5400_AK4535_BSIZE, 
	.period_bytes_min	= 64,   /* was 128 changed to same as
					   sa11xx-uda1341 jamey 2003-nov-20 */
	.period_bytes_max	= H5400_AK4535_MAX_DMA_SIZE, /* <= H5400_AK4535_MAX_DMA_SIZE from
					   asm/arch-sa1100/dma.h */
	.periods_min		= 2,
	.periods_max		= 32,   /* dropped from 255 to  lower
					   overhead costs */
	.fifo_size		= 0,
};

static unsigned int rates[] = {
	8000,  
	16000, 22050,
	44100, 48000,
};

#define RATES sizeof(rates) / sizeof(rates[0])

static snd_pcm_hw_constraint_list_t hw_constraints_rates = {
	.count	= RATES,
	.list	= rates,
	.mask	= 0,
};

static int snd_card_h5400_ak4535_playback_open(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	int err;
        
	DPRINTK("playback_open\n");

	if (chip->after_suspend) {
		chip->after_suspend = 0;
	}

	chip->s[PLAYBACK]->stream = substream;
	chip->s[PLAYBACK]->sent_periods = 0;
	chip->s[PLAYBACK]->sent_total = 0;

	runtime->hw = snd_h5400_ak4535_playback;
	runtime->hw.periods_max = max_periods_allowed;

/* @@@@ check this out also -jamey 2003-nov-20 */
#if 1
	snd_pcm_hw_constraint_list(runtime, 0,
                                   SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
                                   &hw_constraints_period_sizes);
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, H5400_AK4535_BSIZE, H5400_AK4535_BSIZE);
#else
	if ((err = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS)) < 0)
		return err;
#endif

	if ((err = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					      &hw_constraints_rates)) < 0)
		return err;
        
	return 0;
}

static int snd_card_h5400_ak4535_playback_close(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);

	DPRINTK("%s\n", __FUNCTION__);

	chip->s[PLAYBACK]->dma_running = 0;
	chip->s[PLAYBACK]->stream = NULL;
      
	return 0;
}

static int snd_card_h5400_ak4535_playback_ioctl(snd_pcm_substream_t * substream,
					      unsigned int cmd, void *arg)
{
	DPRINTK("playback_ioctl cmd: %d\n", cmd);
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int snd_card_h5400_ak4535_playback_prepare(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
        
	DPRINTK("playback_prepare\n");

	/* set requested samplerate */
	h5400_ak4535_set_samplerate(chip, runtime->rate, 0);
        
	return 0;
}

static int snd_card_h5400_ak4535_playback_trigger(snd_pcm_substream_t * substream, int cmd)
{
	DPRINTK("playback_trigger\n");
	return snd_card_h5400_ak4535_pcm_trigger(PLAYBACK, substream, cmd);
}

static snd_pcm_uframes_t snd_card_h5400_ak4535_playback_pointer(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_uframes_t pos;

	DPRINTK("playback_pointer\n");        
	pos = audio_get_dma_pos(chip->s[PLAYBACK]);
	return pos;
}

static int snd_card_h5400_ak4535_capture_open(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	int err;
	
	DPRINTK("%s: \n", __FUNCTION__);

	if (chip->after_suspend) {
		chip->after_suspend = 0;
	}

	chip->s[CAPTURE]->stream = substream;
	chip->s[CAPTURE]->sent_periods = 0;
	chip->s[CAPTURE]->sent_total = 0;
	
	runtime->hw = snd_h5400_ak4535_capture;
	runtime->hw.periods_max = max_periods_allowed;

/* @@@@ check this out also -jamey 2003-nov-20 */
#if 1 
	snd_pcm_hw_constraint_list(runtime, 0,
                                   SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
                                   &hw_constraints_period_sizes);
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, H5400_AK4535_BSIZE, H5400_AK4535_BSIZE);
#else
	if ((err = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS)) < 0)
		return err;
#endif

	if ((err = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					      &hw_constraints_rates)) < 0)
		return err;
        
	return 0;
}

static int snd_card_h5400_ak4535_capture_close(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);

	DPRINTK("%s:\n", __FUNCTION__);

	/* @@@ check this -- sa11xx-uda1341 does not do this */
	chip->s[CAPTURE]->dma_running = 0;
	chip->s[CAPTURE]->stream = NULL;
      
	return 0;
}

static int snd_card_h5400_ak4535_capture_ioctl(snd_pcm_substream_t * substream,
					     unsigned int cmd, void *arg)
{
	DPRINTK("%s:\n", __FUNCTION__);
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int snd_card_h5400_ak4535_capture_prepare(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;

	DPRINTK("%s\n", __FUNCTION__);

	/* set requested samplerate */
	h5400_ak4535_set_samplerate(chip, runtime->rate, 0);
        chip->samplerate = runtime->rate;

	return 0;
}

static int snd_card_h5400_ak4535_capture_trigger(snd_pcm_substream_t * substream, int cmd)
{
	DPRINTK("%s:\n", __FUNCTION__);
	return snd_card_h5400_ak4535_pcm_trigger(CAPTURE, substream, cmd);
}

static snd_pcm_uframes_t snd_card_h5400_ak4535_capture_pointer(snd_pcm_substream_t * substream)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_uframes_t pos;

	DPRINTK("%s:\n", __FUNCTION__);
	pos = audio_get_dma_pos(chip->s[CAPTURE]);
	return pos;
}

static int snd_h5400_ak4535_hw_params(snd_pcm_substream_t * substream,
				    snd_pcm_hw_params_t * hw_params)
{
	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);
        int err;

	DPRINTK("hw_params, wanna allocate %d\n", params_buffer_bytes(hw_params));

	err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	if (err<0) {
		printk("snd_pcm_lib_malloc_pages failed!\n");
	}
	DPRINTK("hw_params, err is %d\n", err);

	if (chip->s[PLAYBACK]->stream == substream) {
		if (chip->s[PLAYBACK]->dma_dring)
			audio_dma_free_descriptors_ring(chip->s[PLAYBACK]);
		chip->s[PLAYBACK]->buffer = substream->runtime->dma_addr;
		chip->s[PLAYBACK]->buffer_size = params_buffer_bytes(hw_params);
		chip->s[PLAYBACK]->periods = params_periods(hw_params);
		audio_dma_setup_descriptors_ring(chip->s[PLAYBACK]);
	} else {
		if (chip->s[CAPTURE]->dma_dring)
			audio_dma_free_descriptors_ring(chip->s[CAPTURE]);
		chip->s[CAPTURE]->buffer = substream->runtime->dma_addr;
		chip->s[CAPTURE]->buffer_size = params_buffer_bytes(hw_params);
		chip->s[CAPTURE]->periods = params_periods(hw_params);
		audio_dma_setup_descriptors_ring(chip->s[CAPTURE]);
	}
	return err;
}

static int snd_h5400_ak4535_hw_free(snd_pcm_substream_t * substream)
{

	DPRINTK("hw_free\n");

	snd_card_h5400_ak4535_t *chip = snd_pcm_substream_chip(substream);

	/* free the dma descriptors ring */
	if (chip->s[PLAYBACK]->stream == substream) {
		audio_dma_free_descriptors_ring(chip->s[PLAYBACK]);
	} else {
		audio_dma_free_descriptors_ring(chip->s[CAPTURE]);
	}
	return snd_pcm_lib_free_pages(substream);
}

static snd_pcm_ops_t snd_card_h5400_ak4535_playback_ops = {
	.open			= snd_card_h5400_ak4535_playback_open,
	.close			= snd_card_h5400_ak4535_playback_close,
	.ioctl			= snd_card_h5400_ak4535_playback_ioctl,
	.hw_params	        = snd_h5400_ak4535_hw_params,
	.hw_free	        = snd_h5400_ak4535_hw_free,
	.prepare		= snd_card_h5400_ak4535_playback_prepare,
	.trigger		= snd_card_h5400_ak4535_playback_trigger,
	.pointer		= snd_card_h5400_ak4535_playback_pointer,
};

static snd_pcm_ops_t snd_card_h5400_ak4535_capture_ops = {
	.open			= snd_card_h5400_ak4535_capture_open,
	.close			= snd_card_h5400_ak4535_capture_close,
	.ioctl			= snd_card_h5400_ak4535_capture_ioctl,
	.hw_params	        = snd_h5400_ak4535_hw_params,
	.hw_free	        = snd_h5400_ak4535_hw_free,
	.prepare		= snd_card_h5400_ak4535_capture_prepare,
	.trigger		= snd_card_h5400_ak4535_capture_trigger,
	.pointer		= snd_card_h5400_ak4535_capture_pointer,
};


static int __init snd_card_h5400_ak4535_pcm(snd_card_h5400_ak4535_t *h5400_ak4535, int device, int substreams)
{

	int err;
	
	DPRINTK("%s\n", __FUNCTION__);
	// audio power is turned on and a clock is set at the same time so
	// this gives us a default starting rate
	h5400_ak4535->samplerate = AUDIO_RATE_DEFAULT;
	
	if ((err = snd_pcm_new(h5400_ak4535->card, "AK4535 PCM", device,
			       substreams, substreams, &(h5400_ak4535->pcm))) < 0)
		return err;

	// this sets up our initial buffers and sets the dma_type to isa.
	// isa works but I'm not sure why (or if) it's the right choice
	// this may be too large, trying it for now

	err = snd_pcm_lib_preallocate_pages_for_all(h5400_ak4535->pcm, SNDRV_DMA_TYPE_DEV, NULL, 64*1024, 64*1024);
	if (err < 0) {
		printk("preallocate failed with code %d\n", err);
	}
	DPRINTK("%s memory prellocation done\n", __FUNCTION__);
	
	snd_pcm_set_ops(h5400_ak4535->pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_card_h5400_ak4535_playback_ops);
	snd_pcm_set_ops(h5400_ak4535->pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_card_h5400_ak4535_capture_ops);
	h5400_ak4535->pcm->private_data = h5400_ak4535;
	h5400_ak4535->pcm->info_flags = 0;
	strcpy(h5400_ak4535->pcm->name, "AK4535 PCM");

	h5400_ak4535->s[PLAYBACK] = kcalloc(1, sizeof(h5400_audio_stream_t), GFP_KERNEL);
	h5400_ak4535->s[CAPTURE] = kcalloc(1, sizeof(h5400_audio_stream_t), GFP_KERNEL);

	spin_lock_init(&h5400_ak4535->s[PLAYBACK]->dma_lock);
	spin_lock_init(&h5400_ak4535->s[CAPTURE]->dma_lock);
	h5400_ak4535_audio_init(h5400_ak4535);

	/* setup naming */
	h5400_ak4535->s[PLAYBACK]->id = "AK4535 PLAYBACK";
	h5400_ak4535->s[CAPTURE]->id = "AK4535 CAPTURE";

	h5400_ak4535->s[PLAYBACK]->stream_id = PLAYBACK;
	h5400_ak4535->s[CAPTURE]->stream_id = CAPTURE;

	/* setup DMA controller */
	audio_dma_request(h5400_ak4535->s[PLAYBACK], audio_dma_irq);
	audio_dma_request(h5400_ak4535->s[CAPTURE], audio_dma_irq);

	h5400_ak4535->s[PLAYBACK]->dma_running = 0;
	h5400_ak4535->s[CAPTURE]->dma_running = 0;

	/* descriptors ring */
	h5400_ak4535->s[PLAYBACK]->dma_dring = NULL;
	h5400_ak4535->s[PLAYBACK]->dma_dring_p = 0;
	h5400_ak4535->s[PLAYBACK]->dma_dring_store = NULL;
	h5400_ak4535->s[CAPTURE]->dma_dring = NULL;
	h5400_ak4535->s[CAPTURE]->dma_dring_p = 0;
	h5400_ak4535->s[CAPTURE]->dma_dring_store = NULL;
	
	return 0;
}


/* }}} */

/* {{{ module init & exit */

#ifdef CONFIG_PM

static int h5400_ak4535_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	snd_card_h5400_ak4535_t *h5400_ak4535 = pm_dev->data;
	h5400_audio_stream_t *is, *os;	
	snd_card_t *card;
	
	DPRINTK("pm_callback\n");
	DPRINTK("%s: req = %d\n", __FUNCTION__, req);
	
	is = h5400_ak4535->s[PLAYBACK];
	os = h5400_ak4535->s[CAPTURE];
	card = h5400_ak4535->card;
	
	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
		if (os)
			audio_dma_free(os);
		if (is)
			audio_dma_free(is);
		h5400_ak4535_audio_shutdown(h5400_ak4535);
		break;
	case PM_RESUME:  /* enter D0 */
		h5400_ak4535_audio_init(h5400_ak4535);
		if (is)
			audio_dma_request(is, audio_dma_irq);
		if (os)
			audio_dma_request(os, audio_dma_irq);
		h5400_ak4535->after_suspend = 1;
		if (is->dma_running) {
			audio_start_dma(is, 1);
		}
		if (os->dma_running) {
			audio_start_dma(os, 1);
		}
		break;
	}
	DPRINTK("%s: exiting...\n", __FUNCTION__);	
        return 0;
	
}

#endif

void snd_h5400_ak4535_free(snd_card_t *card)
{
	snd_card_h5400_ak4535_t *chip = card->private_data;

	DPRINTK("snd_h5400_ak4535_free\n");

#ifdef CONFIG_PM
	pm_unregister(chip->pm_dev);
#endif

	chip->s[PLAYBACK]->drcmr = 0;
	chip->s[CAPTURE]->drcmr = 0;

	audio_dma_free(chip->s[PLAYBACK]);
	audio_dma_free(chip->s[CAPTURE]);

	kfree(chip->s[PLAYBACK]);
	kfree(chip->s[CAPTURE]);

	chip->s[PLAYBACK] = NULL;
	chip->s[CAPTURE] = NULL;

	kfree(chip);
	card->private_data = NULL;
}

static int __init h5400_ak4535_init(void)
{
	int err = 0;
	snd_card_t *card;
        struct i2c_client *ak4535_get_i2c_client(void);

	DPRINTK("h5400_ak4535_init\n");
        
	/* register the soundcard */
	card = snd_card_new(-1, id, THIS_MODULE, 0);
	if (card == NULL)
		return -ENOMEM;
	h5400_ak4535 = kcalloc(1,sizeof(*h5400_ak4535), GFP_KERNEL);
	if (h5400_ak4535 == NULL)
		return -ENOMEM;
         
	card->private_data = (void *)h5400_ak4535;
	card->private_free = snd_h5400_ak4535_free;
        
	h5400_ak4535->card = card;
	h5400_ak4535->samplerate = AUDIO_RATE_DEFAULT;

#ifdef CONFIG_PM
	h5400_ak4535->pm_dev = pm_register(PM_SYS_DEV, 0, h5400_ak4535_pm_callback);
	if (h5400_ak4535->pm_dev)
		h5400_ak4535->pm_dev->data = h5400_ak4535;
	h5400_ak4535->after_suspend = 0;
#endif
        
	strcpy(card->driver, "AK4535");
	strcpy(card->shortname, "H5xxx AK4535");
	sprintf(card->longname, "Compaq iPAQ H5xxx with AKM AK4535");
        
	/* try to bring in i2c support */
	request_module("i2c-adap-pxa");

	//@@@ h5400_ak4535->ak4535 = i2c_get_client(I2C_DRIVERID_AK4535, I2C_ALGO_PXA, NULL);
	h5400_ak4535->ak4535 = ak4535_get_i2c_client();
	printk("h5400_ak4535_init: card=%p chip=%p clnt=%p\n",
	       h5400_ak4535->card, 
	       h5400_ak4535, 
	       h5400_ak4535->ak4535);
	if (!h5400_ak4535->ak4535) {
		printk("cannot find ak4535!\n");
		goto nodev;
	} else {
		// mixer
		if ((err = snd_card_ak4535_mixer_new(h5400_ak4535->card))) {
			printk("snd_card_ak4535_mixer_new failed\n");
			goto nodev;
		}
		// PCM
		if ((err = snd_card_h5400_ak4535_pcm(h5400_ak4535, 0, 2)) < 0) {
			printk("snd_card_ak4535_pcm failed\n");
			goto nodev;
		}
	}

	if ((err = snd_card_register(card)) == 0) {
		printk("iPAQ audio support initialized\n" );
		return 0;
	}

 nodev:
	snd_card_free(card);
	return err;
}

static void __exit h5400_ak4535_exit(void)
{
	if (h5400_ak4535) {
		snd_card_ak4535_mixer_del(h5400_ak4535->card);
		snd_card_free(h5400_ak4535->card);
		h5400_ak4535 = NULL;
	}
}

module_init(h5400_ak4535_init);
module_exit(h5400_ak4535_exit);

MODULE_DESCRIPTION("AKM AK4535 codec driver");
MODULE_LICENSE("GPL");
