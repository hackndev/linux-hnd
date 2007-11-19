/*
 * char.c -- Character Gadget
 *
 * Copyright (C) 2003-2005 Joshua Wise.
 * Portions copyright (C) 2003-2004 David Brownell.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define DEBUG 1
#define VERBOSE
#define DBUFMAX 8192

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
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
#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/tty.h>
#include <linux/poll.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/fs.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>

#include <linux/usb_ch9.h>
#include <linux/usb_gadget.h>

struct gchar_dev {
	spinlock_t		lock;
	struct usb_gadget	*gadget;
	struct usb_request	*req;		/* for control responses */
	struct usb_ep		*in_ep, *out_ep;
	
	int			configured;
	
	unsigned char		databuf[DBUFMAX];
	int			databuflen;
	spinlock_t		buflock;
	wait_queue_head_t	wait;
};

/*-------------------------------------------------------------------------*/

static int gchar_bind_misc(struct gchar_dev *dev);
static int gchar_unbind_misc(struct gchar_dev *dev);

/*-------------------------------------------------------------------------*/

static const char shortname [] = "char";
static const char longname [] = "Character Gadget";

static const char charmode [] = "Character mode";

/*-------------------------------------------------------------------------*/

#define EP0_MAXPACKET		16
static const char *EP_OUT_NAME;
static const char *EP_IN_NAME;

/*-------------------------------------------------------------------------*/

/* any hub supports this steady state bus power consumption */
#define MAX_USB_POWER	100	/* mA */

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ	256

/*-------------------------------------------------------------------------*/

#define xprintk(d,level,fmt,args...)	dev_printk(level , &(d)->gadget->dev , fmt , ## args)

#ifdef DEBUG
#  undef DEBUG
#  define DEBUG(dev,fmt,args...)	printk(KERN_DEBUG "gchar: " fmt , ## args)
#else
#  define DEBUG(dev,fmt,args...)	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE
#  define VDEBUG			DEBUG
#else
#  define VDEBUG(dev,fmt,args...)	do { } while (0)
#endif /* VERBOSE */

#define ERROR(dev,fmt,args...)		printk(KERN_ERR "gchar: " fmt , ## args)
#define WARN(dev,fmt,args...)		printk(KERN_WARNING "gchar: " fmt , ## args)
#define INFO(dev,fmt,args...)		printk(KERN_INFO "gchar: " fmt , ## args)

/*-------------------------------------------------------------------------*/

static unsigned buflen = 1536;
static unsigned qlen = 4;

module_param (buflen, uint, S_IRUGO|S_IWUSR);
module_param (qlen, uint, S_IRUGO|S_IWUSR);

/*-------------------------------------------------------------------------*/

#define DRIVER_VENDOR_NUM	0x6666		/* Experimental */
#define DRIVER_PRODUCT_NUM	0xB007		/* Bootloader */

/*-------------------------------------------------------------------------*/

/*
 * DESCRIPTORS ... most are static, but strings and (full)
 * configuration descriptors are built on demand.
 */

#define STRING_MANUFACTURER		25
#define STRING_PRODUCT			42
#define STRING_SERIAL			101
#define STRING_CHAR			250

/*
 * This device advertises one configuration.
 */
#define	CONFIG_CHAR		1

static struct usb_device_descriptor
device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		__constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.idVendor =		__constant_cpu_to_le16 (DRIVER_VENDOR_NUM),
	.idProduct =		__constant_cpu_to_le16 (DRIVER_PRODUCT_NUM),
	.bcdDevice =		__constant_cpu_to_le16 (0x0000),
	.iManufacturer =	STRING_MANUFACTURER,
	.iProduct =		STRING_PRODUCT,
	.iSerialNumber =	STRING_SERIAL,
	.bNumConfigurations =	1,
};

static const struct usb_config_descriptor
char_config = {
	.bLength =		sizeof char_config,
	.bDescriptorType =	USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces =	1,
	.bConfigurationValue =	CONFIG_CHAR,
	.iConfiguration =	STRING_CHAR,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER | 0 /* no wakeup support */,
	.bMaxPower =		(MAX_USB_POWER + 1) / 2,
};

