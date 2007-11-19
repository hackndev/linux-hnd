/*
 * Network driver for the SA1100 USB client function
 * Copyright (c) 2001 by Nicolas Pitre
 *
 * This code was loosely inspired by the original initial ethernet test driver
 * Copyright (c) Compaq Computer Corporation, 1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Issues:
 *  - DMA needs 8 byte aligned buffer, but causes inefficiencies
 *    in the IP code.
 *  - stall endpoint operations appeared to be very unstable.
 */

/*
 * Define RX_NO_COPY if you want data to arrive directly into the
 * receive network buffers, instead of arriving into bounce buffer
 * and then get copied to network buffer.
 *
 * Since the SA1100 DMA engine is unable to cope with unaligned
 * buffer addresses, we need to use bounce buffers or suffer the
 * alignment trap performance hit.
 */
#undef RX_NO_COPY

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/usb/ch9.h>

#include "client.h"


#define ETHERNET_VENDOR_ID  0x049f
#define ETHERNET_PRODUCT_ID 0x505A
#define MAX_PACKET 32768

// user space notification support for sa1100 usb state change
// Copyright (c) 2003 N C Bane
//#define NCB_HELPER

#ifdef NCB_HELPER
#include <linux/kmod.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

static char * device_state_names[] =
{ "not attached", "attached", "powered", "default",
  "address", "configured", "suspended" };

static void do_usb_helper(void *status)
{
	char *argv [3], *envp [3];
	int v = 0;

	argv [0] = "/sbin/sausb-hotplug";

	argv [1] = device_state_names[(int)status];
	argv [2] = NULL;

	envp [0] = "HOME=/";
	envp [1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp [2] = NULL;

/* no wait */
	v = call_usermodehelper (argv [0], argv, envp, 0);

	if (v != 0)
		printk ("sausb hotplug returned 0x%x", v);
}

static int usb_work_inited = 0;
static struct work_struct usb_work;

static void usb_helper (int status) {
    if (!usb_work_inited) {
	INIT_WORK(&usb_work,do_usb_helper);
	usb_work_inited = 1;
    }
    else
	PREPARE_WORK(&usb_work,do_usb_helper);
//    usb_task.func=do_usb_helper;
//    usb_task.data=(void *)status;
    schedule_work(&usb_work);
}
#endif

/*
 * This is our usb "packet size", and must match the host "packet size".
 */
static int usb_rsize = 64;
static int usb_wsize = 64;

struct usbe_info {
	struct net_device	dev;
	struct usb_client	client;
	struct sk_buff		*cur_tx_skb;
	struct sk_buff		*next_tx_skb;
	struct sk_buff		*cur_rx_skb;
	struct sk_buff		*next_rx_skb;
#ifndef RX_NO_COPY
	char *dmabuf;	// dma expects it's buffers to be aligned on 8 bytes boundary
#endif
	struct net_device_stats	stats;
};


static int usbeth_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu <= sizeof(struct ethhdr) || new_mtu > MAX_PACKET)
		return -EINVAL;

	// no second zero-length packet read wanted after mtu-sized packets
	if (((new_mtu + sizeof(struct ethhdr)) % usb_rsize) == 0)
		return -EDOM;

	dev->mtu = new_mtu;
	return 0;
}

static struct sk_buff *usb_new_recv_skb(struct usbe_info *usbe)
{
	struct sk_buff *skb;

	skb = alloc_skb(2 + sizeof(struct ethhdr) + usbe->dev.mtu,
			GFP_ATOMIC);

	if (skb)
		skb_reserve(skb, 2);

	return skb;
}

