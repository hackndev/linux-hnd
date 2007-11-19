/*
 * usb/control.c
 *
 * This parses and handles all the control messages to/from endpoint 0.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/gfp.h>
#include <linux/init.h>

#include "buffer.h"
#include "client.h"
#include "usbdev.h"

#include "sa1100_usb.h"

#define USB_ENDPOINT_HALT		0
#define USB_DEVICE_REMOTE_WAKEUP	1

#undef DEBUG

#ifdef DEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG fmt , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

void initialize_descriptors(struct usbctl *ctl);

/*
 * print string descriptor
 */
#if 0
static char * __attribute__((unused))
psdesc(char *str, int len, struct usb_string_descriptor *desc)
{
	char *start = str;
	int nchars = (desc->bLength - 2) / sizeof(__u16) + 2;
	int i;

	if (nchars >= len)
		nchars = len - 1;

	nchars -= 2;

	*str++ = '"';
	for(i = 0; i < nchars; i++)
	 	*str++ = le16_to_cpu(desc->wData[i]);
	*str++ = '"';
	*str = '\0';

	return start;
}
#endif

enum {
	kError		= -1,
	kEvSuspend	= 0,
	kEvReset	= 1,
	kEvResume	= 2,
	kEvAddress	= 3,
	kEvConfig	= 4,
	kEvDeConfig	= 5
};

enum {
	kStateZombie		= 0,
	kStateZombieSuspend	= 1,
	kStateDefault		= 2,
	kStateDefaultSuspend	= 3,
	kStateAddr		= 4,
	kStateAddrSuspend	= 5,
	kStateConfig		= 6,
	kStateConfigSuspend	= 7
};

#define kE	kError
#define kSZ	kStateZombie
#define kSZS	kStateZombieSuspend
#define kSD	kStateDefault
#define kSDS	kStateDefaultSuspend
#define kSA	kStateAddr
#define kSAS	kStateAddrSuspend
#define kSC	kStateConfig
#define kSCS	kStateConfigSuspend

/*
 * Fig 9-1 P192
 *  Zombie == Attached | Powered
 */
static int device_state_machine[8][6] = {
// 	suspend	reset	resume	addr	config	deconfig
{	kSZS, 	kSD,	kE,	kE,	kE,	kE  }, /* zombie */
{	kE,	kSD,	kSZ,	kE,	kE,	kE  }, /* zom sus */ 
{	kSDS,	kError,	kSD,	kSA,	kE,	kE  }, /* default */
{	kE,	kSD,	kSD,	kE,	kE,	kE  }, /* def sus */
{	kSAS,	kSD,	kE,	kE,	kSC,	kE  }, /* addr */
{	kE,	kSD,	kSA,	kE,	kE,	kE  }, /* addr sus */
{	kSCS,	kSD,	kE,	kE,	kE,	kSA }, /* config */
{	kE,	kSD, 	kSC,	kE,	kE,	kE  }  /* cfg sus */
};

/*
 * "device state" is the usb device framework state, as opposed to the
 * "state machine state" which is whatever the driver needs and is much
 * more fine grained
 */
static int sm_state_to_device_state[8] = {
	USB_STATE_POWERED,	/* zombie */
	USB_STATE_SUSPENDED,	/* zombie suspended */
	USB_STATE_DEFAULT,	/* default */
	USB_STATE_SUSPENDED,	/* default suspended */
	USB_STATE_ADDRESS,	/* address */
	USB_STATE_SUSPENDED,	/* address suspended */
	USB_STATE_CONFIGURED,	/* config */
	USB_STATE_SUSPENDED	/* config suspended */
};

static char * state_names[8] = {
	"zombie",
	"zombie suspended",
	"default",
	"default suspended",
	"address",
	"address suspended",
	"configured",
	"config suspended"
};

static char * event_names[6] = {
	"suspend",
	"reset",
	"resume",
	"address assigned",
	"configure",
	"de-configure"
};

