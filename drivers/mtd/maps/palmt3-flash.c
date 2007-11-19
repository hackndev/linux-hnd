/*
 * Palm T|T3 flash support
 *
 * $Id$
 *
 * Written by Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 * original code and (C) by
 *  Jungjun Kim <jungjun.kim@hynix.com>
 *  Thomas Gleixner <tglx@linutronix.de>
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

static struct mtd_info *mymtd;

static struct map_info palmt3_map = {
	.name =		"PALMTT3FLASH",
	.bankwidth =	2,
	.size =		0x1000000, // 16mb
	.phys =		0x0, // starting at CS0
};

static struct mtd_partition palmt3_partitions[] = {
        {
                .name = "SmallROM",
                .size = 0x20000, // 128kb
                .offset = 0,
                .mask_flags = MTD_WRITEABLE
	},{
		.name = "U-BOOT",
		.size = 0x20000, // 128kb
		.offset = 0x20000,
                .mask_flags = MTD_WRITEABLE
        },{
                .name = "BigROM",
                .size = 0xe00000, // 16mb - 256kb(smallrom) - 256kb - 512kb (14Mb)
                .offset = 0x40000,
		.mask_flags = MTD_WRITEABLE,
	},{
		.name = "uboot-env",
		.size = 128*1024, // 128kb
		.offset = 0x00fe0000,
	}
};

#define NUM_PARTITIONS  (sizeof(palmt3_partitions)/sizeof(palmt3_partitions[0]))

static int                   nr_mtd_parts;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };

/*
 * Initialize FLASH support
 */
int __init palmt3_mtd_init(void)
{

	char	*part_type = NULL;

	palmt3_map.virt = ioremap(0x0, 0x1000000);

	if (!palmt3_map.virt) {
		printk(KERN_ERR "T|T3-MTD: ioremap failed\n");
		return -EIO;
	}

	simple_map_init(&palmt3_map);

	// Probe for flash bankwidth 2
	printk (KERN_INFO "T|T3-MTD probing 16bit FLASH\n");
	mymtd = do_map_probe("cfi_probe", &palmt3_map);

	if (mymtd) {
		mymtd->owner = THIS_MODULE;

#ifdef CONFIG_MTD_PARTITIONS
		nr_mtd_parts = parse_mtd_partitions(mymtd, probes, &mtd_parts, 0);
		if (nr_mtd_parts > 0)
			part_type = "command line";
#endif
		if (nr_mtd_parts <= 0) {
			mtd_parts = palmt3_partitions;
			nr_mtd_parts = NUM_PARTITIONS;
			part_type = "builtin";
		}
		
		/*
		printk(KERN_ERR "WARNING, FLASH PARTITION 2 (/dev/mtdblock2) WILL BE UNLOCKED FOR WRITES!\n");
		mymtd->unlock(mymtd, 0xd00000, 3*1024*1024);
		mymtd->unlock(mymtd, 0, 0x40000);
		*/

		printk(KERN_INFO "Using %s partition table\n", part_type);
		add_mtd_partitions(mymtd, mtd_parts, nr_mtd_parts);
		return 0;
	}

	iounmap((void *)palmt3_map.virt);
	return -ENXIO;
}

/*
 * Cleanup
 */
static void __exit palmt3_mtd_cleanup(void)
{

	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}

	/* Free partition info, if commandline partition was used */
	if (mtd_parts && (mtd_parts != palmt3_partitions))
		kfree (mtd_parts);

	if (palmt3_map.virt) {
		iounmap((void *)palmt3_map.virt);
		palmt3_map.virt = 0;
	}
}


module_init(palmt3_mtd_init);
module_exit(palmt3_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Pouzanov <farcaller@gmail.com>");
MODULE_DESCRIPTION("MTD map driver for Palm T|T3");
