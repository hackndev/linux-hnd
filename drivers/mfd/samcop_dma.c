/*
 * SAMCOP DMA core
 *
 * (c) 2003,2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file is based on the Sangwook Lee/Samsung patches, re-written due
 * to various ommisions from the code (such as flexible dma configuration)
 * for use with the BAST system board.
 *
 * The re-write is pretty much complete, and should be good enough for any
 * possible DMA function
 *
 * The SAMCOP adaptation was done by Matt Reimer.
 */

#ifdef CONFIG_SOC_SAMCOP_DMA_DEBUG
#define DEBUG
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sysdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <asm/hardware/samcop-dma.h>

struct samcop_dma {
	struct device *dev;
	struct device *parent;
	void *map;	/* pointer to the DMA registers */
	int irq;	/* IRQ number of the first channel */
	int n_channels;	/* number of DMA channels on this ASIC */
	struct clk *clk;
};

/* dma channel state information */
samcop_dma_chan_t samcop_chans[SAMCOP_DMA_CHANNELS];

/* debugging functions */

#define BUF_MAGIC (0xcafebabe)

#define dmawarn(fmt...) printk(KERN_DEBUG fmt)

static u32
dma_rdreg(void __iomem *regs, u32 reg)
{
	return readl(regs + reg);
}

static void
dma_wrreg(void __iomem *regs, u32 reg, u32 val)
{
	pr_debug("writing %08x to register %08x regs %p\n", val, reg, regs);
	writel(val, regs + reg);
}


/* captured register state for debug */

struct samcop_dma_regstate {
	unsigned long         dcsrc;
	unsigned long         disrc;
	unsigned long         dcdst;
	unsigned long         didst;
	unsigned long         dstat;
	unsigned long         dcon;
	unsigned long         dmsktrig;
};

#ifdef CONFIG_SAMCOP_DMA_DEBUG

/* dmadbg_showregs
 *
 * simple debug routine to print the current state of the dma registers
*/

static void
dmadbg_capture(samcop_dma_chan_t *chan, struct samcop_dma_regstate *regs)
{
	regs->dcsrc    = chan->rdreg(chan->regs, SAMCOP_DMA_DCSRC);
	regs->disrc    = chan->rdreg(chan->regs, SAMCOP_DMA_DISRC);
	regs->dcdst    = chan->rdreg(chan->regs, SAMCOP_DMA_DCDST);
	regs->didst    = chan->rdreg(chan->regs, SAMCOP_DMA_DIDST);
	regs->dstat    = chan->rdreg(chan->regs, SAMCOP_DMA_DSTAT);
	regs->dcon     = chan->rdreg(chan->regs, SAMCOP_DMA_DCON);
	regs->dmsktrig = chan->rdreg(chan->regs, SAMCOP_DMA_DMASKTRIG);
}

static void
dmadbg_showregs(const char *fname, int line, samcop_dma_chan_t *chan,
		 struct samcop_dma_regstate *regs)
{
	printk(KERN_DEBUG "dma%d: %s:%d: DCSRC=%08lx, DISRC=%08lx, DCDST=%08lx, DIDST=%08lx, DSTAT=%08lx DMT=%02lx, DCON=%08lx\n",
	       chan->number, fname, line,
	       regs->dcsrc, regs->disrc, regs->dcdst, regs->didst, regs->dstat, regs->dmsktrig,
	       regs->dcon);
}

static void
dmadbg_showchan(const char *fname, int line, samcop_dma_chan_t *chan)
{
	struct samcop_dma_regstate state;

	dmadbg_capture(chan, &state);

	printk(KERN_DEBUG "dma%d: %s:%d: ls=%d, cur=%p, %p %p\n",
	       chan->number, fname, line, chan->load_state,
	       chan->curr, chan->next, chan->end);

	dmadbg_showregs(fname, line, chan, &state);
}

#define dbg_showregs(chan) dmadbg_showregs(__FUNCTION__, __LINE__, (chan))
#define dbg_showchan(chan) dmadbg_showchan(__FUNCTION__, __LINE__, (chan))
#else
#define dbg_showregs(chan) do { } while(0)
#define dbg_showchan(chan) do { } while(0)
#endif /* CONFIG_SAMCOP_DMA_DEBUG */

#define check_channel(chan) \
  do { if ((chan) >= SAMCOP_DMA_CHANNELS) { \
    printk(KERN_ERR "%s: invalid channel %d\n", __FUNCTION__, (chan)); \
    return -EINVAL; \
  } } while(0)


/* samcop_dma_stats_timeout
 *
 * Update DMA stats from timeout info
*/

