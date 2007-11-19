/*
 *  linux/drivers/sound/pxa-audio.c -- audio interface for the Cotula chip
 *
 *  Author:	Nicolas Pitre
 *  Created:	Aug 15, 2001
 *  Copyright:	MontaVista Software Inc.
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
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>
#include <asm/arch/pxa-regs.h>

#include "pxa-audio.h"
#define io_remap_page_range(vma, vaddr, paddr, size, prot)             \
               remap_pfn_range(vma, vaddr, (paddr) >> PAGE_SHIFT, size, prot)


#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

#define MAX_DMA_SIZE		4096
#define DMA_DESC_SIZE		sizeof(pxa_dma_desc)


/*
 * This function frees all buffers
 */
#define audio_clear_buf pxa_audio_clear_buf

void pxa_audio_clear_buf(audio_stream_t * s)
{
	DECLARE_WAITQUEUE(wait, current);
	int frag;

	if (!s->buffers)
		return;

	/* Ensure DMA isn't running */
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&s->stop_wq, &wait);
	DCSR(s->dma_ch) = DCSR_STOPIRQEN;
	schedule();
	remove_wait_queue(&s->stop_wq, &wait);

	/* free DMA buffers */
	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];
		if (!b->master)
			continue;
		dma_free_writecombine(NULL, b->master, b->data, b->dma_desc->dsadr);
	}

	/* free descriptor ring */
	if (s->buffers->dma_desc)
		dma_free_writecombine(NULL, s->nbfrags * s->descs_per_frag * DMA_DESC_SIZE,
				  s->buffers->dma_desc, s->dma_desc_phys);

	/* free buffer structure array */
	kfree(s->buffers);
	s->buffers = NULL;
}

/*
 * This function allocates the DMA descriptor array and buffer data space
 * according to the current number of fragments and fragment size.
 */
static int audio_setup_buf(audio_stream_t * s)
{
	pxa_dma_desc *dma_desc;
	dma_addr_t dma_desc_phys;
	int nb_desc, frag, i, buf_size = 0;
	char *dma_buf = NULL;
	dma_addr_t dma_buf_phys = 0;

	if (s->buffers)
		return -EBUSY;

	/* Our buffer structure array */
	s->buffers = kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
	if (!s->buffers)
		goto err;
	memzero(s->buffers, sizeof(audio_buf_t) * s->nbfrags);

	/* 
	 * Our DMA descriptor array:
	 * for Each fragment we have one checkpoint descriptor plus one 
	 * descriptor per MAX_DMA_SIZE byte data blocks.
	 */
	nb_desc = (1 + (s->fragsize + MAX_DMA_SIZE - 1)/MAX_DMA_SIZE) * s->nbfrags;
	dma_desc = dma_alloc_writecombine(NULL, nb_desc * DMA_DESC_SIZE,
					  &dma_desc_phys, GFP_KERNEL);
	
	if (!dma_desc)
		goto err;
	s->descs_per_frag = nb_desc / s->nbfrags;
	s->buffers->dma_desc = dma_desc;
	s->dma_desc_phys = dma_desc_phys;
	for (i = 0; i < nb_desc - 1; i++)
		dma_desc[i].ddadr = dma_desc_phys + (i + 1) * DMA_DESC_SIZE;
	dma_desc[i].ddadr = dma_desc_phys;

	/* Our actual DMA buffers */
	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];

		/*
		 * Let's allocate non-cached memory for DMA buffers.
		 * We try to allocate all memory at once.
		 * If this fails (a common reason is memory fragmentation),
		 * then we'll try allocating smaller buffers.
		 */
		if (!buf_size) {
			buf_size = (s->nbfrags - frag) * s->fragsize;
			do {
				dma_buf = dma_alloc_writecombine(NULL, buf_size,
								 &dma_buf_phys,
								 GFP_KERNEL);
				if (!dma_buf)
					buf_size -= s->fragsize;
			} while (!dma_buf && buf_size);
			if (!dma_buf)
				goto err;
			b->master = buf_size;
			memzero(dma_buf, buf_size);
		}

		/* 
		 * Set up our checkpoint descriptor.  Since the count 
		 * is always zero, we'll abuse the dsadr and dtadr fields
		 * just in case this one is picked up by the hardware
		 * while processing SOUND_DSP_GETPTR.
		 */
		dma_desc->dsadr = dma_buf_phys;
		dma_desc->dtadr = dma_buf_phys;
		dma_desc->dcmd = DCMD_ENDIRQEN;
		if (s->output && !s->mapped)
			dma_desc->ddadr |= DDADR_STOP;
		b->dma_desc = dma_desc++;

		/* set up the actual data descriptors */
		for (i = 0; (i * MAX_DMA_SIZE) < s->fragsize; i++) {
			dma_desc[i].dsadr = (s->output) ?
				(dma_buf_phys + i*MAX_DMA_SIZE) : s->dev_addr;
			dma_desc[i].dtadr = (s->output) ?
				s->dev_addr : (dma_buf_phys + i*MAX_DMA_SIZE);
			dma_desc[i].dcmd = s->dcmd |
				((s->fragsize < MAX_DMA_SIZE) ?
					s->fragsize : MAX_DMA_SIZE);
		}
		dma_desc += i;

		/* handle buffer pointers */
		b->data = dma_buf;
		dma_buf += s->fragsize;
		dma_buf_phys += s->fragsize;
		buf_size -= s->fragsize;
	}

	s->usr_frag = s->dma_frag = 0;
	s->bytecount = 0;
	s->fragcount = 0;
	sema_init(&s->sem, (s->output) ? s->nbfrags : 0);
	return 0;

