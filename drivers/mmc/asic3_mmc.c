/* Note that this driver can likely be merged into the tmio driver, so
 * consider this code temporary.  It works, though.
 */
/*
 *  linux/drivers/mmc/asic3_mmc.c
 *
 *  Copyright (c) 2005 SDG Systems, LLC
 *
 *  based on tmio_mmc.c
 *      Copyright (C) 2004 Ian Molton
 *
 *  Refactored to support all ASIC3 devices, 2006 Paul Sokolovsky 
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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/protocol.h>
#include <linux/scatterlist.h>
#include <linux/soc-old.h>
#include <linux/mfd/asic3_base.h>
#include <linux/mfd/tmio_mmc.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <linux/clk.h>
#include <asm/mach-types.h>

#include <asm/hardware/ipaq-asic3.h>
#include "asic3_mmc.h"

struct asic3_mmc_host {
    void                    *ctl_base;
    struct device           *asic3_dev;       /* asic3 device */
    struct tmio_mmc_hwconfig *hwconfig;       /* HW config data/handlers, guaranteed != NULL */
    unsigned long           bus_shift;
    struct mmc_command      *cmd;
    struct mmc_request      *mrq;
    struct mmc_data         *data;
    struct mmc_host         *mmc;
    int                     irq;
    unsigned short          clock_for_sd;

    /* I/O related stuff */
    struct scatterlist      *sg_ptr;
    unsigned int            sg_len;
    unsigned int            sg_off;
};

static void
mmc_finish_request(struct asic3_mmc_host *host)
{
    struct mmc_request *mrq = host->mrq;

    /* Write something to end the command */
    host->mrq  = NULL;
    host->cmd  = NULL;
    host->data = NULL;

    mmc_request_done(host->mmc, mrq);
}


#define ASIC3_MMC_REG(host, block, reg) (*((volatile u16 *) ((host->ctl_base) + ((_IPAQ_ASIC3_## block ## _Base + _IPAQ_ASIC3_ ## block ## _ ## reg) >> (2 - host->bus_shift))) ))

static void
mmc_start_command(struct asic3_mmc_host *host, struct mmc_command *cmd)
{
    struct mmc_data *data = host->data;
    int c                 = cmd->opcode;

    DBG("[1;33mOpcode: %d[0m, base: %p\n", cmd->opcode, host->ctl_base);

    if(cmd->opcode == MMC_STOP_TRANSMISSION) {
        ASIC3_MMC_REG(host, SD_CTRL, StopInternal) = SD_CTRL_STOP_INTERNAL_ISSSUE_CMD12;
        cmd->resp[0] = cmd->opcode;
        cmd->resp[1] = 0;
        cmd->resp[2] = 0;
        cmd->resp[3] = 0;
        cmd->resp[4] = 0;
        return;
    }

    switch(cmd->flags & 0x1f) {
        case MMC_RSP_NONE: c |= SD_CTRL_COMMAND_RESPONSE_TYPE_NORMAL; break;
        case MMC_RSP_R1:   c |= SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R1;   break;
        case MMC_RSP_R1B:  c |= SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R1B;  break;
        case MMC_RSP_R2:   c |= SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R2;   break;
        case MMC_RSP_R3:   c |= SD_CTRL_COMMAND_RESPONSE_TYPE_EXT_R3;   break;
        default:
            DBG("Unknown response type %d\n", cmd->flags & 0x1f);
            break;
    }

    host->cmd = cmd;

    if(cmd->opcode == MMC_APP_CMD) {
        c |= APP_CMD;
    }
    if (cmd->opcode == MMC_GO_IDLE_STATE) {
        c |= (3 << 8); /* This was removed from ipaq-asic3.h for some reason */
    }
    if(data) {
        c |= SD_CTRL_COMMAND_DATA_PRESENT;
        if(data->blocks > 1) {
            ASIC3_MMC_REG(host, SD_CTRL, StopInternal) = SD_CTRL_STOP_INTERNAL_AUTO_ISSUE_CMD12;
            c |= SD_CTRL_COMMAND_MULTI_BLOCK;
        }
        if(data->flags & MMC_DATA_READ) {
            c |= SD_CTRL_COMMAND_TRANSFER_READ;
        }
        /* MMC_DATA_WRITE does not require a bit to be set */
    }

    /* Enable the command and data interrupts */
    ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard) = ~(
          SD_CTRL_INTMASKCARD_RESPONSE_END
        | SD_CTRL_INTMASKCARD_RW_END
        | SD_CTRL_INTMASKCARD_CARD_REMOVED_0
        | SD_CTRL_INTMASKCARD_CARD_INSERTED_0
#if 0
        | SD_CTRL_INTMASKCARD_CARD_REMOVED_3
        | SD_CTRL_INTMASKCARD_CARD_INSERTED_3
#endif
    );

    ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) = ~(
          SD_CTRL_INTMASKBUFFER_UNK7
        | SD_CTRL_INTMASKBUFFER_CMD_BUSY
#if 0
        | SD_CTRL_INTMASKBUFFER_CMD_INDEX_ERROR
        | SD_CTRL_INTMASKBUFFER_CRC_ERROR
        | SD_CTRL_INTMASKBUFFER_STOP_BIT_END_ERROR
        | SD_CTRL_INTMASKBUFFER_DATA_TIMEOUT
        | SD_CTRL_INTMASKBUFFER_BUFFER_OVERFLOW
        | SD_CTRL_INTMASKBUFFER_BUFFER_UNDERFLOW
        | SD_CTRL_INTMASKBUFFER_CMD_TIMEOUT
        | SD_CTRL_INTMASKBUFFER_BUFFER_READ_ENABLE
        | SD_CTRL_INTMASKBUFFER_BUFFER_WRITE_ENABLE
        | SD_CTRL_INTMASKBUFFER_ILLEGAL_ACCESS
#endif
    );

    /* Send the command */
    ASIC3_MMC_REG(host, SD_CTRL, Arg1) = cmd->arg >> 16;
    ASIC3_MMC_REG(host, SD_CTRL, Arg0) = cmd->arg & 0xffff;
    ASIC3_MMC_REG(host, SD_CTRL, Cmd) = c;
}

