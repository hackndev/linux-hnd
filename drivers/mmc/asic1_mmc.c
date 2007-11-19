/*
 * Low-level MMC functions for the iPAQ H3800
 *
 * Copyright 2002 Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Author:  Andrew Christian
 *          6 May 2002
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include <asm/irq.h>        /* H3800-specific interrupts */
#include <asm/unaligned.h>

#include <asm/arch/hardware.h>
#include <asm/arch-sa1100/h3600_asic.h>

#include <linux/mmc/mmc_ll.h>

#include "h3600_asic_mmc.h"
#include "h3600_asic_core.h"

struct response_info {
	int length;
	u16 cdc_flags;
};

/* straight out of the MMC spec.  Different commands return different length responses */
static struct response_info rinfo[] = {
	{ 0,  MMC_CMD_DATA_CONT_FORMAT_NO_RESPONSE                     }, /* R0 */
	{ 6,  MMC_CMD_DATA_CONT_FORMAT_R1                              }, /* R1  */
	{ 6,  MMC_CMD_DATA_CONT_FORMAT_R1 | MMC_CMD_DATA_CONT_BUSY_BIT }, /* R1b */
	{ 17, MMC_CMD_DATA_CONT_FORMAT_R2                              }, /* R2 CID */
	{ 17, MMC_CMD_DATA_CONT_FORMAT_R2                              }, /* R2 CSD */
	{ 6,  MMC_CMD_DATA_CONT_FORMAT_R3                              }, /* R3  */
	{ 6,  0                                                        }, /* R4  */
	{ 6,  0                                                        }, /* R5  */
};

enum h3800_request_type {
	RT_NO_RESPONSE,
	RT_RESPONSE_ONLY,
	RT_READ,
	RT_WRITE
};

struct h3800_mmc_data {
	struct timer_list        sd_detect_timer;
	struct timer_list        reset_timer;
	struct timer_list        irq_timer;    /* Panic timer - make sure we get a response */
	u32                      clock;        /* Current clock frequency */
	struct mmc_request      *request;
	enum h3800_request_type  type;
};

static struct h3800_mmc_data g_h3800_data;

#define MMC_IRQ_TIMEOUT  (3 * HZ)
#define H3800_MASTER_CLOCK 33868800

static __inline__ void mmc_delay( void ) { udelay(1); }

#ifdef CONFIG_MMC_DEBUG
/**************************************************************************
 * Utility routines for debuging
 **************************************************************************/

struct cmd_to_name {
	int   id;
	char *name;
};

static struct cmd_to_name cmd_names[] = {
	{ MMC_CIM_RESET, "CIM_RESET" },
	{ MMC_GO_IDLE_STATE, "GO_IDLE_STATE" },
	{ MMC_SEND_OP_COND, "SEND_OP_COND" },
	{ MMC_ALL_SEND_CID, "ALL_SEND_CID" },
	{ MMC_SET_RELATIVE_ADDR, "SET_RELATIVE_ADDR" },
	{ MMC_SET_DSR, "SET_DSR" },
	{ MMC_SELECT_CARD, "SELECT_CARD" },
	{ MMC_SEND_CSD, "SEND_CSD" },
	{ MMC_SEND_CID, "SEND_CID" },
	{ MMC_READ_DAT_UNTIL_STOP, "READ_DAT_UNTIL_STOP" },
	{ MMC_STOP_TRANSMISSION, "STOP_TRANSMISSION" },
	{ MMC_SEND_STATUS	, "SEND_STATUS	" },
	{ MMC_GO_INACTIVE_STATE, "GO_INACTIVE_STATE" },
	{ MMC_SET_BLOCKLEN, "SET_BLOCKLEN" },
	{ MMC_READ_SINGLE_BLOCK, "READ_SINGLE_BLOCK" },
	{ MMC_READ_MULTIPLE_BLOCK, "READ_MULTIPLE_BLOCK" },
	{ MMC_WRITE_DAT_UNTIL_STOP, "WRITE_DAT_UNTIL_STOP" },
	{ MMC_SET_BLOCK_COUNT, "SET_BLOCK_COUNT" },
	{ MMC_WRITE_BLOCK, "WRITE_BLOCK" },
	{ MMC_WRITE_MULTIPLE_BLOCK, "WRITE_MULTIPLE_BLOCK" },
	{ MMC_PROGRAM_CID, "PROGRAM_CID" },
	{ MMC_PROGRAM_CSD, "PROGRAM_CSD" },
	{ MMC_SET_WRITE_PROT, "SET_WRITE_PROT" },
	{ MMC_CLR_WRITE_PROT, "CLR_WRITE_PROT" },
	{ MMC_SEND_WRITE_PROT, "SEND_WRITE_PROT" },
	{ MMC_ERASE_GROUP_START, "ERASE_GROUP_START" },
	{ MMC_ERASE_GROUP_END, "ERASE_GROUP_END" },
	{ MMC_ERASE, "ERASE" },
	{ MMC_FAST_IO, "FAST_IO" },
	{ MMC_GO_IRQ_STATE, "GO_IRQ_STATE" },
	{ MMC_LOCK_UNLOCK, "LOCK_UNLOCK" },
	{ MMC_APP_CMD, "APP_CMD" },
	{ MMC_GEN_CMD, "GEN_CMD" },
};