static char * device_state_names[] = {
	"not attached",
	"attached",
	"powered",
	"default",
	"address",
	"configured",
	"suspended"
};

static void usbctl_callbacks(struct usbctl *ctl, int state, int oldstate)
{
	struct usb_client *clnt = ctl->clnt;

	/*
	 * Inform any clients currently attached
	 * that the connectivity state changed.
	 */
	if (clnt && clnt->state_change)
		clnt->state_change(clnt->priv, state, oldstate);
}

/*
 * called by the interrupt handler here and the two endpoint
 * files when interesting .."events" happen
 */
static int usbctl_next_state_on_event(struct usbctl *ctl, int event)
{
	int next_state, next_dev_state, old_dev_state;

	printk(KERN_DEBUG "usbctl: %s --[%s]--> ", state_names[ctl->sm_state],
		event_names[event]);

	next_state = device_state_machine[ctl->sm_state][event];
	if (next_state != kError) {
		next_dev_state = sm_state_to_device_state[next_state];

		printk("%s. Device in %s state.\n",
			state_names[next_state],
			device_state_names[next_dev_state]);

		old_dev_state = ctl->state;
		ctl->sm_state = next_state;
		ctl->state = next_dev_state;

		if (old_dev_state != next_dev_state)
			usbctl_callbacks(ctl, next_dev_state, old_dev_state);
	} else
		printk("(error)\n");

	return next_state;
}

/*
 * Driver detected USB HUB reset.
 */
int usbctl_reset(struct usbctl *ctl)
{
	int ret;

	ret = usbctl_next_state_on_event(ctl, kEvReset) == kError;

	if (!ret) {
		ctl->address = 0;
	}
	return ret;
}

EXPORT_SYMBOL(usbctl_reset);

void usbctl_suspend(struct usbctl *ctl)
{
	usbctl_next_state_on_event(ctl, kEvSuspend);
}

EXPORT_SYMBOL(usbctl_suspend);

void usbctl_resume(struct usbctl *ctl)
{
	usbctl_next_state_on_event(ctl, kEvResume);
}

EXPORT_SYMBOL(usbctl_resume);

static struct usb_interface_descriptor *
usbctl_get_interface_descriptor(struct usbctl *ctl, unsigned int interface)
{
	/*FIXME*/
	struct cdb *cdb = sa1100_usb_get_descriptor_ptr();

	return (struct usb_interface_descriptor *)&cdb->intf;
}

static inline int
__usbctl_queue(struct usbctl *ctl, struct usb_ctrlrequest *req,
	       struct usb_buf *buf)
{
	unsigned int reqlen = le16_to_cpu(req->wLength);

	return ctl->driver->ep0_queue(ctl->driver->priv, buf, reqlen) ?
		RET_ERROR : RET_QUEUED;
}

static int
usbctl_queue(struct usbctl *ctl, struct usb_ctrlrequest *req,
	     void *data, unsigned int len)
{
	struct usb_buf *buf;

	buf = usbb_alloc(len, GFP_ATOMIC);
	if (!buf) {
		printk(KERN_ERR "usb: out of memory\n");
		return RET_ERROR;
	}

	if (data)
		memcpy(usbb_push(buf, len), data, len);

	return __usbctl_queue(ctl, req, buf);
}

/*
 * 9.4.5: Get Status (device)
 */
static int
usbctl_parse_dev_get_status(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	u16 status;

	status = /* self_powered_hook() ? 1 : 0 */1;

	status = cpu_to_le16(status);

	return usbctl_queue(ctl, req, &status, 2);
}

/*
 * Send USB device description to the host.
 */
static int
usbctl_desc_device(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	return __usbctl_queue(ctl, req, usbb_get(ctl->dev_desc_buf));
}

/*
 * Send USB configuration information to the host.
 */
