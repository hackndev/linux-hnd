/*
 *  linux/drivers/mmc/tmio_mmc.c
 *
 *  Copyright (C) 2004 Ian Molton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Driver for the SD / SDIO cell found in:
 *
 * TC6393XB
 *
 * This driver draws mainly on scattered spec sheets, Reverse engineering
 * of the toshiba e800  SD driver and some parts of the 2.4 ASIC3 driver (4 bit
 * support).
 *
 * Supports MMC 1 bit transfers and SD 1 and 4 bit modes.
 *
 * TODO:
 *   Eliminate FIXMEs
 *   SDIO support
 *   Power management
 *   Handle MMC errors (at all)
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/protocol.h>
#include <linux/scatterlist.h>
#include <linux/mfd/tmio_mmc.h>

#include <asm/io.h>
#include <linux/clk.h>
#include <asm/mach-types.h>

#include "tmio_mmc.h"

#define DRIVER_NAME "tmio_mmc"

/* From wince:
		TMIO_B(0xA1) |= 0x10;     // Chip buffer of control
                busy_wait(280);

                TMIO_H(0x210) = 0x0800;   // set sd base
                TMIO_H(0x204) |= 0x0002;  // sd command reg
                TMIO_B(0x240) = 0x1F;     // stop clock ctl
                TMIO_B(0x24A) |= 0x02;    // power_ctl_3

                TMIO_H(0x820) = ~0x0018;  // interrupt mask
                TMIO_H(0x822) = 0x0000;   // interrupt mask
                TMIO_H(0x920) = 0x0000;   // interrupt mask
                TMIO_H(0x922) = 0x0000;   // interrupt mask
                TMIO_H(0x936) |= 0x0100;  // card int ctl
                TMIO_H(0x81C) = 0x0000;   // set card stat to 0 (not buf/err status?)
                TMIO_H(0x242) |= 0x01;    // clock mode
        */

static void reset_chip(struct tmio_mmc_host *host) {
        /* Reset */
        writew(0x0000, host->ctl_base + TMIO_CTRL_SD_SOFT_RESET_REG);
        writew(0x0000, host->ctl_base + TMIO_CTRL_SDIO_SOFT_RESET_REG);
        msleep(10);
        writew(0x0001, host->ctl_base + TMIO_CTRL_SD_SOFT_RESET_REG);
        writew(0x0001, host->ctl_base + TMIO_CTRL_SDIO_SOFT_RESET_REG);
        msleep(10);
}

static void
tmio_mmc_finish_request(struct tmio_mmc_host *host)
{
	struct mmc_request *mrq = host->mrq;
	/* Write something to end the command */
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;

	mmc_request_done(host->mmc, mrq);
}

/* These are the bitmasks the tmio chip requires to implement the MMC response
 * types. Note that R1 and R6 are the same in this scheme. */
#define APP_CMD        0x0040
#define RESP_NONE      0x0300
#define RESP_R1        0x0400
#define RESP_R1B       0x0500
#define RESP_R2        0x0600
#define RESP_R3        0x0700 
#define DATA_PRESENT   0x0800
#define TRANSFER_READ  0x1000
#define TRANSFER_MULTI 0x2000
#define SECURITY_CMD   0x4000

