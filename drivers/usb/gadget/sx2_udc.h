/*
 * sx2_udc.h
 * 
 * Cypress EZUSB SX2 - CY7C68001 External master register definitions
 *
 * Author: Marek Vasut <marek.vasut@gmail.com>
 * 	    
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *    
 */
#ifndef _INCLUDE_SX2_UDC_H_ 

#define _INCLUDE_SX2_UDC_H_ 

/***************************************************
 * SX2 REGISTERS				   *
 ***************************************************/
 
/* General configuration */
#define SX2_REG_IFCONFIG	0x01
#define SX2_REG_FLAGSAB		0x02
#define SX2_REG_FLAGSCD		0x03
#define SX2_REG_POLAR		0x04
#define SX2_REG_REVID		0x05

/* Endpoint configuration */
#define SX2_REG_EP2CFG		0x06
#define SX2_REG_EP4CFG		0x07
#define SX2_REG_EP6CFG		0x08
#define SX2_REG_EP8CFG		0x09
#define SX2_REG_EP2PKTLENH	0x0A
#define SX2_REG_EP2PKTLENL	0x0B
#define SX2_REG_EP4PKTLENH	0x0C
#define SX2_REG_EP4PKTLENL	0x0D
#define SX2_REG_EP6PKTLENH	0x0E
#define SX2_REG_EP6PKTLENL	0x0F
#define SX2_REG_EP8PKTLENH	0x10
#define SX2_REG_EP8PKTLENL	0x11
#define SX2_REG_EP2PFH		0x12
#define SX2_REG_EP2PFL		0x13
#define SX2_REG_EP4PFH		0x14
#define SX2_REG_EP4PFL		0x15
#define SX2_REG_EP6PFH		0x16
#define SX2_REG_EP6PFL		0x17
#define SX2_REG_EP8PFH		0x18
#define SX2_REG_EP8PFL		0x19
#define SX2_REG_EP2ISOINPKTS	0x1A
#define SX2_REG_EP4ISOINPKTS	0x1B
#define SX2_REG_EP6ISOINPKTS	0x1C
#define SX2_REG_EP8ISOINPKTS	0x1D

/* Flags */
#define SX2_REG_EP24FLAGS	0x1E
#define SX2_REG_EP68FLAGS	0x1F

/* INPKTEND/FLUSH */
#define SX2_REG_INPKTEND	0x20
#define SX2_REG_FLUSH		0x20

/* USB Configuration */
#define SX2_REG_USBFRAMEH	0x2A
#define SX2_REG_USBFRAMEL	0x2B
#define SX2_REG_MICROFRAME	0x2C
#define SX2_REG_FNADDR		0x2D

/* Interrupts */ 
#define SX2_REG_INTENABLE	0x2E

/* Descriptor */
#define SX2_REG_DESC		0x30

/* Endpoint 0 */
#define SX2_REG_EP0BUF		0x31
#define SX2_REG_SETUP		0x32
#define SX2_REG_EP0BC		0x33

/* Un-indexed Register control */
#define SX2_REG_UIRC0		0x3A
#define SX2_REG_UIRC1		0x3B
#define SX2_REG_UIRC2		0x3C

/* Un-indexed Registers in XDATA space */
#define SX2_REG_FIFOPINPOLAR	0xE609
#define SX2_REG_TOGCTL		0xE683
#define SX2_REG_CT1		0xE6FB

/***************************************************
 * SX2 ADDRESS					   *
 ***************************************************/
#define SX2_ADDR_BASE           0xf0000000

/* We have 16bit bus of the chip connected to 32 bit bus of CPU */

#define SX2_ADDR_FIFO2		(SX2_ADDR_BASE + 0x00)
#define SX2_ADDR_FIFO4		(SX2_ADDR_BASE + 0x01)
#define SX2_ADDR_FIFO6		(SX2_ADDR_BASE + 0x04)
#define SX2_ADDR_FIFO8		(SX2_ADDR_BASE + 0x05)
#define SX2_ADDR_CMD		(SX2_ADDR_BASE + 0x08)
#define SX2_ADDR_RESERVED1	(SX2_ADDR_BASE + 0x09)
#define SX2_ADDR_RESERVED2	(SX2_ADDR_BASE + 0x0C)
#define SX2_ADDR_RESERVED3	(SX2_ADDR_BASE + 0x0D)

/***************************************************
 * SX2 COMMANDS					   *
 ***************************************************/

#define SX2_CMD_WRITE		(0x00)
#define SX2_CMD_READ		(0x40)
#define SX2_CMD_ADDR		(0x80)

/***************************************************
 * SX2 MISC					   *
 ***************************************************/

#define SX2_ADDR_MASK		(0x3f)

struct sx2_dev {
        struct device   		*dev;
        spinlock_t      		lock;
        /*
         * EP0 write thread.
         */
        void            (*wrint)(struct sx2_dev *);
        struct usb_buf  *wrbuf;
        unsigned char   *wrptr;
        unsigned int    wrlen;

        /*
         * EP0 statistics.
         */
        unsigned long   ep0_wr_fifo_errs;
        unsigned long   ep0_wr_bytes;
        unsigned long   ep0_wr_packets;
        unsigned long   ep0_rd_fifo_errs;
        unsigned long   ep0_rd_bytes;
        unsigned long   ep0_rd_packets;
        unsigned long   ep0_stall_sent;
        unsigned long   ep0_early_irqs;

        /*
         * EP1 .. n
         */
        struct {
                unsigned int    buflen;
                void            *pktcpu;
                unsigned int    pktlen;
                unsigned int    pktrem;

                void            *cb_data;
                void            (*cb_func)(void *data, int flag, int size);

                u32             udccs;
                unsigned int    maxpktsize;
                unsigned int    configured;
                unsigned int    host_halt;
                unsigned long   fifo_errs;
                unsigned long   bytes;
                unsigned long   packets;
        } ep[4];
};

#endif
