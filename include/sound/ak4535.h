
/*
 * Sound driver for ak4535 - i2c related stuff
 *
 * Copyright  2005 Erik Hovland
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * based on code by:
 * 
 * Copyright (c) 2004 Giorgio Padrin
 */

#ifndef _AK4535_H_
#define _AK4535_H_

#include <asm/dma.h>
#include <sound/driver.h>
#include <sound/core.h>

#define MAX_DMA_BUF 2

typedef struct {
	void (*set_codec_power) (int mode);
	void (*set_audio_power) (int mode);
	void (*set_audio_mute)  (int mode);
} ak4535_device_controls_t;

typedef struct h5400_audio_stream {
	char *id;		/* identification string */
	int  stream_id;		/* numeric identification */	
	
	int dma_ch;		/* DMA channel used */
	u32 dev_addr;		/* parameter for DMA controller */
	u32 dcmd;		/* parameter for DMA controller */
	volatile u32 *drcmr;		/* the DMA request channel to use */

	dma_addr_t buffer;	  /* buffer (phys addr) */
	unsigned int buffer_size; /* size of the buffer */
	pxa_dma_desc *dma_dring;  /* descriptor ring used to feed the DMA of the
				     pxa250 */
	char *dma_dring_store;	  /* storage space for DMA descriptors ring
				     (alignment issue) */
	dma_addr_t dma_dring_p;	  /* phys addr of the dring */
	dma_addr_t dma_dring_store_p;

	unsigned int periods;
	unsigned int dma_descriptors;	/* DMA descriptors in the DMA ring */
	unsigned int dma_size;		/* DMA transfer size */
	unsigned int dma_dpp;		/* DMA descriptors per period */

	volatile int dma_running;		/* tells if DMA is running*/
	
	int cur_dma_buf;	/* which dma buffer is currently being played/captured */
	int next_period;	/* which period buffer is next to be played/captured */

	int active:1;		/* we are using this stream for transfer now */

	int sent_periods;       /* # of sent periods from actual DMA buffer */
	int sent_total;         /* # of sent periods total (just for info & debug) */

	spinlock_t dma_lock;	/* for locking in DMA operations */
  	snd_pcm_substream_t *stream;
} h5400_audio_stream_t;
/* TODO-CHRI: check that this is aligned on a 16 byte boundary */

typedef enum stream_id_t {
	PLAYBACK=0,
	CAPTURE,
	MAX_STREAMS,
} stream_id_t;

typedef struct snd_card_h5400_ak4535 {
	snd_card_t *card;
	snd_pcm_t *pcm;
	struct i2c_client *ak4535;
	long samplerate;
	h5400_audio_stream_t *s[MAX_STREAMS];
	snd_info_entry_t *proc_entry;
#ifdef CONFIG_PM
	struct pm_dev *pm_dev;
	int after_suspend;
#endif
} snd_card_h5400_ak4535_t;

#endif /* _AK4535_H_ */
