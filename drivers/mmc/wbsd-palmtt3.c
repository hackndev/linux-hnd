/*
 *  linux/drivers/mmc/wbsd-palmt3.c - Winbond W86L488 SD/MMC driver on Palm T|T3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * FIXME:
 * ACMD51 have no data responce - don't know why
 * led blinking is broken
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/blkdev.h>
#include <linux/mmc/protocol.h>
#include <linux/leds.h>

#include <linux/highmem.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/scatterlist.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/tps65010.h>

#include "wbsd-palmtt3.h"

#define DRIVER_NAME "wbsd-palmt3"
#define DRIVER_VERSION "0.10"

#define DMA_INCOMPLETE 0

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...) \
	printk(KERN_ERR DRIVER_NAME ": " x)
#define DBGF(f, x...) \
	printk(KERN_ERR DRIVER_NAME " [%s()]: " f, __func__ , ##x)
#else
#define DBG(x...)	do { } while (0)
#define DBGF(x...)	do { } while (0)
#endif

/*
 * Device resources
 */

struct wbsd_host* host;

/*
 * Basic functions
 */

static inline void wbsd_write_idx_reg(u8 index, u16 value)
{
	SET_IDX_ADDR(index);
	SET_IDX_DATA(value);
}

static inline u16 wbsd_read_idx_reg(u8 index)
{
	SET_IDX_ADDR(index);
	return RREADW(WBR_IDR0);
}

static inline void wbsd_clean_int(void)
{
	u16 tmp;

	tmp = RREADW(WBR_IER);
	RWRITEW(WBR_IER, tmp);
}

static inline void wbsd_clean_transfer(void)
{
	u16 tmp;

	tmp = RREADW(WBR_CR);
	RWRITEW(WBR_CR, tmp | WBSD_CLEAN_TR_INT);
}

static inline void wbsd_clean_fifo(void)
{
	u16 tmp;

	tmp = RREADW(WBR_CR);
	RWRITEW(WBR_CR, tmp | WBSD_RESET_FIFO);
}

static inline struct mmc_data* wbsd_get_data(void)
{
	WARN_ON(!host->mrq);
	if (!host->mrq)
		return NULL;

	WARN_ON(!host->mrq->cmd);
	if (!host->mrq->cmd)
		return NULL;

	if (!host->mrq->cmd->data)
		return NULL;

	return host->mrq->cmd->data;
}

/*
 * Common routines
 */

static void wbsd_init_device(void)
{
	u16 ier;
	
	/*
	 * Data communication 16 bit
	 */
	wbsd_write_idx_reg(WBIR_DLR, WBSD_MODE_16b);

	/*
	 * Reset chip (SD/MMC part) and fifo.
	 */
	RWRITEW(WBR_CR, WBSD_RESET_CHIP | WBSD_RESET_FIFO);

	host->clk = 0x0000; // SIEN and clock turned off

	/*
	 * Power down port.
	 */
	RWRITEW(WBR_CR, host->clk | WBSD_POWER_DOWN);


	/*
	 * Disable any GPIO and GPIO interrupts
	 */
	RWRITEW(WBR_GPC, 0x0000);
	RWRITEW(WBR_GPIE, 0x0000);

	/*
	 * Set width 1
	 */
	wbsd_write_idx_reg(WBIR_MDFR, WBSD_WIDTH_1);

	/*
	 * Set CRC calculation and Ner timeout
	 */
	wbsd_write_idx_reg(WBIR_CR, WBSD_NER_TO_MAX | WBSD_CMD_CRC);
	/*
	 * Set maximum timeout.
	 */
	wbsd_write_idx_reg(WBIR_NATO, WBSD_NATO_MAX);

	/*
	 * Test for card presence
	 */
	if (!(GPLR(2) & GPIO_bit(2)))
		host->flags |= WBSD_FCARD_PRESENT;
	else
		host->flags &= ~WBSD_FCARD_PRESENT;

	/*
	 * Enable interesting interrupts, and clear
	 */
	ier = 0xFF00; // clean by writting
	ier |= WBSD_EINT_INT;
	ier |= WBSD_EINT_TOE;
	ier |= WBSD_EINT_CRC;
	ier |= WBSD_EINT_DREQ;
	ier |= WBSD_EINT_PRGE;
	ier |= WBSD_EINT_CMDE;

	RWRITEW(WBR_IER, ier);
	wbsd_clean_transfer();
	wbsd_clean_fifo();
}

static void wbsd_reset(void)
{
	u16 setup;

	printk(KERN_ERR "%s: Resetting chip\n", mmc_hostname(host->mmc));

	/*
	 * Soft reset of chip (SD/MMC part).
	 */
	setup = RREADW(WBR_CR);
	setup |= WBSD_RESET_CHIP | WBSD_RESET_FIFO;
	RWRITEW(WBR_CR, setup);

}

