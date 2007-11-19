/*
 * Generic receive layer for the SA1100 USB client function
 * Copyright (c) 2001 by Nicolas Pitre
 *
 * This code was loosely inspired by the original version which was
 * Copyright (c) Compaq Computer Corporation, 1998-1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is still work in progress...
 *
 * Please see linux/Documentation/arm/SA1100/SA1100_USB for details.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>

#include <asm/byteorder.h>
#include <asm/dma.h>

#include "sa1100_usb.h"
#include "sa1100usb.h"

static int naking;

#if 0
static void dump_buf(struct sausb_dev *usb, const char *prefix)
{
	printk("%s: buf [dma=%08x len=%3d] pkt [cpu=%08x dma=%08x len=%3d rem=%3d]\n",
		prefix,
		usb->ep[0].bufdma,
		usb->ep[0].buflen,
		(unsigned int)usb->ep[0].pktcpu,
		usb->ep[0].pktdma,
		usb->ep[0].pktlen,
		usb->ep[0].pktrem);
}
#else
#define dump_buf(x,y)
#endif

static void udc_ep1_done(struct sausb_dev *usb, int flag, int size)
{
//	printk("UDC: rxd: %3d %3d\n", flag, size);
	dump_buf(usb, "UDC: rxd");

	if (!usb->ep[0].buflen)
		return;

	dma_unmap_single(usb->dev, usb->ep[0].bufdma, usb->ep[0].buflen,
			 DMA_FROM_DEVICE);

	usb->ep[0].bufdma = 0;
	usb->ep[0].buflen = 0;
	usb->ep[0].pktcpu = NULL;
	usb->ep[0].pktdma = 0;
	usb->ep[0].pktlen = 0;
	usb->ep[0].pktrem = 0;

	if (usb->ep[0].cb_func)
		usb->ep[0].cb_func(usb->ep[0].cb_data, flag, size);
}

/*
 * Initialisation.  Clear out the status, and set FST.
 */
void udc_ep1_init(struct sausb_dev *usb)
{
	sa1100_reset_dma(usb->ep[0].dmach);

	UDC_clear(Ser0UDCCS1, UDCCS1_FST | UDCCS1_RPE | UDCCS1_RPC);

	BUG_ON(usb->ep[0].buflen);
	BUG_ON(usb->ep[0].pktlen);
}

void udc_ep1_halt(struct sausb_dev *usb, int halt)
{
	if (halt) {
		/* force stall at UDC */
		UDC_set(Ser0UDCCS1, UDCCS1_FST);
	} else {
		sa1100_reset_dma(usb->ep[0].dmach);

		UDC_clear(Ser0UDCCS1, UDCCS1_FST);

		udc_ep1_done(usb, -EINTR, 0);
	}
}

/*
 * This gets called when we receive a SET_CONFIGURATION packet to EP0.
 * We were configured.  We can now accept packets from the host.
 */
void udc_ep1_config(struct sausb_dev *usb, unsigned int maxpktsize)
{
	usb->ep[0].maxpktsize = maxpktsize;
	usb->ep[0].configured = 1;

	Ser0UDCOMP = maxpktsize - 1;

	sa1100_reset_dma(usb->ep[0].dmach);
	udc_ep1_done(usb, -EINTR, 0);

	/*
	 * Enable EP1 interrupts.
	 */
	usb->udccr &= ~UDCCR_RIM;
	UDC_write(Ser0UDCCR, usb->udccr);
}

/*
 * We saw a reset from the attached hub.  This means we are no
 * longer configured, and as far as the rest of the world is
 * concerned, we don't exist.
 */
void udc_ep1_reset(struct sausb_dev *usb)
{
	/*
	 * Disable EP1 interrupts.
	 */
	usb->udccr |= UDCCR_RIM;
	UDC_write(Ser0UDCCR, usb->udccr);

	usb->ep[0].configured = 0;
	usb->ep[0].maxpktsize = 0;

	sa1100_reset_dma(usb->ep[0].dmach);
	udc_ep1_done(usb, -EINTR, 0);
}