err:
	printk("pxa-audio: unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}

/*
 * Our DMA interrupt handler
 */
static void audio_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
	audio_stream_t *s = dev_id;
	u_int dcsr;

	dcsr = DCSR(ch);
	DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;

	if (!s->buffers) {
		printk("AC97 DMA: wow... received IRQ for channel %d but no buffer exists\n", ch);
		return;
	}

	if (dcsr & DCSR_BUSERR)
		printk("AC97 DMA: bus error interrupt on channel %d\n", ch);

	if (dcsr & DCSR_ENDINTR) {
		u_long cur_dma_desc;
		u_int cur_dma_frag;

		/* 
		 * Find out which DMA desc is current.  Note that DDADR
		 * points to the next desc, not the current one.
		 */
		cur_dma_desc = DDADR(ch) - s->dma_desc_phys - DMA_DESC_SIZE;

		/*
		 * Let the compiler nicely optimize constant divisors into
		 * multiplications for the common cases which is much faster.
		 * Common cases: x = 1 + (1 << y) for y = [0..3]
		 */
		switch (s->descs_per_frag) {
		case 2:  cur_dma_frag = cur_dma_desc / (2*DMA_DESC_SIZE); break;
		case 3:  cur_dma_frag = cur_dma_desc / (3*DMA_DESC_SIZE); break;
		case 5:  cur_dma_frag = cur_dma_desc / (5*DMA_DESC_SIZE); break;
		case 9:  cur_dma_frag = cur_dma_desc / (9*DMA_DESC_SIZE); break;
		default: cur_dma_frag =
			    cur_dma_desc / (s->descs_per_frag * DMA_DESC_SIZE);
		}

		/* Account for possible wrap back of cur_dma_desc above */
		if (cur_dma_frag >= s->nbfrags)
			cur_dma_frag = s->nbfrags - 1;

		while (s->dma_frag != cur_dma_frag) {
			if (!s->mapped) {
				/* 
				 * This fragment is done - set the checkpoint
				 * descriptor to STOP until it is gets
				 * processed by the read or write function.
				 */
				s->buffers[s->dma_frag].dma_desc->ddadr |= DDADR_STOP;
				up(&s->sem);
			}
			if (++s->dma_frag >= s->nbfrags)
				s->dma_frag = 0;

			/* Accounting */
			s->bytecount += s->fragsize;
			s->fragcount++;
		}

		/* ... and for polling processes */
		wake_up(&s->frag_wq);
	}

	if ((dcsr & DCSR_STOPIRQEN) && (dcsr & DCSR_STOPSTATE))
		wake_up(&s->stop_wq);
}

/*
 * Validate and sets up buffer fragments, etc.
 */
static int audio_set_fragments(audio_stream_t *s, int val)
{
	if (s->mapped || DCSR(s->dma_ch) & DCSR_RUN)
		return -EBUSY;
	if (s->buffers)
		audio_clear_buf(s);
	s->nbfrags = (val >> 16) & 0x7FFF;
	val &= 0xffff;
	if (val < 5)
		val = 5;
	if (val > 15)
		val = 15;
	s->fragsize = 1 << val;
	if (s->nbfrags < 2)
		s->nbfrags = 2;
	if (s->nbfrags * s->fragsize > 256 * 1024)
		s->nbfrags = 256 * 1024 / s->fragsize;
	if (audio_setup_buf(s))
		return -ENOMEM;
	return val|(s->nbfrags << 16);
}


/*
 * The fops functions
 */

