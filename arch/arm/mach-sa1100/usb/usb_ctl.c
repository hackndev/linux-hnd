 /*
 *	Copyright (C) Compaq Computer Corporation, 1998, 1999
 *  Copyright (C) Extenex Corporation, 2001
 *
 *  usb_ctl.c
 *
 *  SA1100 USB controller core driver.
 *
 *  This file provides interrupt routing and overall coordination
 *  of the three endpoints in usb_ep0, usb_receive (1),  and usb_send (2).
 *
 *  Please see linux/Documentation/arm/SA1100/SA1100_USB for details.
 *
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/usb.h>

#include "buffer.h"
#include "client.h"
#include "usbdev.h"
#include "sa1100_usb.h"

//////////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////////

/* device descriptors */
static struct cdb cdb;

//////////////////////////////////////////////////////////////////////////////
// Private Helpers
//////////////////////////////////////////////////////////////////////////////

int sa1100_usb_add_string(struct usbctl *ctl, const char *str)
{
	int nr = 0;

	if (str) {
		struct usb_buf *buf;
		int len;

		len = strlen(str);

		nr = -ENOMEM;
		buf = usbc_string_alloc(len);
		if (buf) {
			usbc_string_from_cstr(buf, str);
			nr = usbc_string_add(&ctl->strings, buf);

			if (nr < 0)
				usbc_string_free(buf);
		}
	}

	return nr;
}

EXPORT_SYMBOL(sa1100_usb_add_string);

static int sa1100_usb_add_language(struct usbctl *ctl, unsigned int lang)
{
	struct usb_buf *buf;
	int nr = -ENOMEM;

	buf = usbc_string_alloc(1);
	if (buf) {
		usbc_string_desc(buf)->wData[0] = cpu_to_le16(lang); /* American English */
		nr = usbc_string_add(&ctl->strings, buf);

		if (nr < 0)
			usbc_string_free(buf);
	}

	return nr;
}

/* setup default descriptors */

void initialize_descriptors(struct usbctl *ctl)
{
	struct usb_client *clnt = ctl->clnt;
	int r;

	ctl->ep_desc[0] = (struct usb_endpoint_descriptor *)&cdb.ep1;
	ctl->ep_desc[1] = (struct usb_endpoint_descriptor *)&cdb.ep2;

	cdb.cfg.bLength             = USB_DT_CONFIG_SIZE;
	cdb.cfg.bDescriptorType     = USB_DT_CONFIG;
	cdb.cfg.wTotalLength        = cpu_to_le16(sizeof(struct cdb));
	cdb.cfg.bNumInterfaces      = 1;
	cdb.cfg.bConfigurationValue = 1;
	cdb.cfg.iConfiguration      = 0;
	cdb.cfg.bmAttributes        = USB_CONFIG_ATT_ONE;
	cdb.cfg.bMaxPower           = USB_POWER( 500 );

	cdb.intf.bLength            = USB_DT_INTERFACE_SIZE;
	cdb.intf.bDescriptorType    = USB_DT_INTERFACE;
	cdb.intf.bInterfaceNumber   = 0;     /* unique intf index*/
	cdb.intf.bAlternateSetting  = 0;
	cdb.intf.bNumEndpoints      = 2;
	cdb.intf.bInterfaceClass    = 0xff;  /* vendor specific */
	cdb.intf.bInterfaceSubClass = 0;
	cdb.intf.bInterfaceProtocol = 0;
	cdb.intf.iInterface         = 0;

	cdb.ep1.bLength             = USB_DT_INTERFACE_SIZE;
	cdb.ep1.bDescriptorType     = USB_DT_ENDPOINT;
	cdb.ep1.bEndpointAddress    = USB_DIR_OUT | 1;
	cdb.ep1.bmAttributes        = USB_ENDPOINT_XFER_BULK;
	cdb.ep1.wMaxPacketSize      = cpu_to_le16(64);
	cdb.ep1.bInterval           = 0;

	cdb.ep2.bLength             = USB_DT_INTERFACE_SIZE;
	cdb.ep2.bDescriptorType     = USB_DT_ENDPOINT;
	cdb.ep2.bEndpointAddress    = USB_DIR_IN | 2;
	cdb.ep2.bmAttributes        = USB_ENDPOINT_XFER_BULK;
	cdb.ep2.wMaxPacketSize      = cpu_to_le16(64);
	cdb.ep2.bInterval           = 0;

	ctl->dev_desc->bLength            = USB_DT_DEVICE_SIZE;
	ctl->dev_desc->bDescriptorType    = USB_DT_DEVICE;
	ctl->dev_desc->bcdUSB             = cpu_to_le16(0x100); /* 1.0 */
	ctl->dev_desc->bDeviceClass       = clnt->class;
	ctl->dev_desc->bDeviceSubClass    = clnt->subclass;
	ctl->dev_desc->bDeviceProtocol    = clnt->protocol;
	ctl->dev_desc->bMaxPacketSize0    = 8;	/* ep0 max fifo size */
	ctl->dev_desc->idVendor           = cpu_to_le16(clnt->vendor);
	ctl->dev_desc->idProduct          = cpu_to_le16(clnt->product);
	ctl->dev_desc->bcdDevice          = cpu_to_le16(clnt->version);
	ctl->dev_desc->bNumConfigurations = 1;

	/* set language */
	/* See: http://www.usb.org/developers/data/USB_LANGIDs.pdf */
	r = sa1100_usb_add_language(ctl, 0x409);
	if (r < 0)
		printk(KERN_ERR "usbc: couldn't add language\n");

	r = sa1100_usb_add_string(ctl, clnt->manufacturer_str);
	if (r < 0)
		printk(KERN_ERR "usbc: couldn't add manufacturer string\n");

	ctl->dev_desc->iManufacturer = r > 0 ? r : 0;

	r = sa1100_usb_add_string(ctl, clnt->product_str);
	if (r < 0)
		printk(KERN_ERR "usbc: couldn't add product string\n");

	ctl->dev_desc->iProduct      = r > 0 ? r : 0;

	r = sa1100_usb_add_string(ctl, clnt->serial_str);
	if (r < 0)
		printk(KERN_ERR "usbc: couldn't add serial string\n");

	ctl->dev_desc->iSerialNumber = r > 0 ? r : 0;
}


/*====================================================
 * Descriptor Manipulation.
 * Use these between open() and start() above to setup
 * the descriptors for your device.
 */

/* get pointer to static default descriptor */
struct cdb *sa1100_usb_get_descriptor_ptr(void)
{
	return &cdb;
}

EXPORT_SYMBOL(sa1100_usb_get_descriptor_ptr);