static void wbsd_request_end(struct mmc_request* mrq)
{
	struct mmc_data* data;

	DBGF("Ending request, cmd (%x)\n", mrq->cmd->opcode);

#if DMA_INCOMPLETE
	unsigned long dmaflags;
	if (host->dma >= 0)
	{
		/*
		 * Release DMA controller.
		 */
		dmaflags = claim_dma_lock();
		disable_dma(host->dma);
		clear_dma_ff(host->dma);
		release_dma_lock(dmaflags);

		/*
		 * Disable DMA on host.
		 */

		tmp = RREADW(WBR_IER);
		tmp &= ~0x0020;
		RWRITEW(WBR_IER, tmp);

		tmp = wbsd_read_wb_index(WBIR_CR);
		tmp &= ~0x0040;
		wbsd_write_wb_index(WBIR_CR, tmp);
	}

#endif //DMA_INCOMPLETE
	data = wbsd_get_data();
	if (data)
		if(data->error != MMC_ERR_NONE)
			data->bytes_xfered = 0;

	host->mrq = NULL;

	/*
	 * MMC layer might call back into the driver so first unlock.
	 */
	spin_unlock(&host->lock);
	mmc_request_done(host->mmc, mrq);

	host->tps_duty |= TPS_LED_OFF;
	host->tps_duty &= ~TPS_LED_ON;
	schedule_work(&host->tps_work);

	spin_lock(&host->lock);

}

/*
 * Scatter/gather functions
 */

static inline void wbsd_init_sg(struct mmc_data* data)
{
	/*
	 * Get info. about SG list from data structure.
	 */
	host->cur_sg = data->sg;
	host->num_sg = data->sg_len;

	host->offset = 0;
	host->remain = host->cur_sg->length;
}

static inline int wbsd_next_sg(void)
{
	/*
	 * Skip to next SG entry.
	 */
	host->cur_sg++;
	host->num_sg--;

	/*
	 * Any entries left?
	 */
	if (host->num_sg > 0)
	  {
	    host->offset = 0;
	    host->remain = host->cur_sg->length;
	  }

	return host->num_sg;
}

static inline char* wbsd_kmap_sg(void)
{
	host->mapped_sg = kmap_atomic(host->cur_sg->page, KM_BIO_SRC_IRQ) +
		host->cur_sg->offset;
	return host->mapped_sg;
}

static inline void wbsd_kunmap_sg(void)
{
	kunmap_atomic(host->mapped_sg, KM_BIO_SRC_IRQ);
}

#if DMA_INCOMPLETE
static inline void wbsd_sg_to_dma(struct mmc_data* data)
{
	unsigned int len, i, size;
	struct scatterlist* sg;
	char* dmabuf = host->dma_buffer;
	char* sgbuf;

	size = host->size;

	sg = data->sg;
	len = data->sg_len;

	/*
	 * Just loop through all entries. Size might not
	 * be the entire list though so make sure that
	 * we do not transfer too much.
	 */
	for (i = 0;i < len;i++)
	{
		sgbuf = kmap_atomic(sg[i].page, KM_BIO_SRC_IRQ) + sg[i].offset;
		if (size < sg[i].length)
			memcpy(dmabuf, sgbuf, size);
		else
			memcpy(dmabuf, sgbuf, sg[i].length);
		kunmap_atomic(sgbuf, KM_BIO_SRC_IRQ);
		dmabuf += sg[i].length;

		if (size < sg[i].length)
			size = 0;
		else
			size -= sg[i].length;

		if (size == 0)
			break;
	}

	/*
	 * Check that we didn't get a REQUEst to transfer
	 * more data than can fit into the SG list.
	 */

	BUG_ON(size != 0);

	host->size -= size;
}

static inline void wbsd_dma_to_sg(struct mmc_data* data)
{
	unsigned int len, i, size;
	struct scatterlist* sg;
	char* dmabuf = host->dma_buffer;
	char* sgbuf;

	size = host->size;

	sg = data->sg;
	len = data->sg_len;

	/*
	 * Just loop through all entries. Size might not
	 * be the entire list though so make sure that
	 * we do not transfer too much.
	 */
	for (i = 0;i < len;i++)
	{
		sgbuf = kmap_atomic(sg[i].page, KM_BIO_SRC_IRQ) + sg[i].offset;
		if (size < sg[i].length)
			memcpy(sgbuf, dmabuf, size);
		else
			memcpy(sgbuf, dmabuf, sg[i].length);
		kunmap_atomic(sgbuf, KM_BIO_SRC_IRQ);
		dmabuf += sg[i].length;

		if (size < sg[i].length)
			size = 0;
		else
			size -= sg[i].length;

		if (size == 0)
			break;
	}

	/*
	 * Check that we didn't get a request to transfer
	 * more data than can fit into the SG list.
	 */

	BUG_ON(size != 0);

	host->size -= size;
}
#endif //DMA_INCOMPLETE

/*
 * Command handling
 */

static inline void wbsd_get_short_reply(struct mmc_command* cmd)
{
	/*
	 * Correct response type?
	 */
	if ((RREADW(WBR_CR) & WBSD_RESP_R2))
	{
		cmd->error = MMC_ERR_INVALID;
		return;
	}

	cmd->resp[0] = RREADW(WBR_CMDR0) << 24;
	cmd->resp[0] |= RREADW(WBR_CMDR0) << 8;
	cmd->resp[0] |= RREADW(WBR_CMDR0) >> 8;
}

static inline void wbsd_get_long_reply(struct mmc_command* cmd)
{
	u8 i;
	u16 dat;

	/*
	 * Correct response type?
	 */
	if (!(RREADW(WBR_CR) & WBSD_RESP_R2))
	{
		cmd->error = MMC_ERR_INVALID;
		return;
	}

	dat = RREADW(WBR_CMDR0);
	for(i = 0; i < 4; i++)
	{
		cmd->resp[i] = dat << 24;

		dat = RREADW(WBR_CMDR0);
		cmd->resp[i] |= dat << 8;

		dat = RREADW(WBR_CMDR0);
		cmd->resp[i] |= dat >> 8;
	}
}

