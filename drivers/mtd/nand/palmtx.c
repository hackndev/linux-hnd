/*
 *  drivers/mtd/nand/palmtx.c
 *
 *  Copyright (C) 2007 Marek Vasut (marek.vasut@gmail.com)
 *
 *  Derived from drivers/mtd/nand/h1910.c
 *       Copyright (C) 2003 Joshua Wise (joshua@joshuawise.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   Palm T|X board which utilizes the Samsung K9F1G08U0A part. This is
 *   a 1Gbit (128MiB x 8 bits) NAND flash device.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <asm/arch/palmtx-gpio.h>
#include <asm/arch/palmtx-init.h>
#include <asm/arch/pxa-regs.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>

/*
 * MTD structure
 */
static struct mtd_info *palmtx_nand_mtd = NULL;

/* 
 * Control lines
 */
void __iomem *nand_ale = NULL;
void __iomem *nand_cle = NULL;

/*
 * Module stuff
 */

#ifdef CONFIG_MTD_PARTITIONS
/*
 * Define static partitions for flash device
 */
static struct mtd_partition partition_info[] = {
      {name:"PalmTX NAND Flash",
	      offset:0,
      size:128 * 1024 * 1024}
};

#define NUM_PARTITIONS 1

#endif

/*
 * Hardware specific access to control-lines
 */
static void palmtx_hwcontrol(struct mtd_info *mtd, int cmd,
			    unsigned int ctrl)
{
	/* If there is a command for the chip, send it */
        if (cmd != NAND_CMD_NONE) {
	    switch ((ctrl & 0x6) >> 1) {
		case 1: /* CLE */
		    iowrite8(cmd, nand_cle);
		    break;
		case 2: /* ALE */
		    iowrite8(cmd, nand_ale);
		    break;
		default:
		    printk("PalmTX NAND: invalid control bit\n");
		    break;
	    }
	}
}

static int palmtx_device_ready(struct mtd_info *mtd)
{
	return GET_PALMTX_GPIO(NAND_READY);
}

/*
 * Main initialization routine
 */
static int __init palmtx_init(void)
{
	struct nand_chip *this;
	const char *part_type = 0;
	int mtd_parts_nb = 0;
	struct mtd_partition *mtd_parts = 0;
	void __iomem *nandaddr;

	if (!machine_is_xscale_palmtx())
		return -ENODEV;

	/* Allocate memory for MTD device structure and private data */
	palmtx_nand_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!palmtx_nand_mtd) {
		printk("Unable to allocate palmtx NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* Remap physical address of flash */
	nandaddr = ioremap(PALMTX_PHYS_NAND_START, 0x1000);
	if (!nandaddr) {
		printk("Failed to ioremap NAND flash.\n");
		iounmap((void *)palmtx_nand_mtd);
		return -ENOMEM;
	}

	/* Remap physical address of control lines */
	nand_ale = ioremap(PALMTX_PHYS_NAND_START | 1<<24, 0x1000);
	if (!nand_ale) {
		printk("Failed to ioremap NAND flash.\n");
		iounmap((void *)palmtx_nand_mtd);
		iounmap((void *)nandaddr);
		return -ENOMEM;
	}
	nand_cle = ioremap(PALMTX_PHYS_NAND_START | 1<<25, 0x1000);
	if (!nand_cle) {
		printk("Failed to ioremap NAND flash.\n");
		iounmap((void *)palmtx_nand_mtd);
		iounmap((void *)nandaddr);
		iounmap((void *)nand_ale);
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&palmtx_nand_mtd[1]);

	/* Initialize structures */
	memset(palmtx_nand_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	palmtx_nand_mtd->priv = this;
	palmtx_nand_mtd->owner = THIS_MODULE;

	/* insert callbacks */
	this->IO_ADDR_R = nandaddr;
	this->IO_ADDR_W = nandaddr;
	this->cmd_ctrl = palmtx_hwcontrol;
	this->dev_ready = palmtx_device_ready;
	/* 15 us command delay time */
	this->chip_delay = 15;
	this->ecc.mode = NAND_ECC_SOFT;
	this->options = NAND_NO_AUTOINCR;

	/* Scan to find existence of the device */
	if (nand_scan(palmtx_nand_mtd, 1)) {
		printk(KERN_NOTICE "No NAND device - returning -ENXIO\n");
		kfree(palmtx_nand_mtd);
		iounmap((void *)palmtx_nand_mtd);
		iounmap((void *)nandaddr);
		iounmap((void *)nand_ale);
		iounmap((void *)nand_cle);
		return -ENXIO;
	}
#ifdef CONFIG_MTD_CMDLINE_PARTS
	mtd_parts_nb = parse_cmdline_partitions(palmtx_nand_mtd, &mtd_parts, "palmtx-nand");
	if (mtd_parts_nb > 0)
		part_type = "command line";
	else
		mtd_parts_nb = 0;
#endif
	if (mtd_parts_nb == 0) {
		mtd_parts = partition_info;
		mtd_parts_nb = NUM_PARTITIONS;
		part_type = "static";
	}

	/* Register the partitions */
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(palmtx_nand_mtd, mtd_parts, mtd_parts_nb);

	/* Return happy */
	return 0;
}

module_init(palmtx_init);

/*
 * Clean up routine
 */
static void __exit palmtx_cleanup(void)
{
	struct nand_chip *this = (struct nand_chip *)&palmtx_nand_mtd[1];

	/* Release resources, unregister device */
	nand_release(palmtx_nand_mtd);

	/* Release io resource */
	iounmap((void *)this->IO_ADDR_W);

	/* Free the MTD device structure */
	kfree(palmtx_nand_mtd);
}

module_exit(palmtx_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("NAND flash driver for Palm T|X");