/* one interface in each configuration */

static const struct usb_interface_descriptor
char_intf = {
	.bLength =		sizeof char_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.iInterface =		STRING_CHAR,
};

/* two full speed bulk endpoints; their use is config-dependent */

static struct usb_endpoint_descriptor
fs_char_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor
fs_char_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

/* if there's no high speed support, maxpacket doesn't change. */
static char				manufacturer [50];
static char				serial [40];

/* static strings, in iso 8859/1 */
static struct usb_string		strings [] = {
	{ STRING_MANUFACTURER, manufacturer, },
	{ STRING_PRODUCT, longname, },
	{ STRING_SERIAL, serial, },
	{ STRING_CHAR, charmode, },
	{  }			/* end of list */
};

static struct usb_gadget_strings	stringtab = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings,
};

static int
config_buf (enum usb_device_speed speed,
		u8 *buf, u8 type, unsigned index)
{
	const unsigned	config_len = USB_DT_CONFIG_SIZE
				+ USB_DT_INTERFACE_SIZE
				+ 2 * USB_DT_ENDPOINT_SIZE;

	/* two configurations will always be index 0 and index 1 */
	if (index > 1)
		return -EINVAL;
	if (config_len > USB_BUFSIZ)
		return -EDOM;

	/* config (or other speed config) */
	memcpy (buf, &char_config, USB_DT_CONFIG_SIZE);
	buf [1] = type;
	((struct usb_config_descriptor *) buf)->wTotalLength
		= __constant_cpu_to_le16 (config_len);
	buf += USB_DT_CONFIG_SIZE;

	/* one interface */
	memcpy (buf, &char_intf, USB_DT_INTERFACE_SIZE);
	buf += USB_DT_INTERFACE_SIZE;

	/* the endpoints in that interface (at that speed) */
	memcpy (buf, &fs_char_in_desc, USB_DT_ENDPOINT_SIZE);
	buf += USB_DT_ENDPOINT_SIZE;
	memcpy (buf, &fs_char_out_desc, USB_DT_ENDPOINT_SIZE);
	buf += USB_DT_ENDPOINT_SIZE;

	return config_len;
}

/*-------------------------------------------------------------------------*/

static struct usb_request *
alloc_ep_req (struct usb_ep *ep, unsigned length)
{
	struct usb_request	*req;

	req = usb_ep_alloc_request (ep, GFP_ATOMIC);
	if (req) {
		req->length = length;
		req->buf = usb_ep_alloc_buffer (ep, length,
				&req->dma, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request (ep, req);
			req = NULL;
		}
	}
	return req;
}

static void free_ep_req (struct usb_ep *ep, struct usb_request *req)
{
	if (req->buf)
		usb_ep_free_buffer (ep, req->buf, req->dma, req->length);
	usb_ep_free_request (ep, req);
}

/*-------------------------------------------------------------------------*/

static void gchar_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct gchar_dev	*dev = ep->driver_data;
	int			status = req->status;

	switch (status) {

	case 0: 			/* normal completion? */
		if (ep != dev->out_ep) {
			ERROR (dev, "complete: invalid endpoint --> %s (%d/%d)\n", ep->name, req->actual, req->length);
			free_ep_req (ep, req);
			return;
		}

		/* it looks like you've got a packet! */
		req->length = req->actual;
		
		spin_lock(&dev->buflock);
		{
			int cpylen;
			
			cpylen = ((dev->databuflen+req->actual) < DBUFMAX) ? req->actual : (DBUFMAX - dev->databuflen);
			memcpy(&dev->databuf[dev->databuflen], req->buf, cpylen);
			dev->databuflen += cpylen;
		}
		spin_unlock(&dev->buflock);
		wake_up_interruptible(&dev->wait);
		
		/* queue the buffer for some later OUT packet */
		req->length = buflen;
		status = usb_ep_queue (dev->out_ep, req, GFP_ATOMIC);
		if (status == 0)
			return;
		
		/* FALLTHROUGH */
	default:
		ERROR (dev, "%s gchar recv complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* FALLTHROUGH */

	/* NOTE:  since this driver doesn't maintain an explicit record
	 * of requests it submitted (just maintains qlen count), we
	 * rely on the hardware driver to clean up on disconnect or
	 * endpoint disable.
	 */
	case -ECONNABORTED: 		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
		
		return;
	}
}