/* This chip always returns (at least?) as much data as you ask for.  I'm
 * unsure what happens if you ask for less than a block. This should be looked
 * into to ensure that a funny length read doesnt mess up the controller data
 * state machine.
 *
 * Aric: Statement above may not apply to ASIC3.
 *
 * FIXME - this chip cannot do 1 and 2 byte data requests in 4 bit mode
 *
 * Aric: Statement above may not apply to ASIC3.
 */

static struct tasklet_struct mmc_data_read_tasklet;

static void
mmc_data_transfer(unsigned long h)
{
    struct asic3_mmc_host *host = (struct asic3_mmc_host *)h;
    struct mmc_data *data = host->data;
    unsigned short *buf;
    int count;
    /* unsigned long flags; */

    if(!data){
        printk(KERN_WARNING DRIVER_NAME ": Spurious Data IRQ\n");
        return;
    }

    /* local_irq_save(flags); */
    /* buf = kmap_atomic(host->sg_ptr->page, KM_BIO_SRC_IRQ); */
    buf = kmap(host->sg_ptr->page);
    buf += host->sg_ptr->offset/2 + host->sg_off/2;

    /*
     * Ensure we dont read more than one block. The chip will interrupt us
     * When the next block is available.
     */
    count = host->sg_ptr->length - host->sg_off;
    if(count > data->blksz) {
        count = data->blksz;
    }

    DBG("count: %08x, page: %p, offset: %08x flags %08x\n",
        count, host->sg_ptr->page, host->sg_off, data->flags);

    host->sg_off += count;

    /* Transfer the data */
    if(data->flags & MMC_DATA_READ) {
        while(count > 0) {
            /* Read two bytes from SD/MMC controller. */
            *buf = ASIC3_MMC_REG(host, SD_CTRL, DataPort);
            buf++;
            count -= 2;
        }
	flush_dcache_page(host->sg_ptr->page);
    } else {
        while(count > 0) {
            /* Write two bytes to SD/MMC controller. */
            ASIC3_MMC_REG(host, SD_CTRL, DataPort) = *buf;
            buf++;
            count -= 2;
        }
    }

    /* kunmap_atomic(host->sg_ptr->page, KM_BIO_SRC_IRQ); */
    kunmap(host->sg_ptr->page);
    /* local_irq_restore(flags); */
    if(host->sg_off == host->sg_ptr->length) {
        host->sg_ptr++;
        host->sg_off = 0;
        --host->sg_len;
    }

    return;
}