static void wbsd_send_command(struct mmc_command* cmd)
{
	DBGF("Sending cmd %x arg: %X\n", cmd->opcode, cmd->arg);

	/*
	 * Clear accumulated ISR. The interrupt routine
	 * will fill this one with events that occur during
	 * transfer.
	 */
	wbsd_clean_int();
	wbsd_clean_transfer();

	/*
	 * Send the command (CRC calculated by host).
	 */
	RWRITEW(WBR_CMDR0, (cmd->opcode << 8) | (cmd->arg >> 24) | 0x4000); // 01b is heading
	RWRITEW(WBR_CMDR0, cmd->arg >> 8);
	RWRITEW(WBR_CMDR0, (cmd->arg << 8) | 1 ); // 1b is trailing

	cmd->error = MMC_ERR_NONE;
}

/*
 * Data functions
 */

static void wbsd_empty_fifo(void)
{
	struct mmc_data* data = host->mrq->cmd->data;
	char* buffer;
	u16 dat;
	u8 fifo;

	/*
	 * Handle excessive data.
	 */
	if (data->bytes_xfered >= host->size)
		return;

	buffer = wbsd_kmap_sg() + host->offset;

	SET_IDX_ADDR(WBIR_DLR); // Heading repeatly during transfer

	while (!(RREADW(WBR_CR) & WBSD_FIFO_EMPTY)) //Not Empty
	{
		fifo = 1;

		for (;;)
		{
			if(fifo < 3)
			{
				fifo = (GET_IDX_DATA() & ~WBSD_DLR_BLV) >> 8;
				if(fifo == 0)
					break;

			}

			dat = RREADW(WBR_RTB0);
			
			if(host->remain > 1)
			{
				*buffer = dat >> 8;
				*(buffer + 1) = dat & 0x00FF;
				buffer += 2;
				host->offset += 2;
				host->remain -= 2;
				data->bytes_xfered += 2;
			}
			else
			{
				*buffer = dat >> 8;
				buffer++;
				host->offset++;
				host->remain--;
				data->bytes_xfered++;
			}

			fifo--;

			/*
			 * Transfer done?
			 */
			if (data->bytes_xfered >= host->size)
			{
				wbsd_kunmap_sg();
				return;
			}

			/*
			 * End of scatter list entry?
			 */
			if (host->remain == 0)
			{
				wbsd_kunmap_sg();

				/*
				 * Get next entry. Check if last.
				 */
				if (!wbsd_next_sg())
				{
					/*
					 * We should never reach this point.
					 * It means that we're trying to
					 * transfer more blocks than can fit
					 * into the scatter list.
					 */
					BUG_ON(1);

					host->size = data->bytes_xfered;

					return;
				}

				buffer = wbsd_kmap_sg();
			}
		}
	}

	wbsd_kunmap_sg();
}

static void wbsd_fill_fifo(void)
{
	struct mmc_data* data = host->mrq->cmd->data;
	char* buffer;
	u8 fifo;
	u16 dat, ier;

	/*
	 * Check that we aren't being called after the
	 * entire buffer has been transfered.
	 */
	if (data->bytes_xfered >= host->size)
		return;

	buffer = wbsd_kmap_sg() + host->offset;

	SET_IDX_ADDR(WBIR_DLR); //Reading repeatly during transfer

	while (!(RREADW(WBR_CR) & WBSD_FIFO_FULL))// Not full
	{
		fifo = 1;

		for (;;)
		{
			if(fifo < 3)
			{
				fifo = (GET_IDX_DATA() & ~WBSD_DLR_BLV) >> 8;
				if(fifo == 0)
					break;
			}

			if(host->remain > 1)
			{
				dat = *buffer << 8;
				dat |= *(buffer + 1);
				buffer += 2;
				host->offset += 2;
				host->remain -= 2;
				data->bytes_xfered += 2;

			}
			else
			{
				dat = *buffer << 8;
				buffer++;
				host->offset++;
				host->remain--;
				data->bytes_xfered++;
			}
			RWRITEW(WBR_RTB0, dat);

			fifo--;

			if(!(data->bytes_xfered % 512))
			{
				for(ier = 0; !(ier & (WBSD_INT_PRGE | WBSD_INT_CRC));)
					ier = RREADW(WBR_IER);

				if(ier & WBSD_INT_CRC)
				{
					data->error = MMC_ERR_BADCRC;
					data->bytes_xfered = host->size;
				}
			}

			/*
			 * Transfer done?
			 */
			if (data->bytes_xfered >= host->size)
			{
				wbsd_kunmap_sg();
				return;
			}

			/*
			 * End of scatter list entry?
			 */
			if (host->remain == 0)
			{
				wbsd_kunmap_sg();

				/*
				 * Get next entry. Check if last.
				 */
				if (!wbsd_next_sg())
				{
					/*
					 * We should never reach this point.
					 * It means that we're trying to
					 * transfer more blocks than can fit
					 * into the scatter list.
					 */
					BUG_ON(1);

					host->size = data->bytes_xfered;

					return;
				}

				buffer = wbsd_kmap_sg();
			}
		}
		/*for(ier = 0, dat = 0; (dat < 1000) && !(ier & 0x4000); dat++)
			ier = RREADW(WBR_IER);

		printk("PRGE at %d\n", fifo);*/
	}

	wbsd_kunmap_sg();
}