static void
samcop_dma_stats_timeout(samcop_dma_stats_t *stats, int val)
{
	if (stats == NULL)
		return;

	if (val > stats->timeout_longest)
		stats->timeout_longest = val;
	if (val < stats->timeout_shortest)
		stats->timeout_shortest = val;

	stats->timeout_avg += val;
}

/* samcop_dma_waitforload
 *
 * wait for the DMA engine to load a buffer, and update the state accordingly
*/

static int
samcop_dma_waitforload(samcop_dma_chan_t *chan, int line)
{
	int timeout = chan->load_timeout;
	int took;

	if (chan->load_state != SAMCOP_DMALOAD_1LOADED) {
		printk(KERN_ERR "dma%d: samcop_dma_waitforload() called in loadstate %d from line %d\n", chan->number, chan->load_state, line);
		return 0;
	}

	if (chan->stats != NULL)
		chan->stats->loads++;

	while (--timeout > 0) {
		if ((chan->rdreg(chan->regs, SAMCOP_DMA_DSTAT) << (32-20)) != 0) {
			took = chan->load_timeout - timeout;

			samcop_dma_stats_timeout(chan->stats, took);

			switch (chan->load_state) {
			case SAMCOP_DMALOAD_1LOADED:
				chan->load_state = SAMCOP_DMALOAD_1RUNNING;
				break;

			default:
				printk(KERN_ERR "dma%d: unknown load_state in samcop_dma_waitforload() %d\n", chan->number, chan->load_state);
			}

			return 1;
		}
	}

	if (chan->stats != NULL) {
		chan->stats->timeout_failed++;
	}

	return 0;
}



/* samcop_dma_loadbuffer
 *
 * load a buffer, and update the channel state
*/

static inline int
samcop_dma_loadbuffer(samcop_dma_chan_t *chan,
		       samcop_dma_buf_t *buf)
{
	unsigned long reload;
	unsigned long tmp;

	if (buf == NULL) {
		dmawarn("buffer is NULL\n");
		return -EINVAL;
	}

	pr_debug("samcop_chan_loadbuffer: loading buff %p (0x%08lx,0x%06x)\n",
		 buf, (unsigned long)buf->data, buf->size);

	/* check the state of the channel before we do anything */

	if (chan->load_state == SAMCOP_DMALOAD_1LOADED) {
		dmawarn("load_state is SAMCOP_DMALOAD_1LOADED\n");
	}

	if (chan->load_state == SAMCOP_DMALOAD_1LOADED_1RUNNING) {
		dmawarn("state is SAMCOP_DMALOAD_1LOADED_1RUNNING\n");
	}

	/* it would seem sensible if we are the last buffer to not bother
	 * with the auto-reload bit, so that the DMA engine will not try
	 * and load another transfer after this one has finished...
	 */
	if (chan->load_state == SAMCOP_DMALOAD_NONE) {
		pr_debug("load_state is none, checking for noreload (next=%p)\n",
			 buf->next);
		reload = (buf->next == NULL) ? SAMCOP_DCON_NORELOAD : 0;
	} else {
		pr_debug("load_state is %d => autoreload\n", chan->load_state);
		reload = SAMCOP_DCON_AUTORELOAD;
	}

	/* Write the offset, preserving the INC and LOC bits. */
	tmp = chan->rdreg(chan->regs, chan->addr_reg);
	tmp &= SAMCOP_DIxxx_INC_LOC_MASK;
	tmp |= buf->data & ~(SAMCOP_DIxxx_INC_LOC_MASK);
	chan->wrreg(chan->regs, chan->addr_reg, tmp);

	chan->wrreg(chan->regs, SAMCOP_DMA_DCON,
			    chan->dcon | reload | (buf->size/chan->xfer_unit));

	chan->next = buf->next;

	/* update the state of the channel */

	switch (chan->load_state) {
	case SAMCOP_DMALOAD_NONE:
		chan->load_state = SAMCOP_DMALOAD_1LOADED;
		break;

	case SAMCOP_DMALOAD_1RUNNING:
		chan->load_state = SAMCOP_DMALOAD_1LOADED_1RUNNING;
		break;

	default:
		dmawarn("dmaload: unknown state %d in loadbuffer\n",
			chan->load_state);
		break;
	}

	return 0;
}

/* samcop_dma_call_op
 *
 * small routine to call the op routine with the given op if it has been
 * registered
*/

static void
samcop_dma_call_op(samcop_dma_chan_t *chan, samcop_chan_op_t op)
{
	if (chan->op_fn != NULL) {
		(chan->op_fn)(chan, op);
	}
}

/* samcop_dma_buffdone
 *
 * small wrapper to check if callback routine needs to be called, and
 * if so, call it
*/