static void usbeth_recv_callback(void *data, int flag, int len)
{
	struct usbe_info *usbe = data;
	struct sk_buff *skb;
	unsigned int size;
	char *buf;

	skb = usbe->cur_rx_skb;

	/* flag validation */
	if (flag != 0)
		goto error;

	/*
	 * Make sure we have enough room left in the buffer.
	 */
	if (len > skb_tailroom(skb)) {
		usbe->stats.rx_over_errors++;
		usbe->stats.rx_errors++;
		goto oversize;
	}

	/*
	 * If the packet is smaller than usb_rsize bytes, the packet
	 * is complete, and we need to use the next receive buffer.
	 */
	if (len != usb_rsize)
		usbe->cur_rx_skb = usbe->next_rx_skb;

	/*
	 * Put the data onto the socket buffer and resume USB receive.
	 */
#ifndef RX_NO_COPY
	memcpy(skb_put(skb, len), usbe->dmabuf, len);
	buf = usbe->dmabuf;
	size = usb_rsize;
#else
	skb_put(skb, len);
	buf = usbe->cur_rx_skb->tail;
	size = skb_tailroom(usbe->cur_rx_skb);
#endif
	usbctl_ep_queue_buffer(usbe->client.ctl, 1, buf, size);

	if (len == usb_rsize)
		return;

	/*
	 * A frame must contain at least an ethernet header.
	 */
	if (skb->len < sizeof(struct ethhdr)) {
		usbe->stats.rx_length_errors++;
		usbe->stats.rx_errors++;
		goto recycle;
	}

	/*
	 * MAC must match our address or the broadcast address.
	 * Really, we should let any packet through, otherwise
	 * things that rely on multicast won't work.
	 */
	if (memcmp(skb->data, usbe->dev.dev_addr, ETH_ALEN) &&
	    memcmp(skb->data, usbe->dev.broadcast, ETH_ALEN)) {
		usbe->stats.rx_frame_errors++;
		usbe->stats.rx_errors++;
		goto recycle;
	}

	/*
	 * We're going to consume this SKB.  Get a new skb to
	 * replace it with.  IF this fails, we'd better recycle
	 * the one we have.
	 */
	usbe->next_rx_skb = usb_new_recv_skb(usbe);
	if (!usbe->next_rx_skb) {
		if (net_ratelimit())
			printk(KERN_ERR "%s: can't allocate new rx skb\n",
				usbe->dev.name);
		usbe->stats.rx_dropped++;
		goto recycle;
	}

// FIXME: eth_copy_and_csum "small" packets to new SKB (small < ~200 bytes) ?

	usbe->stats.rx_packets++;
	usbe->stats.rx_bytes += skb->len;
	usbe->dev.last_rx = jiffies;

	skb->dev = &usbe->dev;
	skb->protocol = eth_type_trans(skb, &usbe->dev);
	skb->ip_summed = CHECKSUM_NONE;

	if (netif_rx(skb) == NET_RX_DROP)
		usbe->stats.rx_dropped++;
	return;

 error:
	/*
	 * Oops, IO error, or stalled.
	 */
	switch (flag) {
	case -EIO:	/* aborted transfer */
		usbe->stats.rx_errors++;
		break;

	case -EPIPE:	/* fifo screwed/no data */
		usbe->stats.rx_fifo_errors++;
		usbe->stats.rx_errors++;
		break;

	case -EINTR:	/* reset */
		break;

	case -EAGAIN:	/* initialisation */
		break;
	}

 oversize:
	skb_trim(skb, 0);

#ifndef RX_NO_COPY
	buf = usbe->dmabuf;
	size = usb_rsize;
#else
	buf = skb->tail;
	size = skb_tailroom(skb);
#endif
	usbctl_ep_queue_buffer(usbe->client.ctl, 1, buf, size);
	return;

 recycle:
	skb_trim(skb, 0);
	usbe->next_rx_skb = skb;
	return;
}

/*
 * Send a skb.
 *
 * Note that the receiver expects the last packet to be a non-multiple
 * of its rsize.  If the packet length is a muliple of wsize (and
 * therefore the remote rsize) tweak the length.
 */
static void usbeth_send(struct sk_buff *skb, struct usbe_info *usbe)
{
	unsigned int len = skb->len;
	int ret;

	if ((len % usb_wsize) == 0)
		len++;

	ret = usbctl_ep_queue_buffer(usbe->client.ctl, 2, skb->data, len);
	if (ret) {
		printk(KERN_ERR "%s: tx dropping packet: %d\n",
		       usbe->dev.name, ret);

		/*
		 * If the USB core can't accept the packet, we drop it.
		 */
		dev_kfree_skb_irq(skb);

		usbe->cur_tx_skb = NULL;
		usbe->stats.tx_carrier_errors++;
	} else {
		usbe->dev.trans_start = jiffies;
	}
}