static void
mmc_data_end_irq(struct asic3_mmc_host *host)
{
    struct mmc_data *data = host->data;

    host->data = NULL;

    if(!data){
        printk(KERN_WARNING DRIVER_NAME ": Spurious data end IRQ\n");
        return;
    }

    if (data->error == MMC_ERR_NONE) {
        data->bytes_xfered = data->blocks * data->blksz;
    } else {
        data->bytes_xfered = 0;
    }

    DBG("Completed data request\n");

    ASIC3_MMC_REG(host, SD_CTRL, StopInternal) = 0;

    /* Make sure read enable interrupt and write enable interrupt are disabled */
    if(data->flags & MMC_DATA_READ) {
        ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) |= SD_CTRL_INTMASKBUFFER_BUFFER_READ_ENABLE;
    } else {
        ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) |= SD_CTRL_INTMASKBUFFER_BUFFER_WRITE_ENABLE;
    }

    mmc_finish_request(host);
}

static void
mmc_cmd_irq(struct asic3_mmc_host *host, unsigned int buffer_stat)
{
    struct mmc_command *cmd = host->cmd;
    u8 *buf = (u8 *)cmd->resp;
    u16 data;

    if(!host->cmd) {
        printk(KERN_WARNING DRIVER_NAME ": Spurious CMD irq\n");
        return;
    }

    host->cmd = NULL;
    if(cmd->flags & MMC_RSP_PRESENT && cmd->flags & MMC_RSP_136) {
        /* R2 */
        buf[12] = 0xff;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response0);
        buf[13] = data & 0xff;
        buf[14] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response1);
        buf[15] = data & 0xff;
        buf[8] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response2);
        buf[9] = data & 0xff;
        buf[10] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response3);
        buf[11] = data & 0xff;
        buf[4] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response4);
        buf[5] = data & 0xff;
        buf[6] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response5);
        buf[7] = data & 0xff;
        buf[0] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response6);
        buf[1] = data & 0xff;
        buf[2] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response7);
        buf[3] = data & 0xff;
    } else if(cmd->flags & MMC_RSP_PRESENT) {
        /* R1, R1B, R3 */
        data = ASIC3_MMC_REG(host, SD_CTRL, Response0);
        buf[0] = data & 0xff;
        buf[1] = data >> 8;
        data = ASIC3_MMC_REG(host, SD_CTRL, Response1);
        buf[2] = data & 0xff;
        buf[3] = data >> 8;
    }
    DBG("Response: %08x %08x %08x %08x\n", cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);

    if(buffer_stat & SD_CTRL_BUFFERSTATUS_CMD_TIMEOUT) {
        cmd->error = MMC_ERR_TIMEOUT;
    } else if((buffer_stat & SD_CTRL_BUFFERSTATUS_CRC_ERROR) && (cmd->flags & MMC_RSP_CRC)) {
        cmd->error = MMC_ERR_BADCRC;
    } else if(buffer_stat &
                (
                      SD_CTRL_BUFFERSTATUS_ILLEGAL_ACCESS
                    | SD_CTRL_BUFFERSTATUS_CMD_INDEX_ERROR
                    | SD_CTRL_BUFFERSTATUS_STOP_BIT_END_ERROR
                    | SD_CTRL_BUFFERSTATUS_BUFFER_OVERFLOW
                    | SD_CTRL_BUFFERSTATUS_BUFFER_UNDERFLOW
                    | SD_CTRL_BUFFERSTATUS_DATA_TIMEOUT
                )
    ) {
        DBG("Buffer status ERROR 0x%04x - inside check buffer\n", buffer_stat);
        DBG("detail0 error status 0x%04x\n", ASIC3_MMC_REG(host, SD_CTRL, ErrorStatus0));
        DBG("detail1 error status 0x%04x\n", ASIC3_MMC_REG(host, SD_CTRL, ErrorStatus1));
        cmd->error = MMC_ERR_FAILED;
    }

    if(cmd->error == MMC_ERR_NONE) {
        switch (cmd->opcode) {
            case SD_APP_SET_BUS_WIDTH:
                if(cmd->arg == SD_BUS_WIDTH_4) {
                    host->clock_for_sd = SD_CTRL_CARDCLOCKCONTROL_FOR_SD_CARD;
                    ASIC3_MMC_REG(host, SD_CTRL, MemCardOptionSetup) =
                              MEM_CARD_OPTION_REQUIRED
                            | MEM_CARD_OPTION_DATA_RESPONSE_TIMEOUT(14)
                            | MEM_CARD_OPTION_C2_MODULE_NOT_PRESENT
                            | MEM_CARD_OPTION_DATA_XFR_WIDTH_4;
                } else {
                    host->clock_for_sd = 0;
                    ASIC3_MMC_REG(host, SD_CTRL, MemCardOptionSetup) =
                              MEM_CARD_OPTION_REQUIRED
                            | MEM_CARD_OPTION_DATA_RESPONSE_TIMEOUT(14)
                            | MEM_CARD_OPTION_C2_MODULE_NOT_PRESENT
                            | MEM_CARD_OPTION_DATA_XFR_WIDTH_1;
                }
                break;
            case MMC_SELECT_CARD:
                if((cmd->arg >> 16) == 0) {
                    /* We have been deselected. */
                    ASIC3_MMC_REG(host, SD_CTRL, MemCardOptionSetup) =
                          MEM_CARD_OPTION_REQUIRED
                        | MEM_CARD_OPTION_DATA_RESPONSE_TIMEOUT(14)
                        | MEM_CARD_OPTION_C2_MODULE_NOT_PRESENT
                        | MEM_CARD_OPTION_DATA_XFR_WIDTH_1;
                }
        }
    }

    /*
     * If there is data to handle we enable data IRQs here, and we will
     * ultimatley finish the request in the mmc_data_end_irq handler.
     */
    if(host->data && (cmd->error == MMC_ERR_NONE)){
        if(host->data->flags & MMC_DATA_READ) {
            /* Enable the read enable interrupt */
            ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) &=
                ~SD_CTRL_INTMASKBUFFER_BUFFER_READ_ENABLE;
        } else {
            /* Enable the write enable interrupt */
            ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) &=
                ~SD_CTRL_INTMASKBUFFER_BUFFER_WRITE_ENABLE;
        }
    } else {
        /* There's no data, or we encountered an error, so finish now. */
        mmc_finish_request(host);
    }

    return;
}

