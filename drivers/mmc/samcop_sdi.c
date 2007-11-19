/*
 *  linux/drivers/mmc/samcop_sdi.c - Samsung SAMCOP SDI Interface driver
 *
 *  Copyright (C) 2004 Thomas Kleffel, All Rights Reserved.
 *  Copyright (C) 2005 Matt Reimer, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>

#include <asm/dma.h>
#include <asm/dma-mapping.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <asm/mach/mmc.h>

#include <asm/hardware/samcop-dma.h>
#include <asm/hardware/samcop-sdi.h>

#include "samcop_sdi.h"

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)       printk(KERN_DEBUG x)
#else
#define DBG(x...)       do { } while (0)
#endif

#define DRIVER_NAME "samcop sdi"
#define PFX DRIVER_NAME ": "

#define RESSIZE(ressource) (((ressource)->end - (ressource)->start)+1)

#define SAMCOP_SDI_TIMEOUT (HZ * 15)

// #define KERN_DEBUG KERN_INFO


static struct samcop_dma_client samcop_sdi_dma_client = {
	.name		= DRIVER_NAME " DMA",
};


static u32
sdi_rdreg(struct samcop_sdi_host *host, u32 reg)
{
	return host->plat->read_reg(mmc_dev(host->mmc), reg);
}

static void
sdi_wrreg(struct samcop_sdi_host *host, u32 reg, u32 val)
{
	host->plat->write_reg(mmc_dev(host->mmc), reg, val);
}


/*
 * ISR for SDI Interface IRQ
 * Communication between driver and ISR works as follows:
 *   host->mrq 			points to current request
 *   host->complete_what	tells the ISR when the request is considered done
 *     COMPLETION_CMDSENT	  when the command was sent
 *     COMPLETION_RSPFIN          when a response was received
 *     COMPLETION_XFERFINISH	  when the data transfer is finished
 *     COMPLETION_XFERFINISH_RSPFIN both of the above.
 *   host->complete_request	is the completion-object the driver waits for
 *
 * 1) Driver sets up host->mrq and host->complete_what
 * 2) Driver prepares the transfer
 * 3) Driver enables interrupts
 * 4) Driver starts transfer
 * 5) Driver waits for host->complete_rquest
 * 6) ISR checks for request status (errors and success)
 * 6) ISR sets host->mrq->cmd->error and host->mrq->data->error
 * 7) ISR completes host->complete_request
 * 8) ISR disables interrupts
 * 9) Driver wakes up and takes care of the request
*/

