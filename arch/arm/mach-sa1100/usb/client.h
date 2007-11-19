#ifndef USBDEV_CLIENT_H
#define USBDEV_CLIENT_H

#include "sa1100_usb.h" /* grr */

struct usbctl;

struct usb_client {
	struct usbctl	*ctl;
	const char	*name;		/* Client name		*/
	void		*priv;		/* Client-private data	*/
	void		(*state_change)(void *priv, int state, int oldstate);
	__u16		vendor;		/* USB vendor ID	*/
	__u16		product;	/* USB product ID	*/
	__u16		version;	/* USB version ID	*/
	__u8		class;		/* USB class		*/
	__u8		subclass;	/* USB subclass		*/
	__u8		protocol;	/* USB protocol		*/
	__u8		unused1;
	__u16		unused2;
	const char	*manufacturer_str;
	const char	*product_str;
	const char	*serial_str;
};

int usbctl_start(struct usb_client *client);
void usbctl_stop(struct usb_client *client);
int usbctl_open(struct usb_client *client);
void usbctl_close(struct usb_client *client);

int
usbctl_ep_queue_buffer(struct usbctl *ctl, unsigned int ep,
		       char *buf, unsigned int len);
void usbctl_ep_reset(struct usbctl *ctl, unsigned int ep);
void
usbctl_ep_set_callback(struct usbctl *ctl, unsigned int ep,
		       usb_callback_t callback, void *data);
int usbctl_ep_idle(struct usbctl *ctl, unsigned int ep);

#endif
