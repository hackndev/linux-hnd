/*
 * MediaQ 11xx USB Device Controller driver
 * Goku-S UDC driver used as a template.
 *
 * Copyright (C) 2004 Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/usb/ch9.h>
#include <linux/usb_gadget.h>
#include <linux/soc-old.h>
#include <linux/platform_device.h>

#include <asm/unaligned.h>

#define __MQ11XX_INTERNAL
#include "mq11xx_udc.h"

/*
 * This device has ep0 and three semi-configurable interrupt/bulk/isochronous
 * endpoints.
 *
 *  - Endpoints 1 and 2 are IN, 3 is OUT
 *  - Endpoint 1 is for interrupt transfers; endpoints 2 and 3 can be
 *    configured to either bulk or isochronous mode.
 *  - Endpoint 0 and 1 use FIFO, endpoints 2 and 3 use DMA transfers.
 *
 * Note that when using debugging output, the delay between the IN token
 * and the actual time of filling the FIFO can exceed 18 bit times
 * (as stated in section 7.1.19.1 of the USB specs), and the transfer
 * times out. But when the host retries the transfer, the data is already
 * in the FIFO so it gets transferred correctly, except that you can see
 * "usb_control/bulk_msg: timeout" messages on the host side. Also on bulk
 * endpoints you can eventually easily get timeouts/misbehaviour from time
 * to time, especially if you're using small rx/tx buffers.
 *
 * NOTE: Isochronous mode is not fully implemented yet.
 *
 * NOTE: Due to crappy chip design, there are still some hardware race
 * conditions and such. I've tried to make these dangerous time periods
 * as short as possible, but nothing guaranteed. I've marked these places
 * with [CRAP] marks; if you have a better solution, go ahead and fix it.
 */

/* Enable debug logs in general (The Big Red Switch) */
//#define USB_DEBUG
/* Enable debugging of packets */
//#define USB_DEBUG_TRACE
/* Enable debugging of packet contents */
//#define USB_DEBUG_DATA
/* Log time of every debug event */
//#define USB_DEBUG_TIME

#ifdef USB_DEBUG_TIME
#  ifdef CONFIG_ARCH_PXA
#    include <asm/arch/hardware.h>
     /* Approximative division by 3.6864 to get microseconds */
#    define USB_DEBUG_TIMER ((OSCR*139)/512)
#  else
#    define USB_DEBUG_TIMER jiffies
#  endif
#endif

#ifdef USB_DEBUG
#  ifdef USB_DEBUG_TIMER
#    define debug(s, args...) printk (KERN_INFO "(%7u) %s: " s, USB_DEBUG_TIMER, __FUNCTION__, ##args)
#  else
#    define debug(s, args...) printk (KERN_INFO "%s: " s, __FUNCTION__, ##args)
#  endif
#else
#  define debug(s, args...)
#endif
#define info(s, args...) printk (KERN_INFO "%s: " s, driver_name, ##args)

/* Endpoint 0 and 1 FIFO depth in bytes */
#define MQ_EP01_MAXPACKET	(MQ_UDC_FIFO_DEPTH * sizeof (u32))
/* Endpoint 2 and 3 packet size */
#define MQ_EP23_MAXPACKET	64

static const char driver_name [] = "mq11xx_udc";

MODULE_DESCRIPTION("MediaQ 11xx USB Device Controller");
MODULE_AUTHOR("Andrew Zabolotny <zap@homelink.ru>");
MODULE_LICENSE("GPL");

/* The size for one buffer in MediaQ internal RAM */
static uint rxbs = 4096;
module_param(rxbs, uint, 0);
MODULE_PARM_DESC (rxbs, "DMA receive buffer size (two buffers)");

static uint txbs = 4096;
module_param(txbs, uint, 0);
MODULE_PARM_DESC (txbs, "DMA transmit buffer size (two buffers)");

/* Static variables are EVIL :) but for now anyways the UDC architecture
 * allows only one UDC controller at a time.
 */
struct mq11xx_udc_mach_info *mach_info;

static int mq_set_halt(struct usb_ep *, int);
static void mq_ep_finish(struct mq_ep *, int status);
static void mq_done_request (struct mq_ep *ep, struct mq_request *req,
			     int status);

#ifdef USB_DEBUG_DATA
static void debug_dump (u8 *data, int count, int base)
{
	char line [80];
	u32 i, j, offs = 0;
	while (count > 0) {
		j = sprintf (line, "%08x | ", base + offs);
		for (i = 0; i < 16; i++)
			if (i < count)
				j += sprintf (line + j, "%02x ", data [offs + i]);
			else
				j += sprintf (line + j, "   ");
		j += sprintf (line + j, "| ");
		for (i = 0; i < 16; i++) {
			u8 c = data [offs + i];
			line [j++] = (i >= count) ? ' ' :
				((c < 32 || c > 126) ? '.' : c);
		}
		line [j] = 0;
		printk ("%s\n", line);
		offs += 16;
		count -= 16;
	}
}
#else
#define debug_dump(x,y,z)
#endif

