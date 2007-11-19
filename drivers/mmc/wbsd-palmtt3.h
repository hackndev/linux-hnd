/*
 *  linux/drivers/mmc/wbsd-palmtt3.h - Winbond W86L488 SD/MMC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>

// Direct registers
#define WBR_CMDR0       0x00
#define WBR_CMDR1       0x01
#define WBR_CR          0x02
#define WBR_SR          0x03
#define WBR_RTB0        0x04
#define WBR_RTB1        0x05
#define WBR_IER         0x06
#define WBR_ISR         0x07
#define WBR_GPC         0x08
#define WBR_GPD         0x09
#define WBR_GPIE        0x0A
#define WBR_GPIS        0x0B
#define WBR_IAR         0x0C
#define WBR_IDR0        0x0E
#define WBR_IDR1        0x0F

// Indirect registers
#define WBIR_CR         0x00
#define WBIR_NATO       0x06
#define WBIR_DLR        0x08
#define WBIR_MDFR       0x02
#define WBIR_MDCB       0x03
#define WBIR_SDFR       0x04
#define WBIR_SDCB       0x05
#define WBIR_MBCR       0x03
#define WBIR_ESR        0x07

#define CLOCKRATE_MIN   375000
#define CLOCKRATE_MAX   40000000

#define WBSD_EINT_INT		0x0001
#define WBSD_EINT_TOE		0x0002
#define WBSD_EINT_CRC		0x0004
#define WBSD_EINT_INS		0x0008
#define WBSD_EINT_GIT		0x0010
#define WBSD_EINT_DREQ		0x0020
#define WBSD_EINT_PRGE		0x0040
#define WBSD_EINT_CMDE		0x0080

#define WBSD_INT_TOE		0x0100
#define WBSD_INT_CRC		0x0400
#define WBSD_INT_DREQ		0x2000
#define WBSD_INT_PRGE		0x4000
#define WBSD_INT_CMDE		0x8000

#define WBSD_CLK_375K		0x00
#define WBSD_CLK_12M		0x01
#define WBSD_CLK_16M		0x02
#define WBSD_CLK_24M		0x03

#define WBSD_MODE_16b		0x0000
#define WBSD_MODE_8b		0x0001
#define WBSD_DLR_BLV		0x8000 //FIXME unknown bit

#define WBSD_SIEN		0x0010
#define WBSD_RESET_FIFO		0x0020
#define WBSD_RESET_CHIP		0x0040
#define WBSD_POWER_DOWN		0x0080

#define WBSD_CLEAN_TR_INT	0x0300
#define WBSD_RESP_R2		0x2000
#define WBSD_FIFO_EMPTY		0x0800
#define WBSD_FIFO_FULL		0x0400

#define WBSD_WIDTH_1		0x0000
#define WBSD_WIDTH_4		0x8000

#define WBSD_CMD_CRC		0x0001
#define WBSD_CMD_NOCRC		0x0002
#define WBSD_DATA_NOCRC		0x0004
#define WBSD_CMD0_HI		0x0008
#define WBSD_NER_TO_MAX		0x0070
#define WBSD_MODE_MMC		0x0100

#define WBSD_NATO_MAX		0x7FFF

#define RREADB(x) readb(host->base + x)
#define RREADW(x) readw(host->base + x)
#define RWRITEB(x, v) writeb(v, host->base + x)
#define RWRITEW(x, v) writew(v, host->base + x)
#define SET_IDX_ADDR(x) RWRITEW(WBR_IAR, x << 1)
#define SET_IDX_DATA(x) RWRITEW(WBR_IDR0, x)
#define GET_IDX_DATA() RREADW(WBR_IDR0)

#define TPS_LED_ON		0x0001
#define TPS_LED_OFF		0x0002
#define TPS_POWER_ON		0x0004
#define TPS_POWER_OFF		0x0008

struct wbsd_host
{
	struct mmc_host*	mmc;		/* MMC structure */

	spinlock_t		lock;		/* Mutex */

	int			flags;		/* Driver states */

#define WBSD_FCARD_PRESENT	(1<<0)		/* Card is present */
#define WBSD_FIGNORE_DETECT	(1<<1)		/* Ignore card detection */

	struct mmc_request*	mrq;		/* Current request */

	struct scatterlist*	cur_sg;		/* Current SG entry */
	unsigned int		num_sg;		/* Number of entries left */
	void*			mapped_sg;	/* vaddr of mapped sg */

	unsigned int		offset;		/* Offset into current entry */
	unsigned int		remain;		/* Data left in curren entry */

	int			size;		/* Total size of transfer */

	char*			dma_buffer;	/* ISA DMA buffer */
	dma_addr_t		dma_addr;	/* Physical address for same */

	u8			clk;		/* Current clock speed */
	unsigned char		bus_width;	/* Current bus width */

	int			base;		/* I/O port base */
	int			irq;		/* Interrupt */
	int			dma;		/* DMA channel */

	struct tasklet_struct	card_tasklet;	/* Tasklet structures */
	struct tasklet_struct	fifo_tasklet;
	struct tasklet_struct	crc_tasklet;
	struct tasklet_struct	timeout_tasklet;
	struct tasklet_struct	finish_tasklet;

	struct work_struct tps_work;

	int			tps_duty;
	struct timer_list	delayed_timer;	/* Ignore detection timer */
};