static int audio_write(struct file *file, const char *buffer,
		       size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	int chunksize, ret = 0;

	if (s->mapped)
		return -ENXIO;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	while (count > 0) {
		audio_buf_t *b = &s->buffers[s->usr_frag];

		/* Grab a fragment */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&s->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&s->sem))
				break;
		}

		/* Feed the current buffer */
		chunksize = s->fragsize - b->offset;
		if (chunksize > count)
			chunksize = count;
		if (copy_from_user(b->data + b->offset, buffer, chunksize)) {
			up(&s->sem);
			return -EFAULT;
		}

		b->offset += chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->offset < s->fragsize) {
			ret = 0;
			up(&s->sem);
			break;
		}

		/* 
		 * Activate DMA on current buffer.
		 * We unlock this fragment's checkpoint descriptor and
		 * kick DMA if it is idle.  Using checkpoint descriptors
		 * allows for control operations without the need for 
		 * stopping the DMA channel if it is already running.
		 */
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}

		/* move the index to the next fragment */
		if (++s->usr_frag >= s->nbfrags)
			s->usr_frag = 0;
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	return ret;
}


static int audio_read(struct file *file, char *buffer,
		      size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->input_stream;
	int chunksize, ret = 0;

	if (s->mapped)
		return -ENXIO;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	while (count > 0) {
		audio_buf_t *b = &s->buffers[s->usr_frag];

		/* prime DMA */
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = 
				s->buffers[s->dma_frag].dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&s->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&s->sem))
				break;
		}

		/* Grab data from current buffer */
		chunksize = s->fragsize - b->offset;
		if (chunksize > count)
			chunksize = count;
		if (copy_to_user(buffer, b->data + b->offset, chunksize)) {
			up(&s->sem);
			return -EFAULT;
		}
		b->offset += chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->offset < s->fragsize) {
			ret = 0;
			up(&s->sem);
			break;
		}

		/* 
		 * Make this buffer available for DMA again.
		 * We unlock this fragment's checkpoint descriptor and
		 * kick DMA if it is idle.  Using checkpoint descriptors
		 * allows for control operations without the need for 
		 * stopping the DMA channel if it is already running.
		 */
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;

		/* move the index to the next fragment */
		if (++s->usr_frag >= s->nbfrags)
			s->usr_frag = 0;
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	return ret;
}


static int audio_sync(struct file *file)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->output_stream;
	audio_buf_t *b;
	pxa_dma_desc *final_desc;
	u_long dcmd_save = 0;
	DECLARE_WAITQUEUE(wait, current);

	if (!(file->f_mode & FMODE_WRITE) || !s->buffers || s->mapped)
		return 0;

	/*
	 * Send current buffer if it contains data.  Be sure to send
	 * a full sample count.
	 */
	final_desc = NULL;
	b = &s->buffers[s->usr_frag];
	if (b->offset &= ~3) {
		final_desc = &b->dma_desc[1 + b->offset/MAX_DMA_SIZE];
		b->offset &= (MAX_DMA_SIZE-1);
		dcmd_save = final_desc->dcmd;
		final_desc->dcmd = b->offset | s->dcmd | DCMD_ENDIRQEN; 
		final_desc->ddadr |= DDADR_STOP;
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
	}

	/* Wait for DMA to complete. */
	set_current_state(TASK_INTERRUPTIBLE);
#if 0
	/* 
	 * The STOPSTATE IRQ never seem to occur if DCSR_STOPIRQEN is set
	 * along wotj DCSR_RUN.  Silicon bug?
	 */
	add_wait_queue(&s->stop_wq, &wait);
	DCSR(s->dma_ch) |= DCSR_STOPIRQEN;
	schedule();
#else 
	add_wait_queue(&s->frag_wq, &wait);
	while ((DCSR(s->dma_ch) & DCSR_RUN) && !signal_pending(current)) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
#endif
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&s->frag_wq, &wait);

	/* Restore the descriptor chain. */
	if (final_desc) {
		final_desc->dcmd = dcmd_save;
		final_desc->ddadr &= ~DDADR_STOP;
		b->dma_desc->ddadr |= DDADR_STOP;
	}
	return 0;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		if (!is->buffers && audio_setup_buf(is))
			return -ENOMEM;
		if (DCSR(is->dma_ch) & DCSR_STOPSTATE) {
			DDADR(is->dma_ch) = 
				is->buffers[is->dma_frag].dma_desc->ddadr;
			DCSR(is->dma_ch) = DCSR_RUN;
		}
		poll_wait(file, &is->frag_wq, wait);
	}

	if (file->f_mode & FMODE_WRITE) {
		if (!os->buffers && audio_setup_buf(os))
			return -ENOMEM;
		poll_wait(file, &os->frag_wq, wait);
	}

	if (file->f_mode & FMODE_READ)
		if (( is->mapped && is->bytecount > 0) ||
		    (!is->mapped && atomic_read(&is->sem.count) > 0))
			mask |= POLLIN | POLLRDNORM;

	if (file->f_mode & FMODE_WRITE)
		if (( os->mapped && os->bytecount > 0) ||
		    (!os->mapped && atomic_read(&os->sem.count) > 0))
			mask |= POLLOUT | POLLWRNORM;

	return mask;
}


