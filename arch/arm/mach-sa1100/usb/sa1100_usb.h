/*
 * sa1100_usb.h
 *
 * Public interface to the sa1100 USB core. For use by client modules
 * like usb-eth and usb-char.
 *
 */
#ifndef _SA1100_USB_H
#define _SA1100_USB_H

typedef void (*usb_callback_t)(void *data, int flag, int size);

/* in usb_send.c */
int sa1100_usb_xmitter_avail( void );
int sa1100_usb_send(char *buf, int len);
void sa1100_usb_send_set_callback(usb_callback_t callback, void *data);
void sa1100_usb_send_reset(void);

/* in usb_recev.c */
int sa1100_usb_recv(char *buf, int len);
void sa1100_usb_recv_set_callback(usb_callback_t callback, void *data);
void sa1100_usb_recv_reset(void);

//////////////////////////////////////////////////////////////////////////////
// Descriptor Management
//////////////////////////////////////////////////////////////////////////////

// MaxPower:
#define USB_POWER(x)  ((x)>>1) /* convert mA to descriptor units of A for MaxPower */

/* "config descriptor buffer" - that is, one config,
   ..one interface and 2 endpoints */
struct cdb {
	 struct usb_config_descriptor	 	cfg;
	 struct usb_interface_descriptor	intf;
	 struct usb_endpoint_descriptor		ep1, ep2;
} __attribute__ ((packed));


/*=======================================================
 * Descriptor API
 */

/* Get the address of the statically allocated desc_t structure
   in the usb core driver. Clients can modify this between
   the time they call sa1100_usb_open() and sa1100_usb_start()
*/
struct cdb *sa1100_usb_get_descriptor_ptr(void);

#endif /* _SA1100_USB_H */
