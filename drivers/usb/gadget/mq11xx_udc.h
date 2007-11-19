/*
 * MediaQ 11xx USB Device Controller driver
 * Based on Goku-S UDC driver
 *
 * Copyright (C) 2004 Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef _MQ11XX_UDC_H
#define _MQ11XX_UDC_H

#ifdef __MQ11XX_INTERNAL

#include <linux/mfd/mq11xx.h>

/* MediaQ UDC request structure.
 */
struct mq_request {
	/* The public request structure */
	struct usb_request req;
	/* The chained list to link requests belonging to one endpoint */
	struct list_head queue;
};

/* USB Endpoint structure. 
 */
struct mq_ep {
	/* Public endpoint structure */
	struct usb_ep ep;
	/* A chained list of mq_request structures */
	struct list_head list;
	/* Endpoint number */
	unsigned num:8;
	/* 1 if endpoint is INput (e.g. TX from UDC point of view) */
	unsigned is_in:1;
	/* 1 if endpoint is stopped */
	unsigned stopped:1;
	/* 1 if MediaQ RAM has been allocated for this endpoint */
	unsigned got_mqram:1;
	/* 1 if endpoint configured in isochronous mode */
	unsigned iso:1;
	/* Toggles between active buffers on DMA transfers */
	unsigned active_buffer:1;
	/* Number of bytes in quick_buff to send plus one */
	unsigned quick_buff_bytes:3;
	/* A quick buffer for small transfers
	 * (typically used during SETUP phase and for ZLP).
	 */
	u32 quick_buff;
	/* A pointer to MediaQ registers associated with this endpoint */
	volatile u32 *regs;
	/* A pointer to MediaQ registers responsible for DMA transfers */
	volatile u32 *dmaregs;
	/* The offset in MediaQ internal RAM of two consecutive buffers
	 * used for DMA transfers.
	 */
	volatile u32 dmabuff;
	/* Parent device */
	struct mqudc *mqdev;
	/* Endpoint descriptor */
	const struct usb_endpoint_descriptor *desc;
};

enum ep0state {
	EP0_DISCONNECT,		/* no host */
	EP0_SUSPEND,		/* usb suspend */
	EP0_IDLE,		/* between STATUS ack and SETUP report */
	EP0_IN, EP0_OUT, 	/* data stage */
	EP0_STALL,		/* something really bad happened */
};

/* The USB Device Controller structure.
 */
struct mqudc {
	/* Public gadget structure */
	struct usb_gadget gadget;
	/* Spinlock for accessing the UDC */
	spinlock_t lock;
	/* All of UDC endpoints */
	struct mq_ep ep[4];
	/* A pointer to bound gadget driver */
	struct usb_gadget_driver *driver;
	/* DATA0/DATA1 toggle */
	u32 toggle;

	/* Current endpoint 0 state */
	enum ep0state ep0state;

	/* MediaQ 11xx base SoC driver public structure pointer */
	struct mediaq11xx_base *base;

	/* The following is used for device cleanup */

	/* Number of IRQs requested so far */
	unsigned got_irq:2;
	/* 1 if the UDC is enabled */
	unsigned enabled:1;
        /* 1 if device has been registered with the kernel */
	unsigned registered:1;
};

#endif

#endif /* _MQ11XX_UDC_H */