static char * get_cmd_name( int cmd )
{
	int i;
	int len = sizeof(cmd_names) / sizeof(struct cmd_to_name);
	for ( i = 0 ; i < len ; i++ )
		if ( cmd == cmd_names[i].id )
			return cmd_names[i].name;

	return "UNKNOWN";
}

struct status_bit_to_name {
	u16   mask;
	int   shift;
	char *name;
};

struct status_bit_to_name status_bit_names[] = {
	{ MMC_STATUS_READ_TIMEOUT, -1, "READ_TIMEOUT" },
	{ MMC_STATUS_RESPONSE_TIMEOUT, -1, "RESPONSE_TIMEOUT" },
	{ MMC_STATUS_CRC_WRITE_ERROR, -1, "CRC_WRITE_ERROR" },
	{ MMC_STATUS_CRC_READ_ERROR, -1, "CRC_READ_ERROR" },
	{ MMC_STATUS_SPI_READ_ERROR, -1, "SPI_READ_ERROR" },
	{ MMC_STATUS_CRC_RESPONSE_ERROR, -1, "CRC_RESPONSE_ERROR" },
	{ MMC_STATUS_FIFO_EMPTY, -1, "FIFO_EMPTY" },
	{ MMC_STATUS_FIFO_FULL, -1, "FIFO_FULL" },
	{ MMC_STATUS_CLOCK_ENABLE, -1, "CLOCK_ENABLE" },
	{ MMC_STATUS_WR_CRC_ERROR_CODE, 9, "WR_CRC_ERROR_CODE" },
	{ MMC_STATUS_DATA_TRANSFER_DONE, -1, "DATA_TRANSFER_DONE" },
	{ MMC_STATUS_END_PROGRAM, -1, "END_PROGRAM" },
	{ MMC_STATUS_END_COMMAND_RESPONSE, -1, "END_COMMAND_RESPONSE" }
};

static void decode_status( u16 status )
{
	int i;
	int len = sizeof(status_bit_names)/sizeof(struct status_bit_to_name);
	struct status_bit_to_name *b = status_bit_names;
	for ( i = 0 ; i < len ; i++, b++ ) {
		if ( status & b->mask ) {
			if ( b->shift >= 0 )
				printk("%s[%d] ", b->name, ((status & b->mask) >> b->shift));
			else
				printk("%s ", b->name);
		}
	}
}

static void printk_block(u8 *b, unsigned len) {
	unsigned i;

	printk(KERN_DEBUG);
	for ( i = 0 ; i < len ; i++ ) {
		printk(" %02x", *b++);
		if ( ((i + 1) % 16) == 0 && i < len - 1)
			printk("\n" KERN_DEBUG);
	}
	printk("\n");
}

static void printk_request( struct mmc_request *request, char *header )
{
	printk(KERN_DEBUG "%sindex  %d\n", header, request->index );
	printk(KERN_DEBUG "%scmd    %d\n", header, request->cmd );
	printk(KERN_DEBUG "%sarg    0x%08x\n", header, request->arg );
	printk(KERN_DEBUG "%srtype  %d\n", header, request->rtype );
	printk(KERN_DEBUG "%snob    %d\n", header, request->nob );
	printk(KERN_DEBUG "%sbl_len %d\n", header, request->block_len );
	printk(KERN_DEBUG "%sbuffer %p\n", header, request->buffer );
	printk(KERN_DEBUG "%sresult %d\n", header, request->result);
}
#endif /* CONFIG_MMC_DEBUG */