static void hwinit2_irqsafe(struct asic3_mmc_host *host);

static irqreturn_t
mmc_irq(int irq, void *irq_desc)
{
    struct asic3_mmc_host *host;
    unsigned int breg, bmask, bstatus, creg, cmask, cstatus;

    host = irq_desc;

    /* asic3 bstatus has errors */
    bstatus = ASIC3_MMC_REG(host, SD_CTRL, BufferCtrl);
    bmask   = ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer);
    cstatus = ASIC3_MMC_REG(host, SD_CTRL, CardStatus);
    cmask   = ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard);
    breg    = bstatus & ~bmask & ~DONT_CARE_BUFFER_BITS;
    creg    = cstatus & ~cmask & ~DONT_CARE_CARD_BITS;

    if (!breg && !creg) {
        /* This occurs sometimes for no known reason.  It doesn't hurt
         * anything, so I don't print it.  */
        ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) &= ~breg;
        ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard) &= ~creg;
        goto out;
    }

    while (breg || creg) {

        /* XXX TODO: Need to handle errors in breg here. */

        /*
         * Card insert/remove.  The mmc controlling code is stateless.  That
         * is, it doesn't care if it was an insert or a remove.  It treats
         * both the same.
         */
        /* XXX Asic3 has _3 versions of these status bits, too, for a second slot, perhaps? */
        if (creg & (SD_CTRL_CARDSTATUS_CARD_INSERTED_0 | SD_CTRL_CARDSTATUS_CARD_REMOVED_0)) {
            ASIC3_MMC_REG(host, SD_CTRL, CardStatus) &=
                ~(SD_CTRL_CARDSTATUS_CARD_REMOVED_0 | SD_CTRL_CARDSTATUS_CARD_INSERTED_0);
            if(creg & SD_CTRL_CARDSTATUS_CARD_INSERTED_0) {
                hwinit2_irqsafe(host);
            }
            mmc_detect_change(host->mmc,1);
        }

        /* Command completion */
        if (creg & SD_CTRL_CARDSTATUS_RESPONSE_END) {
            ASIC3_MMC_REG(host, SD_CTRL, CardStatus) &=
                ~(SD_CTRL_CARDSTATUS_RESPONSE_END);
            mmc_cmd_irq(host, bstatus);
        }

        /* Data transfer */
        if (breg & (SD_CTRL_BUFFERSTATUS_BUFFER_READ_ENABLE | SD_CTRL_BUFFERSTATUS_BUFFER_WRITE_ENABLE)) {
            ASIC3_MMC_REG(host, SD_CTRL, BufferCtrl) &=
                ~(SD_CTRL_BUFFERSTATUS_BUFFER_WRITE_ENABLE | SD_CTRL_BUFFERSTATUS_BUFFER_READ_ENABLE);
	    tasklet_schedule(&mmc_data_read_tasklet);
        }

        /* Data transfer completion */
        if (creg & SD_CTRL_CARDSTATUS_RW_END) {
            ASIC3_MMC_REG(host, SD_CTRL, CardStatus) &= ~(SD_CTRL_CARDSTATUS_RW_END);
            mmc_data_end_irq(host);
        }

        /* Check status - keep going until we've handled it all */
        bstatus = ASIC3_MMC_REG(host, SD_CTRL, BufferCtrl);
        bmask   = ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer);
        cstatus = ASIC3_MMC_REG(host, SD_CTRL, CardStatus);
        cmask   = ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard);
        breg    = bstatus & ~bmask & ~DONT_CARE_BUFFER_BITS;
        creg    = cstatus & ~cmask & ~DONT_CARE_CARD_BITS;
    }