static void
tmio_mmc_start_command(struct tmio_mmc_host *host, struct mmc_command *cmd)
{
	void *base = host->ctl_base;
	struct mmc_data *data = host->data;
	int c = cmd->opcode;

	if(cmd->opcode == MMC_STOP_TRANSMISSION) {
		writew(0x001, host->ctl_base + TMIO_CTRL_STOP_INTERNAL_ACTION_REG);
		return;
	}

	switch(mmc_resp_type(cmd)) {
		case MMC_RSP_NONE: c |= RESP_NONE; break;
		case MMC_RSP_R1:   c |= RESP_R1;   break;
		case MMC_RSP_R1B:  c |= RESP_R1B;  break;
		case MMC_RSP_R2:   c |= RESP_R2;   break;
		case MMC_RSP_R3:   c |= RESP_R3;   break;
		default:
			DBG("Unknown response type %d\n", mmc_resp_type(cmd));
	}

	host->cmd = cmd;

// FIXME - this seems to be ok comented out but the spec say this bit should
//         be set when issuing app commands... the upper MC code doesnt help us
//         here, though.
//	if(cmd->flags & MMC_FLAG_ACMD)
//		c |= APP_CMD;
	if(data) {
		c |= DATA_PRESENT;
		if(data->blocks > 1) {
			writew(0x100, host->ctl_base + TMIO_CTRL_STOP_INTERNAL_ACTION_REG);
			c |= TRANSFER_MULTI;
		}
		if(data->flags & MMC_DATA_READ)
			c |= TRANSFER_READ;
	}

	enable_mmc_irqs(host, TMIO_MASK_CMD);

	/* Fire off the command */
	write_long_reg(cmd->arg, base + TMIO_CTRL_ARG_REG_BASE);
	writew(c, base + TMIO_CTRL_CMD_REG);
}

/* This chip always returns (at least?) as much data as you ask for.
 * Im unsure what happens if you ask for less than a block. This should be
 * looked into to ensure that a funny length read doesnt hose the controller
 * data state machine.
 * FIXME - this chip cannot do 1 and 2 byte data requests in 4 bit mode
 */
static void tmio_mmc_pio_irq(struct tmio_mmc_host *host) {
	struct mmc_data *data = host->data;
        unsigned short *buf;
        unsigned int count;
        unsigned long flags;

        if(!data){
		DBG("Spurious PIO IRQ\n");
                return;
        }

        buf = (unsigned short *)(tmio_mmc_kmap_atomic(host, &flags) +
	      host->sg_off);

        /* Ensure we dont read more than one block. The chip will interrupt us
         * When the next block is available.
	 * FIXME - this is probably not true now IRQ handling is fixed
         */
        count = host->sg_ptr->length - host->sg_off;
        if(count > data->blksz)
                count = data->blksz;

        DBG("count: %08x offset: %08x flags %08x\n",
	    count, host->sg_off, data->flags);

	/* Transfer the data */
	if(data->flags & MMC_DATA_READ)
		readsw(host->ctl_base  + TMIO_CTRL_DATA_REG, buf, count >> 1);
	else
		writesw(host->ctl_base + TMIO_CTRL_DATA_REG, buf, count >> 1);

        host->sg_off += count;

        tmio_mmc_kunmap_atomic(host, &flags);
	
	if(host->sg_off == host->sg_ptr->length)
	        tmio_mmc_next_sg(host);

        return;
}

static void tmio_mmc_data_irq(struct tmio_mmc_host *host) {
	struct mmc_data *data = host->data;
	// FIXME - pxamci does checks for bad CRC, timeout, etc. here.
	// Suspect we shouldnt but check anyhow.

	host->data = NULL;

	if(!data){
                DBG("Spurious data end IRQ\n");
                return;
        }

	// I suspect we can do better here too. PXA MCI cant do this right but
	// we use PIO so we probably can.
	if (data->error == MMC_ERR_NONE)
		data->bytes_xfered = data->blocks * data->blksz;
	else
		data->bytes_xfered = 0;

	DBG("Completed data request\n");

	//FIXME - other drievrs allow an optional stop command of any given type
	//        which we dont do, as the chip can auto generate them.
	//        Perhaps we can be smarter about when to use auto CMD12 and
	//        only issue the auto request when we know this is the desired
	//        stop command, allowing fallback to the stop command the
	//        upper layers expect. For now, we do what works.

	writew(0x000, host->ctl_base + TMIO_CTRL_STOP_INTERNAL_ACTION_REG);

	if(data->flags & MMC_DATA_READ)
                disable_mmc_irqs(host, TMIO_MASK_READOP);
        else
                disable_mmc_irqs(host, TMIO_MASK_WRITEOP);

	tmio_mmc_finish_request(host);
}