static int
set_char_config (struct gchar_dev *dev, int gfp_flags)
{
	int			result = 0;
	struct usb_ep		*ep;
	struct usb_gadget	*gadget = dev->gadget;

	gadget_for_each_ep (ep, gadget) {
		/* one endpoint writes data back IN to the host */
		if (strcmp (ep->name, EP_IN_NAME) == 0) {
			result = usb_ep_enable (ep, &fs_char_in_desc);
			if (result == 0) {
				ep->driver_data = dev;
				dev->in_ep = ep;
				continue;
			}

		/* one endpoint just reads OUT packets */
		} else if (strcmp (ep->name, EP_OUT_NAME) == 0) {
			result = usb_ep_enable (ep, &fs_char_out_desc);
			if (result == 0) {
				ep->driver_data = dev;
				dev->out_ep = ep;
				continue;
			}

		/* ignore any other endpoints */
		} else
			continue;

		/* stop on error */
		ERROR (dev, "can't enable %s, result %d\n", ep->name, result);
		break;
	}

	/* allocate a bunch of read buffers and queue them all at once.
	 * we buffer at most 'qlen' transfers; fewer if any need more
	 * than 'buflen' bytes each.
	 */
	if (result == 0) {
		struct usb_request	*req;
		unsigned		i;

		ep = dev->out_ep;
		for (i = 0; i < qlen && result == 0; i++) {
			DEBUG (dev, "%s alloc req of size %d (%d/%d)\n", ep->name, buflen, i, qlen);
			req = alloc_ep_req (ep, buflen);
			if (req) {
				req->complete = gchar_complete;
				result = usb_ep_queue (ep, req, GFP_ATOMIC);
				if (result)
					DEBUG (dev, "%s queue req --> %d\n",
							ep->name, result);
			} else
				result = -ENOMEM;
		}
		
	}

	/* caller is responsible for cleanup on error */
	return result;
}

/*-------------------------------------------------------------------------*/

static void gchar_reset_config (struct gchar_dev *dev)
{
	if (dev->configured == 0)
		return;

	DEBUG (dev, "reset config\n");

	/* just disable endpoints, forcing completion of pending i/o.
	 * all our completion handlers free their requests in this case.
	 */
	if (dev->in_ep) {
		usb_ep_disable (dev->in_ep);
		dev->in_ep = NULL;
	}
	if (dev->out_ep) {
		usb_ep_disable (dev->out_ep);
		dev->out_ep = NULL;
	}
	dev->configured = 0;
}

/* change our operational config.  this code must agree with the code
 * that returns config descriptors, and altsetting code.
 *
 * it's also responsible for power management interactions. some
 * configurations might not work with our current power sources.
 *
 * note that some device controller hardware will constrain what this
 * code can do, perhaps by disallowing more than one configuration or
 * by limiting configuration choices (like the pxa2xx).
 */