static int
usbctl_desc_config(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	/*FIXME*/
	struct cdb *cdb = sa1100_usb_get_descriptor_ptr();

	return usbctl_queue(ctl, req, cdb, sizeof(struct cdb));
}

/*
 * Send a string to the host from the string table.
 */
static int
usbctl_desc_string(struct usbctl *ctl, struct usb_ctrlrequest *req,
		   unsigned int idx)
{
	struct usb_buf *buf;
	unsigned int lang = le16_to_cpu(req->wIndex);
	char string[32] __attribute__((unused));
	int ret;

	DPRINTK("usbctl: desc_string (index %u, lang 0x%04x): ", idx, lang);

	buf = usbc_string_find(&ctl->strings, lang, idx);
	if (buf) {
		DPRINTK("%s\n", idx == 0 ? "language" :
			 psdesc(string, sizeof(string), usbc_string_desc(buf)));

		ret = __usbctl_queue(ctl, req, buf);
	} else {
		DPRINTK("not found -> stall\n");
		ret = RET_REQERROR;
	}
	return ret;
}

/*
 * Send an interface description (and endpoints) to the host.
 */
static int
usbctl_desc_interface(struct usbctl *ctl, struct usb_ctrlrequest *req,
		      unsigned int idx)
{
	struct usb_interface_descriptor *desc;
	int ret;

	DPRINTK("usbctl: desc_interface (index %d)\n", idx);

	desc = usbctl_get_interface_descriptor(ctl, idx);

	if (desc) {
		ret = usbctl_queue(ctl, req, desc, desc->bLength);
	} else {
		printk("usbctl: unknown interface %d\n", idx);
		ret = RET_REQERROR;
	}

	return ret;
}

/*
 * Send an endpoint (1 .. n) to the host.
 */
static int
usbctl_desc_endpoint(struct usbctl *ctl, struct usb_ctrlrequest *req,
		     unsigned int idx)
{
	int ret;

	DPRINTK("usbctl: desc_endpoint (index %d)\n", idx);

	if (idx >= 1 && idx <= ctl->nr_ep) {
		struct usb_endpoint_descriptor *ep = ctl->ep_desc[idx - 1];

		ret = usbctl_queue(ctl, req, ep, ep->bLength);
	} else {
		printk("usbctl: unknown endpoint %d\n", idx);
		ret = RET_REQERROR;
	}

	return ret;
}

/*
 * 9.4.3: Parse a request for a descriptor.
 *  Unspecified conditions:
 *   None
 *  Valid states: default, address, configured.
 */
static int
usbctl_parse_dev_descriptor(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int idx = le16_to_cpu(req->wValue) & 255;
	unsigned int type = le16_to_cpu(req->wValue) >> 8;
	int ret;

	switch (type) {
	case USB_DT_DEVICE:		/* check if idx matters */
		ret = usbctl_desc_device(ctl, req);
		break;

	case USB_DT_CONFIG:		/* check if idx matters */
		ret = usbctl_desc_config(ctl, req);
		break;

	case USB_DT_STRING:
		ret = usbctl_desc_string(ctl, req, idx);
		break;

	case USB_DT_INTERFACE:
		ret = usbctl_desc_interface(ctl, req, idx);
		break;

	case USB_DT_ENDPOINT:
		ret = usbctl_desc_endpoint(ctl, req, idx);
		break;

	case USB_DT_DEVICE_QUALIFIER:
	case USB_DT_OTHER_SPEED_CONFIG:
	case USB_DT_INTERFACE_POWER:
	default:
		printk(KERN_ERR "usbctl: unknown descriptor: "
			"wValue = 0x%04x wIndex = 0x%04x\n",
			le16_to_cpu(req->wValue), le16_to_cpu(req->wIndex));
		ret = RET_REQERROR;
		break;
	}

	return ret;
}

/*
 * 9.4.6: Set Address
 * The USB1.1 spec says the response to SetAddress() with value 0
 * is undefined.  It then goes on to define the response.  We
 * acknowledge addresses of zero, but take no further action.
 */