static void wbsd_prepare_data(struct mmc_data* data)
{
	DBGF("blksz %04x blks %04x flags %08x\n",
		data->blksz, data->blocks, data->flags);
	DBGF("tsac %d ms nsac %d clk\n",
		data->timeout_ns / 1000000, data->timeout_clks);

	/*
	 * Calculate size.
	 */
	host->size = data->blocks * data->blksz;

	/*
	 * Check timeout values for overflow.
	 * (Yes, some cards cause this value to overflow).
	 */
	//wbsd_write_wb_index(WBIR_NATO, 0x7FFF);
	/*if (data->timeout_ns > 127000000)
		wbsd_write_wb_index(WBIR_NATO, 127);
	else
		wbsd_write_wb_index(WBIR_NATO, data->timeout_ns/1000000);*/

		//FIXME - dont use
	/*if (data->timeout_clks > 255)
		wbsd_write_wb_index(WBIR_NATO, 255);
	else
		wbsd_write_wb_index(WBIR_NATO, data->timeout_clks);*/

	/*
	 * Inform the chip of how large blocks will be
	 * sent. It needs this to determine when to
	 * calculate CRC.
	 */
	wbsd_write_idx_reg(WBIR_MDFR, (data->blksz & 0x0FFF) | 0x0000);

	//wbsd_write_wb_index(WBIR_MDFR, 0x0008);
	//wbsd_write_wb_index(WBIR_SDFR, 0x0008);

	wbsd_write_idx_reg(WBIR_MDCB, data->blocks & 0x01FF);
	//wbsd_write_wb_index(WBIR_SDCB, data->blocks & 0x01FF);

	//wbsd_write_wb_index(WBIR_MDFR +1, 0xf001);
	//wbsd_write_wb_index(WBIR_SDFR +1, 0xf001);

	/*
	 * Clear the FIFO. This is needed even for DMA
	 * transfers since the chip still uses the FIFO
	 * internally.
	 */
	wbsd_clean_fifo();

	/*ier = RREADW(WBR_IER);
	ier |= 0x00ff;
	RWRITEW(WBR_IER, ier);*/

#if DMA_INCOMPLETE
	/*
	 * DMA transfer?
	 */
	if (host->dma >= 0)
	{
		printk("DMA\n");
		/*
		 * The buffer for DMA is only 64 kB.
		 */
		BUG_ON(host->size > 0x10000);
		if (host->size > 0x10000)
		{
			data->error = MMC_ERR_INVALID;
			return;
		}

		/*
		 * Transfer data from the SG list to
		 * the DMA buffer.
		 */
		if (data->flags & MMC_DATA_WRITE)
			wbsd_sg_to_dma(data);

		/*
		 * Initialise the ISA DMA controller.
		 */
		dmaflags = claim_dma_lock();
		disable_dma(host->dma);
		clear_dma_ff(host->dma);
		if (data->flags & MMC_DATA_READ)
			set_dma_mode(host->dma, DMA_MODE_READ & ~0x40);
		else
			set_dma_mode(host->dma, DMA_MODE_WRITE & ~0x40);
		set_dma_addr(host->dma, host->dma_addr);
		set_dma_count(host->dma, host->size);

		enable_dma(host->dma);
		release_dma_lock(dmaflags);

		/*
		 * Enable DMA on the host.
		 */
		wbsd_write_wb_index(WBIR_CR, 0x00f1);
	}
	else
	{
#endif //DMA_INCOMPLETE
		/*
		 * Initialise the SG list.
		 */
		wbsd_init_sg(data);

		/*
		 * Turn off DMA.
		 */
		/*setup = wbsd_read_wb_index(WBIR_CR);
		setup &= ~0x0080;
		wbsd_write_wb_index(WBIR_CR, setup);*/
#if DMA_INCOMPLETE
	}
#endif

	data->error = MMC_ERR_NONE;
}

static void wbsd_finish_data(struct mmc_data* data)
{
	WARN_ON(host->mrq == NULL);

	/*
	 * Send a stop command if needed.
	 */
	if (data->stop)
	{
		wbsd_send_command(data->stop);
		data->stop->opcode = 0;
	}

	/*
	 * Wait for the controller to leave data
	 * transfer state.
	 */
	/* this one is broken in single block transfer
	do {
		status = RREADW(WBR_CR);
	} while (status & 0x0300);
	*/
	
#if DMA_INCOMPLETE
	unsigned long dmaflags;
	int count;
	/*
	 * DMA transfer?
	 */
	if (host->dma >= 0)
	{
		/*
		 * Disable DMA on the host.
		 */
		//wbsd_write_wb_index(WBIR_CR, 0x0000);

		/*
		 * Turn off ISA DMA controller.
		 */
		dmaflags = claim_dma_lock();
		disable_dma(host->dma);
		clear_dma_ff(host->dma);
		count = get_dma_residue(host->dma);
		release_dma_lock(dmaflags);

		/*
		 * Any leftover data?
		 */
		if (count)
		{
			printk(KERN_ERR "%s: Incomplete DMA transfer. "
				"%d bytes left.\n",
				mmc_hostname(host->mmc), count);

			data->error = MMC_ERR_FAILED;
		}
		else
		{
			/*
			 * Transfer data from DMA buffer to
			 * SG list.
			 */
			if (data->flags & MMC_DATA_READ)
				wbsd_dma_to_sg(data);

			data->bytes_xfered = host->size;
		}
	}
#endif //DMA_INCOMPLETE

	DBGF("Ending data transfer (%d bytes)\n", data->bytes_xfered);
}