static irqreturn_t samcop_sdi_irq(int irq, void *dev_id)
{
	struct samcop_sdi_host *host;
	u32 sdi_csta, sdi_dsta, sdi_dcnt, sdi_fsta;
	u32 sdi_cclear, sdi_dclear;
	unsigned long iflags;

	host = (struct samcop_sdi_host *)dev_id;

	//Check for things not supposed to happen
	if(!host) return IRQ_HANDLED;
	
	sdi_csta 	= sdi_rdreg(host, SAMCOP_SDICMDSTAT);
	sdi_dsta 	= sdi_rdreg(host, SAMCOP_SDIDSTA);
	sdi_dcnt 	= sdi_rdreg(host, SAMCOP_SDIDCNT);
	sdi_fsta 	= sdi_rdreg(host, SAMCOP_SDIFSTA);
	
	DBG(PFX "IRQ csta=0x%08x dsta=0x%08x dcnt:0x%08x fsta:0x%08x\n", sdi_csta, sdi_dsta, sdi_dcnt, sdi_fsta);
		
	spin_lock_irqsave( &host->complete_lock, iflags);
	
	if( host->complete_what==COMPLETION_NONE ) {
		goto clear_imask;
	}
	
	if(!host->mrq) { 
		goto clear_imask;
	}

	
	sdi_csta 	= sdi_rdreg(host, SAMCOP_SDICMDSTAT);
	sdi_dsta 	= sdi_rdreg(host, SAMCOP_SDIDSTA);
	sdi_dcnt 	= sdi_rdreg(host, SAMCOP_SDIDCNT);
	sdi_cclear	= 0;
	sdi_dclear	= 0;
	
	
	if(sdi_csta & SAMCOP_SDICMDSTAT_CMDTIMEOUT) {
		host->mrq->cmd->error = MMC_ERR_TIMEOUT;
		goto transfer_closed;
	}

	if(sdi_csta & SAMCOP_SDICMDSTAT_CMDSENT) {
		if(host->complete_what == COMPLETION_CMDSENT) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			goto transfer_closed;
		}

		sdi_cclear |= SAMCOP_SDICMDSTAT_CMDSENT;
	}

	if(sdi_csta & SAMCOP_SDICMDSTAT_CRCFAIL) {

		/* S3C* cores improperly detect CRC errors for commands with
		   long responses (CMD2, 9, 10), so ignore them. */
		if(host->mrq->cmd->flags & MMC_RSP_CRC &&
		   !host->mrq->cmd->flags & MMC_RSP_136) {
			host->mrq->cmd->error = MMC_ERR_BADCRC;
			goto transfer_closed;
		}

		sdi_cclear |= SAMCOP_SDICMDSTAT_CRCFAIL;
	}

	if(sdi_csta & SAMCOP_SDICMDSTAT_RSPFIN) {
		if(host->complete_what == COMPLETION_RSPFIN) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			goto transfer_closed;
		}

		if(host->complete_what == COMPLETION_XFERFINISH_RSPFIN) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->complete_what = COMPLETION_XFERFINISH;
		}

		sdi_cclear |= SAMCOP_SDICMDSTAT_RSPFIN;
	}

	if(sdi_dsta & SAMCOP_SDIDSTA_FIFOFAIL) {
		host->mrq->cmd->error = MMC_ERR_NONE;
		host->mrq->data->error = MMC_ERR_FIFO;
		goto transfer_closed;
	}

	if(sdi_dsta & SAMCOP_SDIDSTA_RXCRCFAIL) {
		host->mrq->cmd->error = MMC_ERR_NONE;
		host->mrq->data->error = MMC_ERR_BADCRC;
		goto transfer_closed;
	}

	if(sdi_dsta & SAMCOP_SDIDSTA_CRCFAIL) {
		host->mrq->cmd->error = MMC_ERR_NONE;
		host->mrq->data->error = MMC_ERR_BADCRC;
		goto transfer_closed;
	}

	if(sdi_dsta & SAMCOP_SDIDSTA_DATATIMEOUT) {
		host->mrq->cmd->error = MMC_ERR_NONE;
		host->mrq->data->error = MMC_ERR_TIMEOUT;
		complete(&host->complete_dma);
		goto transfer_closed;
	}

	if(sdi_dsta & SAMCOP_SDIDSTA_XFERFINISH) {
		if(host->complete_what == COMPLETION_XFERFINISH) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_NONE;
			goto transfer_closed;
		}

		if(host->complete_what == COMPLETION_XFERFINISH_RSPFIN) {
			host->mrq->data->error = MMC_ERR_NONE;
			host->complete_what = COMPLETION_RSPFIN;
		}

		sdi_dclear |= SAMCOP_SDIDSTA_XFERFINISH;
	}

	sdi_wrreg(host, SAMCOP_SDICMDSTAT, sdi_cclear);
	sdi_wrreg(host, SAMCOP_SDIDSTA, sdi_dclear);

	spin_unlock_irqrestore( &host->complete_lock, iflags);
	DBG(PFX "IRQ still waiting.\n");
	return IRQ_HANDLED;


transfer_closed:
	host->complete_what = COMPLETION_NONE;
	complete(&host->complete_request);
	sdi_wrreg(host, SAMCOP_SDIIMSK, 0);
	spin_unlock_irqrestore( &host->complete_lock, iflags);
	DBG(PFX "IRQ transfer closed.\n");
	return IRQ_HANDLED;
	
clear_imask:
	sdi_wrreg(host, SAMCOP_SDIIMSK, 0);
	spin_unlock_irqrestore( &host->complete_lock, iflags);
	DBG(PFX "IRQ clear imask.\n");
	return IRQ_HANDLED;

}


/*
 * ISR for the CardDetect Pin
*/

static irqreturn_t samcop_sdi_irq_cd(int irq, void *dev_id)
{
	struct samcop_sdi_host *host = (struct samcop_sdi_host *)dev_id;

	mmc_detect_change(host->mmc, HZ / 4);
	return IRQ_HANDLED;
}