out:
    /* Ensure all interrupt sources are cleared */
    ASIC3_MMC_REG(host, SD_CTRL, BufferCtrl) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, CardStatus) = 0;
    return IRQ_HANDLED;
}

static void
mmc_start_data(struct asic3_mmc_host *host, struct mmc_data *data)
{
    DBG("setup data transfer: blocksize %08x  nr_blocks %d, page: %08x, offset: %08x\n", data->blksz,
        data->blocks, (int)data->sg->page, data->sg->offset);

    host->sg_len = data->sg_len;
    host->sg_ptr = data->sg;
    host->sg_off = 0;
    host->data   = data;

    /* Set transfer length and blocksize */
    ASIC3_MMC_REG(host, SD_CTRL, TransferSectorCount) = data->blocks;
    ASIC3_MMC_REG(host, SD_CTRL, MemCardXferDataLen)  = data->blksz;
}

/* Process requests from the MMC layer */
static void
mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
    struct asic3_mmc_host *host = mmc_priv(mmc);

    WARN_ON(host->mrq != NULL);

    host->mrq = mrq;

    /* If we're performing a data request we need to setup some
       extra information */
    if(mrq->data) {
        mmc_start_data(host, mrq->data);
    }

    mmc_start_command(host, mrq->cmd);
}

/* Set MMC clock / power.
 * Note: This controller uses a simple divider scheme therefore it cannot run
 * a MMC card at full speed (20MHz). The max clock is 24MHz on SD, but as MMC
 * wont run that fast, it has to be clocked at 12MHz which is the next slowest
 * setting.  This is likely not an issue because we are doing single 16-bit
 * writes for data I/O.
 */
static void
mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
    struct asic3_mmc_host *host = mmc_priv(mmc);
    u32 clk = 0;

    DBG("clock %uHz busmode %u powermode %u Vdd %u\n",
        ios->clock, ios->bus_mode, ios->power_mode, ios->vdd);

    if (ios->clock) {
        clk = 0x80; /* slowest by default */
        if(ios->clock >= 24000000 / 256) clk >>= 1;
        if(ios->clock >= 24000000 / 128) clk >>= 1;
        if(ios->clock >= 24000000 / 64)  clk >>= 1;
        if(ios->clock >= 24000000 / 32)  clk >>= 1;
        if(ios->clock >= 24000000 / 16)  clk >>= 1;
        if(ios->clock >= 24000000 / 8)   clk >>= 1;
        if(ios->clock >= 24000000 / 4)   clk >>= 1;
        if(ios->clock >= 24000000 / 2)   clk >>= 1;
        if(ios->clock >= 24000000 / 1)   clk >>= 1;
        if(clk == 0) { /* For fastest speed we disable the divider. */
            ASIC3_MMC_REG(host, SD_CONFIG, ClockMode) = 0;
        } else {
            ASIC3_MMC_REG(host, SD_CONFIG, ClockMode) = 1;
        }
        ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) = 0;
        ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) =
              host->clock_for_sd
            | SD_CTRL_CARDCLOCKCONTROL_ENABLE_CLOCK
            | clk;
        msleep(10);
    } else {
        ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) = 0;
    }

    switch (ios->power_mode) {
        case MMC_POWER_OFF:
            ASIC3_MMC_REG(host, SD_CONFIG, SDHC_Power1) = 0;
            msleep(1);
            break;
        case MMC_POWER_UP:
            break;
        case MMC_POWER_ON:
            ASIC3_MMC_REG(host, SD_CONFIG, SDHC_Power1) = SD_CONFIG_POWER1_PC_33V;
            msleep(20);
            break;
    }
}