void udc_ep1_int_hndlr(struct sausb_dev *usb)
{
	dma_addr_t dma_addr;
	unsigned int len;
	u32 status = Ser0UDCCS1;

	dump_buf(usb, "UDC: int");

	if (naking) {
		printk("UDC: usbrx: in ISR but naking [0x%02x]\n", status);
		return;
	}

	if (!(status & UDCCS1_RPC)) {
printk("%s: Holding nak\n",__FUNCTION__);
		/* you can get here if we are holding NAK */
		return;
	}

	if (!usb->ep[0].buflen) {
		printk("UDC: usb_recv: RPC for non-existent buffer [0x%02x]\n", status);
		naking = 1;
		return;
	}

	sa1100_stop_dma(usb->ep[0].dmach);

	dma_addr = sa1100_get_dma_pos(usb->ep[0].dmach);

	/*
	 * We've finished with the DMA for this packet.
	 */
	sa1100_clear_dma(usb->ep[0].dmach);

	if (status & UDCCS1_SST) {
		printk("UDC: usb_recv: stall sent\n");
		UDC_flip(Ser0UDCCS1, UDCCS1_SST);

		/*
		 * UDC aborted current transfer, so we do.
		 *
		 * It would be better to re-queue this buffer IMHO.  It
		 * hasn't gone anywhere yet. --rmk
		 */
		UDC_flip(Ser0UDCCS1, UDCCS1_RPC);
		udc_ep1_done(usb, -EIO, 0);
		return;
	}

	if (status & UDCCS1_RPE) {
		printk("UDC: usb_recv: RPError %x\n", status);
		UDC_flip(Ser0UDCCS1, UDCCS1_RPC);
		udc_ep1_done(usb, -EIO, 0);
		return;
	}

	len = dma_addr - usb->ep[0].pktdma;
	if (len < 0) {
		printk("UDC: usb_recv: dma_addr (%x) < pktdma (%x)\n",
			dma_addr, usb->ep[0].pktdma);
		len = 0;
	}

	if (len > usb->ep[0].pktlen)
		len = usb->ep[0].pktlen;

	/*
	 * If our transfer was smaller, and we have bytes left in
	 * the FIFO, we need to read them out manually.
	 */
	if (len < usb->ep[0].pktlen && (Ser0UDCCS1 & UDCCS1_RNE)) {
		char *buf;

		dma_sync_single(usb->dev, usb->ep[0].pktdma + len,
				usb->ep[0].pktlen - len, DMA_FROM_DEVICE);

		buf = (char *)usb->ep[0].pktcpu + len;

		do {
			*buf++ = Ser0UDCDR;
			len++;
		} while (len < usb->ep[0].pktlen && (Ser0UDCCS1 & UDCCS1_RNE));

		/*
		 * Note: knowing the internals of this macro is BAD, but we
		 * need this to cause the data to be written back to memory.
		 */
		dma_sync_single(usb->dev, usb->ep[0].pktdma + len,
				usb->ep[0].pktlen - len, DMA_TO_DEVICE);
	}

	/*
	 * If the FIFO still contains data, something's definitely wrong.
	 */
	if (Ser0UDCCS1 & UDCCS1_RNE) {
		printk("UDC: usb_recv: fifo screwed, shouldn't contain data\n");
		usb->ep[0].fifo_errs++;
		naking = 1;
		udc_ep1_done(usb, -EPIPE, 0);
		return;
	}

	/*
	 * Do statistics.
	 */
	if (len) {
		usb->ep[0].bytes += len;
		usb->ep[0].packets ++;
	}

	/*
	 * Update remaining byte count for this buffer.
	 */
	usb->ep[0].pktrem -= len;

	/*
	 * If we received a full-sized packet, and there's more
	 * data remaining, th, queue up another receive.
	 */
	if (len == usb->ep[0].pktlen && usb->ep[0].pktrem != 0) {
		usb->ep[0].pktcpu += len;
		usb->ep[0].pktdma += len;
		usb->ep[0].pktlen = min(usb->ep[0].pktrem, usb->ep[0].maxpktsize);
		sa1100_start_dma(usb->ep[0].dmach, usb->ep[0].pktdma, usb->ep[0].pktlen);
		/*
		 * Clear RPC to receive next packet.
		 */
		UDC_flip(Ser0UDCCS1, UDCCS1_RPC);
		dump_buf(usb, "UDC: req");
		return;
	}

	naking = 1;
	udc_ep1_done(usb, 0, usb->ep[0].buflen - usb->ep[0].pktrem);
}

int udc_ep1_queue_buffer(struct sausb_dev *usb, char *buf, unsigned int len)
{
	unsigned long flags;
	dma_addr_t dma;
	int ret;

	if (!buf || len == 0)
		return -EINVAL;

	dma = dma_map_single(usb->dev, buf, len, DMA_FROM_DEVICE);

	spin_lock_irqsave(&usb->lock, flags);
	do {
		if (usb->ep[0].buflen) {
			ret = -EBUSY;
			break;
		}

		sa1100_clear_dma(usb->ep[0].dmach);

		usb->ep[0].bufdma = dma;
		usb->ep[0].buflen = len;
		usb->ep[0].pktcpu = buf;
		usb->ep[0].pktdma = dma;
		usb->ep[0].pktlen = min(len, usb->ep[0].maxpktsize);
		usb->ep[0].pktrem = len;

		sa1100_start_dma(usb->ep[0].dmach, usb->ep[0].bufdma, usb->ep[0].buflen);
		dump_buf(usb, "UDC: que");

		if (naking) {
			/* turn off NAK of OUT packets, if set */
			UDC_flip(Ser0UDCCS1, UDCCS1_RPC);
			naking = 0;
		}

		ret = 0;
	} while (0);
	spin_unlock_irqrestore(&usb->lock, flags);

	if (ret)
		dma_unmap_single(usb->dev, dma, len, DMA_FROM_DEVICE);

	return 0;
}

void udc_ep1_recv_reset(struct sausb_dev *usb)
{
	sa1100_reset_dma(usb->ep[0].dmach);
	udc_ep1_done(usb, -EINTR, 0);
}