/*
static void printk_asic_state( char *header )
{
	u16 status = H3800_ASIC1_MMC_Status;
	printk("%sStartStopClock  0x%02x\n", header, H3800_ASIC1_MMC_StartStopClock );
	printk("%sStatus          0x%04x  ", header, status );
	decode_status(status);
	printk("\n%sClockRate       0x%02x\n", header, H3800_ASIC1_MMC_ClockRate );
	printk("%sRevision        0x%08x\n", header, H3800_ASIC1_MMC_Revision );
	printk("%sCmdDataCont     0x%02x\n", header, H3800_ASIC1_MMC_CmdDataCont );
	printk("%sResponseTimeout 0x%02x\n", header, H3800_ASIC1_MMC_ResponseTimeout );
	printk("%sReadTimeout     0x%04x\n", header, H3800_ASIC1_MMC_ReadTimeout );
	printk("%sBlock Length    0x%04x\n", header, H3800_ASIC1_MMC_BlockLength );
	printk("%sNum of Blocks   0x%04x\n", header, H3800_ASIC1_MMC_NumOfBlocks );
	printk("%sInterruptMask   0x%02x\n", header, H3800_ASIC1_MMC_InterruptMask );
	printk("%sCommandNumber   0x%02x\n", header, H3800_ASIC1_MMC_CommandNumber );
	printk("%sArgument H      0x%04x\n", header, H3800_ASIC1_MMC_ArgumentH );
	printk("%sArgument L      0x%04x\n", header, H3800_ASIC1_MMC_ArgumentL );
	printk("%sBufferPartFull  0x%02x\n", header, H3800_ASIC1_MMC_BufferPartFull );
	printk("%sStatus          0x%04x\n", header, H3800_ASIC1_MMC_Status );
}
*/

/**************************************************************************
 *   Clock routines
 *
 * We have to poll the status register until we're sure that the clock has stopped.
 * This may be called in interrupt context, so we can't go to sleep!
 **************************************************************************/

static int mmc_h3800_stop_clock( void )
{
	u16 status;

	H3800_ASIC1_MMC_StartStopClock = MMC_STOP_CLOCK;
	mmc_delay();
	
	status = H3800_ASIC1_MMC_Status;
	if ( !(status & ~(MMC_STATUS_CLOCK_ENABLE | MMC_STATUS_FIFO_EMPTY | MMC_STATUS_FIFO_FULL)) )
		return MMC_NO_ERROR;

	DEBUG(0,": Warning!  Clock not stopping correctly (%04x)", status);
	H3800_ASIC1_MMC_StartStopClock = MMC_START_CLOCK;
	mmc_delay();

	H3800_ASIC1_MMC_StartStopClock = MMC_STOP_CLOCK;
	mmc_delay();
	
	status = H3800_ASIC1_MMC_Status;
	if ( !(status & ~(MMC_STATUS_CLOCK_ENABLE | MMC_STATUS_FIFO_EMPTY | MMC_STATUS_FIFO_FULL)) )
		return MMC_NO_ERROR;

	DEBUG(0,": Error!  Clock not stopping correctly (%04x)", status);

	return MMC_ERROR_DRIVER_FAILURE;
}

static void mmc_h3800_start_clock( void )
{
	H3800_ASIC1_MMC_StartStopClock = MMC_START_CLOCK;
	mmc_delay();
}

static int mmc_h3800_set_clock( u32 rate )
{
	int retval;

	u32 master = H3800_MASTER_CLOCK;   /* Default master clock */
	u8  divisor = 0;         /* No divisor */
	while ( master > rate ) {
		divisor++;
		master /= 2;
	}
	if ( rate > 6 ) rate = 6;
	DEBUG(2,": setting divisor to %d (request=%d result=%d)", 
	      divisor, rate, master );

	retval = mmc_h3800_stop_clock();
	if ( retval )
		return retval;

	H3800_ASIC1_MMC_ClockRate = divisor;
	mmc_delay();

	H3800_ASIC1_MMC_ResponseTimeout = 0x0040;
	mmc_delay();
	
	H3800_ASIC1_MMC_ReadTimeout = 0xffff;
	mmc_delay();

	mmc_h3800_start_clock();
	g_h3800_data.clock = master;
	
	return MMC_NO_ERROR;
}

