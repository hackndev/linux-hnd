/*
 * Generic xmit layer for the SA1100 USB client function
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
 * 15/03/2001 - ep2_start now sets UDCAR to overcome something that is hardware
 * 		bug, I think. green@iXcelerator.com
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/delay.h>	// for the massive_attack hack 28Feb01ww
#include <linux/usb/ch9.h>

#include <asm/dma.h>

#include "usbdev.h"
#include "sa1100_usb.h"
#include "sa1100usb.h"

static unsigned int ep2_curdmalen;
static unsigned int ep2_remain;

static struct sausb_dev *ep2_dev;
int udc_ep2_send(struct sausb_dev *usb, char *buf, int len);

static void udc_set_cs2(u32 val, u32 mask, u32 check)
{
	int i = 0;

	do {
		Ser0UDCCS2 = val;
		udelay(1);
		if ((Ser0UDCCS2 & mask) == check)
			return;
	} while (i++ < 10000);

	printk("UDC: UDCCS2 write timed out: val=0x%08x\n", val);
}

/* set feature stall executing, async */
static void ep2_start(struct sausb_dev *usb)
{
	ep2_curdmalen = min(ep2_remain, usb->ep[1].maxpktsize);
	if (ep2_curdmalen == 0)
		return;

	/*
	 * must do this _before_ queue buffer..
	 * stop NAKing IN tokens
	 */
	udc_set_cs2(usb->ep[1].udccs | UDCCS2_TPC, UDCCS2_TPC, 0);

	UDC_write(Ser0UDCIMP, ep2_curdmalen - 1);

	/* Remove if never seen...8Mar01ww */
	{
		int massive_attack = 20;
		while (Ser0UDCIMP != ep2_curdmalen - 1 && massive_attack--) {
			printk("usbsnd: Oh no you don't! Let me spin...");
			udelay(500);
			printk("and try again...\n");
			UDC_write(Ser0UDCIMP, ep2_curdmalen - 1);
		}
		if (massive_attack != 20) {
			if (Ser0UDCIMP != ep2_curdmalen - 1)
				printk("usbsnd: Massive attack FAILED. %d\n",
				     20 - massive_attack);
			else
				printk("usbsnd: Massive attack WORKED. %d\n",
				     20 - massive_attack);
		}
	}
	/* End remove if never seen... 8Mar01ww */

	/*
	 * fight stupid silicon bug
	 */
	Ser0UDCAR = usb->ctl->address;

	sa1100_start_dma(usb->ep[1].dmach, usb->ep[1].pktdma, ep2_curdmalen);
}

static void udc_ep2_done(struct sausb_dev *usb, int flag)
{
	int size = usb->ep[1].buflen - ep2_remain;

	if (!usb->ep[1].buflen)
		return;

	dma_unmap_single(usb->dev, usb->ep[1].bufdma, usb->ep[1].buflen,
			 DMA_TO_DEVICE);

	usb->ep[1].bufdma = 0;
	usb->ep[1].buflen = 0;
	usb->ep[1].pktdma = 0;

	if (usb->ep[1].cb_func) 
		usb->ep[1].cb_func(usb->ep[1].cb_data, flag, size);
}

/*
 * Initialisation.  Clear out the status.
 */
void udc_ep2_init(struct sausb_dev *usb)
{
	ep2_dev = usb;

	usb->ep[1].udccs = UDCCS2_FST;

	BUG_ON(usb->ep[1].buflen);
	BUG_ON(usb->ep[1].pktlen);

	sa1100_reset_dma(usb->ep[1].dmach);
}

/*
 * Note: rev A0-B2 chips don't like FST
 */