static void tmio_mmc_cmd_irq(struct tmio_mmc_host *host, unsigned int stat) {
        struct mmc_command *cmd = host->cmd;

	if(!host->cmd) {
		DBG("Spurious CMD irq\n");
		return;
	}

        host->cmd = NULL;

        /* This controller is sicker than the PXA one. not only do we need to
         * drop the top 8 bits of the first response word, we also need to
         * modify the order of the response for short response command types.
         */

	// FIXME - this works but readl is probably wrong...
        cmd->resp[3] = readl(host->ctl_base + TMIO_CTRL_RESP_REG_BASE);
        cmd->resp[2] = readl(host->ctl_base + TMIO_CTRL_RESP_REG_BASE + 4);
        cmd->resp[1] = readl(host->ctl_base + TMIO_CTRL_RESP_REG_BASE + 8);
        cmd->resp[0] = readl(host->ctl_base + TMIO_CTRL_RESP_REG_BASE + 12);

        if(cmd->flags &  MMC_RSP_136) {
                cmd->resp[0] = (cmd->resp[0] <<8) | (cmd->resp[1] >>24);
                cmd->resp[1] = (cmd->resp[1] <<8) | (cmd->resp[2] >>24);
                cmd->resp[2] = (cmd->resp[2] <<8) | (cmd->resp[3] >>24);
                cmd->resp[3] <<= 8;
        }
        else if(cmd->flags & MMC_RSP_R3) {
                cmd->resp[0] = cmd->resp[3];
        }

        if (stat & TMIO_STAT_CMDTIMEOUT)
                cmd->error = MMC_ERR_TIMEOUT;
        else if (stat & TMIO_STAT_CRCFAIL && cmd->flags & MMC_RSP_CRC)
                cmd->error = MMC_ERR_BADCRC;

	if(cmd->error == MMC_ERR_NONE) {
		switch (cmd->opcode) {
			case SD_APP_SET_BUS_WIDTH: //FIXME - assumes 4 bit
				writew(0x40e0, host->ctl_base + TMIO_CTRL_SD_MEMORY_CARD_OPTION_SETUP_REG);
				break;
			case MMC_SELECT_CARD:
				if((cmd->arg >> 16) == 0) // deselect event
					writew(0x80e0, host->ctl_base + TMIO_CTRL_SD_MEMORY_CARD_OPTION_SETUP_REG);
				break;
		}
	}

	/* If there is data to handle we enable data IRQs here, and
	 * we will ultimatley finish the request in the data_end handler.
	 * If theres no data or we encountered an error, finish now. */
	if(host->data && cmd->error == MMC_ERR_NONE){
		if(host->data->flags & MMC_DATA_READ)
			enable_mmc_irqs(host, TMIO_MASK_READOP);
		else
			enable_mmc_irqs(host, TMIO_MASK_WRITEOP);
	}
	else {
		tmio_mmc_finish_request(host);
	}

        return;
}


static irqreturn_t tmio_mmc_irq(int irq, void *devid)
{
	struct tmio_mmc_host *host = devid;
	unsigned int ireg, mask, status;

	DBG("MMC IRQ begin\n");

       	status = read_long_reg(host->ctl_base + TMIO_CTRL_STATUS_REG_BASE);
       	mask   = read_long_reg(host->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE);
	ireg   = status & TMIO_MASK_IRQ & ~mask;

#ifdef CONFIG_MMC_DEBUG
        debug_status(status);
        debug_status(ireg);
#endif
	if (!ireg) {
		disable_mmc_irqs(host, status & ~mask);
#ifdef CONFIG_MMC_DEBUG
		printk("tmio_mmc: Spurious MMC irq, disabling! 0x%08x 0x%08x 0x%08x\n", status, mask, ireg);
		debug_status(status);
#endif
		goto out;
	}

	while (ireg) {
		/* Card insert / remove attempts */
		if (ireg & (TMIO_STAT_CARD_INSERT | TMIO_STAT_CARD_REMOVE)){
			ack_mmc_irqs(host, TMIO_STAT_CARD_INSERT | TMIO_STAT_CARD_REMOVE);
	                mmc_detect_change(host->mmc,0);
		}

		/* CRC and other errors */
//		if (ireg & TMIO_STAT_ERR_IRQ)
//			handled |= tmio_error_irq(host, irq, stat);

		/* Command completion */
	        if (ireg & TMIO_MASK_CMD) {
	                tmio_mmc_cmd_irq(host, status);
			ack_mmc_irqs(host, TMIO_MASK_CMD);
		}

		/* Data transfer */
		if (ireg & (TMIO_STAT_RXRDY | TMIO_STAT_TXRQ)) {
			ack_mmc_irqs(host, TMIO_STAT_RXRDY | TMIO_STAT_TXRQ);
	                tmio_mmc_pio_irq(host);
		}

		/* Data transfer completion */
	        if (ireg & TMIO_STAT_DATAEND) {
	                tmio_mmc_data_irq(host);
			ack_mmc_irqs(host, TMIO_STAT_DATAEND);
		}

		/* Check status - keep going until we've handled it all */
		status = read_long_reg(host->ctl_base + TMIO_CTRL_STATUS_REG_BASE);
       		mask   = read_long_reg(host->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE);
		ireg   = status & TMIO_MASK_IRQ & ~mask;

#ifdef CONFIG_MMC_DEBUG
		DBG("Status at end of loop: %08x\n", status);
		debug_status(status);
#endif
	}
	DBG("MMC IRQ end\n");

out:
        return IRQ_HANDLED;
}