static int
mmc_get_ro(struct mmc_host *mmc)
{
    struct asic3_mmc_host *host = mmc_priv(mmc);

    /* Call custom handler for RO status */
    if(host->hwconfig->mmc_get_ro) {
            /* Special case for cards w/o WP lock (like miniSD) */
            if (host->hwconfig->mmc_get_ro == (void*)-1) {
		    return 0;
            } else {
		    struct platform_device *pdev = to_platform_device(mmc_dev(mmc));
        	    return host->hwconfig->mmc_get_ro(pdev);
            }
    }

    /* WRITE_PROTECT is active low */
    return (ASIC3_MMC_REG(host, SD_CTRL, CardStatus) & SD_CTRL_CARDSTATUS_WRITE_PROTECT)?0:1;
}

static struct mmc_host_ops mmc_ops = {
    .request        = mmc_request,
    .set_ios        = mmc_set_ios,
    .get_ro         = mmc_get_ro,
};

static void
hwinit2_irqsafe(struct asic3_mmc_host *host)
{
    ASIC3_MMC_REG(host, SD_CONFIG, Addr1) = 0x0000;
    ASIC3_MMC_REG(host, SD_CONFIG, Addr0) = 0x0800;

    ASIC3_MMC_REG(host, SD_CONFIG, ClkStop) = SD_CONFIG_CLKSTOP_ENABLE_ALL;
    ASIC3_MMC_REG(host, SD_CONFIG, SDHC_CardDetect) = 2;
    ASIC3_MMC_REG(host, SD_CONFIG, Command) = SD_CONFIG_COMMAND_MAE;

    ASIC3_MMC_REG(host, SD_CTRL, SoftwareReset) = 0; /* reset on */
    mdelay(2);

    ASIC3_MMC_REG(host, SD_CTRL, SoftwareReset) = 1; /* reset off */
    mdelay(2);

    ASIC3_MMC_REG(host, SD_CTRL, MemCardOptionSetup) =
          MEM_CARD_OPTION_REQUIRED
        | MEM_CARD_OPTION_DATA_RESPONSE_TIMEOUT(14)
        | MEM_CARD_OPTION_C2_MODULE_NOT_PRESENT
        | MEM_CARD_OPTION_DATA_XFR_WIDTH_1
        ;
    host->clock_for_sd = 0;

    ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, CardStatus) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, BufferCtrl) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, ErrorStatus0) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, ErrorStatus1) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, StopInternal) = 0;

    ASIC3_MMC_REG(host, SDIO_CTRL, ClocknWaitCtrl) = 0x100;
    /* *((unsigned short *)(((char *)host->ctl_base) + 0x938)) = 0x100; */

    ASIC3_MMC_REG(host, SD_CONFIG, ClockMode) = 0;
    ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) = 0;

    mdelay(1);


    ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard) = ~(
          SD_CTRL_INTMASKCARD_RESPONSE_END
        | SD_CTRL_INTMASKCARD_RW_END
        | SD_CTRL_INTMASKCARD_CARD_REMOVED_0
        | SD_CTRL_INTMASKCARD_CARD_INSERTED_0
#if 0
        | SD_CTRL_INTMASKCARD_CARD_REMOVED_3
        | SD_CTRL_INTMASKCARD_CARD_INSERTED_3
#endif
        )
        ; /* check */
    ASIC3_MMC_REG(host, SD_CTRL, IntMaskBuffer) = 0xffff;  /* IRQs off */

    /*
     * ASIC3_MMC_REG(host, SD_CTRL, TransactionCtrl) = SD_CTRL_TRANSACTIONCONTROL_SET;
     *   Wince has 0x1000
     */
    /* ASIC3_MMC_REG(host, SD_CTRL, TransactionCtrl) = 0x1000; */


    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_SDPWR, ASIC3_SDHWCTRL_SDPWR); /* turn on power at controller(?) */

}