static int
usbctl_parse_dev_set_address(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int address = le16_to_cpu(req->wValue) & 0x7f;

	if (ctl->state == USB_STATE_CONFIGURED)
		return RET_REQERROR;

	if (address != 0) {
		ctl->address = address;

		usbctl_next_state_on_event(ctl, kEvAddress);

		ctl->driver->set_address(ctl->driver->priv, address);
	}

	return RET_ACK;
}

/*
 * 9.4.2: Get Configuration.
 *  Unspecified conditions:
 *   - non-zero wIndex, wValue or wLength (ignored)
 *   - default state (request error)
 *  Valid states: address, configured.
 */
static int
usbctl_parse_dev_get_config(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	u8 status = 0;

	if (ctl->state == USB_STATE_CONFIGURED)
		status = 1;

	return usbctl_queue(ctl, req, &status, 1);
}

/*
 * 9.4.7: Set Configuration.
 *  Unspecified conditions:
 *   - default state (request error)
 */
static int
usbctl_parse_dev_set_config(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int cfg = le16_to_cpu(req->wValue);
	int ret = RET_REQERROR;

	if (ctl->state == USB_STATE_DEFAULT)
		return ret;

	if (cfg == 0) {
		/* enter address state, or remain in address state */
		usbctl_next_state_on_event(ctl, kEvDeConfig);

		ctl->driver->set_config(ctl->driver->priv, NULL);

		ret = RET_ACK;
	} else if (cfg == 1) {
		/* enter configured state, and set configuration */
		/*FIXME*/
		struct cdb *cdb = sa1100_usb_get_descriptor_ptr();

		usbctl_next_state_on_event(ctl, kEvConfig);

		ctl->driver->set_config(ctl->driver->priv, cdb);
		ret = RET_ACK;
	}

	return ret;
}

/*
 * Interface handling
 */

/*
 * 9.4.5: Get Status (interface)
 */
static int
usbctl_parse_int_get_status(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int interface = le16_to_cpu(req->wIndex) & 255;
	u16 status;

	switch (ctl->state) {
	case USB_STATE_DEFAULT:
		return RET_REQERROR;

	case USB_STATE_ADDRESS:
		if (interface != 0)
			return RET_REQERROR;
		break;

	case USB_STATE_CONFIGURED:
		if (interface != 1)
			return RET_REQERROR;
		break;
	}

	status = cpu_to_le16(0);

	return usbctl_queue(ctl, req, &status, 2);
}

/*
 * 9.4.4: Get Interface
 *  Unspecified conditions:
 *   - 
 *  States: Default (unspecified), Address (Request Error), Configured (ok)
 */
static int
usbctl_parse_int_get_interface(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int interface = le16_to_cpu(req->wIndex) & 255;
	u8 null = 0;

	if (ctl->state != USB_STATE_CONFIGURED)
		return RET_REQERROR;

	/*
	 * If the interface doesn't exist, respond with request error
	 */
	if (interface != 1)
		return RET_REQERROR;

	printk("usbctl: get interface %d not supported\n", interface);

	return usbctl_queue(ctl, req, &null, 1);
}

static int
usbctl_parse_int_set_interface(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int interface = le16_to_cpu(req->wIndex) & 255;

	if (interface != 0)
		printk("usbctl: set interface %d not supported (ignored)\n",
			interface);

	return RET_ACK;
}

/*
 * Endpoint handling
 */

/*
 * 9.4.5: Get Status (endpoint)
 */
static int
usbctl_parse_ep_get_status(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int ep = le16_to_cpu(req->wIndex) & 15;
	u16 status;

	if ((ep != 0 && ctl->state != USB_STATE_CONFIGURED) ||
	    ep <= ctl->nr_ep)
		return RET_REQERROR;

	status = ctl->driver->ep_get_status(ctl->driver->priv, ep);
	status = cpu_to_le16(status);

	return usbctl_queue(ctl, req, &status, 2);
}