void udc_ep2_halt(struct sausb_dev *usb, int halt)
{
	usb->ep[1].host_halt = halt;

	if (halt) {
		usb->ep[1].udccs |= UDCCS2_FST;
		udc_set_cs2(UDCCS2_FST, UDCCS2_FST, UDCCS2_FST);
	} else {
		sa1100_clear_dma(usb->ep[1].dmach);

		udc_set_cs2(UDCCS2_FST, UDCCS2_FST, UDCCS2_FST);
		udc_set_cs2(0, UDCCS2_FST, 0);
		udc_set_cs2(UDCCS2_SST, UDCCS2_SST, 0);

		usb->ep[1].udccs &= ~UDCCS2_FST;

		udc_ep2_done(usb, -EINTR);
	}
}

/*
 * This gets called when we receive a SET_CONFIGURATION packet to EP0.
 * We were configured.  We can now send packets to the host.
 */
void udc_ep2_config(struct sausb_dev *usb, unsigned int maxpktsize)
{
	/*
	 * We shouldn't be transmitting anything...
	 */
	BUG_ON(usb->ep[1].buflen);
	BUG_ON(usb->ep[1].pktlen);

	/*
	 * Set our configuration.
	 */
	usb->ep[1].maxpktsize = maxpktsize;
	usb->ep[1].configured = 1;

	/*
	 * Clear any pending TPC status.
	 */
	udc_set_cs2(UDCCS2_TPC, UDCCS2_TPC, 0);

	/*
	 * Enable EP2 interrupts.
	 */
	usb->udccr &= ~UDCCR_TIM;
	UDC_write(Ser0UDCCR, usb->udccr);

	usb->ep[1].udccs = 0;
}

/*
 * We saw a reset from the attached hub, or were deconfigured.
 * This means we are no longer configured.
 */
void udc_ep2_reset(struct sausb_dev *usb)
{
	/*
	 * Disable EP2 interrupts.
	 */
	usb->udccr |= UDCCR_TIM;
	UDC_write(Ser0UDCCR, usb->udccr);

	usb->ep[1].configured = 0;
	usb->ep[1].maxpktsize = 0;

	sa1100_reset_dma(usb->ep[1].dmach);
	udc_ep2_done(usb, -EINTR);
}

void udc_ep2_int_hndlr(struct sausb_dev *usb)
{
	u32 status = Ser0UDCCS2;

	// check for stupid silicon bug.
	if (Ser0UDCAR != usb->ctl->address)
		Ser0UDCAR = usb->ctl->address;

	udc_set_cs2(usb->ep[1].udccs | UDCCS2_SST, UDCCS2_SST, 0);

	if (!(status & UDCCS2_TPC)) {
	    char *buf = (char *) usb->ep[1].bufdma;
	    int len = usb->ep[1].buflen;
		printk("usb_send: Not TPC: UDCCS2 = %x\n", status);
printk("%s: hmm, what to do here?\n",__FUNCTION__);	
//		udc_ep2_send_reset(usb);
printk("%s: Buffer is %x length %d\n",__FUNCTION__,(int)buf,len);	
    if (buf && len) {
printk("%s: Retransmitting\n",__FUNCTION__);	
		udc_ep2_send(usb, buf, len);
    }
		return;
	}

	sa1100_stop_dma(usb->ep[1].dmach);

	if (status & (UDCCS2_TPE | UDCCS2_TUR)) {
		printk("usb_send: transmit error %x\n", status);
		usb->ep[1].fifo_errs ++;
		udc_ep2_done(usb, -EIO);
	} else {
		unsigned int imp;
#if 1		// 22Feb01ww/Oleg
		imp = ep2_curdmalen;
#else
		// this is workaround for case when setting
		// of Ser0UDCIMP was failed
		imp = Ser0UDCIMP + 1;
#endif
		usb->ep[1].pktdma += imp;
		ep2_remain -= imp;

		usb->ep[1].bytes += imp;
		usb->ep[1].packets++;

		sa1100_clear_dma(usb->ep[1].dmach);

#if 0
    /* induce horrible 5ms delay */
	    {
		udelay(1000);
		udelay(1000);
		udelay(1000);
		udelay(1000);
		udelay(1000);
	    }
#endif

		if (ep2_remain != 0) {
			ep2_start(usb);
		} else {
			udc_ep2_done(usb, 0);
		}
	}
}