void samcop_sdi_dma_done_callback(samcop_dma_chan_t *dma_ch, void *buf_id,
	int size, samcop_dma_buffresult_t result)
{	unsigned long iflags;
	u32 sdi_csta, sdi_dsta,sdi_dcnt, sdi_fsta;
	struct samcop_sdi_host *host = (struct samcop_sdi_host *)buf_id;
	
	sdi_csta 	= sdi_rdreg(host, SAMCOP_SDICMDSTAT);
	sdi_dsta 	= sdi_rdreg(host, SAMCOP_SDIDSTA);
	sdi_dcnt 	= sdi_rdreg(host, SAMCOP_SDIDCNT);
	sdi_fsta 	= sdi_rdreg(host, SAMCOP_SDIFSTA);
	
	DBG(PFX "DMAD csta=0x%08x dsta=0x%08x dcnt:0x%08x fsta:0x%08x result:0x%08x\n", sdi_csta, sdi_dsta, sdi_dcnt, sdi_fsta, result);
	
	spin_lock_irqsave( &host->complete_lock, iflags);
	
	if(!host->mrq) goto out;
	if(!host->mrq->data) goto out;
	
	
	sdi_csta 	= sdi_rdreg(host, SAMCOP_SDICMDSTAT);
	sdi_dsta 	= sdi_rdreg(host, SAMCOP_SDIDSTA);
	sdi_dcnt 	= sdi_rdreg(host, SAMCOP_SDIDCNT);
		
	if( result!=SAMCOP_RES_OK ) {
		goto fail_request;
	}
	
	
	if(host->mrq->data->flags & MMC_DATA_READ) {
		if( sdi_dcnt>0 ) {
			goto fail_request;
		}
	}
	
out:	
	complete(&host->complete_dma);
	spin_unlock_irqrestore( &host->complete_lock, iflags);
	return;


fail_request:
	host->mrq->data->error = MMC_ERR_FAILED;
	host->complete_what = COMPLETION_NONE;
	complete(&host->complete_dma);
	complete(&host->complete_request);
	sdi_wrreg(host, SAMCOP_SDIIMSK, 0);
	goto out;

}


void samcop_sdi_dma_setup(struct samcop_sdi_host *host, struct mmc_data *data)
{
	samcop_dma_devconfig(host->plat->dma_chan, data->flags & MMC_DATA_READ ?
			     SAMCOP_DMASRC_HW : SAMCOP_DMASRC_MEM,
			     3, // XXX magic for APB | FIXED/NO_INC
			     (unsigned long)host->plat->dma_devaddr);
	samcop_dma_config(host->plat->dma_chan, host->plat->xfer_unit, SAMCOP_DCON_HWTRIG | host->plat->hwsrcsel);
	samcop_dma_set_buffdone_fn(host->plat->dma_chan, samcop_sdi_dma_done_callback);
	samcop_dma_setflags(host->plat->dma_chan, SAMCOP_DMAF_AUTOSTART);
}

