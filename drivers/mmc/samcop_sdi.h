/*
 *  linux/drivers/mmc/samcop_mmc.h - SAMCOP SDI Interface driver
 *
 *  Copyright (C) 2004 Thomas Kleffel, All Rights Reserved.
 *  Copyright (C) 2005 Matthew Reimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

enum samcop_sdi_waitfor {
	COMPLETION_NONE,
	COMPLETION_CMDSENT,
	COMPLETION_RSPFIN,
	COMPLETION_XFERFINISH,
	COMPLETION_XFERFINISH_RSPFIN,
};

struct samcop_sdi_data {
	int		irq_cd;		/* card detect IRQ number */

	int		f_min;		/* minimum freq the controller can do */
	int		f_max;		/* maximum freq the controller can do */
	int		max_sectors;

	int		dma_chan;	/* DMA channel to use */
	void *		dma_devaddr;	/* device address to DMA to/from */
	int		hwsrcsel;	/* DMA source */
	int		xfer_unit;	/* number of bytes in a DMA xfer unit */

	int		timeout;	/* data/busy timeout (in units of SD clock cycles) */

	void	(*init)(struct device *dev);
	void	(*exit)(struct device *dev);
	u32	(*read_reg)(struct device *dev, u32 reg);
	void	(*write_reg)(struct device *dev, u32 reg, u32 val);
	void	(*card_power)(struct device *dev, int on, int clock_req);
};

struct samcop_sdi_host {
	struct device		*parent;
	struct platform_device	*sdev;
	int			asic_type;
	struct mmc_host		*mmc;

	struct samcop_sdi_data	*plat;

	struct resource		*mem;
	void __iomem		*base;
	int			irq;
	struct timer_list	detect_timer;
	struct clk		*clk;
	
	struct mmc_request	*mrq;

	spinlock_t		complete_lock;
	struct completion	complete_request;
	struct completion	complete_dma;
	enum samcop_sdi_waitfor	complete_what;
};