static int mmc_h3800_reset_asic( u32 clock_rate )
{
	int retval;
	u16 version = H3800_ASIC1_MMC_Revision;

	DEBUG(2," version=%04x", version);
	mmc_delay();
	H3800_ASIC1_MMC_Revision = version;
	mmc_delay();

	udelay(5);

	retval = mmc_h3800_set_clock( clock_rate );
	mmc_h3800_start_clock();
	return retval;
}

/* 
   The reset function clears _everything_ in the ASIC,
   so it needs be completely reset.  This code is the
   continuation of the mmc_h3800_reset command.  The
   reset takes the asic up to 100ms to execute.
*/
static void mmc_h3800_reset_timeout( unsigned long nr )
{
//	struct h3800_mmc_data *sd = (struct h3800_mmc_data *) nr;

	/* Send 80 clocks to get things started */
	H3800_ASIC1_MMC_CmdDataCont = MMC_CMD_DATA_CONT_BC_MODE;
	mmc_delay();
	H3800_ASIC1_MMC_InterruptMask = MMC_INT_MASK_ALL & ~MMC_INT_MASK_END_COMMAND_RESPONSE;
	mmc_delay();

	START_DEBUG(2) { 
		u16 status = H3800_ASIC1_MMC_Status;
		printk(KERN_DEBUG "%s: enabling irq mask=%04x status=0x%04x (", __FUNCTION__, 
		       H3800_ASIC1_MMC_InterruptMask, status);
		decode_status(status); 
		printk(")\n"); 
	} END_DEBUG;

	mod_timer( &g_h3800_data.irq_timer, jiffies + MMC_IRQ_TIMEOUT); 
	enable_irq( IRQ_GPIO_H3800_MMC_INT );
}

static int mmc_h3800_reset( struct h3800_mmc_data *sd )
{
	int retval = mmc_h3800_reset_asic( MMC_CLOCK_SLOW );
	g_h3800_data.type = RT_NO_RESPONSE;
	mod_timer( &sd->reset_timer, jiffies + (100 * HZ) / 1000 );
	g_ipaq_asic_statistics.mmc_reset++;

	return retval;
}


static void mmc_h3800_set_command( u16 cmd, u32 arg )
{
	DEBUG(2,": cmd=%d arg=0x%08x", cmd, arg);

	H3800_ASIC1_MMC_ArgumentH = arg >> 16;
	mmc_delay();

	H3800_ASIC1_MMC_ArgumentL = arg & 0xffff;
	mmc_delay();

	H3800_ASIC1_MMC_CommandNumber = cmd;
	mmc_delay();
}

static void mmc_h3800_set_transfer( u16 block_len, u16 nob )
{
	DEBUG(2,": block_len=%d nob=%d", block_len, nob);

	H3800_ASIC1_MMC_BlockLength = block_len;
	mmc_delay();

	H3800_ASIC1_MMC_NumOfBlocks = nob;
	mmc_delay();
}

static void mmc_h3800_transmit_data( struct mmc_request *request )
{
	u8 *buf = request->buffer;
	u16 data;
	int i;

	DEBUG(2,": nob=%d block_len=%d", request->nob, request->block_len);
	if ( request->nob <= 0 ) {
		DEBUG(1,": *** nob already at 0 ***");
		return;
	}

	for ( i = 0 ; i < request->block_len ; i+=2, buf+=2 ) {
		data = *(buf+1) | (((u16) *buf) << 8);
		H3800_ASIC1_MMC_DataBuffer = data;
		mmc_delay();
	}

	START_DEBUG(2) { 
		u16 status = H3800_ASIC1_MMC_Status;
		printk(KERN_DEBUG "%s: irq_mask=%04x status=0x%04x (", __FUNCTION__, 
		       H3800_ASIC1_MMC_InterruptMask, status);
		decode_status(status); 
		printk(")\n"); 
	} END_DEBUG;

	START_DEBUG(3) {
		printk_block(request->buffer, request->block_len);
	} END_DEBUG;

	/* Updated the request buffer to reflect the current state */
	request->buffer = (u8 *) buf;
	request->nob--;
	g_ipaq_asic_statistics.mmc_written++;
}