static inline void
samcop_dma_buffdone(samcop_dma_chan_t *chan, samcop_dma_buf_t *buf,
		     samcop_dma_buffresult_t result)
{
	pr_debug("callback_fn=%p, buf=%p, id=%p, size=%d, result=%d\n",
		 chan->callback_fn, buf, buf->id, buf->size, result);

	if (chan->callback_fn != NULL) {
		(chan->callback_fn)(chan, buf->id, buf->size, result);
	}
}

/* samcop_dma_start
 *
 * start a dma channel going
*/

static int samcop_dma_start(samcop_dma_chan_t *chan)
{
	unsigned long tmp;
	unsigned long flags;

	pr_debug("samcop_start_dma: channel=%d\n", chan->number);

	local_irq_save(flags);

	if (chan->state == SAMCOP_DMA_RUNNING) {
		pr_debug("samcop_start_dma: already running (%d)\n", chan->state);
		local_irq_restore(flags);
		return 0;
	}

	chan->state = SAMCOP_DMA_RUNNING;

	/* check wether there is anything to load, and if not, see
	 * if we can find anything to load
	 */

	if (chan->load_state == SAMCOP_DMALOAD_NONE) {
		if (chan->next == NULL) {
			printk(KERN_ERR "dma%d: channel has nothing loaded\n",
			       chan->number);
			chan->state = SAMCOP_DMA_IDLE;
			local_irq_restore(flags);
			return -EINVAL;
		}

		samcop_dma_loadbuffer(chan, chan->next);
	}

	dbg_showchan(chan);

	/* enable the channel */

	if (!chan->irq_enabled) {
		enable_irq(chan->irq);
		chan->irq_enabled = 1;
	}

	/* start the channel going */

	tmp = chan->rdreg(chan->regs, SAMCOP_DMA_DMASKTRIG);
	tmp &= ~SAMCOP_DMASKTRIG_STOP;
	tmp |= SAMCOP_DMASKTRIG_ON;
	chan->wrreg(chan->regs, SAMCOP_DMA_DMASKTRIG, tmp);

	pr_debug("wrote %08lx to DMASKTRIG\n", tmp);

#if 0
	/* the dma buffer loads should take care of clearing the AUTO
	 * reloading feature */
	tmp = chan->rdreg(chan->regs, SAMCOP_DMA_DCON);
	tmp &= ~SAMCOP_DCON_NORELOAD;
	chan->wrreg(chan->regs, SAMCOP_DMA_DCON, tmp);
#endif

	samcop_dma_call_op(chan, SAMCOP_DMAOP_START);

	dbg_showchan(chan);

	local_irq_restore(flags);
	return 0;
}

/* samcop_dma_canload
 *
 * work out if we can queue another buffer into the DMA engine
*/

static int
samcop_dma_canload(samcop_dma_chan_t *chan)
{
	if (chan->load_state == SAMCOP_DMALOAD_NONE ||
	    chan->load_state == SAMCOP_DMALOAD_1RUNNING)
		return 1;

	return 0;
}


/* samcop_dma_enqueue
 *
 * queue an given buffer for dma transfer.
 *
 * id         the device driver's id information for this buffer
 * data       the physical address of the buffer data
 * size       the size of the buffer in bytes
 *
 * If the channel is not running, then the flag SAMCOP_DMAF_AUTOSTART
 * is checked, and if set, the channel is started. If this flag isn't set,
 * then an error will be returned.
 *
 * It is possible to queue more than one DMA buffer onto a channel at
 * once, and the code will deal with the re-loading of the next buffer
 * when necessary.
*/