static void usbeth_send_callback(void *data, int flag, int size)
{
	struct usbe_info *usbe = data;
	struct sk_buff *skb = usbe->cur_tx_skb;

	switch (flag) {
	case 0:
		usbe->stats.tx_packets++;
if (!skb)
    printk("%s: no skb so no data sent?\n",__FUNCTION__);
else
		usbe->stats.tx_bytes += skb->len;
		break;
	case -EIO:
		usbe->stats.tx_errors++;
		break;
	default:
		usbe->stats.tx_dropped++;
		break;
	}
if (!skb)
    printk("%s: no skb so not freed!\n",__FUNCTION__);
else
	dev_kfree_skb_irq(skb);

	skb = usbe->cur_tx_skb = usbe->next_tx_skb;
	usbe->next_tx_skb = NULL;

	if (skb)
		usbeth_send(skb, usbe);

	netif_wake_queue(&usbe->dev);
}

static int usbeth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct usbe_info *usbe = dev->priv;
	unsigned long flags;

if (!skb)
    printk("%s: Oi! cannot xmit a null *skb\n",__FUNCTION__);
    
	if (usbe->next_tx_skb) {
		printk(KERN_ERR "%s: called with next_tx_skb != NULL\n",
		       usbe->dev.name);
		return 1;
	}

	local_irq_save(flags);
	if (usbe->cur_tx_skb) {
		usbe->next_tx_skb = skb;
		netif_stop_queue(dev);
	} else {
		usbe->cur_tx_skb = skb;

		usbeth_send(skb, usbe);
	}
	local_irq_restore(flags);
	return 0;
}

/*
 * Transmit timed out.  Reset the endpoint, and re-queue the pending
 * packet.  If we have a free transmit slot, wake the transmit queue.
 */
static void usbeth_xmit_timeout(struct net_device *dev)
{
	struct usbe_info *usbe = dev->priv;
	unsigned long flags;

printk("%s: Resetting Tx endpoint\n",__FUNCTION__);
	usbctl_ep_reset(usbe->client.ctl, 2);

	local_irq_save(flags);
	if (usbe->cur_tx_skb) {
printk("%s: Sending packet\n",__FUNCTION__);
		usbeth_send(usbe->cur_tx_skb, usbe);
	}

	if (usbe->next_tx_skb == NULL)
		netif_wake_queue(dev);

	usbe->stats.tx_errors++;
	local_irq_restore(flags);
}

static int usbeth_open(struct net_device *dev)
{
	struct usbe_info *usbe = dev->priv;
	unsigned char *buf;
	unsigned int size;

	usbctl_ep_set_callback(usbe->client.ctl, 2, usbeth_send_callback, usbe);
	usbctl_ep_set_callback(usbe->client.ctl, 1, usbeth_recv_callback, usbe);

	usbe->cur_tx_skb = usbe->next_tx_skb = NULL;
	usbe->cur_rx_skb = usb_new_recv_skb(usbe);
	usbe->next_rx_skb = usb_new_recv_skb(usbe);
	if (!usbe->cur_rx_skb || !usbe->next_rx_skb) {
		printk(KERN_ERR "%s: can't allocate new skb\n",
			usbe->dev.name);
		if (usbe->cur_rx_skb)
			kfree_skb(usbe->cur_rx_skb);
		if (usbe->next_rx_skb)
			kfree_skb(usbe->next_rx_skb);
		return -ENOMEM;;
	}
#ifndef RX_NO_COPY
	buf =  usbe->dmabuf;
	size = usb_rsize;
#else
	buf = usbe->cur_rx_skb->tail;
	size = skb_tailroom(usbe->cur_rx_skb);
#endif
	usbctl_ep_queue_buffer(usbe->client.ctl, 1, buf, size);

	if (netif_carrier_ok(dev))
		netif_start_queue(dev);

	return 0;
}

static int usbeth_close(struct net_device *dev)
{
	struct usbe_info *usbe = dev->priv;

	netif_stop_queue(dev);

	usbctl_ep_set_callback(usbe->client.ctl, 2, NULL, NULL);
	usbctl_ep_set_callback(usbe->client.ctl, 1, NULL, NULL);
	usbctl_ep_reset(usbe->client.ctl, 2);
	usbctl_ep_reset(usbe->client.ctl, 1);

	if (usbe->cur_tx_skb)
		kfree_skb(usbe->cur_tx_skb);
	if (usbe->next_tx_skb)
		kfree_skb(usbe->next_tx_skb);
	if (usbe->cur_rx_skb)
		kfree_skb(usbe->cur_rx_skb);
	if (usbe->next_rx_skb)
		kfree_skb(usbe->next_rx_skb);

	return 0;
}

static struct net_device_stats *usbeth_stats(struct net_device *dev)
{
	struct usbe_info *usbe = dev->priv;