static void mmc_h3800_receive_data( struct mmc_request *request )
{
	u8 *buf = request->buffer;
	u16 data;
	int i;

	DEBUG(2,": nob=%d block_len=%d buf=%p", request->nob, request->block_len, buf);
	if ( request->nob <= 0 ) {
		DEBUG(1,": *** nob already at 0 ***");
		return;
	}

	for ( i = 0 ; i < request->block_len ; i+=2 ) {
		data = H3800_ASIC1_MMC_DataBuffer;
		mmc_delay();
		*buf++ = data >> 8;
		*buf++ = data & 0xff;
	}

	START_DEBUG(3) {
		printk_block(request->buffer, request->block_len);
	} END_DEBUG;

	/* Updated the request buffer to reflect the current state */
	request->buffer = (u8 *) buf;
	request->nob--;
	g_ipaq_asic_statistics.mmc_read++;
}

#define STATBUG(_x) \
	{ u16 status = H3800_ASIC1_MMC_Status; \
          if ( status & ~(MMC_STATUS_FIFO_EMPTY | MMC_STATUS_CLOCK_ENABLE)) \
		printk("..." _x " status=0x%04x\n", status); }

static int mmc_h3800_exec_command( struct mmc_request *request )
{
	int retval;
	int cdc = 0;
	int irq = 0;

	DEBUG(2,": request=%p status=%04x", request, H3800_ASIC1_MMC_Status );

	/* Start the actual transfer */
	retval = mmc_h3800_stop_clock();
	if ( retval )
		return retval;

	STATBUG("stop_clock");

	mmc_h3800_set_command( request->cmd, request->arg );

	STATBUG("set_command");

	switch (request->cmd) {
	case MMC_READ_SINGLE_BLOCK:
	case MMC_READ_MULTIPLE_BLOCK:
		mmc_h3800_set_transfer( request->block_len, request->nob );
		cdc = MMC_CMD_DATA_CONT_READ | MMC_CMD_DATA_CONT_DATA_ENABLE;
		irq = MMC_INT_MASK_ALL;
		g_h3800_data.type = RT_READ;
		break;
	case MMC_WRITE_BLOCK:
	case MMC_WRITE_MULTIPLE_BLOCK:
		mmc_h3800_set_transfer( request->block_len, request->nob );
		cdc = MMC_CMD_DATA_CONT_WRITE | MMC_CMD_DATA_CONT_DATA_ENABLE;
		irq = MMC_INT_MASK_ALL;
		g_h3800_data.type = RT_WRITE;
		break;
	default:
		irq = MMC_INT_MASK_END_COMMAND_RESPONSE;
		g_h3800_data.type = RT_RESPONSE_ONLY;
		break;
	}
	
	cdc |= rinfo[request->rtype].cdc_flags;

	H3800_ASIC1_MMC_CmdDataCont = cdc;
	mmc_delay();

	STATBUG("cmd_data_cont");

	H3800_ASIC1_MMC_InterruptMask = MMC_INT_MASK_ALL & ~irq;
	mmc_delay();

	STATBUG("irq_mask");

	mmc_h3800_start_clock();

	START_DEBUG(2) { 
		u16 status = H3800_ASIC1_MMC_Status;
		printk(KERN_DEBUG "%s: enabling irq mask=%04x status=0x%04x (", __FUNCTION__, 
		       H3800_ASIC1_MMC_InterruptMask, status);
		decode_status(status); 
		printk(")\n"); 
	} END_DEBUG;

	mod_timer( &g_h3800_data.irq_timer, jiffies + MMC_IRQ_TIMEOUT); 
	enable_irq( IRQ_GPIO_H3800_MMC_INT );
	g_ipaq_asic_statistics.mmc_command++;
	return MMC_NO_ERROR;
}

/* wrapper around exec_command.  Make sure that it happens.  If it doesn't, reset
 * and try again, unless, of course, the command itself is a reset.
 */
static void mmc_h3800_send_command( struct mmc_request *request )
{
	int retval;

	DEBUG(1,": request=%p cmd=%d (%s) arg=%08x status=%04x", request, 
	      request->cmd, get_cmd_name(request->cmd), request->arg,
	      H3800_ASIC1_MMC_Status);

	/* TODO: Grab a lock???? */
	g_h3800_data.request = request;
	request->result = MMC_NO_RESPONSE;   /* Flag to indicate don't have a result yet */

	if ( request->cmd == MMC_CIM_RESET ) {
		retval = mmc_h3800_reset( &g_h3800_data );
	}
	else {
		retval = mmc_h3800_exec_command( request );
		if ( retval ) {
			g_ipaq_asic_statistics.mmc_error++;
			DEBUG(0,": ASIC not responding!  Trying to reset");
			mmc_h3800_start_clock();

			retval = mmc_h3800_reset_asic( g_h3800_data.clock );
			if ( retval ) {
				DEBUG(0,": ASIC doesn't reset!  Panic now!");
			}
			else {
				retval = mmc_h3800_exec_command( request );
				if ( retval ) {
					DEBUG(0,": ASIC unable to exec!");
				}
			}
		}
	}

	if ( retval ) {
		request->result = retval;
		mmc_cmd_complete( request );
	}
}

