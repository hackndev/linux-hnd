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
#ifndef SA1100USB_H
#define SA1100USB_H

struct usbctl;

struct sausb_dev {
	struct device	*dev;
	struct usbctl	*ctl;
	spinlock_t	lock;

	u32		udccr;

	/*
	 * EP0 write thread.
	 */
	void		(*wrint)(struct sausb_dev *);
	struct usb_buf	*wrbuf;
	unsigned char	*wrptr;
	unsigned int	wrlen;

	/*
	 * EP0 statistics.
	 */
	unsigned long	ep0_wr_fifo_errs;
	unsigned long	ep0_wr_bytes;
	unsigned long	ep0_wr_packets;
	unsigned long	ep0_rd_fifo_errs;
	unsigned long	ep0_rd_bytes;
	unsigned long	ep0_rd_packets;
	unsigned long	ep0_stall_sent;
	unsigned long	ep0_early_irqs;

	/*
	 * EP1 .. n
	 */
	struct {
		dma_regs_t	*dmach;

		dma_addr_t	bufdma;
		unsigned int	buflen;
		void		*pktcpu;
		dma_addr_t	pktdma;
		unsigned int	pktlen;
		unsigned int	pktrem;

		void		*cb_data;
		void		(*cb_func)(void *data, int flag, int size);

		u32		udccs;
		unsigned int	maxpktsize;
		unsigned int	configured;
		unsigned int	host_halt;
		unsigned long	fifo_errs;
		unsigned long	bytes;
		unsigned long	packets;
	} ep[2];
};

/* receiver */
int  ep1_recv(void);
void udc_ep1_init(struct sausb_dev *);
void udc_ep1_halt(struct sausb_dev *, int);
void udc_ep1_reset(struct sausb_dev *);
void udc_ep1_config(struct sausb_dev *, unsigned int);
void udc_ep1_int_hndlr(struct sausb_dev *);

/* xmitter */
void udc_ep2_init(struct sausb_dev *);
void udc_ep2_halt(struct sausb_dev *, int);
void udc_ep2_reset(struct sausb_dev *);
void udc_ep2_config(struct sausb_dev *, unsigned int);
void udc_ep2_int_hndlr(struct sausb_dev *);

#define UDC_write(reg, val) do { \
	int i = 10000; \
	do { \
	  	(reg) = (val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: write %#x to %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while((reg) != (val)); \
} while (0)

#define UDC_set(reg, val) do { \
	int i = 10000; \
	do { \
		(reg) |= (val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: set %#x of %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while(!((reg) & (val))); \
} while (0)

#define UDC_clear(reg, val) do { \
	int i = 10000; \
	do { \
		(reg) &= ~(val); \
		if (i-- <= 0) { \
			printk( "%s [%d]: clear %#x of %p (%#x) failed\n", \
				__FUNCTION__, __LINE__, (val), &(reg), (reg)); \
			break; \
		} \
	} while((reg) & (val)); \
} while (0)

#define UDC_flip(reg, val) do { \
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
} while (0)

#define CHECK_ADDRESS { if ( Ser0UDCAR == 1 ) { printk("%s:%d I lost my address!!!\n",__FUNCTION__, __LINE__);}}

#endif
