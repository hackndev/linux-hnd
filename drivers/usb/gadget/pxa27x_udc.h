/*
 * linux/drivers/usb/gadget/pxa27x_udc.h
 * Intel PXA27x on-chip full speed USB device controller
 *
 * Copyright (C) 2003 Robert Schwebel <r.schwebel@pengutronix.de>, Pengutronix
 * Copyright (C) 2003 David Brownell
 * Copyright (C) 2004 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_USB_GADGET_PXA27X_H
#define __LINUX_USB_GADGET_PXA27X_H

#include <linux/types.h>

struct pxa27x_udc;

struct pxa27x_ep {
	struct usb_ep				ep;
	struct pxa27x_udc			*dev;
	struct usb_ep				*usb_ep;
	const struct usb_endpoint_descriptor	*desc;

	struct list_head			queue;
	unsigned long				pio_irqs;
	unsigned long				dma_irqs;
	
	int					dma; 
	unsigned				fifo_size;
	unsigned				ep_num;
	unsigned				pxa_ep_num;
	unsigned				ep_type;

	unsigned				stopped : 1;
	unsigned				dma_con : 1;
	unsigned				dir_in : 1;
	unsigned				assigned : 1;

	unsigned				config;
	unsigned				interface;
	unsigned				aisn;
	/* UDCCSR = UDC Control/Status Register for this EP
	 * UBCR = UDC Byte Count Remaining (contents of OUT fifo)
	 * UDCDR = UDC Endpoint Data Register (the fifo)
	 * UDCCR = UDC Endpoint Configuration Registers
	 * DRCM = DMA Request Channel Map
	 */
	volatile u32				*reg_udccsr;
	volatile u32				*reg_udcbcr;
	volatile u32				*reg_udcdr;
	volatile u32				*reg_udccr;
#ifdef USE_DMA
	volatile u32				*reg_drcmr;
#define	drcmr(n)  .reg_drcmr = & DRCMR ## n ,
#else
#define	drcmr(n)
#endif

#ifdef CONFIG_PM
	unsigned				udccsr_value;
	unsigned				udccr_value;
#endif
};

struct pxa27x_virt_ep {
	struct usb_ep				usb_ep;
	const struct usb_endpoint_descriptor	*desc;
	struct pxa27x_ep			*pxa_ep;
};

struct pxa27x_request {
	struct usb_request			req;
	struct list_head			queue;
};

enum ep0_state {
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
//	EP0_END_XFER,
	EP0_STALL,
	EP0_NO_ACTION
};

#define EP0_FIFO_SIZE	((unsigned)16)
#define BULK_FIFO_SIZE	((unsigned)64)
#define ISO_FIFO_SIZE	((unsigned)256)
#define INT_FIFO_SIZE	((unsigned)8)

struct udc_stats {
	struct ep0stats {
		unsigned long		ops;
		unsigned long		bytes;
	} read, write;
	unsigned long			irqs;
};

#ifdef CONFIG_USB_PXA27X_SMALL
/* when memory's tight, SMALL config saves code+data.  */
//#undef	USE_DMA
//#define	UDC_EP_NUM	3
#endif

#ifndef	UDC_EP_NUM
#define	UDC_EP_NUM	24
#endif

struct pxa27x_udc {
	struct usb_gadget			gadget;
	struct usb_gadget_driver		*driver;

	enum ep0_state				ep0state;
	struct udc_stats			stats;
	unsigned				got_irq : 1,
						got_disc : 1,
						has_cfr : 1,
						req_pending : 1,
						req_std : 1,
						req_config : 1;

#define start_watchdog(dev) mod_timer(&dev->timer, jiffies + (HZ/200))
	struct timer_list			timer;

	struct device				*dev;
	struct pxa2xx_udc_mach_info		*mach;
	u64					dma_mask;
	struct pxa27x_virt_ep			virt_ep0;
	struct pxa27x_ep			ep[UDC_EP_NUM];
	unsigned int 				ep_num;

	unsigned				configuration, 
						interface, 
						alternate;
#ifdef CONFIG_PM
	unsigned				udccsr0;
#endif
};

/*-------------------------------------------------------------------------*/
#if 0
#ifdef DEBUG
#define HEX_DISPLAY(n)	do { \
	if (machine_is_mainstone())\
		 { MST_LEDDAT1 = (n); } \
	} while(0)

#define HEX_DISPLAY1(n)	HEX_DISPLAY(n)

#define HEX_DISPLAY2(n)	do { \
	if (machine_is_mainstone()) \
		{ MST_LEDDAT2 = (n); } \
	} while(0)

#endif /* DEBUG */
#endif
/*-------------------------------------------------------------------------*/

/* LEDs are only for debug */
#ifndef HEX_DISPLAY
#define HEX_DISPLAY(n)		do {} while(0)
#endif

#ifndef LED_CONNECTED_ON
#define LED_CONNECTED_ON	do {} while(0)
#define LED_CONNECTED_OFF	do {} while(0)
#endif
#ifndef LED_EP0_ON
#define LED_EP0_ON		do {} while (0)
#define LED_EP0_OFF		do {} while (0)
#endif

static struct pxa27x_udc *the_controller;

#if 0
/*-------------------------------------------------------------------------*/


/* one GPIO should be used to detect host disconnect */
static inline int is_usb_connected(void)
{
	if (!the_controller->mach->udc_is_connected)
		return 1;
	return the_controller->mach->udc_is_connected();
}

/* one GPIO should force the host to see this device (or not) */
static inline void make_usb_disappear(void)
{
	if (!the_controller->mach->udc_command)
		return;
	the_controller->mach->udc_command(PXA27X_UDC_CMD_DISCONNECT);
}