static void tmio_mmc_start_data(struct tmio_mmc_host *host, struct mmc_data *data)
{
	DBG("setup data transfer: blocksize %08x  nr_blocks %d\n",
	    data->blksz, data->blocks);

	tmio_mmc_init_sg(host, data);
	host->data = data; 

	/* Set transfer length / blocksize */
	writew(data->blksz, host->ctl_base + TMIO_CTRL_DATALENGTH_REG);
        writew(data->blocks, host->ctl_base + TMIO_CTRL_TRANSFER_BLOCK_COUNT_REG);
}

/* Process requests from the MMC layer */
static void tmio_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct tmio_mmc_host *host = mmc_priv(mmc);

	WARN_ON(host->mrq != NULL);

	host->mrq = mrq;

	/* If we're performing a data request we need to setup some
	   extra information */
	if (mrq->data)
		tmio_mmc_start_data(host, mrq->data);

	tmio_mmc_start_command(host, mrq->cmd);
}

/* Set MMC clock / power.
 * Note: This controller uses a simple divider scheme therefore it cannot
 * run a MMC card at full speed (20MHz). The max clock is 24MHz on SD, but as
 * MMC wont run that fast, it has to be clocked at 12MHz which is the next
 * slowest setting.
 */
static void tmio_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct tmio_mmc_host *host = mmc_priv(mmc);
	u32 clk = 0, clock;

	DBG("clock %uHz busmode %u powermode %u Vdd %u\n",
	    ios->clock, ios->bus_mode, ios->power_mode, ios->vdd);
	//FIXME - this works but may not quite be the right way to do it
	if (ios->clock) {
		for(clock = 46875, clk = 512 ; ios->clock >= (clock<<1); ){
			clock <<= 1;
			clk >>= 1;
		}
		if(clk & 0x1)
			clk = 0x20000;
		clk >>= 2;
		if(clk & 0x8000) // For 24MHz we disable the divider.
			writew(0, host->cnf_base + TMIO_CONF_CLOCK_MODE_REG);
		else
			writew(1, host->cnf_base + TMIO_CONF_CLOCK_MODE_REG);
		writew(clk | 0x100, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);
		DBG("clk = %04x\n", clk | 0x100);
	}
	else
		writew(0, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		writeb(0x00, host->cnf_base + TMIO_CONF_POWER_CNTL_REG_2);
		break;
	case MMC_POWER_UP:
		//FIXME - investigate the possibility of turning on our main
		//        clock here.
		break;
	case MMC_POWER_ON:
		writeb(0x02, host->cnf_base + TMIO_CONF_POWER_CNTL_REG_2);
		break;
	}
	// Potentially we may need a 140us pause here. FIXME
	udelay(140);
}