static int
gchar_set_config (struct gchar_dev *dev, unsigned number, int gfp_flags)
{
	int			result = 0;
	struct usb_gadget	*gadget = dev->gadget;

#if 0
	if (dev->configured)
		return 0;
#endif

#ifdef CONFIG_USB_CHAR_SA1100
	if (dev->configured) {
		/* tx fifo is full, but we can't clear it...*/
		INFO (dev, "can't change configurations\n");
		return -ESPIPE;
	}
#endif
	gchar_reset_config (dev);

	switch (number) {
	case CONFIG_CHAR:
		result = set_char_config (dev, gfp_flags);
		break;
	default:
		result = -EINVAL;
		/* FALL THROUGH */
	case 0:
		return result;
	}

	if (!result && (!dev->in_ep || !dev->out_ep))
		result = -ENODEV;
	if (result)
		gchar_reset_config (dev);
	else {
		char *speed;

		switch (gadget->speed) {
		case USB_SPEED_LOW:	speed = "low"; break;
		case USB_SPEED_FULL:	speed = "full"; break;
		case USB_SPEED_HIGH:	speed = "high"; break;
		default: 		speed = "?"; break;
		}

		dev->configured = 1;
		INFO (dev, "%s speed\n", speed);
	}
	return result;
}

/*-------------------------------------------------------------------------*/