static void samcop_sdi_request(struct mmc_host *mmc, struct mmc_request *mrq) {
 	struct samcop_sdi_host *host = mmc_priv(mmc);
	struct device *dev = mmc_dev(host->mmc);
	struct platform_device *pdev = to_platform_device(dev);
	u32 sdi_carg, sdi_ccon, sdi_timer;
	u32 sdi_bsize, sdi_dcon, sdi_imsk;
	int dma_len = 0;

	DBG(KERN_DEBUG PFX "request: [CMD] opcode:0x%02x arg:0x%08x flags:%x retries:%u\n",
		mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags, mrq->cmd->retries);


	sdi_ccon = mrq->cmd->opcode & SAMCOP_SDICMDCON_INDEX;
	sdi_ccon|= SAMCOP_SDICMDCON_SENDERHOST;
	sdi_ccon|= SAMCOP_SDICMDCON_CMDSTART;

	sdi_carg = mrq->cmd->arg;

	sdi_timer= host->plat->timeout;

	sdi_bsize= 0;
	sdi_dcon = 0;
	sdi_imsk = 0;

	//enable interrupts for transmission errors
	sdi_imsk |= SAMCOP_SDIIMSK_RESPONSEND;
	sdi_imsk |= SAMCOP_SDIIMSK_CRCSTATUS;


	host->complete_what = COMPLETION_CMDSENT;

	if (mrq->cmd->flags & MMC_RSP_PRESENT) {
		host->complete_what = COMPLETION_RSPFIN;

		sdi_ccon |= SAMCOP_SDICMDCON_WAITRSP;
		sdi_imsk |= SAMCOP_SDIIMSK_CMDTIMEOUT;

	} else {
		//We need the CMDSENT-Interrupt only if we want are not waiting
		//for a response
		sdi_imsk |= SAMCOP_SDIIMSK_CMDSENT;
	}

	if(mrq->cmd->flags & MMC_RSP_136) {
		sdi_ccon|= SAMCOP_SDICMDCON_LONGRSP;
	}

	if(mrq->cmd->flags & MMC_RSP_CRC) {
		sdi_imsk |= SAMCOP_SDIIMSK_RESPONSECRC;
	}


	if (mrq->data) {
		host->complete_what = COMPLETION_XFERFINISH_RSPFIN;

		sdi_bsize = mrq->data->blksz;

		sdi_dcon  = (mrq->data->blocks & SAMCOP_SDIDCON_BLKNUM_MASK);
		sdi_dcon |= SAMCOP_SDIDCON_DMAEN;

		sdi_imsk |= SAMCOP_SDIIMSK_FIFOFAIL;
		sdi_imsk |= SAMCOP_SDIIMSK_DATACRC;
		sdi_imsk |= SAMCOP_SDIIMSK_DATATIMEOUT;
		sdi_imsk |= SAMCOP_SDIIMSK_DATAFINISH;
		sdi_imsk |= 0xFFFFFFE0;

		DBG(PFX "request: [DAT] bsize:%u blocks:%u bytes:%u\n",
			sdi_bsize, mrq->data->blocks, mrq->data->blocks * sdi_bsize);

		if(mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
			sdi_dcon |= SAMCOP_SDIDCON_WIDEBUS;
		}

		if(!(mrq->data->flags & MMC_DATA_STREAM)) {
			sdi_dcon |= SAMCOP_SDIDCON_BLOCKMODE;
		}

		if(mrq->data->flags & MMC_DATA_WRITE) {
			sdi_dcon |= SAMCOP_SDIDCON_TXAFTERRESP;
			sdi_dcon |= SAMCOP_SDIDCON_XFER_TXSTART;
		}

		if(mrq->data->flags & MMC_DATA_READ) {
			sdi_dcon |= SAMCOP_SDIDCON_RXAFTERCMD;
			sdi_dcon |= SAMCOP_SDIDCON_XFER_RXSTART;
		}
		
		samcop_sdi_dma_setup(host, mrq->data);

		/* XXX Should we specify only the number of bytes we're going
		       to transfer, not the length of the whole buffer? */
		dma_len = dma_map_sg(&pdev->dev, mrq->data->sg,
			mrq->data->sg_len, mrq->data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE);

		//start DMA
		samcop_dma_enqueue(host->plat->dma_chan, (void *) host,
			sg_dma_address(&mrq->data->sg[0]),
			mrq->data->blocks * sdi_bsize);
	}

	host->mrq = mrq;

	init_completion(&host->complete_request);
	init_completion(&host->complete_dma);

	//Clear command and data status registers
	sdi_wrreg(host, SAMCOP_SDICMDSTAT, 0xFFFFFFFF);
	sdi_wrreg(host, SAMCOP_SDIDSTA, 0xFFFFFFFF);

	// Setup SDI controller
	sdi_wrreg(host, SAMCOP_SDIBSIZE, sdi_bsize);
	sdi_wrreg(host, SAMCOP_SDITIMER, sdi_timer);
	sdi_wrreg(host, SAMCOP_SDIIMSK, sdi_imsk);

	// Setup SDI command argument and data control
	sdi_wrreg(host, SAMCOP_SDICMDARG, sdi_carg);
	sdi_wrreg(host, SAMCOP_SDIDCON, sdi_dcon);

	// This initiates transfer
	sdi_wrreg(host, SAMCOP_SDICMDCON, sdi_ccon);

	// Wait for transfer to complete
	wait_for_completion_timeout(&host->complete_request, SAMCOP_SDI_TIMEOUT);
	DBG("[CMD] request complete.\n");
	if(mrq->data) {
		wait_for_completion_timeout(&host->complete_dma, SAMCOP_SDI_TIMEOUT);
		DBG("[DAT] DMA complete.\n");
	}
	
	//Cleanup controller
	sdi_wrreg(host, SAMCOP_SDICMDARG, 0);
	sdi_wrreg(host, SAMCOP_SDIDCON, 0);
	sdi_wrreg(host, SAMCOP_SDICMDCON, 0);
	sdi_wrreg(host, SAMCOP_SDIIMSK, 0);

	// Read response
	mrq->cmd->resp[0] = sdi_rdreg(host, SAMCOP_SDIRSP0);
	mrq->cmd->resp[1] = sdi_rdreg(host, SAMCOP_SDIRSP1);
	mrq->cmd->resp[2] = sdi_rdreg(host, SAMCOP_SDIRSP2);
	mrq->cmd->resp[3] = sdi_rdreg(host, SAMCOP_SDIRSP3);

	host->mrq = NULL;

	DBG(PFX "request done.\n");

	// If we have no data transfer we are finished here
	if (!mrq->data) goto request_done;

	dma_unmap_sg(&pdev->dev, mrq->data->sg, dma_len,
			mrq->data->flags & MMC_DATA_READ ? 
			DMA_FROM_DEVICE : DMA_TO_DEVICE);

	// Calulate the amout of bytes transfer, but only if there was
	// no error
	if(mrq->data->error == MMC_ERR_NONE)
		mrq->data->bytes_xfered = mrq->data->blocks * mrq->data->blksz;
	else
		mrq->data->bytes_xfered = 0;

	// If we had an error while transfering data we flush the
	// DMA channel to clear out any garbage
	if(mrq->data->error != MMC_ERR_NONE) {
		samcop_dma_ctrl(host->plat->dma_chan, SAMCOP_DMAOP_FLUSH);
		DBG(PFX "flushing DMA.\n");		
	}
	// Issue stop command
	if(mrq->data->stop) mmc_wait_for_cmd(mmc, mrq->data->stop, 3);


request_done:

	mrq->done(mrq);
}