#if 1
/* NCB added to resolve aligment problem */
#define SEND_BUFFER_SIZE 4096
static char send_buffer[SEND_BUFFER_SIZE];
static int index = 0;
#endif
int udc_ep2_send(struct sausb_dev *usb, char *buf, int len)
{
	unsigned long flags;
	dma_addr_t dma;
	int ret;

	if (!buf || len == 0) {
printk("%s: returning -EINVAL. buf = %x, len = %x\n",__FUNCTION__,(int)buf,len);
		return -EINVAL;
	}

#if 1
/* NCB added to resolve aligment problem */


	if (len > SEND_BUFFER_SIZE) {
printk("%s: buffer too big %d\n",__FUNCTION__,len);
	    return -EINVAL;
	}
	    
/* try to let usb tx drain */
	if (usb->ep[1].buflen) {
#if 0
	    int i;
printk("%s: busy sending %d - waiting 5secs\n",__FUNCTION__,usb->ep[1].buflen);
	    for (i=0; i<500; i++) {
		udelay(10000);
		if (!usb->ep[1].buflen)
		    break;
	    }
#endif
	    if (usb->ep[1].buflen) {
printk("%s: busy sending %d - returning -EBUSY\n",__FUNCTION__,usb->ep[1].buflen);
		return -EBUSY;
	    }
	}
	    
/* start by not manipulating the buffer while anything remaining to be sent */
	spin_lock_irqsave(&usb->lock, flags);

	if (index+len>SEND_BUFFER_SIZE) {
//	    printk("usb_send: udc_ep2_send - len is %x, reduced to 4096\n",len);
	    index=0;
	}
	else
	    // align index on a 32 bit boundary
	    while (index%3)
		index++;
	
	/* copy buffer to aligned memory area */
	memcpy(send_buffer+index,buf,len);
	dma = dma_map_single(usb->dev, send_buffer+index, len, DMA_TO_DEVICE);
#else
	dma = dma_map_single(usb->dev, buf, len, DMA_TO_DEVICE);
	spin_lock_irqsave(&usb->lock, flags);
#endif

	do {
		if (!usb->ep[1].configured) {
printk("%s: -ENODEV\n",__FUNCTION__);
			ret = -ENODEV;
			break;
		}

		if (usb->ep[1].buflen) {
printk("%s: -EBUSY\n",__FUNCTION__);
			ret = -EBUSY;
			break;
		}

		usb->ep[1].bufdma = dma;
		usb->ep[1].buflen = len;
		usb->ep[1].pktdma = dma;
		ep2_remain = len;

		sa1100_clear_dma(usb->ep[1].dmach);

		ep2_start(usb);
		ret = 0;
	} while (0);
	spin_unlock_irqrestore(&usb->lock, flags);

#if 0
if (ret == -EBUSY) {
	printk("%s: busy so resetting endpoint\n",__FUNCTION__);	
//	sa1100_clear_dma(usb->ep[1].dmach);
//	ep2_start(usb);
	udc_ep2_send_reset(usb);
	}
	else
#endif
	if (ret) {
printk("%s: returning -%d\n",__FUNCTION__,ret);
		dma_unmap_single(usb->dev, dma, len, DMA_TO_DEVICE);
	}

	return ret;
}

void udc_ep2_send_reset(struct sausb_dev *usb)
{
	sa1100_reset_dma(usb->ep[1].dmach);
	udc_ep2_done(usb, -EINTR);
}

int udc_ep2_idle(struct sausb_dev *usb)
{
	if (!usb->ep[1].configured)
		return -ENODEV;

	if (usb->ep[1].buflen)
		return -EBUSY;

	return 0;
}