int samcop_dma_enqueue(unsigned int channel, void *id,
			dma_addr_t data, int size)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];
	samcop_dma_buf_t *buf;
	unsigned long flags;

	check_channel(channel);

	pr_debug("%s: id=%p, data=%08x, size=%d\n",
		 __FUNCTION__, id, (unsigned int)data, size);

	buf = (samcop_dma_buf_t *)kmalloc(sizeof(*buf), GFP_ATOMIC);
	if (buf == NULL) {
		pr_debug("%s: out of memory (%d alloc)\n",
			 __FUNCTION__, sizeof(*buf));
		return -ENOMEM;
	}

	pr_debug("%s: new buffer %p\n", __FUNCTION__, buf);

	//dbg_showchan(chan);

	buf->next  = NULL;
	buf->data  = buf->ptr = data;
	buf->size  = size;
	buf->id    = id;
	buf->magic = BUF_MAGIC;

	local_irq_save(flags);

	if (chan->curr == NULL) {
		/* we've got nothing loaded... */
		pr_debug("%s: buffer %p queued onto empty channel\n",
			 __FUNCTION__, buf);

		chan->curr = buf;
		chan->end  = buf;
		chan->next = NULL;
	} else {
		pr_debug("dma%d: %s: buffer %p queued onto non-empty channel\n",
			 chan->number, __FUNCTION__, buf);

		if (chan->end == NULL)
			pr_debug("dma%d: %s: %p not empty, and chan->end==NULL?\n",
				 chan->number, __FUNCTION__, chan);

		chan->end->next = buf;
		chan->end = buf;
	}

	/* if necessary, update the next buffer field */
	if (chan->next == NULL)
		chan->next = buf;

	/* check to see if we can load a buffer */
	if (chan->state == SAMCOP_DMA_RUNNING) {
		if (chan->load_state == SAMCOP_DMALOAD_1LOADED && 1) {
			if (samcop_dma_waitforload(chan, __LINE__) == 0) {
				printk(KERN_ERR "dma%d: loadbuffer:"
				       "timeout loading buffer\n",
				       chan->number);
				dbg_showchan(chan);
				local_irq_restore(flags);
				return -EINVAL;
			}
		}

		while (samcop_dma_canload(chan) && chan->next != NULL) {
			samcop_dma_loadbuffer(chan, chan->next);
		}
	} else if (chan->state == SAMCOP_DMA_IDLE) {
		if (chan->flags & SAMCOP_DMAF_AUTOSTART) {
			samcop_dma_ctrl(chan->number, SAMCOP_DMAOP_START);
		}
	}

	local_irq_restore(flags);
	return 0;
}

EXPORT_SYMBOL(samcop_dma_enqueue);

static inline void
samcop_dma_freebuf(samcop_dma_buf_t *buf)
{
	int magicok = (buf->magic == BUF_MAGIC);

	buf->magic = -1;

	if (magicok) {
		kfree(buf);
	} else {
		printk("samcop_dma_freebuf: buff %p with bad magic\n", buf);
	}
}

/* samcop_dma_lastxfer
 *
 * called when the system is out of buffers, to ensure that the channel
 * is prepared for shutdown.
*/

static inline void
samcop_dma_lastxfer(samcop_dma_chan_t *chan)
{
	pr_debug("dma%d: samcop_dma_lastxfer: load_state %d\n",
		 chan->number, chan->load_state);

	switch (chan->load_state) {
	case SAMCOP_DMALOAD_NONE:
		break;

	case SAMCOP_DMALOAD_1LOADED:
		if (samcop_dma_waitforload(chan, __LINE__) == 0) {
				/* flag error? */
			printk(KERN_ERR "dma%d: timeout waiting for load\n",
			       chan->number);
			return;
		}
		break;

	default:
		pr_debug("dma%d: lastxfer: unhandled load_state %d with no next",
			 chan->number, chan->load_state);
		return;

	}

	/* hopefully this'll shut the damned thing up after the transfer... */
	chan->wrreg(chan->regs, SAMCOP_DMA_DCON, chan->dcon | SAMCOP_DCON_NORELOAD);
}


#define dmadbg2(x...)