	return &usbe->stats;
}

static int __init usbeth_probe(struct net_device *dev)
{
	u8 node_id[ETH_ALEN];

	SET_MODULE_OWNER(dev);

	/*
	 * Assign the hardware address of the board:
	 * generate it randomly, as there can be many such
	 * devices on the bus.
	 */
	get_random_bytes(node_id, sizeof node_id);
	node_id[0] &= 0xfe;	// clear multicast bit
	memcpy(dev->dev_addr, node_id, sizeof node_id);

	ether_setup(dev);
	dev->flags &= ~IFF_MULTICAST;
	dev->flags &= ~IFF_BROADCAST;
	//dev->flags |= IFF_NOARP;

	return 0;
}

/*
 * This is called when something in the upper usb client layers
 * changes that affects the endpoint connectivity state (eg,
 * connection or disconnection from the host.)  We probably want
 * to do some more handling here, like kicking off a pending
 * transmission if we're running?
 */
static void usbeth_state_change(void *data, int state, int oldstate)
{
	struct usbe_info *usbe = data;

	if (state == USB_STATE_CONFIGURED) {
		netif_carrier_on(&usbe->dev);
		if (netif_running(&usbe->dev))
			netif_wake_queue(&usbe->dev);
	} else {
		if (netif_running(&usbe->dev))
			netif_stop_queue(&usbe->dev);
		netif_carrier_off(&usbe->dev);
	}
#ifdef NCB_HELPER
	usb_helper(state);
#endif
}

static struct usbe_info usbe_info = {
	.dev	= {
		.name			= "usbf",
		.init			= usbeth_probe,
		.get_stats		= usbeth_stats,
		.watchdog_timeo		= 1 * HZ,
		.open			= usbeth_open,
		.stop			= usbeth_close,
		.hard_start_xmit	= usbeth_xmit,
		.change_mtu		= usbeth_change_mtu,
		.tx_timeout		= usbeth_xmit_timeout,
		.priv			= &usbe_info,
	},
	.client	= {
		.name			= "usbeth",
		.priv			= &usbe_info,
		.state_change		= usbeth_state_change,

		/*
		 * USB client identification for host use in CPU endian.
		 */
		.vendor			= ETHERNET_VENDOR_ID,
		.product		= ETHERNET_PRODUCT_ID,
		.version		= 0,
		.class			= 0xff,	/* vendor specific */
		.subclass		= 0,
		.protocol		= 0,

		.product_str		= "SA1100 USB NIC",
	},
};

static int __init usbeth_init(void)
{
	int rc;

#ifndef RX_NO_COPY
	usbe_info.dmabuf = kmalloc(usb_rsize, GFP_KERNEL | GFP_DMA);
	if (!usbe_info.dmabuf)
		return -ENOMEM;
#endif

	if (register_netdev(&usbe_info.dev) != 0) {
#ifndef RX_NO_COPY
		kfree(usbe_info.dmabuf);
#endif
		return -EIO;
	}

	rc = usbctl_open(&usbe_info.client);
	if (rc == 0) {
		struct cdb *cdb = sa1100_usb_get_descriptor_ptr();

		cdb->ep1.wMaxPacketSize = cpu_to_le16(usb_rsize);
		cdb->ep2.wMaxPacketSize = cpu_to_le16(usb_wsize);

		rc = usbctl_start(&usbe_info.client);
		if (rc)
			usbctl_close(&usbe_info.client);
	}

	if (rc) {
		unregister_netdev(&usbe_info.dev);
#ifndef RX_NO_COPY
		kfree(usbe_info.dmabuf);
#endif
	}

	return rc;
}

static void __exit usbeth_cleanup(void)
{
	usbctl_stop(&usbe_info.client);
	usbctl_close(&usbe_info.client);

	unregister_netdev(&usbe_info.dev);
#ifndef RX_NO_COPY
	kfree(usbe_info.dmabuf);
#endif
}

module_init(usbeth_init);
module_exit(usbeth_cleanup);

MODULE_DESCRIPTION("USB client ethernet driver");
//MODULE_PARM(usb_rsize, "1i");
MODULE_PARM_DESC(usb_rsize, "number of bytes in packets from host to sa11x0");
//MODULE_PARM(usb_wsize, "1i");
MODULE_PARM_DESC(usb_wsize, "number of bytes in packets from sa11x0 to host");
MODULE_LICENSE("GPL");