/**************************************************************************/
/* TODO: Need to mask interrupts?? */

static void mmc_h3800_get_response( struct mmc_request *request )
{
	int i;
	int len = rinfo[request->rtype].length;
	u8 *buf = request->response;
	
	request->result = MMC_NO_ERROR;    /* Mark this as having a request result of some kind */

	if ( len <= 0 )
		return;

	for ( i = 0 ; i < len ; ) {
		u16 data = H3800_ASIC1_MMC_ResFifo;
		buf[i++] = data >> 8;
		buf[i++] = data & 0xff;
	}

	START_DEBUG(2) {
		printk_block(buf, len);
	} END_DEBUG;
}

static void mmc_h3800_handle_int( struct h3800_mmc_data *sd, u16 status, int timeout )
{
	int retval = MMC_NO_ERROR;

	disable_irq( IRQ_GPIO_H3800_MMC_INT );

	if ( status & (MMC_STATUS_READ_TIMEOUT 
		       | MMC_STATUS_RESPONSE_TIMEOUT )) {
		retval = MMC_ERROR_TIMEOUT;
		goto terminate_int;
	}
	
	if ( status & ( MMC_STATUS_CRC_WRITE_ERROR 
			| MMC_STATUS_CRC_READ_ERROR 
			| MMC_STATUS_SPI_READ_ERROR 
			| MMC_STATUS_CRC_RESPONSE_ERROR
			| MMC_STATUS_WR_CRC_ERROR_CODE )) {
		retval = MMC_ERROR_CRC;
		goto terminate_int;
	}

	if ( status & MMC_STATUS_END_COMMAND_RESPONSE && sd->request->result == MMC_NO_RESPONSE ) 
		mmc_h3800_get_response( sd->request );

	if ( g_h3800_data.type == RT_READ && 
	     (status & (MMC_STATUS_FIFO_FULL | MMC_STATUS_DATA_TRANSFER_DONE)) != 0)
		mmc_h3800_receive_data( sd->request );

	if ( g_h3800_data.type == RT_WRITE && (status & MMC_STATUS_FIFO_EMPTY ) != 0
	     && sd->request->nob > 0 )
		mmc_h3800_transmit_data( sd->request );

	switch (g_h3800_data.type) {
	case RT_NO_RESPONSE:
		break;

	case RT_RESPONSE_ONLY:
		if ( sd->request->result < 0 ) {
			printk(KERN_INFO "%s: illegal interrupt - command hasn't finished\n", __FUNCTION__);
			retval = MMC_ERROR_TIMEOUT;
		}
		break;
	case RT_READ:
		if ( sd->request->nob ) {
			DEBUG(2,": read re-enabling IRQ mask=0x%04x", H3800_ASIC1_MMC_InterruptMask);
			mod_timer( &sd->irq_timer, jiffies + MMC_IRQ_TIMEOUT); 
			mmc_h3800_start_clock();
			enable_irq( IRQ_GPIO_H3800_MMC_INT );
			return;
		}
		break;
	case RT_WRITE:
		if ( sd->request->nob || !(status & MMC_STATUS_END_PROGRAM)  ) {
			DEBUG(2,": write re-enabling IRQ mask=0x%04x", H3800_ASIC1_MMC_InterruptMask);
			mod_timer( &sd->irq_timer, jiffies + MMC_IRQ_TIMEOUT); 
			mmc_h3800_start_clock();
			enable_irq( IRQ_GPIO_H3800_MMC_INT );
			return;
		}
		break;
	}
	
	DEBUG(2,": terminating status=0x%04x", H3800_ASIC1_MMC_Status );
terminate_int:
	H3800_ASIC1_MMC_InterruptMask = MMC_INT_MASK_ALL & ~MMC_INT_MASK_END_COMMAND_RESPONSE;
	del_timer_sync( &sd->irq_timer );
	sd->request->result = retval;
	mmc_cmd_complete( sd->request );
}