static void samcop_sdi_set_ios(struct mmc_host *mmc, struct mmc_ios *ios) {
	struct device *dev = mmc_dev(mmc);
	struct samcop_sdi_data	*plat = dev->platform_data;

	switch(ios->power_mode) {
		case MMC_POWER_UP:
			break;

		case MMC_POWER_ON:
			plat->card_power(dev, 1, ios->clock);
			break;

		case MMC_POWER_OFF:
		default:
			plat->card_power(dev, 0, ios->clock);
			break;
	}
}

static struct mmc_host_ops samcop_sdi_ops = {
	.request	= samcop_sdi_request,
	.set_ios	= samcop_sdi_set_ios,
};

#ifdef CONFIG_PM
static int samcop_sdi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
 	struct samcop_sdi_host *host = mmc_priv(mmc);
	int ret = 0;

	if (!mmc)
		return 0;

	ret = mmc_suspend_host(mmc, state);

	/* XXX Work around a hang on resume by freeing the card
	       detect irq at suspend, and re-requesting it on resume. */
	free_irq(host->plat->irq_cd, host);
	clk_disable(host->clk);

	return ret;
}

static int samcop_sdi_resume(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
 	struct samcop_sdi_host *host = mmc_priv(mmc);
	int ret = 0;

	if (!mmc)
		return 0;

	clk_enable(host->clk);
	if (request_irq(host->plat->irq_cd, samcop_sdi_irq_cd, 0,
			DRIVER_NAME " card detect", host)) {
		printk(KERN_WARNING PFX "failed to request card detect interrupt.\n" );
	}
	ret = mmc_resume_host(mmc);

	return ret;
}

#else
#define samcop_sdi_suspend  NULL
#define samcop_sdi_resume   NULL
#endif