static inline void let_usb_appear(void)
{
	if (!the_controller->mach->udc_command)
		return;
	the_controller->mach->udc_command(PXA2XX_UDC_CMD_CONNECT);
}
#endif

/*-------------------------------------------------------------------------*/

/*
 * Debugging support vanishes in non-debug builds.  DBG_NORMAL should be
 * mostly silent during normal use/testing, with no timing side-effects.
 */
#define DBG_NORMAL	1	/* error paths, device state transitions */
#define DBG_VERBOSE	2	/* add some success path trace info */
#define DBG_NOISY	3	/* ... even more: request level */
#define DBG_VERY_NOISY	4	/* ... even more: packet level */

#ifdef DEBUG

static const char *state_name[] = {
	"EP0_IDLE",
	"EP0_IN_DATA_PHASE", "EP0_OUT_DATA_PHASE",
	"EP0_END_XFER", "EP0_STALL"
};

#define DMSG(stuff...) printk(KERN_ERR "udc: " stuff)

#ifdef VERBOSE
#    define UDC_DEBUG DBG_VERBOSE
#else
#    define UDC_DEBUG DBG_NORMAL
#endif

static void __attribute__ ((__unused__))
dump_udccr(const char *label)
{
	u32	udccr = UDCCR;
	DMSG("%s 0x%08x =%s%s%s%s%s%s%s%s%s%s, con=%d,inter=%d,altinter=%d\n",
		label, udccr,
		(udccr & UDCCR_OEN) ? " oen":"",
		(udccr & UDCCR_AALTHNP) ? " aalthnp":"",
		(udccr & UDCCR_AHNP) ? " rem" : "",
		(udccr & UDCCR_BHNP) ? " rstir" : "",
		(udccr & UDCCR_DWRE) ? " dwre" : "",
		(udccr & UDCCR_SMAC) ? " smac" : "",
		(udccr & UDCCR_EMCE) ? " emce" : "",
		(udccr & UDCCR_UDR) ? " udr" : "",
		(udccr & UDCCR_UDA) ? " uda" : "",
		(udccr & UDCCR_UDE) ? " ude" : "",
		(udccr & UDCCR_ACN) >> UDCCR_ACN_S,
		(udccr & UDCCR_AIN) >> UDCCR_AIN_S,
		(udccr & UDCCR_AAISN)>> UDCCR_AAISN_S );
}

static void __attribute__ ((__unused__))
dump_udccsr0(const char *label)
{
	u32		udccsr0 = UDCCSR0;

	DMSG("%s %s 0x%08x =%s%s%s%s%s%s%s\n",
		label, state_name[the_controller->ep0state], udccsr0,
		(udccsr0 & UDCCSR0_SA) ? " sa" : "",
		(udccsr0 & UDCCSR0_RNE) ? " rne" : "",
		(udccsr0 & UDCCSR0_FST) ? " fst" : "",
		(udccsr0 & UDCCSR0_SST) ? " sst" : "",
		(udccsr0 & UDCCSR0_DME) ? " dme" : "",
		(udccsr0 & UDCCSR0_IPR) ? " ipr" : "",
		(udccsr0 & UDCCSR0_OPC) ? " opr" : "");
}

static void __attribute__ ((__unused__))
dump_state(struct pxa27x_udc *dev)
{
	unsigned	i;

	DMSG("%s, udcicr %02X.%02X, udcsir %02X.%02x, udcfnr %02X\n",
		state_name[dev->ep0state],
		UDCICR1, UDCICR0, UDCISR1, UDCISR0, UDCFNR);
	dump_udccr("udccr");

	if (!dev->driver) {
		DMSG("no gadget driver bound\n");
		return;
	} else
		DMSG("ep0 driver '%s'\n", dev->driver->driver.name);

	
	dump_udccsr0 ("udccsr0");
	DMSG("ep0 IN %lu/%lu, OUT %lu/%lu\n",
		dev->stats.write.bytes, dev->stats.write.ops,
		dev->stats.read.bytes, dev->stats.read.ops);

	for (i = 1; i < UDC_EP_NUM; i++) {
		if (dev->ep [i].desc == 0)
			continue;
		DMSG ("udccs%d = %02x\n", i, *dev->ep->reg_udccsr);
	}
}

#if 0
static void dump_regs(u8 ep)
{
	DMSG("EP:%d UDCCSR:0x%08x UDCBCR:0x%08x\n UDCCR:0x%08x\n",
		ep,UDCCSN(ep), UDCBCN(ep), UDCCN(ep));
}
static void dump_req (struct pxa27x_request *req)
{
	struct usb_request *r = &req->req;

	DMSG("%s: buf:0x%08x length:%d dma:0x%08x actual:%d\n",
			__FUNCTION__, (unsigned)r->buf, r->length,
			r->dma,	r->actual);
}
#endif

#else

#define DMSG(stuff...)		do{}while(0)

#define	dump_udccr(x)	do{}while(0)
#define	dump_udccsr0(x)	do{}while(0)
#define	dump_state(x)	do{}while(0)

#define UDC_DEBUG ((unsigned)0)

#endif

#define DBG(lvl, stuff...) do{if ((lvl) <= UDC_DEBUG) DMSG(stuff);}while(0)

#define WARN(stuff...) printk(KERN_WARNING "udc: " stuff)
#define INFO(stuff...) printk(KERN_INFO "udc: " stuff)


#endif /* __LINUX_USB_GADGET_PXA27X_H */