/*****************************************************************************\
 *                                                                           *
 * MMC layer callbacks                                                       *
 *                                                                           *
\*****************************************************************************/

static void wbsd_request(struct mmc_host* mmc, struct mmc_request* mrq)
{
	struct mmc_command* cmd;
	u16 esr;

	/*
	 * Disable tasklets to avoid a deadlock.
	 */

	host->tps_duty |= TPS_LED_ON;
	host->tps_duty &= ~TPS_LED_OFF;
	schedule_work(&host->tps_work);

	spin_lock_bh(&host->lock);

	BUG_ON(host->mrq != NULL);

	cmd = mrq->cmd;

	host->mrq = mrq;

	cmd->error = MMC_ERR_NONE;

	/*
	 * If there is no card in the slot then
	 * timeout immediatly.
	 */
	if (!(host->flags & WBSD_FCARD_PRESENT))
	{
		cmd->error = MMC_ERR_TIMEOUT;
		wbsd_request_end(mrq);
		spin_unlock(&host->lock);
		goto done;
	}

	/*
	 * Tell the chip if CRC is expected
	 */

	SET_IDX_ADDR(WBIR_CR);
	esr = GET_IDX_DATA();
	if(cmd->flags & MMC_RSP_CRC)
		esr &= ~WBSD_CMD_NOCRC;
	else
		esr |= WBSD_CMD_NOCRC;

	SET_IDX_DATA(esr);

	/*
	 * Does the request include data?
	 */
	if (cmd->data)
	{
		wbsd_prepare_data(cmd->data);
		tasklet_schedule(&host->fifo_tasklet); // because of CMD 51

		if (cmd->data->error != MMC_ERR_NONE)
			goto done;
	}

	wbsd_send_command(cmd);
done:
	spin_unlock_bh(&host->lock);
}

static void wbsd_set_ios(struct mmc_host* mmc, struct mmc_ios* ios)
{
	u16 clk, pwr;
	//u16 setup;

	DBGF("clock %uHz busmode %u powermode %u cs %u Vdd %u width %u\n",
	     ios->clock, ios->bus_mode, ios->power_mode, ios->chip_select,
	     ios->vdd, ios->bus_width);

	spin_lock_bh(&host->lock);

	/*
	 * Reset the chip on each power off.
	 * Should clear out any weird states.
	 * Or Power Up chip.	 
	 */
	if (ios->power_mode == MMC_POWER_OFF)
		wbsd_init_device();
	else if(ios->power_mode == MMC_POWER_UP)
	{
    		pwr = RREADW(WBR_CR);
		pwr &= ~WBSD_POWER_DOWN;
		RWRITEW(WBR_CR, pwr);  
  	}

	if (ios->clock >= 20000000)
		clk = 0x001F | WBSD_SIEN;
	else if (ios->clock >= 16000000)
		clk = 0x000A | WBSD_SIEN;
	else if (ios->clock >= 12000000)
		clk = 0x0008 | WBSD_SIEN;
	else if (ios->clock >= 375000)
		clk = 0x0003 | WBSD_SIEN;
	else
		clk = 0; //shut down

	/*
	 * Only write to the clock register when
	 * there is an actual change.
	 */
	if (clk != host->clk)
	{
		host->clk = clk;
		clk = (RREADW(WBR_CR) & (WBSD_POWER_DOWN | WBSD_RESET_FIFO | WBSD_RESET_CHIP)) | clk;
		RWRITEW(WBR_CR, clk);

		wbsd_clean_int();
		wbsd_clean_transfer();
	}

	pwr = wbsd_read_idx_reg(WBIR_CR);

	if(mmc->mode & MMC_MODE_MMC)
		wbsd_write_idx_reg(WBIR_CR, pwr | WBSD_MODE_MMC);
	else
		wbsd_write_idx_reg(WBIR_CR, pwr & ~WBSD_MODE_MMC);

	/*
	 * We are just able to set CS high with sending CMD0
	 * but in that case card switchs to SPI mode instead of MMC
	 * which is slower, so we better don't care about CS
	 */
	/*setup = wbsd_read_wb_index(WBIR_CR);
	if (ios->chip_select == MMC_CS_HIGH)
	{
		BUG_ON(ios->bus_width != MMC_BUS_WIDTH_1);
		setup |= 0x0008;
	}
	wbsd_write_wb_index(WBIR_CR, setup);*/

	/*
	 * Store bus width for later. Will be used when
	 * setting up the data transfer.
	 */
	host->bus_width = ios->bus_width;

	spin_unlock_bh(&host->lock);
}

static int wbsd_get_ro(struct mmc_host* mmc)
{
	u16 gpd;

	spin_lock_bh(&host->lock);

	gpd = RREADW(WBR_GPD) & 0x0002;

	spin_unlock_bh(&host->lock);

	return !gpd;
}

static struct mmc_host_ops wbsd_ops = {
	.request	= wbsd_request,
	.set_ios	= wbsd_set_ios,
	.get_ro		= wbsd_get_ro,
};

