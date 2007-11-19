/*
 *	Copyright (C)  Compaq Computer Corporation, 1998, 1999
 *	Copyright (C)  Extenex Corporation 2001
 *
 *  usb_ctl.h
 *
 *  PRIVATE interface used to share info among components of the SA-1100 USB
 *  core: usb_ctl, usb_ep0, usb_recv and usb_send. Clients of the USB core
 *  should use sa1100_usb.h.
 *
 */

#ifndef _USB_CTL_H
#define _USB_CTL_H

#include <asm/dma.h>  /* dmach_t */

struct usb_client;

struct usb_stats_t {
	 unsigned long ep0_fifo_write_failures;
	 unsigned long ep0_bytes_written;
	 unsigned long ep0_fifo_read_failures;
	 unsigned long ep0_bytes_read;
};

struct usb_info_t
{
	struct usb_client *client;
	dma_regs_t *dmach_tx, *dmach_rx;
	int state;
	unsigned char address;
	struct usb_stats_t stats;
};

/* in usb_ctl.c */
extern struct usb_info_t usbd_info;

/*
 * Function Prototypes
 */
enum { kError=-1, kEvSuspend=0, kEvReset=1,
	   kEvResume=2, kEvAddress=3, kEvConfig=4, kEvDeConfig=5 };
int usbctl_next_state_on_event( int event );

/* endpoint zero */
void ep0_reset(void);
void ep0_int_hndlr(void);

/* receiver */
int  ep1_recv(void);
int  ep1_init(dma_regs_t *dma);
void ep1_int_hndlr(int status);
void ep1_reset(void);
void ep1_stall(void);

/* xmitter */
void ep2_reset(void);
int  ep2_init(dma_regs_t *dma);
void ep2_int_hndlr(int status);
void ep2_stall(void);

#define UDC_write(reg, val) { \
	int i = 10000; \
	do { \
	  	(reg) = (val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: write %#x to %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while((reg) != (val)); \
}

#define UDC_set(reg, val) { \
	int i = 10000; \
	do { \
		(reg) |= (val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: set %#x of %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while(!((reg) & (val))); \
}

#define UDC_clear(reg, val) { \
	int i = 10000; \
	do { \
		(reg) &= ~(val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: clear %#x of %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while((reg) & (val)); \
}

#define UDC_flip(reg, val) { \
	int i = 10000; \
	(reg) = (val); \
	do { \
		(reg) = (val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: flip %#x of %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while(((reg) & (val))); \
}


#define CHECK_ADDRESS { if ( Ser0UDCAR == 1 ) { printk("%s:%d I lost my address!!!\n",__FUNCTION__, __LINE__);}}
#endif /* _USB_CTL_H */