static void gchar_setup_complete (struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		DEBUG ((struct gchar_dev *) ep->driver_data,
				"setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

/*
 * The setup() callback implements all the ep0 functionality that's
 * not handled lower down, in hardware or the hardware driver (like
 * device and endpoint feature flags, and their status).  It's all
 * housekeeping for the gadget function we're implementing.  Most of
 * the work is in config-specific setup.
 */
static int
gchar_setup (struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct gchar_dev		*dev = get_gadget_data (gadget);
	struct usb_request		*req = dev->req;
	int				value = -EOPNOTSUPP;

	/* usually this stores reply data in the pre-allocated ep0 buffer,
	 * but config change events will reconfigure hardware.
	 */
	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		switch (ctrl->wValue >> 8) {

		case USB_DT_DEVICE:
			value = min (ctrl->wLength, (u16) sizeof device_desc);
			memcpy (req->buf, &device_desc, value);
			break;
		case USB_DT_CONFIG:
			value = config_buf (gadget->speed, req->buf,
					ctrl->wValue >> 8,
					ctrl->wValue & 0xff);
			if (value >= 0)
				value = min (ctrl->wLength, (u16) value);
			break;

		case USB_DT_STRING:
			/* wIndex == language code.
			 * this driver only handles one language, you can
			 * add others even if they don't use iso8859/1
			 */
			value = usb_gadget_get_string (&stringtab,
					ctrl->wValue & 0xff, req->buf);
			if (value >= 0)
				value = min (ctrl->wLength, (u16) value);
			break;
		}
		break;

	/* currently two configs, two speeds */
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			break;
		spin_lock (&dev->lock);
		value = gchar_set_config (dev, ctrl->wValue, GFP_ATOMIC);
		spin_unlock (&dev->lock);
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		*(u8 *)req->buf = dev->configured ? CONFIG_CHAR : 0;
		value = min (ctrl->wLength, (u16) 1);
		break;
	case USB_REQ_SET_INTERFACE:
		if (ctrl->bRequestType != USB_RECIP_INTERFACE)
			break;
		spin_lock (&dev->lock);
		if (dev->configured && ctrl->wIndex == 0 && ctrl->wValue == 0) {
			u8 config = dev->configured ? CONFIG_CHAR : 0;

			gchar_reset_config (dev);
			gchar_set_config (dev, config, GFP_ATOMIC);
			value = 0;
		}
		spin_unlock (&dev->lock);
		break;
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE))
			break;
		if (!dev->configured)
			break;
		if (ctrl->wIndex != 0) {
			value = -EDOM;
			break;
		}
		*(u8 *)req->buf = 0;
		value = min (ctrl->wLength, (u16) 1);
		break;

	default:
		VDEBUG (dev,
			"unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			ctrl->wValue, ctrl->wIndex, ctrl->wLength);
	}

	/* respond with data transfer before status phase? */
	if (value >= 0) {
		req->length = value;
		value = usb_ep_queue (gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			DEBUG (dev, "ep_queue --> %d\n", value);
			req->status = 0;
			gchar_setup_complete (gadget->ep0, req);
		}
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static void
gchar_disconnect (struct usb_gadget *gadget)
{
	struct gchar_dev	*dev = get_gadget_data (gadget);
	unsigned long		flags;

	spin_lock_irqsave (&dev->lock, flags);
	gchar_reset_config (dev);

	/* a more significant application might have some non-usb
	 * activities to quiesce here, saving resources like power
	 * or pushing the notification up a network stack.
	 */
	spin_unlock_irqrestore (&dev->lock, flags);

	/* next we may get setup() calls to enumerate new connections;
	 * or an unbind() during shutdown (including removing module).
	 */
}

/*-------------------------------------------------------------------------*/

static void
gchar_unbind (struct usb_gadget *gadget)
{
	struct gchar_dev	*dev = get_gadget_data (gadget);

	DEBUG (dev, "unbind\n");

	/* we've already been disconnected ... no i/o is active */
	if (dev->req)
		free_ep_req (gadget->ep0, dev->req);
	kfree (dev);
	set_gadget_data (gadget, NULL);
	
	gchar_unbind_misc(dev);
	
}

static int
gchar_bind (struct usb_gadget *gadget)
{
	struct gchar_dev	*dev;
	struct usb_ep		*ep;
	int err;

	usb_ep_autoconfig_reset (gadget);
	
	ep = usb_ep_autoconfig (gadget, &fs_char_in_desc);
	if (!ep) {
autoconf_fail:
		printk(KERN_ERR "%s: can't autoconfigure on %s\n", shortname, gadget->name);
		return -ENODEV;
	}
	EP_IN_NAME = ep->name;
	ep->driver_data = ep;	/* claim */
	
	ep = usb_ep_autoconfig (gadget, &fs_char_out_desc);
	if (!ep)
		goto autoconf_fail;
	EP_OUT_NAME = ep->name;
	ep->driver_data = ep;	/* claim */

	dev = kmalloc (sizeof *dev, SLAB_KERNEL);
	if (!dev)
		return -ENOMEM;

	memset (dev, 0, sizeof *dev);
	spin_lock_init (&dev->lock);
	spin_lock_init (&dev->buflock);
	dev->gadget = gadget;
	init_waitqueue_head (&dev->wait);

	set_gadget_data (gadget, dev);

	/* preallocate control response and buffer */
	dev->req = usb_ep_alloc_request (gadget->ep0, GFP_KERNEL);
	if (!dev->req)
		goto enomem;
	dev->req->buf = usb_ep_alloc_buffer (gadget->ep0, USB_BUFSIZ,
				&dev->req->dma, GFP_KERNEL);
	if (!dev->req->buf)
		goto enomem;

	dev->req->complete = gchar_setup_complete;
	device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
	gadget->ep0->driver_data = dev;

	snprintf (manufacturer, sizeof manufacturer, "%s %s with %s",
		system_utsname.sysname, system_utsname.release,
		gadget->name);
	
	/* grab a misc device */
	if ((err = gchar_bind_misc(dev))) {
		gchar_unbind(gadget);
		return err;
	}
	
	return 0;

enomem:
	gchar_unbind (gadget);
	return -ENOMEM;
}

/*-------------------------------------------------------------------------*/

static struct usb_gadget_driver gchar_driver = {
	.speed		= USB_SPEED_FULL,
	.function	= (char *) longname,
	.bind		= gchar_bind,
	.unbind		= gchar_unbind,

	.setup		= gchar_setup,
	.disconnect	= gchar_disconnect,

	.driver 	= {
		.name		= (char *) shortname,
		// .shutdown = ...
		// .suspend = ...
		// .resume = ...
	},
};

MODULE_AUTHOR ("Joshua Wise/David Brownell");
MODULE_LICENSE ("Dual BSD/GPL");

static ssize_t read_gchar(struct file* file, char __user *buf, size_t count, loff_t *ppos)
{
	struct gchar_dev *dev = (struct gchar_dev *)file->private_data;
	ssize_t readcnt;
	DECLARE_WAITQUEUE(wait, current);
	
	add_wait_queue(&dev->wait, &wait);
	current->state = TASK_INTERRUPTIBLE;

	do {
		if (dev->databuflen != 0)
			break;

		if (file->f_flags & O_NONBLOCK)
		{
			readcnt = -EAGAIN;
			goto out;
		}

		schedule();
		if (signal_pending(current))
		{
			readcnt = -ERESTARTSYS;
			goto out;
		}
	} while (1);

	spin_lock_irq(&dev->buflock);
		readcnt = (count > dev->databuflen) ? dev->databuflen : count;
		copy_to_user(buf, dev->databuf, readcnt);
		memmove(dev->databuf, dev->databuf+readcnt, dev->databuflen-readcnt);
		dev->databuflen -= readcnt;
	spin_unlock_irq(&dev->buflock);

out:
	current->state = TASK_RUNNING;
	remove_wait_queue(&dev->wait, &wait);

	return readcnt;
}

static void gchar_send_complete (struct usb_ep *ep, struct usb_request *req)
{
	free_ep_req(ep, req);
}

/*-------------------------------------------------------------------------*/

#define USBC_MINOR		240
#define USBC_MAX_DEVICES	1

static ssize_t write_gchar(struct file* file, const char __user *buf, size_t count, loff_t* ppos)
{
	struct gchar_dev *dev = (struct gchar_dev *)file->private_data;
	struct usb_request *req;
	int result;
	
	if (!dev->configured)
		return count;
	
	req = alloc_ep_req (dev->in_ep, count);
	if (!req)
		return -ENOMEM;
		
	req->complete = gchar_send_complete;
	req->length = req->actual = count;
	req->zero = 0;
	
	copy_from_user(req->buf, buf, count);
	
	result = usb_ep_queue (dev->in_ep, req, GFP_ATOMIC);
	if (result)
		DEBUG (dev, "%s queue req --> %d\n",
				dev->in_ep->name, result);
		
	return count;
}

static unsigned int poll_gchar( struct file * filp, poll_table *wait )
{
	struct gchar_dev *dev = (struct gchar_dev *)filp->private_data;
	
	poll_wait(filp, &dev->wait, wait);
	
	if (dev->databuflen)
		return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
	return POLLOUT | POLLWRNORM;
}

/*-------------------------------------------------------------------------*/

struct gchar_dev *gc_devs[USBC_MAX_DEVICES];

static int open_gchar(struct inode *inode, struct file *file)
{
	struct gchar_dev *dev = gc_devs[iminor(inode)-USBC_MINOR];
	
	if (!dev->configured && !dev->databuflen)
		return -ENXIO;

	file->private_data = (void*)dev;
	nonseekable_open(inode, file);

	return 0;
}

static struct file_operations gchar_fops = {
	.llseek		= no_llseek,
	.read		= read_gchar,
	.write		= write_gchar,
	.poll		= poll_gchar,
	.open		= open_gchar
};

static struct miscdevice gchar_misc_device = {
	USBC_MINOR, "usbchar", &gchar_fops
};

static int gchar_bind_misc(struct gchar_dev *dev)
{
	int error;
	
	if ((error = misc_register(&gchar_misc_device)))
	{
		printk("gchar: Unable to register device 10, %d. Errno: %d\n", USBC_MINOR, error);
		return error;
	}

	gc_devs[gchar_misc_device.minor-USBC_MINOR] = dev;
	return 0;
}

static int gchar_unbind_misc(struct gchar_dev *dev)
{
	misc_deregister (&gchar_misc_device);
	return 0;
}

/*-------------------------------------------------------------------------*/

static int __init gchar_init (void)
{
	/* a real value would likely come through some id prom
	 * or module option.  this one takes at least two packets.
	 */
	strlcpy (serial, "hp iPAQ: Linux As Bootloader    ", sizeof serial);

	return usb_gadget_register_driver (&gchar_driver);
}
module_init (gchar_init);

static void __exit cleanup (void)
{
	usb_gadget_unregister_driver (&gchar_driver);
	
}
module_exit (cleanup);