static int audio_ioctl( struct inode *inode, struct file *file,
			uint cmd, ulong arg)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val;

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(os->fragsize, (int *)arg);
		else
			return put_user(is->fragsize, (int *)arg);

	case SNDCTL_DSP_GETCAPS:
		val = DSP_CAP_REALTIME|DSP_CAP_TRIGGER|DSP_CAP_MMAP;
		if (is && os)
			val |= DSP_CAP_DUPLEX;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (long *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			int ret = audio_set_fragments(is, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			int ret = audio_set_fragments(os, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		return 0;

	case SNDCTL_DSP_SYNC:
		return audio_sync(file);

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		val = 0;
		if (file->f_mode & FMODE_READ && DCSR(is->dma_ch) & DCSR_RUN)
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && DCSR(os->dma_ch) & DCSR_RUN)
			val |= PCM_ENABLE_OUTPUT;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT) {
				if (!is->buffers && audio_setup_buf(is))
					return -ENOMEM;
				if (!(DCSR(is->dma_ch) & DCSR_RUN)) {
					audio_buf_t *b = &is->buffers[is->dma_frag];
					DDADR(is->dma_ch) = b->dma_desc->ddadr;
					DCSR(is->dma_ch) = DCSR_RUN;
				}
			} else {
				DCSR(is->dma_ch) = 0;
			}
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT) {
				if (!os->buffers && audio_setup_buf(os))
					return -ENOMEM;
				if (!(DCSR(os->dma_ch) & DCSR_RUN)) {
					audio_buf_t *b = &os->buffers[os->dma_frag];
					DDADR(os->dma_ch) = b->dma_desc->ddadr;
					DCSR(os->dma_ch) = DCSR_RUN;
				}
			} else {
				DCSR(os->dma_ch) = 0;
			}
		}
		return 0;

	case SNDCTL_DSP_GETOSPACE:
	case SNDCTL_DSP_GETISPACE:
	    {
		audio_buf_info inf = { 0, };
		audio_stream_t *s = (cmd == SNDCTL_DSP_GETOSPACE) ? os : is;

		if ((s == is && !(file->f_mode & FMODE_READ)) ||
		    (s == os && !(file->f_mode & FMODE_WRITE)))
			return -EINVAL;
		if (!s->buffers && audio_setup_buf(s))
			return -ENOMEM;
		inf.bytes = atomic_read(&s->sem.count) * s->fragsize;
		inf.bytes -= s->buffers[s->usr_frag].offset;
		inf.fragments = inf.bytes / s->fragsize;
		inf.fragsize = s->fragsize;
		inf.fragstotal = s->nbfrags;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_GETIPTR:
	    {
		count_info inf = { 0, };
		audio_stream_t *s = (cmd == SNDCTL_DSP_GETOPTR) ? os : is;
		dma_addr_t ptr;
		int bytecount, offset;
		unsigned long flags;

		if ((s == is && !(file->f_mode & FMODE_READ)) ||
		    (s == os && !(file->f_mode & FMODE_WRITE)))
			return -EINVAL;
		local_irq_save(flags);
		if (DCSR(s->dma_ch) & DCSR_RUN) {
			audio_buf_t *b;
			ptr = (s->output) ? DSADR(s->dma_ch) : DTADR(s->dma_ch);
			b = &s->buffers[s->dma_frag];
			offset = ptr - b->dma_desc->dsadr;
			if (offset >= s->fragsize)
				offset = s->fragsize - 4;
		} else {
			offset = 0;
		}
		inf.ptr = s->dma_frag * s->fragsize + offset;
		bytecount = s->bytecount + offset;
		s->bytecount = -offset;
		inf.blocks = s->fragcount;
		s->fragcount = 0;
		local_irq_restore(flags);
		if (bytecount < 0)
			bytecount = 0;
		inf.bytes = bytecount;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_WRITE) 
			audio_clear_buf(os);
		if (file->f_mode & FMODE_READ)
			audio_clear_buf(is);
		return 0;

	default:
		return state->client_ioctl ?
			state->client_ioctl(inode, file, cmd, arg) : -EINVAL;
	}

	return 0;
}


