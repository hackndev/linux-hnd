/* 
 *
 * Copyright (C) 2003,2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * SAMCOP DMA support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Changelog:
 *  ??-May-2003 BJD   Created file
 *  ??-Jun-2003 BJD   Added more dma functionality to go with arch
 *  10-Nov-2004 BJD   Added sys_device support
*/

#ifndef __ASM_ARCH_SAMCOP_DMA_H
#define __ASM_ARCH_SAMCOP_DMA_H __FILE__

#include <linux/sysdev.h>
//#include "hardware.h"


/*
 * This is the maximum DMA address(physical address) that can be DMAd to.
 *
 */
#define SAMCOP_MAX_DMA_ADDRESS		0x20000000
#define SAMCOP_MAX_DMA_TRANSFER_SIZE   0x100000 /* Data Unit is half word  */


/* according to the samsung port, we cannot use the regular
 * dma channels... we must therefore provide our own interface
 * for DMA, and allow our drivers to use that.
 */
/* XXX What to do about PXA? Remove this ? */
#define MAX_DMA_CHANNELS	0


/* we have 2 dma channels */
#define SAMCOP_DMA_CHANNELS        (2)

/* types */

typedef enum {
	SAMCOP_DMA_IDLE,
	SAMCOP_DMA_RUNNING,
	SAMCOP_DMA_PAUSED
} samcop_dma_state_t;


/* samcop_dma_loadst_t
 *
 * This represents the state of the DMA engine, wrt to the loaded / running
 * transfers. Since we don't have any way of knowing exactly the state of
 * the DMA transfers, we need to know the state to make decisions on wether
 * we can
 *
 * SAMCOP_DMA_NONE
 *
 * There are no buffers loaded (the channel should be inactive)
 *
 * SAMCOP_DMA_1LOADED
 *
 * There is one buffer loaded, however it has not been confirmed to be
 * loaded by the DMA engine. This may be because the channel is not
 * yet running, or the DMA driver decided that it was too costly to
 * sit and wait for it to happen.
 *
 * SAMCOP_DMA_1RUNNING
 *
 * The buffer has been confirmed running, and not finisged
 *
 * SAMCOP_DMA_1LOADED_1RUNNING
 *
 * There is a buffer waiting to be loaded by the DMA engine, and one
 * currently running.
*/

typedef enum {
	SAMCOP_DMALOAD_NONE,
	SAMCOP_DMALOAD_1LOADED,
	SAMCOP_DMALOAD_1RUNNING,
	SAMCOP_DMALOAD_1LOADED_1RUNNING,
} samcop_dma_loadst_t;

typedef enum {
	SAMCOP_RES_OK,
	SAMCOP_RES_ERR,
	SAMCOP_RES_ABORT
} samcop_dma_buffresult_t;


typedef enum samcop_dmasrc_e samcop_dmasrc_t;

enum samcop_dmasrc_e {
	SAMCOP_DMASRC_HW,      /* source is memory */
	SAMCOP_DMASRC_MEM      /* source is hardware */
};

/* enum samcop_chan_op_e
 *
 * operation codes passed to the DMA code by the user, and also used
 * to inform the current channel owner of any changes to the system state
*/

enum samcop_chan_op_e {
	SAMCOP_DMAOP_START,
	SAMCOP_DMAOP_STOP,
	SAMCOP_DMAOP_PAUSE,
	SAMCOP_DMAOP_RESUME,
	SAMCOP_DMAOP_FLUSH,
	SAMCOP_DMAOP_TIMEOUT,           /* internal signal to handler */
};

typedef enum samcop_chan_op_e samcop_chan_op_t;

/* flags */

#define SAMCOP_DMAF_SLOW         (1<<0)   /* slow, so don't worry about
					    * waiting for reloads */
#define SAMCOP_DMAF_AUTOSTART    (1<<1)   /* auto-start if buffer queued */

/* dma buffer */

typedef struct samcop_dma_buf_s samcop_dma_buf_t;

struct samcop_dma_client {
	char                *name;
};

typedef struct samcop_dma_client samcop_dma_client_t;

/* samcop_dma_buf_s
 *
 * internally used buffer structure to describe a queued or running
 * buffer.
*/

