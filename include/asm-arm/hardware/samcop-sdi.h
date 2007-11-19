/* linux/include/asm/hardware/samcop-sdi.h
 *
 * Copyright (c) 2004 Simtec Electronics <linux@simtec.co.uk>
 *		      http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SAMCOP MMC/SDIO register definitions
 * Adapted from S3C2410 driver by Matt Reimer.
*/

#ifndef __ASM_HARDWARE_SAMCOP_SDI
#define __ASM_HARDWARE_SAMCOP_SDI "samcop-sdi.h"

#define SAMCOP_SDICON                (0x00)
#define SAMCOP_SDIPRE                (0x04)
#define SAMCOP_SDICMDARG             (0x08)
#define SAMCOP_SDICMDCON             (0x0C)
#define SAMCOP_SDICMDSTAT            (0x10)
#define SAMCOP_SDIRSP0               (0x14)
#define SAMCOP_SDIRSP1               (0x18)
#define SAMCOP_SDIRSP2               (0x1C)
#define SAMCOP_SDIRSP3               (0x20)
#define SAMCOP_SDITIMER              (0x24)
#define SAMCOP_SDIBSIZE              (0x28)
#define SAMCOP_SDIDCON               (0x2C)
#define SAMCOP_SDIDCNT               (0x30)
#define SAMCOP_SDIDSTA               (0x34)
#define SAMCOP_SDIFSTA               (0x38)
#define SAMCOP_SDIDATA               (0x3C)
#define SAMCOP_SDIIMSK               (0x40)

#define SAMCOP_SDICON_BYTEORDER      (1<<4)
#define SAMCOP_SDICON_SDIOIRQ        (1<<3)
#define SAMCOP_SDICON_RWAITEN        (1<<2)
#define SAMCOP_SDICON_FIFORESET      (1<<1)
#define SAMCOP_SDICON_CLOCKTYPE      (1<<0)

#define SAMCOP_SDICMDCON_ABORT       (1<<12)
#define SAMCOP_SDICMDCON_WITHDATA    (1<<11)
#define SAMCOP_SDICMDCON_LONGRSP     (1<<10)
#define SAMCOP_SDICMDCON_WAITRSP     (1<<9)
#define SAMCOP_SDICMDCON_CMDSTART    (1<<8)
#define SAMCOP_SDICMDCON_SENDERHOST  (1<<6)
#define SAMCOP_SDICMDCON_INDEX       (0x3f)

#define SAMCOP_SDICMDSTAT_CRCFAIL    (1<<12)
#define SAMCOP_SDICMDSTAT_CMDSENT    (1<<11)
#define SAMCOP_SDICMDSTAT_CMDTIMEOUT (1<<10)
#define SAMCOP_SDICMDSTAT_RSPFIN     (1<<9)
#define SAMCOP_SDICMDSTAT_XFERING    (1<<8)
#define SAMCOP_SDICMDSTAT_INDEX      (0x3f)
#define SAMCOP_SDICMDSTAT_RESET      (0xFFFFFFFF)

#define SAMCOP_SDIDCON_IRQPERIOD     (1<<21)
#define SAMCOP_SDIDCON_TXAFTERRESP   (1<<20)
#define SAMCOP_SDIDCON_RXAFTERCMD    (1<<19)
#define SAMCOP_SDIDCON_BUSYAFTERCMD  (1<<18)
#define SAMCOP_SDIDCON_BLOCKMODE     (1<<17)
#define SAMCOP_SDIDCON_WIDEBUS       (1<<16)
#define SAMCOP_SDIDCON_DMAEN         (1<<15)
#define SAMCOP_SDIDCON_STOP          (1<<14)
#define SAMCOP_SDIDCON_DATMODE	     (3<<12)
#define SAMCOP_SDIDCON_BLKNUM_MASK   (0x7ff)