/*****************************************************************************\
 *                                                                           *
 * Interrupt handling                                                        *
 *                                                                           *
\*****************************************************************************/

/*
 * Tasklets
 */

static void wbsd_tasklet_card(unsigned long param)
{
	int delay = -1;

	spin_lock(&host->lock);

	if (!(GPLR(2) & GPIO_bit(2))) //Card Present
	{
		if (!(host->flags & WBSD_FCARD_PRESENT))
		{
			DBG("Card inserted\n");
			host->flags |= WBSD_FCARD_PRESENT;

			delay = 1500;

			//Power up card
			host->tps_duty |= TPS_POWER_OFF;
			host->tps_duty &= ~TPS_POWER_ON;
			schedule_work(&host->tps_work);
		}
	}
	else if (host->flags & WBSD_FCARD_PRESENT)
	{
		DBG("Card removed\n");
		host->flags &= ~WBSD_FCARD_PRESENT;

		if (host->mrq)
		{
			printk(KERN_ERR "%s: Card removed during transfer!\n",
				mmc_hostname(host->mmc));
			wbsd_reset();

			host->mrq->cmd->error = MMC_ERR_FAILED;
			wbsd_request_end(host->mrq);
		}

		delay = 0;

		//Power down card
		host->tps_duty |= TPS_POWER_OFF;
		host->tps_duty &= ~TPS_POWER_ON;
		schedule_work(&host->tps_work);
	}

	/*
	 * Unlock first since we might get a call back.
	 */

	spin_unlock(&host->lock);

	if (delay != -1)
		mmc_detect_change(host->mmc, msecs_to_jiffies(delay));
}

static void wbsd_delayed_fifo(unsigned long unused)
{
	struct mmc_data* data;
	static int count = 0;

	count++;

	spin_lock_bh(&host->lock);

	if (!host->mrq)
		goto end;

	data = wbsd_get_data();
	if (!data)
		goto end;

	if(data->error != MMC_ERR_NONE)
		data->bytes_xfered = host->size;

	if (data->flags & MMC_DATA_WRITE)
		wbsd_fill_fifo();
	else
		wbsd_empty_fifo();

	if(count >= 20)
	{
		printk("WBSD: Shouldn't happend, cmd: %d\n", host->mrq->cmd->opcode);

		data->bytes_xfered = host->size;
		count = 0;
	}

	/*
	 * Done?
	 */
	if (host->size <= data->bytes_xfered)
	{
		if(data->stop)
		{
			if(data->stop->opcode == 0)
			{
				wbsd_get_short_reply(data->stop);
				wbsd_request_end(host->mrq);
				wbsd_clean_int();
			}
			else
			{
				wbsd_finish_data(data);
				tasklet_schedule(&host->fifo_tasklet);
			}
		}
		else
		{
			wbsd_finish_data(data);
			wbsd_request_end(host->mrq);
		}

		count = 0;
	}
	else
		tasklet_schedule(&host->fifo_tasklet);

end:
	spin_unlock_bh(&host->lock);
}


static void wbsd_tasklet_fifo(unsigned long param)
{
	spin_lock(&host->lock);

	mod_timer(&host->delayed_timer, jiffies + 5);

	spin_unlock(&host->lock);

}

static void wbsd_tasklet_crc(unsigned long param)
{
	struct mmc_data* data;

	spin_lock(&host->lock);

	if (!host->mrq)
		goto end;

	data = wbsd_get_data();
	if (!data)
		goto end;

	data->error = MMC_ERR_BADCRC;

	host->mrq->cmd->error = MMC_ERR_FAILED;

end:
	tasklet_schedule(&host->finish_tasklet);

	spin_unlock(&host->lock);
}

static void wbsd_tasklet_timeout(unsigned long param)
{
	struct mmc_data* data;

	spin_lock(&host->lock);

	if (!host->mrq)
		goto end;

	if(host->mrq->cmd)
		host->mrq->cmd->error = MMC_ERR_TIMEOUT;

	data = wbsd_get_data();
	if (!data)
		goto end;

	data->error = MMC_ERR_TIMEOUT;

end:
	tasklet_schedule(&host->finish_tasklet);

	spin_unlock(&host->lock);
}

static void wbsd_tasklet_finish(unsigned long param)
{
	struct mmc_data* data;

	spin_lock(&host->lock);

	if (!host->mrq)
		goto end;

	data = wbsd_get_data();

	if (!((data) && (data->stop) && (data->stop->opcode == 0)) &&
	(host->mrq->cmd->error == MMC_ERR_NONE) )
		if (host->mrq->cmd->flags & MMC_RSP_PRESENT)
		{
			if (host->mrq->cmd->flags & MMC_RSP_136)
				wbsd_get_long_reply(host->mrq->cmd);
			else
				wbsd_get_short_reply(host->mrq->cmd);
		}

	wbsd_clean_int();

	if (data)
	{
		tasklet_schedule(&host->fifo_tasklet); //fail safe - needed
		spin_unlock(&host->lock);
		return;
	}

	wbsd_request_end(host->mrq);
end:
	spin_unlock(&host->lock);
}

