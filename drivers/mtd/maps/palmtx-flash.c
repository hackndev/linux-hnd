/*
 * Palm T|X flash support
 *
 * Copyright (C) 2007 Marek Vasut <marek.vasut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/arch/pxa-regs.h>

static struct mtd_info *palmtx_mtd_info;

static struct map_info palmtx_map = {
	.name		= "PALMTX_FLASH",
	.bankwidth	= 1,
	.size		= 0x800000,	/* 8MB */
	.phys		= PXA_CS0_PHYS,	/* At PXA_CS0 */
};

static struct mtd_partition palmtx_partitions[] = {
        {
                .name		= "PalmTX ROM",
                .size		= 8 * 1024 * 1024, /* 8MB */
                .offset		= 0,
		/* Uncomment following if you dont 
		   like your device ... */	
		/* .mask_flags	= MTD_WRITEABLE */
	}
};

#define NUM_PARTITIONS	1

static int nr_mtd_parts;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };

/*
 * Initialize FLASH support
 */
int __init palmtx_mtd_init(void)
{
	char *part_type = NULL;

	palmtx_map.virt = ioremap(PXA_CS0_PHYS, palmtx_map.size);

	if (!palmtx_map.virt) {
		printk(KERN_ERR "PalmTX Flash: ioremap failed\n");
		return -EIO;
	}

	simple_map_init(&palmtx_map);

	/* Probe for flash bankwidth 1 */
	printk (KERN_INFO "PalmTX Flash: probing 8bit FLASH\n");
	palmtx_mtd_info = do_map_probe("cfi_probe", &palmtx_map);

	if (palmtx_mtd_info) {
		palmtx_mtd_info->owner = THIS_MODULE;

#ifdef CONFIG_MTD_PARTITIONS
		nr_mtd_parts = parse_mtd_partitions(palmtx_mtd_info, probes,
						    &mtd_parts, 0);
		if (nr_mtd_parts > 0)
			part_type = "command line";
#endif
		if (nr_mtd_parts <= 0) {
			mtd_parts = palmtx_partitions;
			nr_mtd_parts = NUM_PARTITIONS;
			part_type = "builtin";
		}

		printk(KERN_INFO "Using %s partition table\n", part_type);
		add_mtd_partitions(palmtx_mtd_info, mtd_parts, nr_mtd_parts);
		return 0;
	}

	iounmap((void *)palmtx_map.virt);
	return -ENXIO;
}

/*
 * Cleanup
 */
static void __exit palmtx_mtd_cleanup(void)
{

	if (palmtx_mtd_info) {
		del_mtd_partitions(palmtx_mtd_info);
		map_destroy(palmtx_mtd_info);
	}

	/* Free partition info, if commandline partition was used */
	if (mtd_parts && (mtd_parts != palmtx_partitions))
		kfree (mtd_parts);

	if (palmtx_map.virt) {
		iounmap((void *)palmtx_map.virt);
		palmtx_map.virt = 0;
	}
}


module_init(palmtx_mtd_init);
module_exit(palmtx_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("Flash driver for Palm TX");