static irqreturn_t
samcop_dma_irq(int irq, void *devpw)
{
	samcop_dma_chan_t *chan = (samcop_dma_chan_t *)devpw;
	samcop_dma_buf_t  *buf;

	buf = chan->curr;

	dbg_showchan(chan);

	/* modify the channel state */

	switch (chan->load_state) {
	case SAMCOP_DMALOAD_1RUNNING:
		/* TODO - if we are running only one buffer, we probably
		 * want to reload here, and then worry about the buffer
		 * callback */

		chan->load_state = SAMCOP_DMALOAD_NONE;
		break;

	case SAMCOP_DMALOAD_1LOADED:
		/* iirc, we should go back to NONE loaded here, we
		 * had a buffer, and it was never verified as being
		 * loaded.
		 */

		chan->load_state = SAMCOP_DMALOAD_NONE;
		break;

	case SAMCOP_DMALOAD_1LOADED_1RUNNING:
		/* we'll worry about checking to see if another buffer is
		 * ready after we've called back the owner. This should
		 * ensure we do not wait around too long for the DMA
		 * engine to start the next transfer
		 */

		chan->load_state = SAMCOP_DMALOAD_1LOADED;
		break;

	case SAMCOP_DMALOAD_NONE:
		printk(KERN_ERR "dma%d: IRQ with no loaded buffer?\n",
		       chan->number);
		break;

	default:
		printk(KERN_ERR "dma%d: IRQ in invalid load_state %d\n",
		       chan->number, chan->load_state);
		break;
	}

	if (buf != NULL) {
		/* update the chain to make sure that if we load any more
		 * buffers when we call the callback function, things should
		 * work properly */

		chan->curr = buf->next;
		buf->next  = NULL;

		if (buf->magic != BUF_MAGIC) {
			printk(KERN_ERR "dma%d: %s: buf %p incorrect magic\n",
			       chan->number, __FUNCTION__, buf);
			return IRQ_HANDLED;
		}

		samcop_dma_buffdone(chan, buf, SAMCOP_RES_OK);

		/* free resouces */
		samcop_dma_freebuf(buf);
	} else {
	}

	if (chan->next != NULL) {
		unsigned long flags;

		switch (chan->load_state) {
		case SAMCOP_DMALOAD_1RUNNING:
			/* don't need to do anything for this state */
			break;

		case SAMCOP_DMALOAD_NONE:
			/* can load buffer immediately */
			break;

		case SAMCOP_DMALOAD_1LOADED:
			if (samcop_dma_waitforload(chan, __LINE__) == 0) {
				/* flag error? */
				printk(KERN_ERR "dma%d: timeout waiting for load\n",
				       chan->number);
				return IRQ_HANDLED;
			}

			break;

		case SAMCOP_DMALOAD_1LOADED_1RUNNING:
			goto no_load;

		default:
			printk(KERN_ERR "dma%d: unknown load_state in irq, %d\n",
			       chan->number, chan->load_state);
			return IRQ_HANDLED;
		}

		local_irq_save(flags);
		samcop_dma_loadbuffer(chan, chan->next);
		local_irq_restore(flags);
	} else {
		samcop_dma_lastxfer(chan);

		/* see if we can stop this channel.. */
		if (chan->load_state == SAMCOP_DMALOAD_NONE) {
			pr_debug("dma%d: end of transfer, stopping channel (%ld)\n",
				 chan->number, jiffies);
			samcop_dma_ctrl(chan->number, SAMCOP_DMAOP_STOP);
		}
	}

 no_load:
	return IRQ_HANDLED;
}



/* samcop_request_dma
 *
 * get control of an dma channel
*/

int samcop_dma_request(unsigned int channel, samcop_dma_client_t *client,
			void *dev)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];
	unsigned long flags;
	int err;

	pr_debug("dma%d: samcop_request_dma: client=%s, dev=%p\n",
		 channel, client->name, dev);

	check_channel(channel);

	local_irq_save(flags);

	dbg_showchan(chan);

	if (chan->in_use) {
		if (client != chan->client) {
			printk(KERN_ERR "dma%d: already in use\n", channel);
			local_irq_restore(flags);
			return -EBUSY;
		} else {
			printk(KERN_ERR "dma%d: client already has channel\n", channel);
		}
	}

	chan->client = client;
	chan->in_use = 1;

	if (!chan->irq_claimed) {
		pr_debug("dma%d: %s : requesting irq %d\n",
			 channel, __FUNCTION__, chan->irq);

		err = request_irq(chan->irq, samcop_dma_irq, SA_INTERRUPT,
				  client->name, (void *)chan);

		if (err) {
			chan->in_use = 0;
			local_irq_restore(flags);

			printk(KERN_ERR "%s: cannot get IRQ %d for DMA %d\n",
			       client->name, chan->irq, chan->number);
			return err;
		}

		chan->irq_claimed = 1;
		chan->irq_enabled = 1;
	}

	local_irq_restore(flags);

	/* need to setup */

	pr_debug("%s: channel initialised, %p\n", __FUNCTION__, chan);

	return 0;
}

EXPORT_SYMBOL(samcop_dma_request);

/* samcop_dma_free
 *
 * release the given channel back to the system, will stop and flush
 * any outstanding transfers, and ensure the channel is ready for the
 * next claimant.
 *
 * Note, although a warning is currently printed if the freeing client
 * info is not the same as the registrant's client info, the free is still
 * allowed to go through.
*/

int samcop_dma_free(dmach_t channel, samcop_dma_client_t *client)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];
	unsigned long flags;

	check_channel(channel);

	local_irq_save(flags);


	if (chan->client != client) {
		printk(KERN_WARNING "dma%d: possible free from different client (channel %p, passed %p)\n",
		       channel, chan->client, client);
	}

	/* sort out stopping and freeing the channel */

	if (chan->state != SAMCOP_DMA_IDLE) {
		pr_debug("%s: need to stop dma channel %p\n",
		       __FUNCTION__, chan);

		/* possibly flush the channel */
		samcop_dma_ctrl(channel, SAMCOP_DMAOP_STOP);
	}

	chan->client = NULL;
	chan->in_use = 0;

	if (chan->irq_claimed) {
		free_irq(chan->irq, (void *)chan);
		chan->irq_claimed = 0;
	}

	local_irq_restore(flags);

	return 0;
}