struct samcop_dma_buf_s {
	samcop_dma_buf_t   *next;
	int                  magic;        /* magic */
	int                  size;         /* buffer size in bytes */
	dma_addr_t           data;         /* start of DMA data */
	dma_addr_t           ptr;          /* where the DMA got to [1] */
	void                *id;           /* client's id */
};

/* [1] is this updated for both recv/send modes? */

typedef struct samcop_dma_chan_s samcop_dma_chan_t;

/* samcop_dma_cbfn_t
 *
 * buffer callback routine type
*/

typedef void (*samcop_dma_cbfn_t)(samcop_dma_chan_t *, void *buf, int size,
				   samcop_dma_buffresult_t result);

typedef int  (*samcop_dma_opfn_t)(samcop_dma_chan_t *,
				   samcop_chan_op_t );

struct samcop_dma_stats_s {
	unsigned long          loads;
	unsigned long          timeout_longest;
	unsigned long          timeout_shortest;
	unsigned long          timeout_avg;
	unsigned long          timeout_failed;
};

typedef struct samcop_dma_stats_s samcop_dma_stats_t;

/* struct samcop_dma_chan_s
 *
 * full state information for each DMA channel
*/

struct samcop_dma_chan_s {

	struct samcop_dma	*parent;

	/* channel state flags and information */
	unsigned char          number;      /* number of this dma channel */
	unsigned char          in_use;      /* channel allocated */
	unsigned char          irq_claimed; /* irq claimed for channel */
	unsigned char          irq_enabled; /* irq enabled for channel */
	unsigned char          xfer_unit;   /* size of an transfer */

	/* channel state */

	samcop_dma_state_t    state;
	samcop_dma_loadst_t   load_state;
	samcop_dma_client_t  *client;

	/* channel configuration */
	samcop_dmasrc_t       source;
	unsigned long          dev_addr;
	unsigned long          load_timeout;
	unsigned int           flags;        /* channel flags */

	/* channel's hardware position and configuration */
	void __iomem           *regs;        /* channels registers */
	unsigned long	       addr_reg;    /* data address register */
	unsigned int           irq;          /* channel irq */
	unsigned long          dcon;         /* default value of DCON */

	/* driver handles */
	samcop_dma_cbfn_t     callback_fn;  /* buffer done callback */
	samcop_dma_opfn_t     op_fn;        /* channel operation callback */

	/* stats gathering */
	samcop_dma_stats_t   *stats;
	samcop_dma_stats_t    stats_store;

	/* buffer list and information */
	samcop_dma_buf_t      *curr;        /* current dma buffer */
	samcop_dma_buf_t      *next;        /* next buffer to load */
	samcop_dma_buf_t      *end;         /* end of queue */

	/* system device */
	struct sys_device	dev;

	u32  (*rdreg)(void __iomem *regs, u32 reg);
	void (*wrreg)(void __iomem *regs, u32 reg, u32 val);
};

/* the currently allocated channel information */
extern samcop_dma_chan_t samcop_chans[];

/* note, we don't really use dma_device_t at the moment */
typedef unsigned long dma_device_t;

/* functions --------------------------------------------------------------- */

/* samcop_dma_request
 *
 * request a dma channel exclusivley
*/

extern int samcop_dma_request(dmach_t channel,
			       samcop_dma_client_t *, void *dev);


/* samcop_dma_ctrl
 *
 * change the state of the dma channel
*/

extern int samcop_dma_ctrl(dmach_t channel, samcop_chan_op_t op);

/* samcop_dma_setflags
 *
 * set the channel's flags to a given state
*/

extern int samcop_dma_setflags(dmach_t channel,
				unsigned int flags);

/* samcop_dma_free
 *
 * free the dma channel (will also abort any outstanding operations)
*/

extern int samcop_dma_free(dmach_t channel, samcop_dma_client_t *);

/* samcop_dma_enqueue
 *
 * place the given buffer onto the queue of operations for the channel.
 * The buffer must be allocated from dma coherent memory, or the Dcache/WB
 * drained before the buffer is given to the DMA system.
*/

extern int samcop_dma_enqueue(dmach_t channel, void *id,
			       dma_addr_t data, int size);

/* samcop_dma_config
 *
 * configure the dma channel
*/

extern int samcop_dma_config(dmach_t channel, int xferunit, int dcon);