static int audio_mmap(struct file *file, struct vm_area_struct *vma)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *s;
	unsigned long size, vma_addr;
	int i, ret;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE) {
		if (!state->wr_ref)
			return -EINVAL;;
		s = state->output_stream;
	} else if (vma->vm_flags & VM_READ) {
		if (!state->rd_ref)
			return -EINVAL;
		s = state->input_stream;
	} else return -EINVAL;

	if (s->mapped)
		return -EINVAL;
	size = vma->vm_end - vma->vm_start;
	if (size != s->fragsize * s->nbfrags)
		return -EINVAL;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;
	vma_addr = vma->vm_start;
	for (i = 0; i < s->nbfrags; i++) {
		audio_buf_t *buf = &s->buffers[i];
		if (!buf->master)
			continue;
		ret = io_remap_page_range(vma, vma->vm_start, buf->dma_desc->dsadr,
				       buf->master, vma->vm_page_prot);
		if (ret)
			return ret;
		vma_addr += buf->master;
	}
	for (i = 0; i < s->nbfrags; i++)
		s->buffers[i].dma_desc->ddadr &= ~DDADR_STOP;
	s->mapped = 1;
	return 0;
}


static int audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = file->private_data;

	down(&state->sem);

	if (file->f_mode & FMODE_READ) {
		audio_clear_buf(state->input_stream);
		*state->input_stream->drcmr = 0;
		pxa_free_dma(state->input_stream->dma_ch);
		state->rd_ref = 0;
	}

	if (file->f_mode & FMODE_WRITE) {
		audio_sync(file);
		audio_clear_buf(state->output_stream);
		*state->output_stream->drcmr = 0;
		pxa_free_dma(state->output_stream->dma_ch);
		state->wr_ref = 0;
	}

	up(&state->sem);
	return 0;
}


int pxa_audio_attach(struct inode *inode, struct file *file,
			 audio_state_t *state)
{
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	int err;

	down(&state->sem);

	/* access control */
	err = -ENODEV;
	if ((file->f_mode & FMODE_WRITE) && !os)
		goto out;
	if ((file->f_mode & FMODE_READ) && !is)
		goto out;
	err = -EBUSY;
	if ((file->f_mode & FMODE_WRITE) && state->wr_ref)
		goto out;
	if ((file->f_mode & FMODE_READ) && state->rd_ref)
		goto out;

	/* request DMA channels */
	if (file->f_mode & FMODE_WRITE) {
		err = pxa_request_dma(os->name, DMA_PRIO_LOW, 
					  audio_dma_irq, os);
		if (err < 0)
			goto out;
		os->dma_ch = err;
	}
	if (file->f_mode & FMODE_READ) {
		err = pxa_request_dma(is->name, DMA_PRIO_LOW,
					  audio_dma_irq, is);
		if (err < 0) {
			if (file->f_mode & FMODE_WRITE) {
				*os->drcmr = 0;
				pxa_free_dma(os->dma_ch);
			}
			goto out;
		}
		is->dma_ch = err;
	}

	file->private_data	= state;
	file->f_op->release	= audio_release;
	file->f_op->write	= audio_write;
	file->f_op->read	= audio_read;
	file->f_op->mmap	= audio_mmap;
	file->f_op->poll	= audio_poll;
	file->f_op->ioctl	= audio_ioctl;
	file->f_op->llseek	= no_llseek;

	if ((file->f_mode & FMODE_WRITE)) {
		state->wr_ref = 1;
		os->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		os->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		os->output = 1;
		os->mapped = 0;
		init_waitqueue_head(&os->frag_wq);
		init_waitqueue_head(&os->stop_wq);
		*os->drcmr = os->dma_ch | DRCMR_MAPVLD;
	}
	if (file->f_mode & FMODE_READ) {
		state->rd_ref = 1;
		is->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		is->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		is->output = 0;
		is->mapped = 0;
		init_waitqueue_head(&is->frag_wq);
		init_waitqueue_head(&is->stop_wq);
		*is->drcmr = is->dma_ch | DRCMR_MAPVLD;
	}

	err = 0;

out:
	up(&state->sem);
	return err;
}

EXPORT_SYMBOL(pxa_audio_attach);
EXPORT_SYMBOL(pxa_audio_clear_buf);

MODULE_AUTHOR("Nicolas Pitre, MontaVista Software Inc.");
MODULE_DESCRIPTION("audio interface for the Cotula chip");
MODULE_LICENSE("GPL");