static void wbsd_tasklet_tps(struct work_struct *work)
{
	if(host->tps_duty & TPS_POWER_OFF)
	{
		DBG("Powering down card\n");
		tps65010_set_gpio_out_value(GPIO3, LOW);
	}
	else if(host->tps_duty & TPS_POWER_ON)
	{
		DBG("Powering card up\n");
		tps65010_set_gpio_out_value(GPIO3, HIGH);
	}

	if(host->tps_duty & TPS_LED_ON)
	{
		DBG("Blinking led\n");
		ledtrig_mmc_activity();
	}

	host->tps_duty = 0;
}

/*
 * Interrupt handling
 */

static irqreturn_t wbsd_irq(int irq, void *dev_id)
{
	u16 isr;

	if (((int) dev_id) == 2)
	{
		tasklet_schedule(&host->card_tasklet);
		return IRQ_HANDLED;
	}

	if(((int)(dev_id)) != 8)
		printk(KERN_ERR "PANIC PANIC PANIC INTERRUPT INTERRUPT %d\n", (int) dev_id);

	isr = RREADW(WBR_IER);

	if(isr & ~0xEDFF)
		printk(KERN_ERR "Interrupt %d, int reg %04X						INT\n", (int) dev_id, isr);

	/*
	 * Was it actually our hardware that caused the interrupt?
	 */
	if (!(isr & 0xFF00))
		return IRQ_NONE;

	/*
	 * Schedule tasklets as needed.
	 */

	if (isr & WBSD_INT_DREQ)
		tasklet_schedule(&host->fifo_tasklet);

	if (isr & WBSD_INT_TOE)
		tasklet_hi_schedule(&host->timeout_tasklet);

	if (isr & WBSD_INT_CRC)
		tasklet_hi_schedule(&host->crc_tasklet);

	if (isr & WBSD_INT_CMDE)
		tasklet_schedule(&host->finish_tasklet);

	return IRQ_HANDLED;
}

/*****************************************************************************\
 *                                                                           *
 * Device initialisation and shutdown                                        *
 *                                                                           *
\*****************************************************************************/

/*
 * Allocate/free MMC structure.
 */