/* samcop_dma_devconfig
 *
 * configure the device we're talking to
*/

extern int samcop_dma_devconfig(int channel, samcop_dmasrc_t source,
				 int hwcfg, unsigned long devaddr);

/* samcop_dma_getposition
 *
 * get the position that the dma transfer is currently at
*/

extern int samcop_dma_getposition(dmach_t channel,
				   dma_addr_t *src, dma_addr_t *dest);

extern int samcop_dma_set_opfn(dmach_t, samcop_dma_opfn_t rtn);
extern int samcop_dma_set_buffdone_fn(dmach_t, samcop_dma_cbfn_t rtn);

/* DMA Register definitions */

#define SAMCOP_DMA_DISRC       (0x00)
#define SAMCOP_DMA_DIDST       (0x04)
#define SAMCOP_DMA_DCON        (0x08)
#define SAMCOP_DMA_DSTAT       (0x0c)
#define SAMCOP_DMA_DCSRC       (0x10)
#define SAMCOP_DMA_DCDST       (0x14)
#define SAMCOP_DMA_DMASKTRIG   (0x18)

#define SAMCOP_DIxxx_INC_LOC_SHIFT (29)
#define SAMCOP_DIxxx_INC_LOC_MASK (3<<29)
#define SAMCOP_DISRC_INC_INC	(0<<29)
#define SAMCOP_DISRC_INC_FIXED	(1<<29)
#define SAMCOP_DISRC_AHB	(0<<30)
#define SAMCOP_DISRC_APB	(1<<30)

#define SAMCOP_DIDST_INC_INC	(0<<29)
#define SAMCOP_DIDST_INC_FIXED	(1<<29)
#define SAMCOP_DIDST_AHB	(0<<30)
#define SAMCOP_DIDST_APB	(1<<30)

#define SAMCOP_DMASKTRIG_STOP   (1<<2)
#define SAMCOP_DMASKTRIG_ON     (1<<1)
#define SAMCOP_DMASKTRIG_SWTRIG (1<<0)

#define SAMCOP_DCON_DEMAND     (0<<30)
#define SAMCOP_DCON_HANDSHAKE  (1<<30)
#define SAMCOP_DCON_SYNC_PCLK  (0<<29)
#define SAMCOP_DCON_SYNC_HCLK  (1<<29)

#define SAMCOP_DCON_INTREQ     (1<<28)

#define SAMCOP_DCON_TSZ		(1<<27)

#define SAMCOP_DCON_SERVMODE        (1<<26)
#define SAMCOP_DCON_SERVMODE_SINGLE (0<<26)
#define SAMCOP_DCON_SERVMODE_WHOLE  (1<<26)

#define SAMCOP_DCON_CH0_UART0	(0<<24)
#define SAMCOP_DCON_CH0_FCD	(1<<24)
#define SAMCOP_DCON_CH0_UART1	(2<<24)
#define SAMCOP_DCON_CH0_SD	(3<<24)

#define SAMCOP_DCON_CH1_UART1	(0<<24)
#define SAMCOP_DCON_CH1_SD	(1<<24)
#define SAMCOP_DCON_CH1_FCD	(2<<24)
#define SAMCOP_DCON_CH1_UART0	(3<<24)

#define SAMCOP_DCON_SRCSHIFT	(24)
#define SAMCOP_DCON_SRCMASK	(3<<24)

#define SAMCOP_DCON_HWTRIG     (1<<23)
#define SAMCOP_DCON_AUTORELOAD (0<<22)
#define SAMCOP_DCON_NORELOAD   (1<<22)

#define SAMCOP_DCON_BYTE       (0<<20)
#define SAMCOP_DCON_HALFWORD   (1<<20)
#define SAMCOP_DCON_WORD       (2<<20)

#define SAMCOP_DSTAT_STAT	(3<<20)
#define SAMCOP_DSTAT_CURR_TC	0xfffff /* bits 0-19 */

struct samcop_dma_plat_data {
	int	n_channels; /* number of DMA channels */

	u32	(*read_reg)(void __iomem *regs, u32 reg);
	void	(*write_reg)(void __iomem *regs, u32 reg, u32 val);
};

#endif /* __ASM_ARCH_SAMCOP_DMA_H */