EXPORT_SYMBOL(samcop_dma_free);

static int samcop_dma_dostop(samcop_dma_chan_t *chan)
{
	unsigned long tmp;
	unsigned long flags;

	pr_debug("%s:\n", __FUNCTION__);

	dbg_showchan(chan);

	local_irq_save(flags);

	samcop_dma_call_op(chan,  SAMCOP_DMAOP_STOP);

	tmp = chan->rdreg(chan->regs, SAMCOP_DMA_DMASKTRIG);
	tmp |= SAMCOP_DMASKTRIG_STOP;
	chan->wrreg(chan->regs, SAMCOP_DMA_DMASKTRIG, tmp);

#if 0
	/* should also clear interrupts, according to WinCE BSP */
	tmp = chan->rdreg(chan->regs, SAMCOP_DMA_DCON);
	tmp |= SAMCOP_DCON_NORELOAD;
	chan->wrreg(chan->regs, SAMCOP_DMA_DCON, tmp);
#endif

	chan->state      = SAMCOP_DMA_IDLE;
	chan->load_state = SAMCOP_DMALOAD_NONE;

	local_irq_restore(flags);

	return 0;
}

/* samcop_dma_flush
 *
 * stop the channel, and remove all current and pending transfers
*/

static int samcop_dma_flush(samcop_dma_chan_t *chan)
{
	samcop_dma_buf_t *buf, *next;
	unsigned long flags;

	pr_debug("%s:\n", __FUNCTION__);

	local_irq_save(flags);

	if (chan->state != SAMCOP_DMA_IDLE) {
		pr_debug("%s: stopping channel...\n", __FUNCTION__ );
		samcop_dma_ctrl(chan->number, SAMCOP_DMAOP_STOP);
	}

	buf = chan->curr;
	if (buf == NULL)
		buf = chan->next;

	chan->curr = chan->next = chan->end = NULL;

	if (buf != NULL) {
		for ( ; buf != NULL; buf = next) {
			next = buf->next;

			pr_debug("%s: free buffer %p, next %p\n",
			       __FUNCTION__, buf, buf->next);

			samcop_dma_buffdone(chan, buf, SAMCOP_RES_ABORT);
			samcop_dma_freebuf(buf);
		}
	}

	local_irq_restore(flags);

	return 0;
}


int
samcop_dma_ctrl(dmach_t channel, samcop_chan_op_t op)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	check_channel(channel);

	switch (op) {
	case SAMCOP_DMAOP_START:
		return samcop_dma_start(chan);

	case SAMCOP_DMAOP_STOP:
		return samcop_dma_dostop(chan);

	case SAMCOP_DMAOP_PAUSE:
		return -ENOENT;

	case SAMCOP_DMAOP_RESUME:
		return -ENOENT;

	case SAMCOP_DMAOP_FLUSH:
		return samcop_dma_flush(chan);

	case SAMCOP_DMAOP_TIMEOUT:
		return 0;

	}

	return -ENOENT;      /* unknown, don't bother */
}

EXPORT_SYMBOL(samcop_dma_ctrl);

/* DMA configuration for each channel
 *
 * DISRCC -> source of the DMA (AHB,APB)
 * DISRC  -> source address of the DMA
 * DIDSTC -> destination of the DMA (AHB,APD)
 * DIDST  -> destination address of the DMA
*/

/* samcop_dma_config
 *
 * xfersize:     size of unit in bytes (1,2,4)
 * dcon:         base value of the DCONx register
*/

int samcop_dma_config(dmach_t channel,
		       int xferunit,
		       int dcon)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	pr_debug("%s: chan=%d, xfer_unit=%d, dcon=%08x\n",
		 __FUNCTION__, channel, xferunit, dcon);

	check_channel(channel);

	switch (xferunit) {
	case 1:
		dcon |= SAMCOP_DCON_BYTE;
		break;

	case 2:
		dcon |= SAMCOP_DCON_HALFWORD;
		break;

	case 4:
		dcon |= SAMCOP_DCON_WORD;
		break;

	default:
		pr_debug("%s: bad transfer size %d\n", __FUNCTION__, xferunit);
		return -EINVAL;
	}

	dcon |= SAMCOP_DCON_HWTRIG;
	dcon |= SAMCOP_DCON_INTREQ;

	pr_debug("%s: dcon now %08x\n", __FUNCTION__, dcon);

	chan->dcon = dcon;
	chan->xfer_unit = xferunit;

	return 0;
}

