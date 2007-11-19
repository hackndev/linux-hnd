#ifndef USBDEV_H
#define USBDEV_H

#include "strings.h"

struct usb_buf;
struct module;
struct cdb;
struct usb_client;

struct usbc_driver {
	struct module	*owner;
	const char	*name;
	void		*priv;
	int		(*start)(void *);
	int		(*stop)(void *);

	int		(*ep0_queue)(void *, struct usb_buf *buf, unsigned int req_len);
	void		(*set_address)(void *, unsigned int addr);
	void		(*set_config)(void *, struct cdb *config);

	/*
	 * Get specified endpoint status, as defined in 9.4.5.
	 */
	unsigned int	(*ep_get_status)(void *, unsigned int ep);
	void		(*ep_halt)(void *, unsigned int ep, int halt);

	/*
	 * Client
	 */
	int		(*ep_queue)(void *, unsigned int, char *, unsigned int);
	void		(*ep_reset)(void *, unsigned int);
	void		(*ep_callback)(void *, unsigned int, void (*)(void *, int, int), void *);
	int		(*ep_idle)(void *, unsigned int);
};

struct usbc_endpoint {
	struct usb_endpoint_descriptor	*desc;
};

struct usbc_interface {
	struct usb_interface_descriptor	*desc;
	unsigned int			nr_ep;
	struct usbc_endpoint		*ep[0];
};

struct usbc_config {
	struct usb_config_descriptor	*desc;
	unsigned int			nr_interface;
	struct usbc_interface		*interface[0];
};

struct usbctl {
	struct usb_client		*clnt;
	const struct usbc_driver	*driver;

	/* Internal state */
	unsigned int			address;	/* host assigned address */
	unsigned int			state;		/* our device state */
	unsigned int			sm_state;	/* state machine state */

	struct usbc_config		*config;	/* active configuration */
	struct usbc_strs		strings;

	/* Descriptors */
	struct usb_device_descriptor	*dev_desc;	/* device descriptor */
	struct usb_buf			*dev_desc_buf;	/* device descriptor buffer */


	int				nr_ep;
	struct usb_endpoint_descriptor	*ep_desc[2];
};

/*
 * Function Prototypes
 */

#define RET_ERROR	(-1)
#define RET_NOACTION	(0)
#define RET_QUEUED	(1)
#define RET_ACK		(2)
#define RET_REQERROR	(3)

int usbctl_parse_request(struct usbctl *ctl, struct usb_ctrlrequest *req);

int usbctl_reset(struct usbctl *ctl);
void usbctl_suspend(struct usbctl *ctl);
void usbctl_resume(struct usbctl *ctl);

#endif

