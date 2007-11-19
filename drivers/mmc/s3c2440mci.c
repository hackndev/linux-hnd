/*
 *  linux/drivers/mmc/s3c2440mci.c - Samsung S3C2410 SDI Interface driver
 *
 *  Copyright (C) 2004 Thomas Kleffel, All Rights Reserved.
 *
 * Hacked by Pierre Hebert for s3c2440. It is not clean, use with care.
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
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/protocol.h>
#include <linux/clk.h>
#include <linux/irq.h>

#include <asm/dma.h>
#include <asm/dma-mapping.h>
#include <asm/arch/dma.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/mmc.h>

#include <asm/arch/regs-sdi.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/mmc.h>

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)       printk(KERN_INFO x)
#else
#define DBG(x...)       do { } while (0)
#endif

#include "s3c2440mci.h"

#define DRIVER_NAME "mmci-s3c2410"
#define PFX DRIVER_NAME ": "

#define RESSIZE(ressource) (((ressource)->end - (ressource)->start)+1)

static struct s3c2410_dma_client s3c2410sdi_dma_client = {
	.name		= "s3c2410-sdi",
};

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

static irqreturn_t s3c2410sdi_irq(int irq, void *dev_id)
{
	struct s3c2410sdi_host *host;
	u32 sdi_csta, sdi_fsta, sdi_dsta, sdi_dcnt;
	u32 sdi_cclear, sdi_dclear;
	unsigned long iflags;

	host = (struct s3c2410sdi_host *)dev_id;


	/* Check for things not supposed to happen */
	if(!host) return IRQ_HANDLED;
	
	sdi_csta 	= readl(host->base + S3C2410_SDICMDSTAT);
	sdi_dsta 	= readl(host->base + S3C2410_SDIDSTA);
	sdi_fsta 	= readl(host->base + S3C2410_SDIFSTA);
	sdi_dcnt 	= readl(host->base + S3C2410_SDIDCNT);
	
	DBG(PFX "[IRQ] csta=%08x dsta=%08x fsta=%08x dcnt=%08x\n", sdi_csta, sdi_dsta, sdi_fsta, sdi_dcnt);
		
	spin_lock_irqsave( &host->complete_lock, iflags);
	
	if( host->complete_what==COMPLETION_NONE ) {
		goto clear_imask;
	}
	
	if(!host->mrq) { 
		goto clear_imask;
	}

	
	sdi_cclear	= 0;
	sdi_dclear	= 0;
	
	
	if(sdi_csta & S3C2410_SDICMDSTAT_CMDTIMEOUT) {
		host->mrq->cmd->error = MMC_ERR_TIMEOUT;
		goto transfer_closed;
	}

	if(sdi_csta & S3C2410_SDICMDSTAT_CMDSENT) {
		if(host->complete_what == COMPLETION_CMDSENT) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			goto transfer_closed;
		}

		sdi_cclear |= S3C2410_SDICMDSTAT_CMDSENT;
	}

	if(sdi_csta & S3C2410_SDICMDSTAT_CRCFAIL) {
		if (host->mrq->cmd->flags & MMC_RSP_136) {
			DBG(PFX "s3c2410 fixup : ignore CRC fail with long rsp\n");
		}
		else {
			DBG(PFX "COMMAND CRC FAILED %x\n", sdi_csta);
			if(host->mrq->cmd->flags & MMC_RSP_CRC) {
				host->mrq->cmd->error = MMC_ERR_BADCRC;
				goto transfer_closed;
			}
		}
		sdi_cclear |= S3C2410_SDICMDSTAT_CRCFAIL;
	}

	if(sdi_csta & S3C2410_SDICMDSTAT_RSPFIN) {
		if(host->complete_what == COMPLETION_RSPFIN) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			goto transfer_closed;
		}

		if(host->complete_what == COMPLETION_XFERFINISH_RSPFIN) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->complete_what = COMPLETION_XFERFINISH;
		}

		sdi_cclear |= S3C2410_SDICMDSTAT_RSPFIN;
	}

	if(host->mrq->data) {
		if(sdi_fsta & S3C2440_SDIFSTA_FIFOFAIL) {
			writel(sdi_fsta, host->base + S3C2410_SDIFSTA);
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_FIFO;
			goto transfer_closed;
		}

		if(sdi_dsta & S3C2410_SDIDSTA_RXCRCFAIL) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_BADCRC;
			goto transfer_closed;
		}

		/*if(sdi_dsta & S3C2410_SDIDSTA_CRCFAIL) {
		  DBG(PFX "DATA CRC FAILED %u\n", sdi_csta);
		  host->mrq->cmd->error = MMC_ERR_NONE;
		  host->mrq->data->error = MMC_ERR_BADCRC;
		  complete(&host->complete_dma);
		  goto transfer_closed;
		  }*/

		/* I don't understand this CRC failure */
		if(sdi_dsta & S3C2410_SDIDSTA_CRCFAIL) {
			DBG(PFX "FIXME DATA CRC FAILED %u\n", sdi_csta);
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_NONE;
			complete(&host->complete_dma);
			goto transfer_closed;
		}

		if(sdi_dsta & S3C2410_SDIDSTA_DATATIMEOUT) {
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_TIMEOUT;
			goto transfer_closed;
		}

		if(sdi_dsta & S3C2410_SDIDSTA_XFERFINISH) {
			if(host->complete_what == COMPLETION_XFERFINISH) {
				host->mrq->cmd->error = MMC_ERR_NONE;
				host->mrq->data->error = MMC_ERR_NONE;
				goto transfer_closed;
			}

			if(host->complete_what == COMPLETION_XFERFINISH_RSPFIN) {
				host->mrq->data->error = MMC_ERR_NONE;
				host->complete_what = COMPLETION_RSPFIN;
			}

			sdi_dclear |= S3C2410_SDIDSTA_XFERFINISH;
		}

		if(sdi_fsta & S3C2410_SDIFSTA_RFLAST) {
			writel(sdi_fsta, host->base + S3C2410_SDIFSTA);
			host->mrq->cmd->error = MMC_ERR_NONE;
			host->mrq->data->error = MMC_ERR_NONE;
			goto transfer_closed;
		}
	}

	// FIXME no dma for write operation
	if(sdi_dsta & 2) {
		while(sdi_fsta & S3C2410_SDIFSTA_TFDET && host->pio_words) {
			writel(*host->pio_ptr, host->base+S3C2440_SDIDATA);
			(host->pio_ptr)++;
			host->pio_words--;
			sdi_fsta 	= readl(host->base + S3C2410_SDIFSTA);
		}
	}

	spin_unlock_irqrestore( &host->complete_lock, iflags);
	//DBG(PFX "IRQ still waiting.\n");
	return IRQ_HANDLED;