static void
hwinit(struct asic3_mmc_host *host, struct platform_device *pdev)
{
    /* Call custom handler for enabling clock (if needed) */
    if(host->hwconfig->set_mmc_clock)
            host->hwconfig->set_mmc_clock(pdev, MMC_CLOCK_ENABLED);

    /* Not sure if it must be done bit by bit, but leaving as-is */
    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_LEVCD, ASIC3_SDHWCTRL_LEVCD);
    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_LEVWP, ASIC3_SDHWCTRL_LEVWP);
    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_SUSPEND, 0);
    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_PCLR, 0);

    asic3_set_clock_cdex (host->asic3_dev,
        CLOCK_CDEX_EX1 | CLOCK_CDEX_EX0, CLOCK_CDEX_EX1 | CLOCK_CDEX_EX0);
    msleep(1);

    asic3_set_clock_sel (host->asic3_dev,
        CLOCK_SEL_SD_HCLK_SEL | CLOCK_SEL_SD_BCLK_SEL,
	CLOCK_SEL_SD_HCLK_SEL | 0);	/* ? */

    asic3_set_clock_cdex (host->asic3_dev,
        CLOCK_CDEX_SD_HOST | CLOCK_CDEX_SD_BUS,
	CLOCK_CDEX_SD_HOST | CLOCK_CDEX_SD_BUS);
    msleep(1);

    asic3_set_extcf_select(host->asic3_dev, ASIC3_EXTCF_SD_MEM_ENABLE, ASIC3_EXTCF_SD_MEM_ENABLE);

    /* Long Delay */
    if( !machine_is_h4700())
        msleep(500);

    hwinit2_irqsafe(host);
}

#ifdef CONFIG_PM
static int
mmc_suspend(struct platform_device *pdev, pm_message_t state)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct asic3_mmc_host *host = mmc_priv(mmc);
    int ret;

    ret = mmc_suspend_host(mmc, state);

    if (ret) {
	printk(KERN_ERR DRIVER_NAME ": Could not suspend MMC host, hardware not suspended");
	return ret;
    }

    /* disable the card insert / remove interrupt while sleeping */
    ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard) = ~(
          SD_CTRL_INTMASKCARD_RESPONSE_END
        | SD_CTRL_INTMASKCARD_RW_END);

    /* disable clock */
    ASIC3_MMC_REG(host, SD_CTRL, CardClockCtrl) = 0;
    ASIC3_MMC_REG(host, SD_CONFIG, ClkStop) = 0;

    /* power down */
    ASIC3_MMC_REG(host, SD_CONFIG, SDHC_Power1) = 0;

    asic3_set_clock_cdex (host->asic3_dev,
        CLOCK_CDEX_SD_HOST | CLOCK_CDEX_SD_BUS, 0);

    /* disable core clock */
    if(host->hwconfig->set_mmc_clock)
        host->hwconfig->set_mmc_clock(pdev, MMC_CLOCK_DISABLED);

    /* Put in suspend mode */
    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_SUSPEND, ASIC3_SDHWCTRL_SUSPEND);
    return 0;
}

static int
mmc_resume(struct platform_device *pdev)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct asic3_mmc_host *host = mmc_priv(mmc);

    printk(KERN_INFO "%s: starting resume\n", DRIVER_NAME);

    asic3_set_sdhwctrl(host->asic3_dev, ASIC3_SDHWCTRL_SUSPEND, 0);
    hwinit(host, pdev);

    /* re-enable card remove / insert interrupt */
    ASIC3_MMC_REG(host, SD_CTRL, IntMaskCard) = ~(
          SD_CTRL_INTMASKCARD_RESPONSE_END
        | SD_CTRL_INTMASKCARD_RW_END
        | SD_CTRL_INTMASKCARD_CARD_REMOVED_0
        | SD_CTRL_INTMASKCARD_CARD_INSERTED_0 );

    mmc_resume_host(mmc);

    printk(KERN_INFO "%s: finished resume\n", DRIVER_NAME);
    return 0;
}
#endif