static int tmio_mmc_get_ro(struct mmc_host *mmc) {
	struct tmio_mmc_host *host = mmc_priv(mmc);

	return (readw(host->ctl_base + TMIO_CTRL_STATUS_REG_BASE) & TMIO_STAT_WRPROTECT)?0:1;
}

static struct mmc_host_ops tmio_mmc_ops = {
	.request	= tmio_mmc_request,
	.set_ios	= tmio_mmc_set_ios,
	.get_ro         = tmio_mmc_get_ro,
};

#define platform_get_platdata(d) ((void*)((d)->dev.platform_data))

static void hwinit(struct tmio_mmc_host *host, struct platform_device *dev) {

	struct tmio_mmc_hwconfig *hwconfig = platform_get_platdata(dev);
	unsigned long addr_shift = 0;

	addr_shift = hwconfig?hwconfig->address_shift:0;

	/* Enable the MMC function */
	writeb(SDCREN, host->cnf_base + TMIO_CONF_COMMAND_REG);

	/* Map the MMC registers into the chips config space*/
	writel((dev->resource[0].start & 0xfffe) >> addr_shift, host->cnf_base + TMIO_CONF_SD_CTRL_BASE_ADDRESS_REG);

	/* Enable our clock (if needed!) */
	if(hwconfig && hwconfig->set_mmc_clock)
		hwconfig->set_mmc_clock(dev, MMC_CLOCK_ENABLED);

        writeb(0x01f, host->cnf_base + TMIO_CONF_STOP_CLOCK_CONTROL_REG);
        writeb(readb(host->cnf_base + TMIO_CONF_POWER_CNTL_REG_3) | 0x02,
	       host->cnf_base + TMIO_CONF_POWER_CNTL_REG_3);
        writeb(readb(host->cnf_base + TMIO_CONF_CLOCK_MODE_REG) | 0x01,
	       host->cnf_base + TMIO_CONF_CLOCK_MODE_REG);

        /* disable clock */
        writew(0x0000, host->ctl_base + TMIO_CTRL_CLOCK_AND_WAIT_CONTROL_REG);
        msleep(10);
        writew(readw(host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG) &
	       ~0x0100, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);
        msleep(10);

        /* power down */
        writeb(0x0000, host->cnf_base + TMIO_CONF_POWER_CNTL_REG_2);

	/* Reset */
	reset_chip(host);

        /* select 400kHz clock speed */
        writew(0x180, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);

        /* power up */
        writeb(0x0002, host->cnf_base + TMIO_CONF_POWER_CNTL_REG_2);

        /* enable clock */
        writew(0x0100, host->ctl_base + TMIO_CTRL_CLOCK_AND_WAIT_CONTROL_REG);
        msleep(10);
        writew(readw(host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG) |
	       0x0100, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);
        msleep(10);

	/* Select 1 bit mode (daft chip starts in 4 bit mode) */
	writew(0x80e0, host->ctl_base + TMIO_CTRL_SD_MEMORY_CARD_OPTION_SETUP_REG);

}

static int tmio_mmc_suspend(struct platform_device *dev, pm_message_t state) {
	struct tmio_mmc_hwconfig *hwconfig = platform_get_platdata(dev);
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct tmio_mmc_host *host = mmc_priv(mmc);
	int ret;

	ret = mmc_suspend_host(mmc, state);

	if (ret) {
		printk(KERN_ERR DRIVER_NAME ": Could not suspend MMC host, hardware not suspended");
		return ret;
	}

	/* disable clock */
	writew(0x0000, host->ctl_base + TMIO_CTRL_CLOCK_AND_WAIT_CONTROL_REG);
        msleep(10);
        writew(readw(host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG) &
               ~0x0100, host->ctl_base + TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG);
        msleep(10);

	/* power down */
	writeb(0x0000, host->cnf_base + TMIO_CONF_POWER_CNTL_REG_2);

        /* disable core clock */
        if(hwconfig && hwconfig->set_mmc_clock)
                hwconfig->set_mmc_clock(dev, MMC_CLOCK_DISABLED);

	/* Disable SD/MMC function */
	writeb(0, host->cnf_base + TMIO_CONF_COMMAND_REG);
	return 0;
}