EXPORT_SYMBOL(samcop_dma_config);

int samcop_dma_setflags(dmach_t channel, unsigned int flags)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	check_channel(channel);

	pr_debug("%s: chan=%p, flags=%08x\n", __FUNCTION__, chan, flags);

	chan->flags = flags;

	return 0;
}

EXPORT_SYMBOL(samcop_dma_setflags);


/* do we need to protect the settings of the fields from
 * irq?
*/

int samcop_dma_set_opfn(dmach_t channel, samcop_dma_opfn_t rtn)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	check_channel(channel);

	pr_debug("%s: chan=%p, op rtn=%p\n", __FUNCTION__, chan, rtn);

	chan->op_fn = rtn;

	return 0;
}

EXPORT_SYMBOL(samcop_dma_set_opfn);

int samcop_dma_set_buffdone_fn(dmach_t channel, samcop_dma_cbfn_t rtn)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	check_channel(channel);

	pr_debug("%s: chan=%p, callback rtn=%p\n", __FUNCTION__, chan, rtn);

	chan->callback_fn = rtn;

	return 0;
}

EXPORT_SYMBOL(samcop_dma_set_buffdone_fn);

/* samcop_dma_devconfig
 *
 * configure the dma source/destination hardware type and address
 *
 * source:    SAMCOP_DMASRC_HW: source is hardware
 *            SAMCOP_DMASRC_MEM: source is memory
 *
 * hwcfg:     the value for xxxSTCn register,
 *            bit 0: 0=increment pointer, 1=leave pointer
 *            bit 1: 0=soucre is AHB, 1=soucre is APB
 *
 * devaddr:   physical address of the source
*/

int samcop_dma_devconfig(int channel,
			  samcop_dmasrc_t source,
			  int hwcfg,
			  unsigned long devaddr)
{
	samcop_dma_chan_t *chan = &samcop_chans[channel];

	check_channel(channel);

	pr_debug("%s: source=%d, hwcfg=%08x, devaddr=%08lx\n",
		 __FUNCTION__, (int)source, hwcfg, devaddr);

	chan->source = source;
	chan->dev_addr = devaddr;

	switch (source) {
	case SAMCOP_DMASRC_HW:
		/* source is hardware */
		pr_debug("%s: hw source, devaddr=%08lx, hwcfg=%d\n",
			 __FUNCTION__, devaddr, hwcfg);
		chan->wrreg(chan->regs, SAMCOP_DMA_DISRC,
			  devaddr | ((hwcfg & 3) << SAMCOP_DIxxx_INC_LOC_SHIFT));
		/* The address gets written later. */
		chan->wrreg(chan->regs, SAMCOP_DMA_DIDST,
			  SAMCOP_DIDST_AHB | SAMCOP_DIDST_INC_INC);

		chan->addr_reg = SAMCOP_DMA_DIDST;

		return 0;

	case SAMCOP_DMASRC_MEM:
		/* source is memory */
		pr_debug( "%s: mem source, devaddr=%08lx, hwcfg=%d\n",
			  __FUNCTION__, devaddr, hwcfg);
		/* The address gets written later. */
		chan->wrreg(chan->regs, SAMCOP_DMA_DISRC,
			  SAMCOP_DISRC_AHB | SAMCOP_DISRC_INC_INC);
		chan->wrreg(chan->regs, SAMCOP_DMA_DIDST,
			  devaddr | ((hwcfg & 3) << SAMCOP_DIxxx_INC_LOC_SHIFT));

		chan->addr_reg = SAMCOP_DMA_DISRC;

		return 0;
	}

	printk(KERN_ERR "dma%d: invalid source type (%d)\n", channel, source);
	return -EINVAL;
}

EXPORT_SYMBOL(samcop_dma_devconfig);

/* samcop_dma_getposition
 *
 * returns the current transfer points for the dma source and destination
*/

int samcop_dma_getposition(dmach_t channel, dma_addr_t *src, dma_addr_t *dst)
{
 	samcop_dma_chan_t *chan = &samcop_chans[channel];

 	check_channel(channel);

	if (src != NULL)
 		*src = chan->rdreg(chan->regs, SAMCOP_DMA_DCSRC);

 	if (dst != NULL)
 		*dst = chan->rdreg(chan->regs, SAMCOP_DMA_DCDST);

 	return 0;
}

EXPORT_SYMBOL(samcop_dma_getposition);


/* system device class */

#ifdef CONFIG_PM