static int
mmc_probe(struct platform_device *pdev)
{
    struct mmc_host *mmc;
    struct asic3_mmc_host *host = NULL;
    int retval = 0;
    struct tmio_mmc_hwconfig *mmc_config = (struct tmio_mmc_hwconfig *)pdev->dev.platform_data;

    /* bus_shift is mandatory */
    if (!mmc_config) {
        printk(KERN_ERR DRIVER_NAME ": Invalid configuration\n");
    	return -EINVAL;
    }

    mmc = mmc_alloc_host(sizeof(struct asic3_mmc_host) + 128, &pdev->dev);
    if (!mmc) {
        retval = -ENOMEM;
        goto exceptional_return;
    }

    host = mmc_priv(mmc);
    host->mmc = mmc;
    platform_set_drvdata(pdev, mmc);

    host->ctl_base = 0;
    host->hwconfig = mmc_config;
    host->bus_shift = mmc_config->address_shift;
    host->asic3_dev = pdev->dev.parent;
    host->clock_for_sd = 0;

    tasklet_init(&mmc_data_read_tasklet, mmc_data_transfer, (unsigned long)host);

    host->ctl_base = ioremap_nocache ((unsigned long)pdev->resource[0].start, pdev->resource[0].end - pdev->resource[0].start);
    if(!host->ctl_base){
	printk(KERN_ERR DRIVER_NAME ": Could not map ASIC3 SD controller\n");
        retval = -ENODEV;
        goto exceptional_return;
    }

    printk(DRIVER_NAME ": ASIC3 MMC/SD Driver, controller at 0x%lx\n", (unsigned long)pdev->resource[0].start);

    mmc->ops = &mmc_ops;
    mmc->caps = MMC_CAP_4_BIT_DATA;
    mmc->f_min = 46875; /* ARIC: not sure what these should be */
    mmc->f_max = 24000000; /* ARIC: not sure what these should be */
    mmc->ocr_avail = MMC_VDD_32_33;

    hwinit(host, pdev);


    host->irq = pdev->resource[1].start;

    retval = request_irq(host->irq, mmc_irq, 0, DRIVER_NAME, host);
    if(retval) {
        printk(KERN_ERR DRIVER_NAME ": Unable to get interrupt\n");
        retval = -ENODEV;
        goto exceptional_return;
    }
    set_irq_type(host->irq, IRQT_FALLING);

    mmc_add_host(mmc);

#ifdef CONFIG_PM
    // resume_timer.function = resume_timer_callback;
    // resume_timer.data = 0;
    // init_timer(&resume_timer);
#endif

    return 0;

exceptional_return:
    if (mmc) {
        mmc_free_host(mmc);
    }
    if(host && host->ctl_base) iounmap(host->ctl_base);
    return retval;
}

static int
mmc_remove(struct platform_device *pdev)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);

    platform_set_drvdata(pdev, NULL);

    if (mmc) {
        struct asic3_mmc_host *host = mmc_priv(mmc);
        mmc_remove_host(mmc);
        free_irq(host->irq, host);
        /* FIXME - we might want to consider stopping the chip here... */
        iounmap(host->ctl_base);
        mmc_free_host(mmc); /* FIXME - why does this call hang? */
    }
    return 0;
}

/* ------------------- device registration ----------------------- */

static struct platform_driver mmc_asic3_driver = {
    .driver = {
	.name    = DRIVER_NAME,
    },
    .probe   = mmc_probe,
    .remove  = mmc_remove,
#ifdef CONFIG_PM
    .suspend = mmc_suspend,
    .resume  = mmc_resume,
#endif
};

static int __init mmc_init(void)
{
    return platform_driver_register(&mmc_asic3_driver);
}

static void __exit mmc_exit(void)
{
    platform_driver_unregister(&mmc_asic3_driver);
}

late_initcall(mmc_init);
module_exit(mmc_exit);

MODULE_DESCRIPTION("HTC ASIC3 SD/MMC driver");
MODULE_AUTHOR("Aric Blumer, SDG Systems, LLC");
MODULE_LICENSE("GPL");