transfer_closed:
	writel(sdi_cclear, host->base + S3C2410_SDICMDSTAT);
	writel(sdi_dclear, host->base + S3C2410_SDIDSTA);
	host->complete_what = COMPLETION_NONE;
	complete(&host->complete_request);
	writel(0, host->base + S3C2440_SDIIMSK);
	spin_unlock_irqrestore( &host->complete_lock, iflags);
	DBG(PFX "IRQ transfer closed.\n");
	return IRQ_HANDLED;
	
clear_imask:
	writel(0, host->base + S3C2440_SDIIMSK);
	spin_unlock_irqrestore( &host->complete_lock, iflags);
	DBG(PFX "IRQ clear imask.\n");
	return IRQ_HANDLED;

}


/*
 * ISR for the CardDetect Pin
*/

static irqreturn_t s3c2410sdi_irq_cd(int irq, void *dev_id)
{
	struct s3c2410sdi_host *host = (struct s3c2410sdi_host *)dev_id;
	mmc_detect_change(host->mmc, S3C2410SDI_CDLATENCY);

	return IRQ_HANDLED;
}



static void s3c2410sdi_dma_done_callback(struct s3c2410_dma_chan *dma_ch, void *buf_id,
	int size, enum s3c2410_dma_buffresult result)
{	unsigned long iflags;
	struct s3c2410sdi_host *host;
	u32 sdi_dcnt;
#ifdef CONFIG_MMC_DEBUG
	u32 sdi_csta, sdi_dsta, sdi_fsta;
#endif
	
	host = (struct s3c2410sdi_host *)buf_id;
	sdi_dcnt 	= readl(host->base + S3C2410_SDIDCNT);

#ifdef CONFIG_MMC_DEBUG
	sdi_csta 	= readl(host->base + S3C2410_SDICMDSTAT);
	sdi_dsta 	= readl(host->base + S3C2410_SDIDSTA);
	sdi_fsta 	= readl(host->base + S3C2410_SDIFSTA);
	
	DBG(PFX "DMAD csta=0x%08x dsta=0x%08x fsta=%08x dcnt=0x%08x result:0x%08x\n", sdi_csta, sdi_dsta, sdi_fsta, sdi_dcnt, result);
#endif


	spin_lock_irqsave( &host->complete_lock, iflags);
	
	if(!host->mrq) goto out;
	if(!host->mrq->data) goto out;
	
	
		
	if( result!=S3C2410_RES_OK ) {
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
	complete(&host->complete_request);
	writel(0, host->base + S3C2440_SDIIMSK);
	goto out;

}


static void s3c2410sdi_dma_setup(struct s3c2410sdi_host *host, enum s3c2410_dmasrc source) {
	s3c2410_dma_devconfig(host->dma, source, 3, host->mem->start + S3C2440_SDIDATA);
	s3c2410_dma_config(host->dma, 4, (1<<23) | (2<<24));
	s3c2410_dma_set_buffdone_fn(host->dma, s3c2410sdi_dma_done_callback);
	s3c2410_dma_setflags(host->dma, S3C2410_DMAF_AUTOSTART);
}

static void s3c2410sdi_request(struct mmc_host *mmc, struct mmc_request *mrq) {
 	struct s3c2410sdi_host *host = mmc_priv(mmc);
	struct device *dev = mmc_dev(host->mmc);
	struct platform_device *pdev = to_platform_device(dev);
	u32 sdi_carg, sdi_ccon, sdi_timer;
	u32 sdi_bsize, sdi_dcon, sdi_imsk;
	int dma_len = 0;
	int i, res;

	DBG(KERN_DEBUG PFX "request: [CMD] opcode:0x%02x arg:0x%08x flags:%x retries:%u\n",
		mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags, mrq->cmd->retries);
	DBG(PFX "request : %s mode\n",mmc->mode == MMC_MODE_MMC ? "mmc" : "sd");

	sdi_ccon = mrq->cmd->opcode & S3C2410_SDICMDCON_INDEX;
	sdi_ccon|= S3C2410_SDICMDCON_SENDERHOST;
	sdi_ccon|= S3C2410_SDICMDCON_CMDSTART;

	sdi_carg = mrq->cmd->arg;

	sdi_timer= 0x7FFFFF;

	sdi_bsize= 0;
	sdi_dcon = 0;
	sdi_imsk = 0;

	/* enable interrupts for transmission errors */
	sdi_imsk |= S3C2410_SDIIMSK_RESPONSEND;
	sdi_imsk |= S3C2410_SDIIMSK_CRCSTATUS;

	host->complete_what = COMPLETION_CMDSENT;

	if (mrq->cmd->flags & MMC_RSP_PRESENT) {
		host->complete_what = COMPLETION_RSPFIN;

		sdi_ccon |= S3C2410_SDICMDCON_WAITRSP;
		sdi_imsk |= S3C2410_SDIIMSK_CMDTIMEOUT;

	} else {
		/* We need the CMDSENT-Interrupt only if we want are not waiting
		 * for a response
		 */
		sdi_imsk |= S3C2410_SDIIMSK_CMDSENT;
	}

	if(mrq->cmd->flags & MMC_RSP_136) {
		sdi_ccon|= S3C2410_SDICMDCON_LONGRSP;
	}

	if(mrq->cmd->flags & MMC_RSP_CRC) {
			sdi_imsk |= S3C2410_SDIIMSK_RESPONSECRC;
	}


	if (mrq->data) {
		host->complete_what = COMPLETION_XFERFINISH_RSPFIN;

		sdi_bsize = mrq->data->blksz;
		host->size = mrq->data->blksz;

		sdi_dcon  = (mrq->data->blocks & S3C2410_SDIDCON_BLKNUM_MASK);
		sdi_dcon |= S3C2440_SDIDCON_DATA_START;
		sdi_dcon |= S3C2440_SDIDCON_DS_WORD;

		sdi_imsk |= S3C2410_SDIIMSK_FIFOFAIL;
		sdi_imsk |= S3C2410_SDIIMSK_DATACRC;
		sdi_imsk |= S3C2410_SDIIMSK_DATATIMEOUT;
		sdi_imsk |= S3C2410_SDIIMSK_DATAFINISH;
		//sdi_imsk |= 0xFFFFFFE0;

		DBG(PFX "request: [DAT] bsize:%u blocks:%u bytes:%u\n",
			sdi_bsize, mrq->data->blocks, mrq->data->blocks * sdi_bsize);

		if (host->bus_width == MMC_BUS_WIDTH_4) {
			sdi_dcon |= S3C2410_SDIDCON_WIDEBUS;
		}

		if(!(mrq->data->flags & MMC_DATA_STREAM)) {
			sdi_dcon |= S3C2410_SDIDCON_BLOCKMODE;
		}

		if(mrq->data->flags & MMC_DATA_WRITE) {
			sdi_dcon |= S3C2410_SDIDCON_TXAFTERRESP;
			sdi_dcon |= S3C2410_SDIDCON_XFER_TXSTART;
		}

		if(mrq->data->flags & MMC_DATA_READ) {
			sdi_dcon |= S3C2410_SDIDCON_RXAFTERCMD;
			sdi_dcon |= S3C2410_SDIDCON_XFER_RXSTART;
		}

		// FIXME no dma for write operation
		if(!(mrq->data->flags & MMC_DATA_WRITE)) {
			s3c2410sdi_dma_setup(host, mrq->data->flags & MMC_DATA_WRITE ? S3C2410_DMASRC_MEM : S3C2410_DMASRC_HW);
			sdi_dcon |= S3C2410_SDIDCON_DMAEN;

			/* see DMA-API.txt */
			dma_len = dma_map_sg(&pdev->dev, mrq->data->sg, \
					mrq->data->sg_len, \
					mrq->data->flags & MMC_DATA_READ ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

			/* start DMA */
			for(i=0; i<dma_len; i++) {
				res=s3c2410_dma_enqueue(host->dma, (void *) host,
						sg_dma_address(&mrq->data->sg[i]),
						sg_dma_len(&mrq->data->sg[i]) );
			}
		}

		// FIXME no dma for write operation
		if(mrq->data->flags & MMC_DATA_WRITE)
		{
			struct scatterlist *sg;
			sg=&mrq->data->sg[0];
			host->pio_words=sdi_bsize>>2;
			host->pio_ptr=page_address(sg->page)+sg->offset;
		}

		writel(sdi_dcon, host->base + S3C2410_SDIDCON);
	}

	host->mrq = mrq;

	init_completion(&host->complete_request);
	if(mrq->data) {
		// FIXME no dma for write operation
		if(!(mrq->data->flags & MMC_DATA_WRITE)) {
			init_completion(&host->complete_dma);
		}
	}

	writel(sdi_bsize,host->base + S3C2410_SDIBSIZE);
	writel(0xFFFFFFFF, host->base + S3C2410_SDIFSTA);

	/* Clear command and data status registers */
	writel(0xFFFFFFFF, host->base + S3C2410_SDICMDSTAT);
	writel(0xFFFFFFFF, host->base + S3C2410_SDIDSTA);

	/* Setup SDI controller */
	writel(sdi_timer,host->base + S3C2410_SDITIMER);
	writel(sdi_imsk,host->base + S3C2440_SDIIMSK);

	/* Setup SDI command argument and data control */
	writel(sdi_carg, host->base + S3C2410_SDICMDARG);

	/* This initiates transfer */
	writel(sdi_ccon, host->base + S3C2410_SDICMDCON);

	/* Wait for transfer to complete */
	wait_for_completion(&host->complete_request);
	DBG(PFX "[CMD] request complete.\n");
	if(mrq->data) {
		// FIXME no dma for write operation
		if(!(mrq->data->flags & MMC_DATA_WRITE)) {
			wait_for_completion(&host->complete_dma);
			DBG(PFX "[DAT] DMA complete.\n");
		}
	}

	/* Cleanup controller */
	writel(0, host->base + S3C2410_SDICMDARG);
	writel(0, host->base + S3C2410_SDIDCON);
	writel(0, host->base + S3C2410_SDICMDCON);
	writel(0, host->base + S3C2440_SDIIMSK);

	/*  Read response */
	mrq->cmd->resp[0] = readl(host->base + S3C2410_SDIRSP0);
	mrq->cmd->resp[1] = readl(host->base + S3C2410_SDIRSP1);
	mrq->cmd->resp[2] = readl(host->base + S3C2410_SDIRSP2);
	mrq->cmd->resp[3] = readl(host->base + S3C2410_SDIRSP3);

	host->mrq = NULL;

	DBG(PFX "request done.\n");

	/* If we have no data transfer we are finished here */
	if (!mrq->data) goto request_done;

	// FIXME no dma for write operation
	if(!(mrq->data->flags & MMC_DATA_WRITE)) {
		dma_unmap_sg(&pdev->dev, mrq->data->sg, dma_len, \
				mrq->data->flags & MMC_DATA_READ ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}

	/* Calulate the amout of bytes transfer, but only if there was
	 * no error
	 */
	if(mrq->data->error == MMC_ERR_NONE) {
		mrq->data->bytes_xfered = mrq->data->blksz;
	} else {
		mrq->data->bytes_xfered = 0;
	}

	/* If we had an error while transfering data we flush the
	 * DMA channel to clear out any garbage
	 */
	if(mrq->data->error != MMC_ERR_NONE) {
		s3c2410_dma_ctrl(host->dma, S3C2410_DMAOP_FLUSH);
		DBG(PFX "flushing DMA.\n");		
	}

	if(mrq->data->stop) mmc_wait_for_cmd(mmc, mrq->data->stop, 3);

request_done:

	mrq->done(mrq);
}

static void s3c2410sdi_set_ios(struct mmc_host *mmc, struct mmc_ios *ios) {
	struct s3c2410sdi_host *host = mmc_priv(mmc);
	u32 sdi_psc, sdi_con;

	/* Set power */
	sdi_con = readl(host->base + S3C2410_SDICON);
	switch(ios->power_mode) {
		case MMC_POWER_ON:
		case MMC_POWER_UP:
			DBG(PFX "power on\n");
			s3c2410_gpio_cfgpin(S3C2410_GPE5, S3C2410_GPE5_SDCLK);
			s3c2410_gpio_cfgpin(S3C2410_GPE6, S3C2410_GPE6_SDCMD);
			s3c2410_gpio_cfgpin(S3C2410_GPE7, S3C2410_GPE7_SDDAT0);
			s3c2410_gpio_cfgpin(S3C2410_GPE8, S3C2410_GPE8_SDDAT1);
			s3c2410_gpio_cfgpin(S3C2410_GPE9, S3C2410_GPE9_SDDAT2);
			s3c2410_gpio_cfgpin(S3C2410_GPE10, S3C2410_GPE10_SDDAT3);

			if (host->pdata->set_power)
				(host->pdata->set_power)(1);

			break;

		case MMC_POWER_OFF:
		default:
			if (host->pdata->set_power)
				(host->pdata->set_power)(0);
			break;
	}

	/* Set clock */
	for(sdi_psc=0;sdi_psc<255;sdi_psc++) {
		if( (clk_get_rate(host->clk) / (2*(sdi_psc+1))) <= ios->clock) break;
	}

	if(sdi_psc > 255) sdi_psc = 255;
	writel(sdi_psc, host->base + S3C2410_SDIPRE);

	/* Set CLOCK_ENABLE */
	if(ios->clock) 	sdi_con |= S3C2440_SDICON_CLOCKENABLE;
	else		sdi_con &=~S3C2440_SDICON_CLOCKENABLE;

  // use MMC type clock
  sdi_con|=S3C2440_SDICON_MMCCLOCK;

	writel(sdi_con, host->base + S3C2410_SDICON);

	host->bus_width = ios->bus_width;

}

static struct mmc_host_ops s3c2410sdi_ops = {
	.request	= s3c2410sdi_request,
	.set_ios	= s3c2410sdi_set_ios,
};

static void s3c2410_mmc_def_setpower(unsigned int to)
{
	s3c2410_gpio_cfgpin(S3C2410_GPA17, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPA17, to);
}

static struct s3c24xx_mmc_platdata s3c2410_mmc_defplat = {
	.gpio_detect	= S3C2410_GPF2,
	.set_power	= s3c2410_mmc_def_setpower,
	.f_max		= 3000000,
	.ocr_avail	= MMC_VDD_32_33,
};

static int s3c2410sdi_probe(struct platform_device *pdev)
{
	struct mmc_host 	*mmc;
	s3c24xx_mmc_pdata_t	*pdata;
	struct s3c2410sdi_host 	*host;


	int ret;

	mmc = mmc_alloc_host(sizeof(struct s3c2410sdi_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto probe_out;
	}

	host = mmc_priv(mmc);

	spin_lock_init( &host->complete_lock );
	host->complete_what 	= COMPLETION_NONE;
	host->mmc 		= mmc;
	host->dma		= S3C2410SDI_DMA;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		pdev->dev.platform_data = &s3c2410_mmc_defplat;
		pdata = &s3c2410_mmc_defplat;
	}

	host->pdata = pdata;

	host->irq_cd = s3c2410_gpio_getirq(pdata->gpio_detect);
	s3c2410_gpio_cfgpin(pdata->gpio_detect, S3C2410_GPIO_IRQ);

	host->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!host->mem) {
		printk(KERN_ERR PFX "failed to get io memory region resouce.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}

	host->mem = request_mem_region(host->mem->start,
		RESSIZE(host->mem), pdev->name);

	if (!host->mem) {
		printk(KERN_ERR PFX "failed to request io memory region.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}

	host->base = ioremap(host->mem->start, RESSIZE(host->mem));
	if (host->base == 0) {
		printk(KERN_ERR PFX "failed to ioremap() io memory region.\n");
		ret = -EINVAL;
		goto probe_free_mem_region;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq == 0) {
		printk(KERN_ERR PFX "failed to get interrupt resouce.\n");
		ret = -EINVAL;
		goto probe_iounmap;
	}

	if(request_irq(host->irq, s3c2410sdi_irq, 0, DRIVER_NAME, host)) {
		printk(KERN_ERR PFX "failed to request sdi interrupt.\n");
		ret = -ENOENT;
		goto probe_iounmap;
	}

	set_irq_type(host->irq_cd, IRQT_BOTHEDGE);
	if(request_irq(host->irq_cd, s3c2410sdi_irq_cd, 0, DRIVER_NAME, host)) {
		printk(KERN_ERR PFX "failed to request card detect interrupt.\n" );
		ret = -ENOENT;
		goto probe_free_irq;
	}

	if(s3c2410_dma_request(S3C2410SDI_DMA, &s3c2410sdi_dma_client, NULL)) {
		printk(KERN_ERR PFX "unable to get DMA channel.\n" );
		ret = -EBUSY;
		goto probe_free_irq_cd;
	}

	host->clk = clk_get(&pdev->dev, "sdi");
	if (IS_ERR(host->clk)) {
		printk(KERN_ERR PFX "failed to find clock source.\n");
		ret = PTR_ERR(host->clk);
		host->clk = NULL;
		goto probe_free_host;
	}

	if((ret = clk_enable(host->clk))) {
		printk(KERN_ERR PFX "failed to enable clock source.\n");
		goto clk_unuse;
	}


	mmc->ops 	= &s3c2410sdi_ops;
	mmc->ocr_avail	= pdata->ocr_avail;
	mmc->f_min 	= clk_get_rate(host->clk) / 512;
	mmc->f_max 	= clk_get_rate(host->clk) / 2;
	mmc->caps	= MMC_CAP_4_BIT_DATA | MMC_CAP_MULTIWRITE | MMC_CAP_BYTEBLOCK;

	if(pdata->f_max && (mmc->f_max>pdata->f_max))
		mmc->f_max = pdata->f_max;

	/*
	 * Set the maximum segment size.  Since we aren't doing DMA
	 * (yet) we are only limited by the data length register.
	 */

	mmc->max_blk_count = 4096;
	mmc->max_blk_size = 64;
	mmc->max_req_size = mmc->max_blk_size * 512; /* 512B per sector */
	mmc->max_seg_size = mmc->max_req_size;

	printk(KERN_INFO PFX "probe: mapped sdi_base=%p irq=%u irq_cd=%u \n",
		host->base, host->irq, host->irq_cd);

	if((ret = mmc_add_host(mmc))) {
		printk(KERN_ERR PFX "failed to add mmc host.\n");
		goto clk_disable;
	}

	platform_set_drvdata(pdev, mmc);

	printk(KERN_INFO PFX "initialisation done.\n");
	return 0;
	
 clk_disable:
	clk_disable(host->clk);

 clk_unuse:
	clk_put(host->clk);

 probe_free_irq_cd:
 	free_irq(host->irq_cd, host);

 probe_free_irq:
 	free_irq(host->irq, host);

 probe_iounmap:
	iounmap(host->base);

 probe_free_mem_region:
	release_mem_region(host->mem->start, RESSIZE(host->mem));

 probe_free_host:
	mmc_free_host(mmc);
 probe_out:
	return ret;
}

static int s3c2410sdi_remove(struct platform_device *pdev)
{
	struct mmc_host 	*mmc  = platform_get_drvdata(pdev);
	struct s3c2410sdi_host 	*host = mmc_priv(mmc);

	mmc_remove_host(mmc);
	s3c2410_dma_free(S3C2410SDI_DMA, &s3c2410sdi_dma_client);
	clk_disable(host->clk);
	clk_put(host->clk);
 	free_irq(host->irq_cd, host);
 	free_irq(host->irq, host);
	iounmap(host->base);
	release_mem_region(host->mem->start, RESSIZE(host->mem));
	mmc_free_host(mmc);

	return 0;
}

#ifdef CONFIG_PM
static int s3c2410mci_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct s3c2410sdi_host  *host;
	int ret = 0;

	if (mmc) {
		host = mmc_priv(mmc);

		ret = mmc_suspend_host(mmc, state);

		clk_disable(host->clk);

		disable_irq(host->irq_cd);
		disable_irq(host->irq);
	}

	return ret;
}

static int s3c2410mci_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct s3c2410sdi_host  *host;
	int ret = 0;

	if (mmc) {
		host = mmc_priv(mmc);

		enable_irq(host->irq_cd);
		enable_irq(host->irq);

		clk_enable(host->clk);

		ret = mmc_resume_host(mmc);
	}

	return ret;
}
#else
#define s3c2410mci_suspend	NULL
#define s3c2410mci_resume	NULL
#endif

static struct platform_driver s3c2410sdi_driver =
{
	.driver		= {
        	.name	= "s3c2410-sdi",
		.owner	= THIS_MODULE,
	},
        .probe          = s3c2410sdi_probe,
        .remove         = s3c2410sdi_remove,
	.suspend	= s3c2410mci_suspend,
	.resume		= s3c2410mci_resume,
};

static int __init s3c2410sdi_init(void)
{
	return platform_driver_register(&s3c2410sdi_driver);
}

static void __exit s3c2410sdi_exit(void)
{
	platform_driver_unregister(&s3c2410sdi_driver);
}

module_init(s3c2410sdi_init);
module_exit(s3c2410sdi_exit);

MODULE_DESCRIPTION("Samsung S3C2410 Multimedia Card Interface driver");
MODULE_LICENSE("GPL");
