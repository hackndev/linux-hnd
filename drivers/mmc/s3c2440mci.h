/*
 *  linux/drivers/mmc/s3c2410mci.h - Samsung S3C2410 SDI Interface driver
 *
 *  Copyright (C) 2004 Thomas Kleffel, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct clk;

#define S3C2410SDI_DMA 0

#define S3C2410SDI_CDLATENCY 50

enum s3c2410sdi_waitfor {
	COMPLETION_NONE,
	COMPLETION_CMDSENT,
	COMPLETION_RSPFIN,
	COMPLETION_XFERFINISH,
	COMPLETION_XFERFINISH_RSPFIN,
};

typedef struct s3c24xx_mmc_platdata s3c24xx_mmc_pdata_t;

struct s3c2410sdi_host {
	struct mmc_host		*mmc;
	s3c24xx_mmc_pdata_t	*pdata;

	struct resource		*mem;
	struct clk		*clk;
	void __iomem		*base;
	int			irq;
	int			irq_cd;
	int			dma;

	struct scatterlist*	cur_sg;		/* Current SG entry */
	unsigned int		num_sg;		/* Number of entries left */
	void*			mapped_sg;	/* vaddr of mapped sg */

	unsigned int		offset;		/* Offset into current entry */
	unsigned int		remain;		/* Data left in curren entry */

	int			size;		/* Total size of transfer */

	struct mmc_request	*mrq;

	unsigned char		bus_width;	/* Current bus width */

	spinlock_t		complete_lock;
	struct completion	complete_request;
	struct completion	complete_dma;
	enum s3c2410sdi_waitfor	complete_what;

	long *pio_ptr;
	u32 pio_words;
};