/* handle an interrupt because we got interrupted */
static void mmc_h3800_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct h3800_mmc_data *sd = (struct h3800_mmc_data *) dev_id;
	u16 status = H3800_ASIC1_MMC_Status;
	
	START_DEBUG(2) { 
		printk(KERN_DEBUG "%s: sd=%p status=0x%04x (", __FUNCTION__, sd, status);
		decode_status(status); 
		printk(")\n"); 
	} END_DEBUG;

	mmc_h3800_handle_int( sd, status, 0 );
}

/* handle an interrupt because we expected to get interrupted and didn't */
static void mmc_h3800_irq_timeout( unsigned long nr )
{
	struct h3800_mmc_data *sd = (struct h3800_mmc_data *) nr;
	u16 status = H3800_ASIC1_MMC_Status;

	START_DEBUG(0) { 
		printk(KERN_DEBUG "%s: irq_mask=%04x status=0x%04x (", __FUNCTION__, 
		       H3800_ASIC1_MMC_InterruptMask, status);
		decode_status(status); 
		printk(")\n" KERN_DEBUG "Request info:\n"); 
		printk_request( sd->request, "  " );
	} END_DEBUG;

	g_ipaq_asic_statistics.mmc_timeout++;
	mmc_h3800_handle_int( sd, status, 1 );
}


/**************************************************************************/

static void mmc_h3800_fix_sd_detect( unsigned long nr )
{
	int sd_card = (H3800_ASIC2_GPIOPIOD & GPIO2_SD_DETECT ? 1 : 0);
	int sd_con_slot = (H3800_ASIC2_GPIOPIOD & GPIO2_SD_CON_SLT ? 1 : 0);

	DEBUG(2,": card=%d con_slot=%d", sd_card, sd_con_slot);

	if ( sd_card )
		H3800_ASIC2_GPIINTESEL &= ~GPIO2_SD_DETECT; /* Falling */
	else
		H3800_ASIC2_GPIINTESEL |= GPIO2_SD_DETECT; /* Rising */

	if ( sd_card != sd_con_slot ) {
		mmc_insert(0);
		g_ipaq_asic_statistics.mmc_insert++;
	}
	else {
		mmc_eject(0);
		g_ipaq_asic_statistics.mmc_eject++;
	}
}

/* the ASIC interrupted us because it detected card insertion or removal.
 * wait 1/4 second and see which it was.
 */
static void mmc_h3800_sd_detect_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct h3800_mmc_data *sd = (struct h3800_mmc_data *) dev_id;
	mod_timer( &sd->sd_detect_timer, jiffies + (250 * HZ) / 1000 );
}

static int mmc_h3800_slot_is_empty( int slot )
{
	int sd_card = (H3800_ASIC2_GPIOPIOD & GPIO2_SD_DETECT ? 1 : 0);
	int sd_con_slot = (H3800_ASIC2_GPIOPIOD & GPIO2_SD_CON_SLT ? 1 : 0);

	return sd_card == sd_con_slot;
}


/**************************************************************************/

static unsigned long mmc_shared;

static void mmc_h3800_slot_up( void )
{
	H3800_ASIC2_GPIINTTYPE |= GPIO2_SD_DETECT;          /* Edge-type interrupt */
	if ( H3800_ASIC2_GPIOPIOD & GPIO2_SD_DETECT )
		H3800_ASIC2_GPIINTESEL &= ~GPIO2_SD_DETECT; /* Falling */
	else
		H3800_ASIC2_GPIINTESEL |= GPIO2_SD_DETECT;  /* Rising */

	/* Let's turn on everything. In the future, we should 
	   only do this when we know there is a card (???) */

	H3800_ASIC1_GPIO_OUT     |= GPIO1_SD_PWR_ON;
	ipaq_asic_shared_add( &mmc_shared, ASIC_SHARED_CLOCK_EX2 );
	H3800_ASIC2_CLOCK_Enable |= ASIC2_CLOCK_SD_3;

	enable_irq( IRQ_H3800_SD_DETECT );
}

static void mmc_h3800_slot_down( void )
{
	disable_irq( IRQ_H3800_SD_DETECT );

	H3800_ASIC2_CLOCK_Enable &= ~ASIC2_CLOCK_SD_MASK;
	ipaq_asic_shared_release( &mmc_shared, ASIC_SHARED_CLOCK_EX2 );
	H3800_ASIC1_GPIO_OUT     &= ~GPIO1_SD_PWR_ON;

	del_timer_sync(&g_h3800_data.sd_detect_timer);
	del_timer_sync(&g_h3800_data.reset_timer);
	del_timer_sync(&g_h3800_data.irq_timer);
}