static int samcop_sdi_probe(struct platform_device *pdev)
{
	struct samcop_sdi_host 	*host;
	struct samcop_sdi_data	*plat = pdev->dev.platform_data;
	struct mmc_host 	*mmc;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct samcop_sdi_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto probe_out;
	}

	host = mmc_priv(mmc);

	spin_lock_init( &host->complete_lock );
	host->parent		= pdev->dev.parent;
	host->complete_what 	= COMPLETION_NONE;
	host->mmc 		= mmc;
	host->plat		= plat;

	host->irq = platform_get_irq(pdev, 0);

	if(request_irq(host->irq, samcop_sdi_irq, IRQF_DISABLED, DRIVER_NAME,
	               host)) {
		printk(KERN_WARNING PFX "failed to request sdi interrupt.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}

	set_irq_type(host->plat->irq_cd, IRQT_BOTHEDGE);

	/* Do platform-specific initialization */
	plat->init(&pdev->dev);

	if(request_irq(host->plat->irq_cd, samcop_sdi_irq_cd, IRQF_DISABLED |
	               IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		       DRIVER_NAME " card detect", host)) {
		printk(KERN_WARNING PFX "failed to request card detect interrupt.\n" );
		ret = -ENOENT;
		goto probe_free_irq;
	}

	if(samcop_dma_request(host->plat->dma_chan, &samcop_sdi_dma_client, NULL)) {
		printk(KERN_WARNING PFX "unable to get DMA channel.\n" );
		ret = -EBUSY;
		goto probe_free_irq_cd;
	}

	host->clk = clk_get(&pdev->dev, "sdi");
	if (IS_ERR(host->clk)) {
		printk(KERN_ERR "failed to get sdi clock\n");
		ret = -EBUSY;
		goto probe_free_dma;
	}
	clk_enable(host->clk);

	mmc->ops 	= &samcop_sdi_ops;
	mmc->ocr_avail	= MMC_VDD_32_33;
	mmc->caps	= MMC_CAP_4_BIT_DATA;
	mmc->f_min 	= plat->f_min;
	mmc->f_max 	= plat->f_max;

	/*
	 * Set the maximum request and segment size.
	 */
	mmc->max_req_size = plat->max_sectors * 512; /* 512B per sector */
	mmc->max_seg_size = mmc->max_req_size;

	/*
	 * Set the maximum block count. (XXX: check me once again)
	 * According to samcop SDI spec, SDIDatCnt register is:
	 * [11:0] Remaining data byte of 1 block
	 * [23:12] Remaining block number  <- 12 bits, max is 4095?
	 */
	mmc->max_blk_count = 4095;

	printk(KERN_INFO PFX "probe: irq=%u irq_cd=%u dma=%u.\n", 
	       host->irq, host->plat->irq_cd, host->plat->dma_chan);

	if((ret = mmc_add_host(mmc))) {
		printk(KERN_INFO PFX "failed to add mmc host.\n");
		goto error_add_host;
	}

	platform_set_drvdata(pdev, mmc);

	printk(KERN_INFO PFX "initialisation done.\n");
	return 0;

 error_add_host:

	clk_disable(host->clk);
	clk_put(host->clk);

	plat->exit(&pdev->dev);

 probe_free_dma:
	samcop_dma_free(host->plat->dma_chan, &samcop_sdi_dma_client);

 probe_free_irq_cd:
 	free_irq(host->plat->irq_cd, host);

 probe_free_irq:
 	free_irq(host->irq, host);

 probe_free_host:
	mmc_free_host(mmc);
 probe_out:
	return ret;
}

static void samcop_sdi_shutdown(struct platform_device *pdev)
{
	struct mmc_host 	*mmc  = platform_get_drvdata(pdev);
	struct samcop_sdi_host 	*host = mmc_priv(mmc);
	struct samcop_sdi_data	*plat = pdev->dev.platform_data;

	mmc_remove_host(mmc);

	plat->exit(&pdev->dev);

	samcop_dma_free(host->plat->dma_chan, &samcop_sdi_dma_client);
	clk_disable(host->clk);
	clk_put(host->clk);
 	free_irq(host->plat->irq_cd, host);
 	free_irq(host->irq, host);
	mmc_free_host(mmc);
}

static int samcop_sdi_remove(struct platform_device *pdev)
{
	samcop_sdi_shutdown(pdev);
	return 0;
}

/* ------------------- device registration ----------------------- */

static struct platform_driver samcop_sdi_device = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = samcop_sdi_probe,
	.remove = samcop_sdi_remove,
	.shutdown = samcop_sdi_shutdown,
#ifdef CONFIG_PM
	.suspend = samcop_sdi_suspend,
	.resume = samcop_sdi_resume,
#endif
};


static int __init samcop_sdi_init(void)
{
	return platform_driver_register (&samcop_sdi_device);
}

static void __exit samcop_sdi_exit(void)
{
	return platform_driver_unregister (&samcop_sdi_device);
}

module_init(samcop_sdi_init);
module_exit(samcop_sdi_exit);

MODULE_DESCRIPTION("SAMCOP SDI Interface driver");
MODULE_AUTHOR("Thomas Kleffel");
MODULE_LICENSE("GPL");