/* constants for SAMCOP_SDIDCON_DATMODE */
#define SAMCOP_SDIDCON_XFER_READY    (0<<12)
#define SAMCOP_SDIDCON_XFER_CHKSTART (1<<12)
#define SAMCOP_SDIDCON_XFER_RXSTART  (2<<12)
#define SAMCOP_SDIDCON_XFER_TXSTART  (3<<12)

#define SAMCOP_SDIDCNT_BLKNUM_SHIFT  (12)

#define SAMCOP_SDIDSTA_RDYWAITREQ    (1<<10)
#define SAMCOP_SDIDSTA_SDIOIRQDETECT (1<<9)
#define SAMCOP_SDIDSTA_FIFOFAIL      (1<<8)	/* reserved on 2440 */
#define SAMCOP_SDIDSTA_CRCFAIL       (1<<7)
#define SAMCOP_SDIDSTA_RXCRCFAIL     (1<<6)
#define SAMCOP_SDIDSTA_DATATIMEOUT   (1<<5)
#define SAMCOP_SDIDSTA_XFERFINISH    (1<<4)
#define SAMCOP_SDIDSTA_BUSYFINISH    (1<<3)
#define SAMCOP_SDIDSTA_SBITERR       (1<<2)	/* reserved on 2410a/2440 */
#define SAMCOP_SDIDSTA_TXDATAON      (1<<1)
#define SAMCOP_SDIDSTA_RXDATAON      (1<<0)
#define SAMCOP_SDIDSTA_RESET         (0xFFFFFFFF)

#define SAMCOP_SDIFSTA_TFDET          (1<<13)
#define SAMCOP_SDIFSTA_RFDET          (1<<12)
#define SAMCOP_SDIFSTA_TXHALF         (1<<11)
#define SAMCOP_SDIFSTA_TXEMPTY        (1<<10)
#define SAMCOP_SDIFSTA_RFLAST         (1<<9)
#define SAMCOP_SDIFSTA_RFFULL         (1<<8)
#define SAMCOP_SDIFSTA_RFHALF         (1<<7)
#define SAMCOP_SDIFSTA_COUNTMASK      (0x7f)

#define SAMCOP_SDIIMSK_RESPONSECRC    (1<<17)
#define SAMCOP_SDIIMSK_CMDSENT        (1<<16)
#define SAMCOP_SDIIMSK_CMDTIMEOUT     (1<<15)
#define SAMCOP_SDIIMSK_RESPONSEND     (1<<14)
#define SAMCOP_SDIIMSK_READWAIT       (1<<13)
#define SAMCOP_SDIIMSK_SDIOIRQ        (1<<12)
#define SAMCOP_SDIIMSK_FIFOFAIL       (1<<11)
#define SAMCOP_SDIIMSK_CRCSTATUS      (1<<10)
#define SAMCOP_SDIIMSK_DATACRC        (1<<9)
#define SAMCOP_SDIIMSK_DATATIMEOUT    (1<<8)
#define SAMCOP_SDIIMSK_DATAFINISH     (1<<7)
#define SAMCOP_SDIIMSK_BUSYFINISH     (1<<6)
#define SAMCOP_SDIIMSK_SBITERR        (1<<5)	/* reserved 2440/2410a */
#define SAMCOP_SDIIMSK_TXFIFOHALF     (1<<4)
#define SAMCOP_SDIIMSK_TXFIFOEMPTY    (1<<3)
#define SAMCOP_SDIIMSK_RXFIFOLAST     (1<<2)
#define SAMCOP_SDIIMSK_RXFIFOFULL     (1<<1)
#define SAMCOP_SDIIMSK_RXFIFOHALF     (1<<0)

/* number of cycles the sdi controller should wait for the card to
 * initialize once the sdi clock (SDICON bit 0) and prescaler
 * have been set.
 */
#define SAMCOP_CARD_INIT_CYCLES		74

#endif /* __ASM_HARDWARE_SAMCOP_SDI */