/*
 * 9.4.1: Clear an endpoint feature.  We only support ENDPOINT_HALT.
 *  Unspecified conditions:
 *   - non-zero wLength is not specified (ignored)
 *  Valid states: Address, Configured.
 */
static int
usbctl_parse_ep_clear_feature(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int feature = le16_to_cpu(req->wValue);
	unsigned int ep = le16_to_cpu(req->wIndex) & 15;
	int ret;

	if ((ep != 0 && ctl->state != USB_STATE_CONFIGURED) ||
	    ep <= ctl->nr_ep)
		return RET_REQERROR;

	if (feature == USB_ENDPOINT_HALT) {
		ctl->driver->ep_halt(ctl->driver->priv, ep, 0);
		ret = RET_ACK;
	} else {
		printk(KERN_ERR "usbctl: unsupported clear feature: "
			"wValue = 0x%04x wIndex = 0x%04x\n",
			feature, ep);

		ret = RET_REQERROR;
	}
	return ret;
}

/*
 * 9.4.9: Set Feature (endpoint)
 */
static int
usbctl_parse_ep_set_feature(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int feature = le16_to_cpu(req->wValue);
	unsigned int ep = le16_to_cpu(req->wIndex) & 15;
	int ret;

	if ((ep != 0 && ctl->state != USB_STATE_CONFIGURED) ||
	    ep <= ctl->nr_ep)
		return RET_REQERROR;

	if (feature == USB_ENDPOINT_HALT) {
		ctl->driver->ep_halt(ctl->driver->priv, ep, 1);
		ret = RET_ACK;
	} else {
		printk(KERN_ERR "usbctl: unsupported set feature "
			"wValue = 0x%04x wIndex = 0x%04x\n",
			feature, ep);

		ret = RET_REQERROR;
	}
	return ret;
}

/*
 * This reflects Table 9.3 (p186) in the USB1.1 spec.
 *
 * Some notes:
 *  - USB1.1 specifies remote wakeup feature, so we don't implement
 *    USB_RECIP_DEVICE USB_REQ_{SET,CLEAR}_FEATURE
 *  - USB1.1 doesn't actually specify any interface features, so we
 *    don't implement USB_RECIP_INTERFACE USB_REQ_{SET,CLEAR}_FEATURE
 */
static int (*request_fns[4][16])(struct usbctl *, struct usb_ctrlrequest *) = {
	[USB_RECIP_DEVICE] = {
		[USB_REQ_GET_STATUS]	    = usbctl_parse_dev_get_status,
		[USB_REQ_CLEAR_FEATURE]	    = NULL,
		[USB_REQ_SET_FEATURE]	    = NULL,
		[USB_REQ_SET_ADDRESS]	    = usbctl_parse_dev_set_address,
		[USB_REQ_GET_DESCRIPTOR]    = usbctl_parse_dev_descriptor,
		[USB_REQ_SET_DESCRIPTOR]    = NULL,
		[USB_REQ_GET_CONFIGURATION] = usbctl_parse_dev_get_config,
		[USB_REQ_SET_CONFIGURATION] = usbctl_parse_dev_set_config,
	},

	[USB_RECIP_INTERFACE] = {
		[USB_REQ_GET_STATUS]	    = usbctl_parse_int_get_status,
		[USB_REQ_CLEAR_FEATURE]	    = NULL,
		[USB_REQ_SET_FEATURE]	    = NULL,
		[USB_REQ_GET_INTERFACE]	    = usbctl_parse_int_get_interface,
		[USB_REQ_SET_INTERFACE]	    = usbctl_parse_int_set_interface,
	},

	[USB_RECIP_ENDPOINT] = {
		[USB_REQ_GET_STATUS]	    = usbctl_parse_ep_get_status,
		[USB_REQ_CLEAR_FEATURE]	    = usbctl_parse_ep_clear_feature,
		[USB_REQ_SET_FEATURE]	    = usbctl_parse_ep_set_feature,
	},
};