static int __devinit wbsd_alloc_mmc(struct device* dev)
{
	struct mmc_host* mmc;

	/*
	 * Allocate MMC structure.
	 */
	mmc = mmc_alloc_host(sizeof(struct wbsd_host), dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	host->mmc = mmc;

	host->dma = -1;

	/*
	 * Set host parameters.
	 */
	mmc->ops = &wbsd_ops;
	mmc->f_min = CLOCKRATE_MIN;
	mmc->f_max = CLOCKRATE_MAX;
	mmc->ocr_avail = MMC_VDD_32_33|MMC_VDD_33_34;
	mmc->caps = MMC_CAP_4_BIT_DATA;

	spin_lock_init(&host->lock);

	mmc->max_hw_segs = MAX_HW_SEGMENTS;
	mmc->max_phys_segs = MAX_PHYS_SEGMENTS;
	mmc->max_blk_count = 4095;
	mmc->max_blk_size = 1024;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;

	dev_set_drvdata(dev, mmc);

	init_timer(&host->delayed_timer);
	host->delayed_timer.data = (unsigned long) NULL;
	host->delayed_timer.function = wbsd_delayed_fifo;

	return 0;
}

static void __devexit wbsd_free_mmc(struct device* dev)
{
	struct mmc_host* mmc;

	mmc = dev_get_drvdata(dev);
	if (!mmc)
		return;

	BUG_ON(host == NULL);

	del_timer_sync(&host->delayed_timer);

	mmc_free_host(mmc);

	dev_set_drvdata(dev, NULL);
}

/*
 * Allocate/free io port ranges
 */

static int __devinit wbsd_request_region(void)
{
	host->base = (int) ioremap(0x08000000, 10);

	return 0;
}

static void __devexit wbsd_release_regions(void)
{
	if (host->base)
		iounmap((void *) host->base);

	host->base = 0;
}

/*
 * Allocate/free IRQ.
 */

static int __devinit wbsd_request_irq(void)
{
	int ret;

	/*
	 * Allocate interrupt.
	 */

#define REG(x)								\
	ret = request_irq(IRQ_GPIO(x), wbsd_irq,			\
			SA_INTERRUPT | SA_TRIGGER_RISING |		\
			SA_TRIGGER_FALLING, DRIVER_NAME, (void *) x);	\
	if (ret)							\
		return ret;

	/* Chip interrupt */
	ret = request_irq(IRQ_GPIO(8), wbsd_irq,
			SA_INTERRUPT | SA_TRIGGER_FALLING,
			DRIVER_NAME, (void *) 8);
	if (ret)
		return ret;

	/* Card insert/release */
	REG(2);

	host->irq = 8;
	/*
	 * Set up tasklets.
	 */
	tasklet_init(&host->card_tasklet, wbsd_tasklet_card, (unsigned long)host);
	tasklet_init(&host->fifo_tasklet, wbsd_tasklet_fifo, (unsigned long)host);
	tasklet_init(&host->crc_tasklet, wbsd_tasklet_crc, (unsigned long)host);
	tasklet_init(&host->timeout_tasklet, wbsd_tasklet_timeout, (unsigned long)host);
	tasklet_init(&host->finish_tasklet, wbsd_tasklet_finish, (unsigned long)host);
	INIT_WORK(&host->tps_work, wbsd_tasklet_tps);

	return 0;
}

static void __devexit wbsd_release_irq(void)
{
	if (!host->irq)
		return;

#define UREG(x) free_irq(IRQ_GPIO(x), (void *) x)

	UREG(2);
	UREG(8);

	host->irq = 0;

	tasklet_kill(&host->card_tasklet);
	tasklet_kill(&host->fifo_tasklet);
	tasklet_kill(&host->crc_tasklet);
	tasklet_kill(&host->timeout_tasklet);
	tasklet_kill(&host->finish_tasklet);
}

/*
 * Allocate all resources for the host.
 */

static int __devinit wbsd_request_resources(void)
{
	int ret;

	/*
	 * Allocate I/O ports.
	 */
	ret = wbsd_request_region();
	if (ret)
		return ret;

	/*
	 * Allocate interrupt.
	 */
	ret = wbsd_request_irq();
	if (ret)
		return ret;

	return 0;
}

/*
 * Release all resources for the host.
 */

static void __devexit wbsd_release_resources(void)
{
	wbsd_release_irq();
	wbsd_release_regions();
}

/*
 * Configure the resources the chip should use.
 */

static void wbsd_chip_config(void)
{
	MSC1 = (MSC1 & 0xFFFF0000) | 0x000087A9;
	//printk("Mem: %x %x %x %x %x\n", MDCNFG, MSC0, MSC1, MSC2, SXCNFG);
}

/*
 * Check that configured resources are correct.
 */

/*
 * Powers down chip
 */
static void wbsd_chip_poweroff(void)
{

	spin_lock(&host->lock);

	RWRITEW(WBR_CR, 0x0080);
	printk("Power Down\n");

	spin_unlock(&host->lock);
}

/*****************************************************************************\
 *                                                                           *
 * Devices setup and shutdown                                                *
 *                                                                           *
\*****************************************************************************/

static int __devinit wbsd_init(struct device* dev)
{
	struct mmc_host* mmc = NULL;
	int ret;

	host = NULL;

	ret = wbsd_alloc_mmc(dev);
	if (ret)
		return ret;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

  	wbsd_chip_config();

	/*
	 * Request resources.
	 */
	ret = wbsd_request_resources();
	if (ret)
	{
		wbsd_release_resources();
		wbsd_free_mmc(dev);
		return ret;
	}

	/*
	 * Reset the chip into a known state.
	 */
	wbsd_init_device();

	mmc_add_host(mmc);

	printk(KERN_INFO "%s: W86L488Y", mmc_hostname(mmc));
	printk(" at 0x%x irq %d\n", (int)host->base, (int)host->irq);
	return 0;
}

static void __devexit wbsd_shutdown(struct device* dev)
{
	struct mmc_host* mmc = dev_get_drvdata(dev);
	struct wbsd_host* host;

	if (!mmc)
		return;

	host = mmc_priv(mmc);

	wbsd_chip_poweroff();

	mmc_remove_host(mmc);

	/*
	 * Power down the SD/MMC function.
	 */
	wbsd_release_resources();

	wbsd_free_mmc(dev);
}

/*
 * Probe/Remove
 */

static int __devinit wbsd_probe(struct platform_device* dev)
{
	return wbsd_init(&dev->dev);
}

static int __devexit wbsd_remove(struct platform_device* dev)
{
	wbsd_shutdown(&dev->dev);
	return 0;
}

/*
 * Power management
 */

#ifdef CONFIG_PM

static int wbsd_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct wbsd_host *host;
	int ret;

	DBG("Suspending...\n");
	
	if (!mmc)
		return 0;

	ret = mmc_suspend_host(mmc, state);
	if (!ret)
		return ret;

	host = mmc_priv(mmc);

	wbsd_chip_poweroff();

	return 0;
}

static int wbsd_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct wbsd_host *host;

	DBG("Resuming...\n");

	if (!mmc)
		return 0;

	host = mmc_priv(mmc);

	wbsd_chip_config();

	wbsd_init_device();

	return mmc_resume_host(mmc);
}

#else /* CONFIG_PM */

#define wbsd_suspend NULL
#define wbsd_resume NULL

#endif /* CONFIG_PM */

static struct platform_device *wbsd_device;

static struct platform_driver wbsd_driver = {
	.probe		= wbsd_probe,
	.remove		= __devexit_p(wbsd_remove),

	.suspend	= wbsd_suspend,
	.resume		= wbsd_resume,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

/*
 * Module loading/unloading
 */

static int __init wbsd_drv_init(void)
{
	int result;

	printk(KERN_INFO DRIVER_NAME
		": W86L488Y Palm T|T3 SD/MMC card interface driver, "
		DRIVER_VERSION "\n");

	result = platform_driver_register(&wbsd_driver);
	if (result < 0)
		return result;

	wbsd_device = platform_device_register_simple(DRIVER_NAME, -1,
		NULL, 0);
	if (IS_ERR(wbsd_device))
		return PTR_ERR(wbsd_device);

	return 0;
}

static void __exit wbsd_drv_exit(void)
{
	platform_device_unregister(wbsd_device);

	platform_driver_unregister(&wbsd_driver);

	DBG("unloaded\n");
}

module_init(wbsd_drv_init);
module_exit(wbsd_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Kupec <magon@dobaledoba.net>");
MODULE_DESCRIPTION("Winbond W86L488Y SD/MMC card interface driver on Palm T|T3");
MODULE_VERSION(DRIVER_VERSION);