static int samcop_dma_suspend(struct platform_device *pdev, pm_message_t state)
{
        struct samcop_dma *dma = platform_get_drvdata(pdev);
	samcop_dma_chan_t *cp;
	int channel;

	for (channel = 0; channel < dma->n_channels; channel++) {

		cp = &samcop_chans[channel];

		printk(KERN_DEBUG "suspending dma channel %d\n", cp->number);

		if (cp->rdreg(cp->regs, SAMCOP_DMA_DMASKTRIG) & SAMCOP_DMASKTRIG_ON) {
			/* the dma channel is still working, which is probably
			 * a bad thing to do over suspend/resume. We stop the
			 * channel and assume that the client is either going to
			 * retry after resume, or that it is broken.
			 */

			printk(KERN_INFO "dma: stopping channel %d due to suspend\n",
			       cp->number);

			samcop_dma_dostop(cp);
		}
	}

	if (state.event == PM_EVENT_SUSPEND)
		clk_disable(dma->clk);

	return 0;
}

static int samcop_dma_resume(struct platform_device *pdev)
{
        struct samcop_dma *dma = platform_get_drvdata(pdev);

	clk_enable(dma->clk);
		
	return 0;
}

#else
#define samcop_dma_suspend NULL
#define samcop_dma_resume  NULL
#endif /* CONFIG_PM */

/* initialisation code */

static int samcop_init_dma(struct samcop_dma *dma)
{
	samcop_dma_chan_t *cp;
	int channel;
	struct samcop_dma_plat_data *plat = dma->dev->platform_data;

	printk("SAMCOP DMA Driver, (c) 2003-2004 Simtec Electronics\n");

	for (channel = 0; channel < dma->n_channels; channel++) {
		cp = &samcop_chans[channel];

		memset(cp, 0, sizeof(samcop_dma_chan_t));

		/* dma channel irqs are in order.. */
		cp->parent = dma;
		cp->number = channel;
		cp->irq    = dma->irq + channel;
		cp->regs   = dma->map + (channel * 0x20);
		cp->rdreg = plat->read_reg  ? plat->read_reg  : dma_rdreg;
		cp->wrreg = plat->write_reg ? plat->write_reg : dma_wrreg;

		/* point current stats somewhere */
		cp->stats  = &cp->stats_store;
		cp->stats_store.timeout_shortest = LONG_MAX;

		/* basic channel configuration */

		cp->load_timeout = 1<<18;

		printk("DMA channel %d at %p, irq %d\n",
		       cp->number, cp->regs, cp->irq);
	}

	return 0;
}

static void samcop_dma_shutdown(struct platform_device *pdev)
{
	samcop_dma_suspend(pdev, PMSG_SUSPEND);
}

/* ----------------------------------------------------------------- */

static int 
samcop_dma_probe (struct platform_device *pdev)
{
        struct samcop_dma *dma;
	struct samcop_dma_plat_data *plat = pdev->dev.platform_data;
	struct resource *res;

        dma = kmalloc(sizeof (*dma), GFP_KERNEL);
        if (!dma)
                return -ENOMEM;
        memset(dma, 0, sizeof (*dma));

	dma->dev = &pdev->dev;
	dma->parent = pdev->dev.parent;
	platform_set_drvdata(pdev, dma);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        dma->map = ioremap(res->start, res->end - res->start + 1);
        dma->irq = platform_get_irq(pdev, 0);
	dma->n_channels = plat->n_channels;

	dma->clk = clk_get(&pdev->dev, "dma");
	if (IS_ERR(dma->clk)) {
		printk(KERN_ERR "failed to get clock\n");
		kfree(dma);
		return -ENOENT;
	}
	clk_enable(dma->clk);

	samcop_init_dma(dma);

        return 0;
}

static int
samcop_dma_remove (struct platform_device *pdev)
{
        struct samcop_dma *dma = platform_get_drvdata(pdev);

	clk_disable(dma->clk);
	clk_put(dma->clk);

	iounmap(dma->map);
        kfree(dma);

        return 0;
}

static struct platform_driver samcop_dma_driver = {
	.driver   = {
		.name = "samcop dma",
	},
	.probe    = samcop_dma_probe,
	.remove   = samcop_dma_remove,
	.suspend  = samcop_dma_suspend,
	.resume   = samcop_dma_resume,
	.shutdown = samcop_dma_shutdown,
};

static int
samcop_dma_init (void)
{
        return platform_driver_register (&samcop_dma_driver);
}

static void
samcop_dma_cleanup (void)
{
        platform_driver_unregister (&samcop_dma_driver);
}

subsys_initcall(samcop_dma_init);
module_exit(samcop_dma_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Matt Reimer <mreimer@vpop.net>");
MODULE_DESCRIPTION("SAMCOP/HAMCOP DMA driver");