static int tmio_mmc_resume(struct platform_device *dev) {
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct tmio_mmc_host *host = mmc_priv(mmc);

	hwinit(host, dev);
	mmc_resume_host(mmc);

	return 0;
}

static int tmio_mmc_probe(struct platform_device *dev)
{
	struct tmio_mmc_host *host;
	struct mmc_host *mmc;
	int ret = -ENOMEM;

	mmc = mmc_alloc_host(sizeof(struct tmio_mmc_host), &dev->dev);
	if (!mmc) {
		goto out;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;
	platform_set_drvdata(dev, mmc); // Used so we can de-init safely.

	host->cnf_base = ioremap((unsigned long)dev->resource[1].start,
	                         (unsigned long)dev->resource[1].end -
	                         (unsigned long)dev->resource[1].start);
	if(!host->cnf_base)
		goto host_free;

	host->ctl_base = ioremap((unsigned long)dev->resource[0].start,
	                         (unsigned long)dev->resource[0].end -
	                         (unsigned long)dev->resource[0].start);
	if (!host->ctl_base) {
		goto unmap_cnf_base;
	}

	printk( "%08x %08x\n", dev->resource[0].start, dev->resource[1].start);
	mmc->ops = &tmio_mmc_ops;
	mmc->caps = MMC_CAP_4_BIT_DATA;
	mmc->f_min = 46875;
	mmc->f_max = 24000000;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	hwinit(host, dev);

	host->irq = (unsigned long)dev->resource[2].start;
	ret = request_irq(host->irq, tmio_mmc_irq, SA_INTERRUPT, DRIVER_NAME, host);
	if (ret){
		ret = -ENODEV;
		goto unmap_ctl_base;
	}
	set_irq_type(host->irq, IRQT_FALLING);

	mmc_add_host(mmc);

	printk(KERN_INFO "%s at 0x%08lx irq %d\n",
	       mmc_hostname(host->mmc), (unsigned long)host->ctl_base, host->irq);

	/* Lets unmask the IRQs we want to know about */
        disable_mmc_irqs(host, TMIO_MASK_ALL);
	enable_mmc_irqs(host,  TMIO_MASK_IRQ);
	/* FIXME - when we reset this chip, it will generate a card_insert IRQ
	 * this triggers a scan for cards. the MMC layer does a scan in anycase
	 * so its a bit wasteful.
	 * Ideally the MMC layer would not scan for cards by default.
	 */

	return 0;

unmap_ctl_base:
	iounmap(host->ctl_base);
unmap_cnf_base:
	iounmap(host->cnf_base);
host_free:
	mmc_free_host(mmc);
out:
	return ret;
}

static int tmio_mmc_remove(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	if (mmc) {
		struct tmio_mmc_host *host = mmc_priv(mmc);
		mmc_remove_host(mmc);
		free_irq(host->irq, host);
		// FIXME - we might want to consider stopping the chip here...
		iounmap(host->ctl_base);
		iounmap(host->cnf_base);
		mmc_free_host(mmc); // FIXME - why does this call hang ?
	}
	return 0;
}

/* ------------------- device registration ----------------------- */

//static platform_device_id tmio_mmc_platform_device_ids[] = {{TMIO_MMC_DEVICE_ID}, {0}};

struct platform_driver tmio_mmc_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = tmio_mmc_probe,
	.remove = tmio_mmc_remove,
#ifdef CONFIG_PM
	.suspend = tmio_mmc_suspend,
	.resume = tmio_mmc_resume,
#endif
};


static int __init tmio_mmc_init(void)
{
        return platform_driver_register (&tmio_mmc_driver);
	return 0;
}

static void __exit tmio_mmc_exit(void)
{
	 return platform_driver_unregister (&tmio_mmc_driver);
}

module_init(tmio_mmc_init);
module_exit(tmio_mmc_exit);

MODULE_DESCRIPTION("Toshiba common SD/MMC driver");
MODULE_AUTHOR("Ian Molton");
MODULE_LICENSE("GPL");