static void __attribute__((unused))
usbctl_dump_request(const char *prefix, const struct usb_ctrlrequest *req)
{
	printk("%sbRequestType=0x%02x bRequest=0x%02x "
		"wValue=0x%04x wIndex=0x%04x wLength=0x%04x\n",
		prefix, req->bRequestType, req->bRequest,
		le16_to_cpu(req->wValue), le16_to_cpu(req->wIndex),
		le16_to_cpu(req->wLength));
}

int usbctl_parse_request(struct usbctl *ctl, struct usb_ctrlrequest *req)
{
	unsigned int type;
	int (*fn)(struct usbctl *, struct usb_ctrlrequest *) = NULL;
	int ret = RET_REQERROR;

	//usbctl_dump_request("usbctl: ", req);

	type = req->bRequestType & USB_TYPE_MASK;
	if (type == USB_TYPE_STANDARD) {
		unsigned int recip;

		recip = req->bRequestType & USB_RECIP_MASK;
		if (recip < ARRAY_SIZE(request_fns) &&
		    req->bRequest < ARRAY_SIZE(request_fns[0]))
			fn = request_fns[recip][req->bRequest];
	}

	if (fn)
		ret = fn(ctl, req);
	else
		usbctl_dump_request(KERN_ERR "usbctl: unknown request: ",
				    req);

	/*
	 * Make sure we're doing the right thing.
	 */
	if (req->bRequestType & USB_DIR_IN) {
		if (ret != RET_QUEUED && ret != RET_REQERROR)
			printk("Error: device to host transfer expected\n");
	} else {
		if (ret == RET_QUEUED)
			printk("Error: no device to host transfer expected\n");
	}

	return ret;
}

EXPORT_SYMBOL(usbctl_parse_request);

/* Start running. Must have called usb_open (above) first */
int usbctl_start(struct usb_client *client)
{
	struct usbctl *ctl = client->ctl;

	if (ctl == NULL || ctl->clnt != client) {
		printk("usbctl: start: no client registered\n");
		return -EPERM;
	}

	ctl->sm_state = kStateZombie;
	ctl->state = USB_STATE_POWERED;

	/*
	 * Notify the client as to our state.
	 */
	usbctl_callbacks(ctl, USB_STATE_POWERED, USB_STATE_SUSPENDED);

	return ctl->driver->start(ctl->driver->priv);
}

EXPORT_SYMBOL(usbctl_start);

/*
 * Stop USB core from running
 */
void usbctl_stop(struct usb_client *client)
{
	struct usbctl *ctl = client->ctl;

	if (ctl == NULL || ctl->clnt != client) {
		printk("USBDEV: stop: no client/driver registered\n");
		return;
	}

	ctl->driver->stop(ctl->driver->priv);
}

EXPORT_SYMBOL(usbctl_stop);

struct usbctl usbctl;

EXPORT_SYMBOL(usbctl);

/* Open SA usb core on behalf of a client, but don't start running */

int usbctl_open(struct usb_client *client)
{
	struct usbctl *ctl = &usbctl;
	int ret;
printk("usbctl_open: ctl %p driver %p\n", ctl, ctl->driver);
	if (!ctl->driver || !try_module_get(ctl->driver->owner))
		return -ENODEV;

	if (ctl->clnt != NULL) {
		ret = -EBUSY;
		goto err;
	}

	ctl->clnt     = client;
	ctl->state    = USB_STATE_SUSPENDED;
	ctl->nr_ep    = 2;
	/* start in zombie suspended state */
	ctl->sm_state = kStateZombieSuspend;
	ctl->state    = USB_STATE_SUSPENDED;
	client->ctl   = ctl;

	ctl->dev_desc_buf = usbb_alloc(sizeof(struct usb_device_descriptor),
				       GFP_KERNEL);
	if (!ctl->dev_desc_buf) {
		ret = -ENOMEM;
		goto err;
	}

	ctl->dev_desc = usbb_push(ctl->dev_desc_buf,
				  sizeof(struct usb_device_descriptor));

	/* create descriptors for enumeration */
	initialize_descriptors(ctl);

	return 0;

 err:
	module_put(ctl->driver->owner);
	return ret;
}