static int mq_ep_enable (struct usb_ep *_ep,
			    const struct usb_endpoint_descriptor *desc)
{
	struct mqudc *mqdev;
	struct mq_ep *ep;
	u32 max, dmactl;
	unsigned long flags;
	volatile struct mediaq11xx_regs *regs;

	debug ("%s type:%d addr:%x maxpkt:%d\n", _ep->name,
	       desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK,
	       desc->bEndpointAddress,
	       desc->wMaxPacketSize);

	ep = container_of(_ep, struct mq_ep, ep);
	if (!_ep || !desc || ep->desc ||
	    desc->bDescriptorType != USB_DT_ENDPOINT ||
	    (desc->bEndpointAddress & 0x0f) > 3)
		return -EINVAL;
	mqdev = ep->mqdev;
	if (ep == &mqdev->ep[0])
		return -EINVAL;
	if (!mqdev->driver)
		return -ESHUTDOWN;
	if (ep->num != (desc->bEndpointAddress & 0x0f))
		return -EINVAL;

	regs = mqdev->base->regs;

	/* Endpoint 1 cannot do anything but interrupt transfers
	   while endpoints 2 and 3 can do only bulk/isochronous transfers. */
	switch (desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
	case USB_ENDPOINT_XFER_INT:
		if (ep->num != 1)
			return -EINVAL;
		break;
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_ISOC:
		if (ep->num > 1) {
			max = (ep->num == 2) ? MQ_UDC_EP2_ISOCHRONOUS :
				MQ_UDC_EP3_ISOCHRONOUS;
			if ((desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
				regs->UDC.control &= ~max;
				ep->iso = 0;
				ep->regs [0] &= ~MQ_UDC_FORCE_DATA0;
			} else {
				regs->UDC.control |= max;
				ep->iso = 1;
				ep->regs [0] |= MQ_UDC_FORCE_DATA0;
			}
			break;
		}
	default:
		return -EINVAL;
	}

	if (ep->regs [0] & MQ_UDC_EP_ENABLE)
		debug ("%s: already enabled\n", ep->ep.name);

	/* MediaQ UDC understands only 16 byte packets for EP0 and EP1,
         * and 64-byte packets for EP2 & EP3.
	 */
	max = le16_to_cpu (get_unaligned (&desc->wMaxPacketSize));
	if (max > ((ep->num < 2) ? MQ_EP01_MAXPACKET : MQ_EP23_MAXPACKET))
		return -EINVAL;

	/* Check pipe direction - 1 & 2 are IN, 3 is OUT */
	if (desc->bEndpointAddress & USB_DIR_IN) {
		if (ep->num > 2)
			return -EINVAL;
	} else
		if (ep->num < 3)
			return -EINVAL;

	spin_lock_irqsave (&ep->mqdev->lock, flags);

	ep->regs [0] |= MQ_UDC_EP_ENABLE;

	/* ep2 and ep3 can do double buffering and/or dma */
	if (ep->dmaregs) {
		ep->active_buffer = 0;
		/* DMA burst transfer length - 24 bytes */
//		regs->MIU.miu_1 = (regs->MIU.miu_1 & ~MQ_MIU_UDC_BURST_MASK) |
//			MQ_MIU_UDC_BURST_COUNT_6;
		/* For isochronous mode: burst threshold for RX is 6 free
		 * locations in the RX FIFO; for TX is when 6 locations in
		 * FIFO are received (6 locations == 24 bytes).
		 */
//		ep->regs [0] = (ep->regs [0] & ~MQ_UDC_FIFO_THRESHOLD_MASK) |
//			MQ_UDC_FIFO_THRESHOLD (6);
		/* Switch between 2 DMA buffers on RX/TX */
		dmactl = MQ_UDC_DMA_ENABLE | MQ_UDC_DMA_PINGPONG | \
			MQ_UDC_DMA_NUMBUFF (2);
		if (ep->num == 3)
			dmactl |= MQ_UDC_DMA_BUFF1_OWNER | MQ_UDC_DMA_BUFF2_OWNER;
		ep->dmaregs [0] = dmactl;
	}

	mq_set_halt (_ep, 0);
	ep->ep.maxpacket = max;
	ep->stopped = 1;
	ep->desc = desc;
	spin_unlock_irqrestore (&ep->mqdev->lock, flags);

	return 0;
}

static void mq_ep_clear_fifo (struct mq_ep *ep)
{
	if (ep->num < 2) {
		ep->quick_buff_bytes = 0;
		ep->regs [0] |= MQ_UDC_CLEAR_FIFO;
		ep->regs [0] &= ~MQ_UDC_CLEAR_FIFO;
		if (ep->num == 0) {
			ep->regs [2] |= MQ_UDC_CLEAR_FIFO;
			ep->regs [2] &= ~MQ_UDC_CLEAR_FIFO;
		}
	}
}

static void mq_ep_reset (struct mq_ep *ep)
{
	/* Disable the endpoint (if not ep0) or clear the FIFO (if ep0) */
	ep->regs [0] = MQ_UDC_CLEAR_FIFO;
	ep->regs [0] = 0;
	if (ep->num == 0) {
		ep->regs [2] = MQ_UDC_CLEAR_FIFO;
		ep->regs [2] = 0;
	}
	if (ep->dmaregs)
		ep->dmaregs [0] = 0;

	ep->ep.maxpacket = (ep->num < 2) ? MQ_EP01_MAXPACKET : MQ_EP23_MAXPACKET;
	ep->desc = 0;
	ep->stopped = 1;
}

/* enable bit 0 - enable primary endpoint interrupt;
 * bit 1 - enable secondary endpoint interrupt (TX for ep0).
 */
static void mq_ep_ints (struct mq_ep *ep, int enable)
{
	volatile struct mediaq11xx_regs *regs = ep->mqdev->base->regs;
	u32 rxmask = 0, txmask = 0;

	debug ("%s: rx:%d tx:%d\n", ep->ep.name, enable & 1, (enable >> 1) & 1);

	switch (ep->num) {
	case 0:
		rxmask = MQ_UDC_IEN_EP0_RX;
		txmask = MQ_UDC_IEN_EP0_TX;
		break;
	case 1:
		txmask = MQ_UDC_IEN_EP1_TX;
		break;
	case 2:
		txmask = /*MQ_UDC_IEN_EP2_TX_EOT | */MQ_UDC_IEN_DMA_TX;
		break;
	case 3:
		rxmask = /*MQ_UDC_IEN_EP3_RX_EOT | */MQ_UDC_IEN_DMA_RX | MQ_UDC_IEN_DMA_RX_EOT;
		break;
	default:
		return;
	}

	if (enable & 1)
		regs->UDC.control |= rxmask;
	else
		regs->UDC.control &= ~rxmask;

	if (enable & 2)
		regs->UDC.control |= txmask;
	else
		regs->UDC.control &= ~txmask;
}

static int mq_ep_disable (struct usb_ep *_ep)
{
	struct mq_ep *ep;
	struct mqudc *mqdev;
	unsigned long flags;

	ep = container_of(_ep, struct mq_ep, ep);
	if (!_ep || !ep->desc)
		return -ENODEV;
	mqdev = ep->mqdev;
	if (mqdev->ep0state == EP0_SUSPEND)
		return -EBUSY;

	spin_lock_irqsave(&mqdev->lock, flags);

	/* Drop all pending requests */
	mq_ep_finish(ep, -ESHUTDOWN);

	/* Disable endpoint interrupts */
	mq_ep_ints (ep, 0);

	/* Set the endpoint to RESET state */
	mq_ep_reset(ep);

	spin_unlock_irqrestore(&mqdev->lock, flags);

	return 0;
}

/*---------------------------------------------------------------------------*/

/* Send a packed through endpoint 0 or 1 using FIFO.
 */
static void mq_tx_fifo (struct mqudc *mqdev, struct mq_ep *ep, int rst_status)
{
	struct mq_request *req;
	int bytes_left, packet_len;
	u32 *buf;
	volatile u32 *fifo;

	/* Acknowledge all status bits, we already got the status */
	if (rst_status)
		ep->regs [1] = 0xff;

	req = list_empty (&ep->list) ? NULL :
		list_entry (ep->list.next, struct mq_request, queue);

	/* If there's a short packet to be sent on endpoint 0, do it. */
	if ((ep->quick_buff_bytes) &&
	    (!req || req->req.actual >= req->req.length)) {
		bytes_left = ep->quick_buff_bytes - 1;
		ep->quick_buff_bytes = 0;
		buf = &ep->quick_buff;
		req = NULL;
		goto send_packet;
	}

	if (unlikely (!req))
		return;

	/* Since the start of request buffer is always aligned to
	 * word boundary, we can move whole words to FIFO until we
	 * get to the last (incomplete) word. Also since memory is
	 * never allocated in chunks smaller than one word, we can
	 * safely read the last incomplete 32-bit word, to avoid
	 * copying bytes by one.
	 */
	bytes_left = req->req.length - req->req.actual;
	if (bytes_left > ep->ep.maxpacket)
		bytes_left = ep->ep.maxpacket;
	buf = (u32 *)(req->req.actual + (u8 *)req->req.buf);
	req->req.actual += bytes_left;

send_packet:
	packet_len = bytes_left;
	fifo = (ep->num == 0) ? &ep->regs [5] : &ep->regs [2];
	while (bytes_left > 0) {
		*fifo = le32_to_cpu (*buf++);
		bytes_left -= sizeof (u32);
	}

	ep->regs [0] = (ep->regs [0] & MQ_UDC_EP_ENABLE) | mqdev->toggle |
		MQ_UDC_TX_EDT | MQ_UDC_TX_LAST_ENABLE (4 + bytes_left);

	debug ("%s: Pktsize %d ctrl %08x stat %08x\n", ep->ep.name, packet_len,
	       ep->regs [0], ep->regs [1]);

	/* Toggle DATA0/DATA1 */
	mqdev->toggle ^= MQ_UDC_TX_PID_DATA1;

	if (!req || (req->req.actual >= req->req.length)) {
		/* Emmit a ZLP if the last packet is full size and
		 * req->zero is set.
		 */
		if (unlikely (req && req->req.zero &&
			      (packet_len == ep->ep.maxpacket))) {
			/* Send ZLP on next interrupt */
			ep->quick_buff_bytes = 1;
			debug ("%s: ZLP\n", ep->ep.name);
		}

		/* In any case, the request has been submitted.
		 * Note that inside done_request we can get new requests
		 * hooked on this endpoint, so be careful.
		 */
		if (req)
			mq_done_request (ep, req, 0);

		if (!ep->stopped && list_empty (&ep->list) && !ep->quick_buff_bytes) {
			/* If this is the last packet of a request, report
			 * successful completion to the gadget driver and
			 * disable further TX interrupts (enable them only
			 * when needed).
			 */
			ep->stopped = 1;
			/* Disable transmit interrupts */
			mq_ep_ints (ep, 1);
			if (ep->num == 0)
				mqdev->ep0state = EP0_IDLE;
			debug ("%s: Stop TX\n", ep->ep.name);
		}
	}
}

/* Send a packet through endpoint 2 using DMA.
 */
static void mq_tx_dma (struct mqudc *mqdev, struct mq_ep *ep)
{
	struct mq_request *req;
	int transfer_size;
	u32 dmactl, dmadesc, buffno, owner_mask, eot_mask;

	debug ("dmactl:%08x status:%08x\n", ep->dmaregs [0], ep->regs [1]);

	for (;;) {
		dmactl = ep->dmaregs [0];

		if (list_empty (&ep->list)) {
			/* Disable DMA only after both DMA buffers are transferred */
			if (dmactl & (MQ_UDC_DMA_BUFF1_OWNER | MQ_UDC_DMA_BUFF2_OWNER))
				return;
			mq_ep_ints (ep, 1);
			ep->stopped = 1;
			debug ("%s: stop TX (ctrl:%08x)\n", ep->ep.name, ep->dmaregs [0]);
			return;
		}

		if ((buffno = ep->active_buffer) == 0) {
			owner_mask = MQ_UDC_DMA_BUFF1_OWNER;
			eot_mask = MQ_UDC_DMA_BUFF1_EOT;
		} else {
			owner_mask = MQ_UDC_DMA_BUFF2_OWNER;
			eot_mask = MQ_UDC_DMA_BUFF2_EOT;
		}

		/* Ok, DMA buffers loaded up - return */
		if (dmactl & owner_mask)
			return;

		req = list_entry (ep->list.next, struct mq_request, queue);

		transfer_size = req->req.length - req->req.actual;
		if (transfer_size > txbs)
			transfer_size = txbs;

		/* Zero-length transfers not supported (?) */
		if (!transfer_size) {
			debug ("WARNING: ZERO-LENGTH TRANSFER REQUESTED BUT NOT SUPPORTED BY HARDWARE!\n");
			mq_done_request (ep, req, 0);
			continue;
		}

		/* Transfer next portion of data to MediaQ RAM */
		memcpy ((u8 *)mqdev->base->ram + ep->dmabuff + (buffno ? txbs : 0),
			req->req.actual + (u8 *)req->req.buf, transfer_size);
		req->req.actual += transfer_size;
		ep->active_buffer ^= 1;

		/* Now construct the buffer descriptor */
		dmadesc = MQ_UDC_DMA_BUFFER_ADDR (ep->dmabuff + (buffno ? txbs : 0)) |
			MQ_UDC_DMA_BUFFER_SIZE (transfer_size);

		/* Find if this is last packet and multiple of maxpacket size */
		if ((req->req.actual >= req->req.length) &&
		    !(transfer_size & 63))
			dmadesc |= MQ_UDC_DMA_BUFFER_LAST;
		ep->dmaregs [1 + buffno] = dmadesc;

		/* Decide if we need a ZLP after last packet */
		if (!ep->iso && req->req.zero &&
		    (req->req.actual >= req->req.length))
			dmactl = ~0;
		else {
			dmactl = ~eot_mask;
			eot_mask = 0;
		}

		/* Read register value again in the case it has been
		 * changed in the meantime (esp. during memcpy()).
		 * [CRAP] while we're holding dmaregs[0] in CPU registers,
		 * it could change in hardware. Thus this period of time
                 * must be as short as possible.
		 */
		dmactl &= (ep->dmaregs [0] | owner_mask | eot_mask);
		ep->dmaregs [0] = dmactl;

		debug ("buff:%d size:%d dmactrl:%08x dmadesc:%08x\n",
		       buffno, transfer_size, dmactl, ep->dmaregs [1 + buffno]);
		debug_dump ((u8 *)req->req.buf + req->req.actual - transfer_size,
			    transfer_size, ep->dmabuff + (buffno ? txbs : 0));

		if (req->req.actual >= req->req.length)
			mq_done_request (ep, req, 0);
	}
}

/* Prepare to receive packets through endpoint 3 using DMA.
 */
static void mq_rx_dma_prepare (struct mq_ep *ep)
{
	u32 dmactl, addr, buffno;

	dmactl = ep->dmaregs [0] | MQ_UDC_DMA_BUFF1_OWNER | MQ_UDC_DMA_BUFF2_OWNER;

	for (buffno = 0; buffno <= 1; buffno++) {
		if (buffno == 0)
			dmactl &= ~(MQ_UDC_DMA_BUFF1_OWNER | MQ_UDC_DMA_BUFF1_EOT);
		else
			dmactl &= ~(MQ_UDC_DMA_BUFF2_OWNER | MQ_UDC_DMA_BUFF2_EOT);

		addr = ep->dmabuff + (buffno ? rxbs : 0);
		ep->dmaregs [1 + buffno] = MQ_UDC_DMA_BUFFER_ADDR (addr) |
			MQ_UDC_DMA_BUFFER_EADDR (addr + rxbs);
		debug ("prepare buff %d, addr %08x\n", buffno, addr);
	}

	ep->dmaregs [0] = dmactl;
}

/* Transfer all the data in DMA buffers to pending request.
 * After this re-submit the buffers to chip DMA controller.
 */
static void mq_rx_dma (struct mqudc *mqdev, struct mq_ep *ep)
{
	struct mq_request *req = NULL;
	int end_transf, transfer_size, space_avail;
	u32 dmactl, buffno, owner_mask, eot_mask;
	volatile struct mediaq11xx_regs *regs = mqdev->base->regs;

	end_transf = 0;
try_again:
	dmactl = ep->dmaregs [0];
	debug ("ctrl:%08x status:%08x\n", dmactl, ep->regs [1]);

	for (;;) {
		/**
		 * [CRAP] the MQ_UDC_INT_DMA_RX_EOT is set for ANY of the
		 * two buffers being completely received. Thus we have to
		 * do a lot of guesswork in the case when two buffers are
                 * marked as "owned by CPU", e.g. contain received data.
		 */
		if (ep->iso) {
			// ??? todo
			if (dmactl & MQ_UDC_ISO_TRANSFER_END)
				end_transf |= ~ep->dmaregs [0] & (MQ_UDC_DMA_BUFF1_OWNER |
								  MQ_UDC_DMA_BUFF2_OWNER);
		} else {
			if (regs->UDC.intstatus & MQ_UDC_INT_DMA_RX_EOT) {
				end_transf |= ~ep->dmaregs [0] & (MQ_UDC_DMA_BUFF1_OWNER |
								  MQ_UDC_DMA_BUFF2_OWNER);
				regs->UDC.intstatus = MQ_UDC_INT_DMA_RX_EOT;
			}
		}

		if ((buffno = ep->active_buffer) == 0) {
			owner_mask = MQ_UDC_DMA_BUFF1_OWNER;
			eot_mask = MQ_UDC_DMA_BUFF1_EOT;
		} else {
			owner_mask = MQ_UDC_DMA_BUFF2_OWNER;
			eot_mask = MQ_UDC_DMA_BUFF2_EOT;
		}

		/* No more received buffers? */
		if (!(dmactl & owner_mask)) {
			if (dmactl & (owner_mask ^ (MQ_UDC_DMA_BUFF1_EOT | MQ_UDC_DMA_BUFF2_EOT))) {
				debug ("Desyncronization! (buff %d, ctrl %08x)\n",
				       buffno, dmactl);
				ep->active_buffer ^= 1;
				continue;
			}
			if (end_transf) {
				debug ("EOT received w/o data\n");
				goto try_again;
			}
			return;
		}

		ep->active_buffer ^= 1;

		if (list_empty (&ep->list)) {
			ep->stopped = 1;
			debug ("Received data, but no requests in queue!\n");
			return;
		}

		/* Get the next pending receive request */
		req = list_entry (ep->list.next, struct mq_request, queue);

		/* Find out the size of data pending in DMA buffers */
		transfer_size = rxbs;
		if (!ep->iso && (dmactl & eot_mask))
			transfer_size = ep->dmaregs [3 + buffno];

		/* Find out the available space in request' buffer */
		space_avail = req->req.length - req->req.actual;
		if (transfer_size > space_avail) {
			transfer_size = space_avail;
			if (!ep->iso)
				req->req.status = -EOVERFLOW;
		}

		/* Copy data from DMA buffers to request buffer */
		memcpy (req->req.actual + (u8 *)req->req.buf,
			(u8 *)mqdev->base->ram + ep->dmabuff + (buffno ? rxbs : 0),
			transfer_size);

		/* Mark immediately the DMA buffer as processed and free
		 * for more incoming data (maximize throughput).
                 *
		 * [CRAP] While we hold the old value in CPU registers,
		 * the hardware register state can change and we'll write
		 * the OLD value back.
		 */
		dmactl = ~(owner_mask | eot_mask | MQ_UDC_ISO_TRANSFER_END);
		dmactl &= ep->dmaregs [0];
		ep->dmaregs [0] = dmactl;

		debug ("buff:%d size:%d dmactl:%08x end:%x\n",
		       buffno, transfer_size, dmactl, end_transf);
		debug_dump ((u8 *)req->req.buf + req->req.actual, transfer_size,
			    ep->dmabuff + (buffno ? rxbs : 0));

		req->req.actual += transfer_size;

		/* If we have a short buffer, we obviously have a end
		 * of transfer.
		 */
		if (transfer_size < rxbs)
			end_transf = owner_mask;
		/* If we have a full buffer, and the other buffer is available,
		 * consider the EOP belonging to the next buffer, not this.
		 * This often happens when we have an incoming packet with
		 * the length equal to rxbs+a little (say, 4 bytes).
		 */
		else if (end_transf & ~owner_mask)
			end_transf &= ~owner_mask;

		/* NOTE: we don't handle for now properly the case 
                 * when a incoming ISO packet can fill several requests.
		 */

		if ((ep->iso &&
		     (end_transf || (req->req.actual >= req->req.length))) ||
		    (!ep->iso &&
		     (end_transf & owner_mask))) {
			if (ep->iso)
				end_transf = 0;
			else
				end_transf &= ~owner_mask;
			mq_done_request (ep, req, 0);
		}
	}
}

/*---------------------------------------------------------------------------*/

static struct usb_request *mq_alloc_request (struct usb_ep *_ep,
						gfp_t gfp_flags)
{
	struct mq_request *req;

	req = kmalloc (sizeof (*req), gfp_flags);
	if (!req)
		return 0;

	memset (req, 0, sizeof (*req));
	INIT_LIST_HEAD (&req->queue);

	return &req->req;
}

static void mq_free_request (struct usb_ep *_ep, struct usb_request *_req)
{
	struct mq_request *req;

	if (!_ep || !_req)
		return;

	req = container_of (_req, struct mq_request, req);

	if (!list_empty (&req->queue)) {
		debug ("%s: request %p still processed, unlinking\n",
		       _ep->name, _req);
		list_del (&req->queue);
		if (likely (req->req.status == -EINPROGRESS))
			req->req.status = -ESHUTDOWN;
	}
	kfree (req);
}

static void *mq_alloc_buffer (struct usb_ep *_ep, unsigned bytes,
				 dma_addr_t *dma, gfp_t gfp_flags)
{
	return kmalloc (bytes, gfp_flags);
}

static void mq_free_buffer (struct usb_ep *_ep, void *buf, dma_addr_t dma,
			    unsigned bytes)
{
	kfree (buf);
}

static void mq_done_request (struct mq_ep *ep, struct mq_request *req,
			     int status)
{
	struct mqudc *mqdev;

	list_del_init (&req->queue);

	if (likely (req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	mqdev = ep->mqdev;

#ifndef USB_DEBUG_TRACE
	if (status && status != -ESHUTDOWN)
#endif
		debug ("complete %s req %p stat %d len %u/%u\n",
		       ep->ep.name, &req->req, status,
		       req->req.actual, req->req.length);

	spin_unlock (&mqdev->lock);
	req->req.complete (&ep->ep, &req->req);
	spin_lock (&mqdev->lock);
}

static int mq_start_queue (struct mq_ep *ep)
{
	unsigned long flags;

	debug ("%s\n", ep->ep.name);

	spin_lock_irqsave (&ep->mqdev->lock, flags);

	/* Enable both RX and TX interrupts from this endpoint */
	if (ep->stopped)
		mq_ep_ints (ep, 3);

	/* For DMA-backed endpoints we must pre-fill the DMA buffers
	 * and then transmit buffers ownership to the mq chip; for FIFO
	 * endpoints we must fill the FIFO.
	 */
	if (!ep->dmaregs) {
		ep->stopped = 0;
		if (ep->is_in)
			/* Fill the transmit FIFO for this endpoint */
			mq_tx_fifo (ep->mqdev, ep, 1);
	} else if (ep->stopped) {
		ep->stopped = 0;
		if (ep->is_in)
			mq_tx_dma (ep->mqdev, ep);
		else
			mq_rx_dma_prepare (ep);
	}

	spin_unlock_irqrestore (&ep->mqdev->lock, flags);

	return 0;
}

static int mq_queue (struct usb_ep *_ep, struct usb_request *_req,
		     gfp_t gfp_flags)
{
	struct mq_ep *ep;
	struct mqudc *mqdev;
	unsigned long flags;
	int status;
	struct mq_request *req = container_of (_req, struct mq_request, req);

	if (unlikely(!_req || !req->req.complete
		     || !req->req.buf || !list_empty (&req->queue))) {
		debug ("%s: Invalid request (%p %p %p %d)\n",
		       _ep->name, req, req->req.complete, req->req.buf,
		       list_empty (&req->queue));
		return -EINVAL;
	}

	ep = container_of(_ep, struct mq_ep, ep);
	if (unlikely (!_ep ||
		      ((ep->num != 0) &&
		       (!ep->desc || !(ep->regs [0] & MQ_UDC_EP_ENABLE))))) {
		debug ("%s: Invalid endpoint\n", _ep->name);
		return -EINVAL;
	}

	mqdev = ep->mqdev;
	if (unlikely(!mqdev->driver))
		return -ESHUTDOWN;

	/* can't touch registers when suspended */
	if (mqdev->ep0state == EP0_SUSPEND)
		return -EBUSY;

#ifdef USB_DEBUG_TRACE
	debug("%s queue req %p, len %u buf %p\n",
	      _ep->name, _req, req->req.length, req->req.buf);
#endif

	spin_lock_irqsave (&mqdev->lock, flags);

	req->req.status = -EINPROGRESS;
	req->req.actual = 0;

	/* for ep0 IN without premature status, zlp is required and
	 * writing EOP starts the status stage (OUT).
	 */
	if (unlikely (ep->num == 0 && ep->is_in))
		req->req.zero = 1;

	list_add_tail (&req->queue, &ep->list);

	/* kickstart this i/o queue? */
	status = mq_start_queue (ep);

	spin_unlock_irqrestore (&mqdev->lock, flags);

	return status;
}

/* dequeue ALL requests */
static void mq_ep_finish (struct mq_ep *ep, int status)
{
	struct mq_request *req;
	unsigned long flags;

	spin_lock_irqsave (&ep->mqdev->lock, flags);

	/* Disable tx interrupts */
	mq_ep_ints (ep, 1);
	ep->stopped = 1;
	/* Clear both FIFOs */
	mq_ep_clear_fifo (ep);

	while (!list_empty(&ep->list)) {
		req = list_entry (ep->list.next, struct mq_request, queue);
		mq_done_request (ep, req, status);
	}

	spin_unlock_irqrestore (&ep->mqdev->lock, flags);
}

static int mq_dequeue (struct usb_ep *_ep, struct usb_request *_req)
{
	struct mq_ep *ep;
	struct mqudc *mqdev;
	unsigned long flags;
	struct mq_request *req = container_of (_req, struct mq_request, req);

	if (!_req || list_empty (&req->queue))
		return -EINVAL;
	ep = container_of (_ep, struct mq_ep, ep);
	if (!_ep || (!ep->desc && ep->num != 0))
		return -EINVAL;
	mqdev = ep->mqdev;
	if (!mqdev->driver)
		return -ESHUTDOWN;

	/* we can't touch registers when suspended */
	if (mqdev->ep0state == EP0_SUSPEND)
		return -EBUSY;

	spin_lock_irqsave (&mqdev->lock, flags);
	mq_done_request (ep, req, -ECONNRESET);
	spin_unlock_irqrestore (&mqdev->lock, flags);

	return 0;
}

/*-------------------------------------------------------------------------*/

static int mq_set_halt(struct usb_ep *_ep, int value)
{
	struct mq_ep *ep;

	if (!_ep)
		return -ENODEV;

	ep = container_of (_ep, struct mq_ep, ep);
	debug ("%s %s halt\n", _ep->name, value ? "set" : "clear");

	if (ep->num == 0) {
		if (value)
			ep->mqdev->ep0state = EP0_STALL;
		else
			return -EINVAL;
	}

	if (value) {
		ep->regs [0] |= MQ_UDC_STALL;
		if (ep->num == 0)
			ep->regs [2] |= MQ_UDC_STALL;
		ep->stopped = 1;
	} else {
		ep->regs [0] &= ~MQ_UDC_STALL;
		if (ep->num == 0)
			ep->regs [2] &= ~MQ_UDC_STALL;
		ep->stopped = 0;
	}

	return 0;
}

static int mq_fifo_status(struct usb_ep *_ep)
{
	struct mq_ep	*ep;
	u32 status;

	if (!_ep)
		return -ENODEV;
	ep = container_of (_ep, struct mq_ep, ep);

	if (ep->num > 1)
		return -EOPNOTSUPP;

	if (ep->num == 0 && ep->is_in)
		status = ep->regs [3];
	else
		status = ep->regs [1];

	return (MQ_UDC_FIFO_DEPTH - MQ_UDC_FIFO (status)) * 4;
}

static void mq_fifo_flush(struct usb_ep *_ep)
{
	struct mq_ep	*ep;

	if (!_ep)
		return;
	ep = container_of (_ep, struct mq_ep, ep);

	ep->regs [0] |= MQ_UDC_CLEAR_FIFO;
	ep->regs [0] &= ~MQ_UDC_CLEAR_FIFO;
	if (ep->num == 0) {
		/* For endpoint 0, clear also RX FIFO */
		ep->regs [2] |= MQ_UDC_CLEAR_FIFO;
		ep->regs [2] &= ~MQ_UDC_CLEAR_FIFO;
	}
	return;
}

static struct usb_ep_ops mq_ep_ops = {
	.enable		= mq_ep_enable,
	.disable	= mq_ep_disable,

	.alloc_request	= mq_alloc_request,
	.free_request	= mq_free_request,

	.alloc_buffer	= mq_alloc_buffer,
	.free_buffer	= mq_free_buffer,

	.queue		= mq_queue,
	.dequeue	= mq_dequeue,

	.set_halt	= mq_set_halt,
	.fifo_status	= mq_fifo_status,
	.fifo_flush	= mq_fifo_flush,
};

/*-------------------------------------------------------------------------*/

static int mq_get_frame(struct usb_gadget *gadget)
{
	struct mqudc *mqdev = container_of (gadget, struct mqudc, gadget);
	return mqdev->base->regs->UDC.frame_number & MQ_UDC_FRAME_MASK;
}

int mq_wakeup (struct usb_gadget *gadget)
{
	struct mqudc *mqdev = container_of (gadget, struct mqudc, gadget);
	mqdev->base->regs->UDC.control |= MQ_UDC_WAKEUP_USBHOST;
	return 0;
}

int mq_set_selfpowered (struct usb_gadget *gadget, int value)
{
	if (value)
		return 0;
	return -EOPNOTSUPP;
}

static const struct usb_gadget_ops mq_ops = {
	.get_frame		= mq_get_frame,
	.wakeup			= mq_wakeup,
	.set_selfpowered	= mq_set_selfpowered,
};

/*-------------------------------------------------------------------------*/

static void mq_reinit (struct mqudc *mqdev)
{
	volatile struct mediaq11xx_regs *regs = mqdev->base->regs;
	unsigned i;

	mqdev->ep0state = EP0_IDLE;

	for (i = 0; i < 4; i++) {
		struct mq_ep *ep = &mqdev->ep[i];

		INIT_LIST_HEAD (&ep->list);
		mq_ep_reset(ep);
	}

	regs->UDC.control = (regs->UDC.control & 0xffff) |
		MQ_UDC_IEN_EP0_RX |
		MQ_UDC_IEN_GLOBAL_SUSPEND |
		MQ_UDC_IEN_WAKEUP | MQ_UDC_IEN_RESET |
		MQ_UDC_SUSPEND_ENABLE |
		MQ_UDC_REMOTEHOST_WAKEUP_ENABLE;

	/* Set endpoint address to zero after reset */
	regs->UDC.address = 0;
	/* Drop endpoint status bits, whatever they were */
	regs->UDC.ep0txstatus = 0xff;
	regs->UDC.ep0rxstatus = 0xff;
}

static void mq_reset(struct mqudc *mqdev)
{
	volatile struct mediaq11xx_regs *regs = mqdev->base->regs;

	/* Set D+ to Hi-Z (no pullup) */
	mach_info->udc_command (MQ11XX_UDC_CMD_DISCONNECT);

	/* Initialize device registers */
	regs->UDC.control = 0;
	regs->UDC.address = 0;
	regs->UDC.ep0txcontrol = MQ_UDC_CLEAR_FIFO;
	regs->UDC.ep0txcontrol = 0;
	regs->UDC.ep0rxcontrol = MQ_UDC_CLEAR_FIFO;
	regs->UDC.ep0rxcontrol = 0;
	regs->UDC.ep1control = 0;
	regs->UDC.ep2control = 0;
	regs->UDC.ep3control = 0;
	regs->UDC.intstatus = 0xffffffff;
	regs->UDC.dmatxcontrol = 0;
	regs->UDC.dmarxcontrol = 0;
}

static void mq_enable(struct mqudc *mqdev)
{
	mq_reset (mqdev);
	mq_reinit (mqdev);

	/* Enable 1.5k D+ pullup resistor */
	mach_info->udc_command (MQ11XX_UDC_CMD_CONNECT);
}

/*---------------------------------------------------------------------------*/
/*                             USB Gadget exported API                       */
/*---------------------------------------------------------------------------*/

static struct mqudc *the_controller;

int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct mqudc *mqdev = the_controller;
	int rc;

	if (!driver
	 || driver->speed != USB_SPEED_FULL
	 || !driver->bind
	 || !driver->unbind
	 || !driver->disconnect
	 || !driver->setup)
		return -EINVAL;
	if (!mqdev)
		return -ENODEV;
	if (mqdev->driver)
		return -EBUSY;

	/* hook up the driver */
	driver->driver.bus = 0;
	mqdev->driver = driver;
	mqdev->gadget.dev.driver = &driver->driver;
	rc = driver->bind (&mqdev->gadget);
	if (rc) {
		debug ("error %d while binding to driver %s\n",
		       rc, driver->driver.name);
		mqdev->driver = 0;
		mqdev->gadget.dev.driver = 0;
		return rc;
	}

	/* enable host detection and ep0; and we're ready
	 * for set_configuration as well as eventual disconnect.
	 */
	mq_enable (mqdev);

	info ("Registered gadget driver '%s'\n", driver->driver.name);
	return 0;
}
EXPORT_SYMBOL(usb_gadget_register_driver);

static void
stop_activity(struct mqudc *mqdev, struct usb_gadget_driver *driver)
{
	unsigned i;

	mq_reset (mqdev);
	for (i = 0; i < 4; i++)
		mq_ep_finish(&mqdev->ep [i], -ESHUTDOWN);
	if (driver) {
		spin_unlock(&mqdev->lock);
		driver->disconnect(&mqdev->gadget);
		spin_lock(&mqdev->lock);
	}

	if (mqdev->driver)
		mq_enable(mqdev);
}

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct mqudc *mqdev = the_controller;
	unsigned long flags;

	if (!mqdev)
		return -ENODEV;
	if (!driver || driver != mqdev->driver)
		return -EINVAL;

	spin_lock_irqsave(&mqdev->lock, flags);
	mqdev->driver = 0;
	stop_activity(mqdev, driver);
	spin_unlock_irqrestore(&mqdev->lock, flags);

	driver->unbind(&mqdev->gadget);

	debug ("unregistered driver '%s'\n", driver->driver.name);

	return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);

/*---------------------------------------------------------------------------*/
/*                                 IRQ handling                              */
/*---------------------------------------------------------------------------*/

static void ep01_transmit (struct mqudc *mqdev, struct mq_ep *ep)
{
	u32 txstatus = ep->regs [1];

	/* Clear the status of last packet sent */
	ep->regs [1] = 0xff;

	/* If no ACK for last sent packet, or the End Of Data flag is
	 * still set, don't disturb the transmitter - it's not ready yet.
	 */
	if (!(txstatus & MQ_UDC_ACK) &&
	    (ep->regs [0] & MQ_UDC_TX_EDT)) {
		debug ("No ACK and EDT is still set\n");
		return;
	}

	if (MQ_UDC_FIFO (txstatus) < MQ_UDC_FIFO_DEPTH) {
		debug ("TX FIFO not empty yet\n");
		return;
	}

	if ((ep->num == 0) && (mqdev->ep0state == EP0_STALL)) {
		ep->regs [0] |= MQ_UDC_STALL;
		return;
	}

	mq_tx_fifo (mqdev, ep, 0);
}

static void ep0_receive (struct mqudc *mqdev)
{
	volatile struct mediaq11xx_regs *regs = mqdev->base->regs;
	struct mq_ep *ep = &mqdev->ep[0];
	struct usb_ctrlrequest *ctrl;
	u32 rxstatus = regs->UDC.ep0rxstatus;
	u32 rxbuff [4];
	int i, count;

	/* Acknowledge all status bits, we already got the status */
	regs->UDC.ep0rxstatus = 0xff;

	/* If the data is broken, ignore it - the hardware already sent a NAK
	 * if the packet is not a SETUP packet, and won't generate an
	 * interrupt if the packet is a broken SETUP packet...
	 */
	if ((rxstatus & (MQ_UDC_ERR | MQ_UDC_TIMEOUT | MQ_UDC_FIFO_OVERRUN | MQ_UDC_ACK)) != MQ_UDC_ACK) {
		regs->UDC.ep0rxcontrol = MQ_UDC_CLEAR_FIFO;
		regs->UDC.ep0rxcontrol = 0;
		return;
	}

	/* Get the data from RX FIFO */
	count = MQ_UDC_FIFO (rxstatus);
	for (i = 0; i < count; i++)
		/* According to docs, lowest bits are always earlier bytes */
		rxbuff [i] = cpu_to_le32 (regs->UDC.ep0rxfifo);
	if (count) {
		i = MQ_UDC_RX_VALID_BYTES (rxstatus);
		if (i == 0)
			i = 4;
		count = (count - 1) * 4 + i;
	}

	/* Set output DATA0/DATA1 PID from the PID of incoming packet */
	mqdev->toggle = (rxstatus & MQ_UDC_RX_PID_DATA1) ? 0 : MQ_UDC_TX_PID_DATA1;

	if (!(rxstatus & MQ_UDC_RX_TOKEN_SETUP)) {
		struct mq_request *req;

		if (unlikely (ep->is_in))
			return;
		if (unlikely (list_empty (&ep->list)))
			return;

		req = list_entry (ep->list.next, struct mq_request, queue);

		if (req->req.actual + count > req->req.length) {
			debug ("%s: buffer overflow, got %d, max %d\n",
			       ep->ep.name, count, req->req.length);
			req->req.status = -EOVERFLOW;
			count = req->req.length - req->req.actual;
		}

		memcpy (req->req.buf + req->req.actual, &rxbuff, count);
		req->req.actual += count;

		/* If packet is short, or the transfer is complete, finish */
		if (count < ep->ep.maxpacket ||
		    req->req.actual >= req->req.length) {
			ep->stopped = 1;
			mqdev->ep0state = EP0_IDLE;
			mq_done_request (ep, req, 0);
		}

		return;
	}

	ctrl = (struct usb_ctrlrequest *)&rxbuff;

	le16_to_cpus (&ctrl->wValue);
	le16_to_cpus (&ctrl->wIndex);
	le16_to_cpus (&ctrl->wLength);

	mq_ep_finish (ep, 0);

	if (likely (ctrl->bRequestType & USB_DIR_IN)) {
		ep->is_in = 1;
		mqdev->ep0state = EP0_IN;
	} else {
		ep->is_in = 0;
		mqdev->ep0state = EP0_OUT;
	}

#ifdef USB_DEBUG_TRACE
	debug ("SETUP %02x.%02x v%04x i%04x l%04x\n",
	       ctrl->bRequestType, ctrl->bRequest,
	       ctrl->wValue, ctrl->wIndex, ctrl->wLength);
#endif

	/* Handle some SETUP packets ourselves */
	switch (ctrl->bRequest) {
	case USB_REQ_SET_ADDRESS:
		if (ctrl->bRequestType != (USB_TYPE_STANDARD | USB_RECIP_DEVICE))
			break;

		debug ("SET_ADDRESS (%d)\n", ctrl->wValue);

		regs->UDC.address = ctrl->wValue;
		/* Reply with a ZLP on next IN token */
		ep->quick_buff_bytes = 1;
		mq_tx_fifo (mqdev, ep, 1);
		return;

	case USB_REQ_GET_STATUS: {
		struct mq_ep *qep;
		int epn = ctrl->wIndex & ~USB_DIR_IN;

		/* hw handles device and interface status */
		if (ctrl->bRequestType != (USB_DIR_IN | USB_RECIP_ENDPOINT) ||
		    ctrl->wLength > 2 || epn > 3)
			break;

		debug ("GET_STATUS (%x)\n", ctrl->wIndex);

		qep = &mqdev->ep [epn];
		if (qep->is_in != ((ctrl->wIndex & USB_DIR_IN) ? 1 : 0))
			break;

		/* Return status on next IN token */
		ep->quick_buff_bytes = 2;
		ep->quick_buff = cpu_to_le32 (qep->stopped);
		mq_tx_fifo (mqdev, ep, 1);
		return;
	}

	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
		if (ctrl->bRequestType == USB_RECIP_ENDPOINT) {
			struct mq_ep *qep;

			debug ("SET/CLR_FEATURE (%d)\n", ctrl->wValue);

			/* Support only HALT feature */
			if (ctrl->wValue != 0 ||
			    ctrl->wLength != 0 ||
			    ctrl->wIndex > 3)
				goto ep0_stall;

			qep = &mqdev->ep [ctrl->wIndex];
			if (ctrl->bRequest == USB_REQ_SET_FEATURE) {
				qep->regs [0] |= MQ_UDC_STALL;
				if (ctrl->wIndex == 0)
					qep->regs [2] |= MQ_UDC_STALL;
			} else {
				qep->regs [0] &= ~MQ_UDC_STALL;
				if (ctrl->wIndex == 0)
					qep->regs [2] &= ~MQ_UDC_STALL;
			}

			/* Reply with a ZLP on next IN token */
			ep->quick_buff_bytes = 1;
			mq_tx_fifo (mqdev, ep, 1);
			return;
		}
		break;
	}

	if (!mqdev->driver)
		return;

	/* delegate everything to the gadget driver.
	 * it may respond after this irq handler returns.
	 */
	spin_unlock (&mqdev->lock);
	i = mqdev->driver->setup (&mqdev->gadget, ctrl);
	spin_lock (&mqdev->lock);

	if (unlikely (i < 0)) {
ep0_stall:
#ifdef USB_DEBUG_TRACE
		debug("req %02x.%02x protocol STALL\n",
		      ctrl->bRequestType, ctrl->bRequest);
#endif
		ep->stopped = 1;
		mqdev->ep0state = EP0_STALL;
		regs->UDC.ep0txcontrol |= MQ_UDC_STALL;
	}
}

static irqreturn_t mq_dev_irq(int irq, void *data)
{
	struct mqudc *mqdev = (struct mqudc *)data;
	volatile struct mediaq11xx_regs *regs = mqdev->base->regs;
	int irq_handled = 0;
	u32 mask;

	spin_lock(&mqdev->lock);

	while ((mask = regs->UDC.intstatus) != 0) {
		irq_handled = 1;

		/* Sort in decreasing importance order */
		if (likely (mask & MQ_UDC_INT_DMA_TX)) {
			/* EP2 DMA end-of-transmit buffer reached */
			regs->UDC.intstatus = MQ_UDC_INT_DMA_TX;
			mq_tx_dma (mqdev, &mqdev->ep [2]);
		} else if (likely (mask & (MQ_UDC_INT_DMA_RX | MQ_UDC_INT_DMA_RX_EOT))) {
			/* EP3 DMA end-of-receive buffer reached */
			regs->UDC.intstatus = mask & MQ_UDC_INT_DMA_RX;
			mq_rx_dma (mqdev, &mqdev->ep [3]);
		} else if (likely (mask & MQ_UDC_INT_EP0_RX)) {
			/* EP0 packet received */
			regs->UDC.intstatus = MQ_UDC_INT_EP0_RX;
			ep0_receive (mqdev);
		} else if (likely (mask & MQ_UDC_INT_EP0_TX)) {
			/* EP0 packet transmitted (FIFO empty) */
			regs->UDC.intstatus = MQ_UDC_INT_EP0_TX;
			ep01_transmit (mqdev, &mqdev->ep [0]);
		} else if (likely (mask & MQ_UDC_INT_EP1_TX)) {
			/* EP1 packet transmitted (FIFO empty) */
			regs->UDC.intstatus = MQ_UDC_INT_EP1_TX;
			ep01_transmit (mqdev, &mqdev->ep [1]);
		} else if (unlikely (mask & MQ_UDC_INT_RESET)) {
			debug ("reset\n");
			/* Device reset */
			regs->UDC.intstatus = MQ_UDC_INT_RESET;
			mq_reinit (mqdev);
		} else if (unlikely (mask & MQ_UDC_INT_WAKEUP)) {
                        regs->UDC.intstatus = MQ_UDC_INT_WAKEUP;

			if (mqdev->ep0state == EP0_SUSPEND) {
				debug ("resume\n");
				mqdev->ep0state = EP0_IDLE;
				if (mqdev->driver && mqdev->driver->resume) {
					spin_unlock (&mqdev->lock);
					mqdev->driver->resume (&mqdev->gadget);
					spin_lock (&mqdev->lock);
				}
			}
		} else if (unlikely (mask & MQ_UDC_INT_GLOBAL_SUSPEND)) {
			regs->UDC.intstatus = MQ_UDC_INT_GLOBAL_SUSPEND;

			if (mqdev->ep0state != EP0_DISCONNECT &&
			    mqdev->ep0state != EP0_SUSPEND) {
				debug ("suspend\n");
				mqdev->ep0state = EP0_SUSPEND;
				if (mqdev->driver && mqdev->driver->suspend) {
					spin_unlock (&mqdev->lock);
					mqdev->driver->suspend (&mqdev->gadget);
					spin_lock (&mqdev->lock);
				}
			}
		} else {
			/*
			 * MQ_UDC_INT_SOF
			 * MQ_UDC_INT_EP2_TX_EOT
			 * MQ_UDC_INT_EP3_RX_EOT
			 */
			debug ("unhandled mask: %08x\n", mask);
			regs->UDC.intstatus = mask;
		}
	}

        spin_unlock(&mqdev->lock);

	return IRQ_RETVAL (irq_handled);
}

static irqreturn_t mq_wup_irq(int irq, void *data)
{
	debug ("WakeUP IRQ\n");

	return IRQ_HANDLED;
}

/*-------------------------------------------------------------------------*/

static struct
{
	char *desc;
	int delta;
	irqreturn_t (*handler) (int, void *);
} irq_desc [] =
{
	{ "MediaQ UDC", IRQ_MQ_UDC, mq_dev_irq },
	{ "MediaQ UDC wake-up", IRQ_MQ_UDC_WAKE_UP, mq_wup_irq }
};

static int mq_remove(struct platform_device *pdev);

static void gadget_release(struct device *dev)
{
	struct mqudc *mqdev = dev_get_drvdata (dev);
	kfree (mqdev);
}

static int mq_probe(struct platform_device *pdev)
{
	static char *names [] = { "ep0", "ep1in-int", "ep2in", "ep3out" };
	struct mqudc *mqdev;
	struct mediaq11xx_base *base = pdev->dev.platform_data;
	struct mediaq11xx_init_data *mediaq11xx_init_data = pdev->dev.parent->platform_data;
	volatile struct mediaq11xx_regs *regs = base->regs;
	int i;

	if (!base || !base->irq_base)
		return -ENXIO;

	mqdev = kmalloc (sizeof (*mqdev), GFP_KERNEL);
	if (!mqdev)
		return -ENOMEM;

	mach_info = mediaq11xx_init_data->udc_info;

	memset(mqdev, 0, sizeof (*mqdev));
	mqdev->base = base;
	spin_lock_init(&mqdev->lock);
	mqdev->gadget.ops = &mq_ops;

	strcpy(mqdev->gadget.dev.bus_id, "gadget");
	mqdev->gadget.ep0 = &mqdev->ep [0].ep;
	mqdev->gadget.dev.parent = &pdev->dev;
	mqdev->gadget.name = driver_name;
	mqdev->gadget.dev.release = gadget_release;
	mqdev->gadget.speed = USB_SPEED_FULL;

	/* Initialize UDC */
	base->set_power (base, MEDIAQ_11XX_UDC_DEVICE_ID, 1);
	/* Enable UDC transceiver */
	base->regs->DC.config_4 |= MQ_CONFIG_UDC_TRANSCEIVER_ENABLE;
	mqdev->enabled = 1;

	platform_set_drvdata(pdev, mqdev);

	/* Initialize endpoint list */
	INIT_LIST_HEAD (&mqdev->gadget.ep_list);
	for (i = 0; i < 4; i++) {
		struct mq_ep *ep = &mqdev->ep[i];

		ep->num = i;
		ep->ep.name = names[i];
		ep->mqdev = mqdev;
		ep->ep.ops = &mq_ep_ops;
		/* Endpoint 0 is not on the UDC's list of endpoints */
		if (i)
			list_add_tail (&ep->ep.ep_list, &mqdev->gadget.ep_list);
	}

	mqdev->ep[0].ep.maxpacket = MQ_EP01_MAXPACKET;

	mqdev->ep[0].is_in = 0;
	mqdev->ep[1].is_in = 1;
	mqdev->ep[2].is_in = 1;
	mqdev->ep[3].is_in = 0;

	mqdev->ep[0].regs = &regs->UDC.ep0txcontrol;
	mqdev->ep[1].regs = &regs->UDC.ep1control;
	mqdev->ep[2].regs = &regs->UDC.ep2control;
	mqdev->ep[3].regs = &regs->UDC.ep3control;

        mqdev->ep[2].dmaregs = &regs->UDC.dmatxcontrol;
	mqdev->ep[3].dmaregs = &regs->UDC.dmarxcontrol;

	/* Grab all used IRQs */
	for (i = 0; i < ARRAY_SIZE (irq_desc); i++) {
		int irqn = base->irq_base + irq_desc [i].delta;
		debug ("Requesting IRQ %d for %s\n", irqn, irq_desc [i].desc);
		if (request_irq (irqn, irq_desc [i].handler, SA_INTERRUPT,
				 irq_desc [i].desc, mqdev)) {
			printk (KERN_ERR "%s: failed to request %s IRQ %d\n",
				driver_name, irq_desc [i].desc, irqn);
			mqdev->got_irq = i - 1;
			mq_remove(pdev);
			return -EBUSY;
		}
	}
	mqdev->got_irq = ARRAY_SIZE (irq_desc);

	/* Init the device to known state */
	mq_reset (mqdev);
	mq_reinit (mqdev);

	/* Allocate two 4k buffers (used in turn) in MediaQ RAM for
	   every DMA-aware endpoint for MediaQ internal DMA transfers */
	for (i = 2; i <= 3; i++) {
		int buffsize = 2 * ((i == 2) ? txbs : rxbs);
		mqdev->ep[i].dmabuff = base->alloc (base, buffsize, 0);
		if (mqdev->ep[i].dmabuff == (u32)-1) {
			printk (KERN_ERR "%s: cannot allocate %d bytes in onboard RAM\n",
				driver_name, buffsize);
			mq_remove(pdev);
			return -ENOMEM;
		}
		else
			debug ("Allocated %d bytes in MediaQ RAM, addr = 0x%x\n",
			       buffsize, mqdev->ep[i].dmabuff);
	}

	device_register (&mqdev->gadget.dev);
	mqdev->registered = 1;

	the_controller = mqdev;

	info ("MediaQ 11xx UDC driver successfully initialized\n");
	return 0;
}

static int mq_remove(struct platform_device *pdev)
{
	struct mqudc *mqdev = platform_get_drvdata(pdev);
	int i;

	debug ("\n");

	if (!mqdev)
		return 0;

	/* start with the driver above us */
	if (mqdev->driver) {
		/* should have been done already by driver model core */
		debug ("driver '%s' is still registered\n",
			    mqdev->driver->driver.name);
		usb_gadget_unregister_driver(mqdev->driver);
	}

	if (mqdev->base)
		mq_reset(mqdev);

	for (i = 3; i >= 2; i--)
		if (mqdev->ep[i].dmabuff) {
			int buffsize = 2 * ((i == 2) ? txbs : rxbs);
			debug ("Freeing MediaQ RAM at 0x%x, size %d\n",
			       mqdev->ep[i].dmabuff, buffsize);
			mqdev->base->free (mqdev->base, mqdev->ep[i].dmabuff,
					   buffsize);
		}

	for (i = ((int)mqdev->got_irq) - 1; i >= 0; i--) {
		int irqn = mqdev->base->irq_base + irq_desc [i].delta;
		debug ("Freeing IRQ %d\n", irqn);
		free_irq(irqn, mqdev);
	}

	if (mqdev->enabled) {
		/* Disable UDC transceiver */
		mqdev->base->regs->DC.config_4 &= ~MQ_CONFIG_UDC_TRANSCEIVER_ENABLE;
		mqdev->base->set_power (mqdev->base, MEDIAQ_11XX_UDC_DEVICE_ID, 0);
	}

        if (mqdev->registered)
		device_unregister (&mqdev->gadget.dev);

	platform_set_drvdata(pdev, NULL);
	the_controller = NULL;

	kfree (mqdev);
	return 0;
}

static void mq_shutdown(struct platform_device *dev)
{
	debug ("\n");
}

static int mq_suspend(struct platform_device *dev, pm_message_t state)
{
	debug ("\n");

	return 0;
}

static int mq_resume(struct platform_device *dev)
{
	debug ("\n");

	return 0;
}

/* Initialization: we actually depend on two 'devices' on two 'busses':
 * the MediaQ UDC subdevice, and the platform-specific device (which handles
 * platform-specific UDC aspects such as connection reporting). Thus we
 * register two device drivers in sequence - when we find the platform
 * device, we register the mq UDC device driver.
 */

//static platform_device_id mq_platform_device_ids[] = { MEDIAQ_11XX_UDC_DEVICE_ID, 0 };

struct platform_driver mq_platform_driver = {
	.driver	= {
		.name     = (char *) driver_name,
	},
	.probe    = mq_probe,
	.remove	  = mq_remove,
	.shutdown = mq_shutdown,
	.suspend  = mq_suspend,
	.resume   = mq_resume
};

static int __init mq11xx_udc_init(void)
{
	return platform_driver_register(&mq_platform_driver);
}

static void __exit mq11xx_udc_exit(void)
{
	platform_driver_unregister(&mq_platform_driver);
}

module_init (mq11xx_udc_init);
module_exit (mq11xx_udc_exit);
