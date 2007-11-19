/*
 *  linux/drivers/sound/pxa-audio.h -- audio interface for the Cotula chip
 *
 *  Author:	Nicolas Pitre
 *  Created:	Aug 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

typedef struct {
	int offset;			/* current buffer position */
	char *data; 	           	/* actual buffer */
	pxa_dma_desc *dma_desc;	/* pointer to the starting desc */
	int master;             	/* owner for buffer allocation, contain size whn true */
} audio_buf_t;

typedef struct {
	char *name;			/* stream identifier */
	audio_buf_t *buffers;   	/* pointer to audio buffer array */
	u_int usr_frag;			/* user fragment index */
	u_int dma_frag;			/* DMA fragment index */
	u_int fragsize;			/* fragment size */
	u_int nbfrags;			/* number of fragments */
	u_int dma_ch;			/* DMA channel number */
	dma_addr_t dma_desc_phys;	/* phys addr of descriptor ring */
	u_int descs_per_frag;		/* nbr descriptors per fragment */
	int bytecount;			/* nbr of processed bytes */
	int fragcount;			/* nbr of fragment transitions */
	struct semaphore sem;		/* account for fragment usage */
	wait_queue_head_t frag_wq;	/* for poll(), etc. */
	wait_queue_head_t stop_wq;	/* for users of DCSR_STOPIRQEN */
	u_long dcmd;			/* DMA descriptor dcmd field */
	volatile u32 *drcmr;		/* the DMA request channel to use */
	u_long dev_addr;		/* device physical address for DMA */
	int mapped:1;			/* mmap()'ed buffers */
	int output:1;			/* 0 for input, 1 for output */
} audio_stream_t;
	
typedef struct {
	audio_stream_t *output_stream;
	audio_stream_t *input_stream;
	int dev_dsp;			/* audio device handle */
	int rd_ref:1;           /* open reference for recording */
	int wr_ref:1;           /* open reference for playback */
	int (*client_ioctl)(struct inode *, struct file *, uint, ulong);
	struct semaphore sem;		/* prevent races in attach/release */
} audio_state_t;

extern int pxa_audio_attach(struct inode *inode, struct file *file,
				audio_state_t *state);
extern void pxa_audio_clear_buf(audio_stream_t *s);