int ipaq_asic_mmc_suspend(void)
{
	disable_irq( IRQ_GPIO_H3800_MMC_INT );
	mmc_h3800_slot_down();
	return 0;
}

void ipaq_asic_mmc_resume(void)
{
	mmc_h3800_slot_up();
	enable_irq( IRQ_GPIO_H3800_MMC_INT );	/* Not in slot_up because it starts out disabled */
}

static int mmc_h3800_slot_init( void )
{
	int retval;
	DEBUG(1,"");

	/* Set up timers */
	g_h3800_data.sd_detect_timer.function = mmc_h3800_fix_sd_detect;
	g_h3800_data.sd_detect_timer.data     = (unsigned long) &g_h3800_data;
	init_timer(&g_h3800_data.sd_detect_timer);
	
	g_h3800_data.reset_timer.function = mmc_h3800_reset_timeout;
	g_h3800_data.reset_timer.data     = (unsigned long) &g_h3800_data;
	init_timer(&g_h3800_data.reset_timer);
	
	g_h3800_data.irq_timer.function = mmc_h3800_irq_timeout;
	g_h3800_data.irq_timer.data     = (unsigned long) &g_h3800_data;
	init_timer(&g_h3800_data.irq_timer);
	
	/* Basic service interrupt */
	H3800_ASIC1_MMC_InterruptMask = MMC_INT_MASK_ALL;
	set_irq_type( IRQ_GPIO_H3800_MMC_INT, IRQT_RISING );
	retval = request_irq( IRQ_GPIO_H3800_MMC_INT, mmc_h3800_int,
			      SA_INTERRUPT, "mmc_h3800_int", &g_h3800_data );
	if ( retval ) {
		printk(KERN_CRIT "%s: unable to grab MMC IRQ\n", __FUNCTION__);
		return retval;
	}
	disable_irq( IRQ_GPIO_H3800_MMC_INT );

	mmc_h3800_slot_up(); /* Fixes the sd_detect interrupt direction */

	retval = request_irq( IRQ_H3800_SD_DETECT, mmc_h3800_sd_detect_int, 
			      SA_INTERRUPT, "mmc_h3800_sd_detect", &g_h3800_data );

	if ( retval ) {
		printk(KERN_CRIT "%s: unable to grab SD_DETECT IRQ\n", __FUNCTION__);
		mmc_h3800_slot_down();
		free_irq(IRQ_GPIO_H3800_MMC_INT, &g_h3800_data);
	}
	return retval;
}

static void mmc_h3800_slot_cleanup( void )
{
	DEBUG(1,"");

	mmc_h3800_slot_down();

	free_irq(IRQ_H3800_SD_DETECT, &g_h3800_data);
	free_irq(IRQ_GPIO_H3800_MMC_INT, &g_h3800_data);
}


/***********************************************************/

static struct mmc_slot_driver dops = {
	owner:     THIS_MODULE,
	name:      "H3800 MMC",
	ocr:       0x00ffc000,
	flags:     MMC_SDFLAG_MMC_MODE,

	init:      mmc_h3800_slot_init,
	cleanup:   mmc_h3800_slot_cleanup,
	is_empty:  mmc_h3800_slot_is_empty,
	send_cmd:  mmc_h3800_send_command,
	set_clock: mmc_h3800_set_clock,
};

int __init ipaq_asic_mmc_init(void)
{
	int retval;

	DEBUG(0,"");
	retval = mmc_register_slot_driver(&dops, 1);
	if ( retval < 0 )
		printk(KERN_INFO "%s: unable to register slot\n", __FUNCTION__);

	return retval;
}

void __exit ipaq_asic_mmc_cleanup(void)
{
	DEBUG(0,"");
	mmc_unregister_slot_driver(&dops);
}

struct device_driver ipaq_asic1_mmc_device_driver = {
	.name = "asic1 mmc",
	.probe    = ipaq_asic1_mmc_init,
	.shutdown = ipaq_asic1_mmc_cleanup,
	.suspend  = ipaq_asic1_mmc_suspend,
	.resume   = ipaq_asic1_mmc_resume
};
EXPORT_SYMBOL(ipaq_asic1_mmc_device_driver);