EXPORT_SYMBOL(usbctl_open);

/* Tell SA core client is through using it */
void usbctl_close(struct usb_client *client)
{
	struct usbctl *ctl = client->ctl;

	if (ctl == NULL || ctl->clnt != client) {
		printk("usbctl: close: no client registered\n");
		return;
	}

	usbb_put(ctl->dev_desc_buf);

	client->ctl       = NULL;
	ctl->clnt         = NULL;
	ctl->dev_desc     = NULL;
	ctl->dev_desc_buf = NULL;
	/* reset to zombie suspended state */
	ctl->sm_state     = kStateZombieSuspend;
	ctl->state        = USB_STATE_SUSPENDED;

	usbc_string_free_all(&ctl->strings);

	if (ctl->driver->owner)
		module_put(ctl->driver->owner);
}

EXPORT_SYMBOL(usbctl_close);

int usbctl_proc_info(struct usbctl *ctl, char *buf)
{
	char *p = buf;

	p += sprintf(p, "USB Gadget Core:\n");
	p += sprintf(p, "Driver\t: %s\n",
		ctl->driver ? ctl->driver->name : "none");
	p += sprintf(p, "Client\t: %s\n",
		ctl->clnt ? ctl->clnt->name : "none");
	p += sprintf(p, "State\t: %s (%s) %d\n",
		device_state_names[sm_state_to_device_state[ctl->sm_state]],
		state_names[ctl->sm_state],
		ctl->sm_state);
	p += sprintf(p, "Address\t: %d\n", ctl->address);

	return p - buf;
}

EXPORT_SYMBOL(usbctl_proc_info);

int
usbctl_ep_queue_buffer(struct usbctl *ctl, unsigned int ep,
		       char *buf, unsigned int len)
{
	return ctl->driver->ep_queue(ctl->driver->priv, ep, buf, len);
}

EXPORT_SYMBOL(usbctl_ep_queue_buffer);

void usbctl_ep_reset(struct usbctl *ctl, unsigned int ep)
{
	return ctl->driver->ep_reset(ctl->driver->priv, ep);
}

EXPORT_SYMBOL(usbctl_ep_reset);

void
usbctl_ep_set_callback(struct usbctl *ctl, unsigned int ep,
		       usb_callback_t callback, void *data)
{
	ctl->driver->ep_callback(ctl->driver->priv, ep, callback, data);
}

EXPORT_SYMBOL(usbctl_ep_set_callback);

int usbctl_ep_idle(struct usbctl *ctl, unsigned int ep)
{
	return ctl->driver->ep_idle(ctl->driver->priv, ep);
}

EXPORT_SYMBOL(usbctl_ep_idle);

/*
 * usbctl_init()
 * Module load time. Allocate dma and interrupt resources. Setup /proc fs
 * entry. Leave UDC disabled.
 */
int usbctl_init(struct usbctl *ctl, struct usbc_driver *drv)
{
	usbc_string_init(&ctl->strings);
printk("usbctl_init: %p %p\n", ctl, drv);
	/*
	 * start in zombie suspended state
	 */
	ctl->sm_state	= kStateZombieSuspend;
	ctl->state	= USB_STATE_SUSPENDED;
	ctl->driver	= drv;

	return 0;
}

/*
 * usbctl_exit()
 */
void usbctl_exit(struct usbctl *ctl)
{
	usbc_string_free_all(&ctl->strings);

	ctl->driver	= NULL;
}

EXPORT_SYMBOL(usbctl_init);
EXPORT_SYMBOL(usbctl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USB gadget core");
