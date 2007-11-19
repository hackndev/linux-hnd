/*
 * Palm Tungsten|C MTD map
 *
 * Author: Marek Vasut <marek.vasut@gmail.com>
 *
 * Based on Palm T|T3 flash support by
 *    Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 * Original code and (C) by
 *    Jungjun Kim <jungjun.kim@hynix.com>
 *    Thomas Gleixner <tglx@linutronix.de>
 *
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
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

static struct map_info palmtc_map = {
	.name		= "palmtc-flash",
	.bankwidth	= 2,
	.size		= 0x800000, /* 8MB */
	.phys		= 0x0, /* starting at CS0 */
};

static struct mtd_partition palmtc_partitions[] = {
        {
                .name = "SmallROM",
                .size = 0x40000, /* 256kB */
                .offset = 0,
                .mask_flags = MTD_WRITEABLE
	},{
                .name = "BigROM",
                .size = 0x7c0000, /* 8MB - 256kB (smallrom) */
                .offset = 0x40000,
		.mask_flags = MTD_WRITEABLE,
	}
};

#define NUM_PARTITIONS  (sizeof(palmtc_partitions)/sizeof(palmtc_partitions[0]))

static int nr_mtd_parts;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };

/*
 * Initialize FLASH support
 */
int __init palmtc_mtd_init(void)
{

	char *part_type = NULL;

	palmtc_map.virt = ioremap(0x0, 0x1000000);

	if (!palmtc_map.virt) {
		printk(KERN_ERR "palmtc-flash: ioremap failed\n");
		return -EIO;
	}

	simple_map_init(&palmtc_map);

	/* Probe for flash bankwidth 2 */
	printk(KERN_INFO "palmtc-flash: probing 16bit FLASH\n");
	mymtd = do_map_probe("cfi_probe", &palmtc_map);

	if (mymtd) {
		mymtd->owner = THIS_MODULE;

#ifdef CONFIG_MTD_PARTITIONS
		nr_mtd_parts = parse_mtd_partitions(mymtd, probes, &mtd_parts, 0);
		if (nr_mtd_parts > 0)
			part_type = "command line";
#endif
		if (nr_mtd_parts <= 0) {
			mtd_parts = palmtc_partitions;
			nr_mtd_parts = NUM_PARTITIONS;
			part_type = "builtin";
		}

		printk(KERN_INFO "Using %s partition table\n", part_type);
		add_mtd_partitions(mymtd, mtd_parts, nr_mtd_parts);
		return 0;
	}

	iounmap((void *)palmtc_map.virt);
	return -ENXIO;
}

/*
 * Cleanup
 */
static void __exit palmtc_mtd_cleanup(void)
{

	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}

	/* Free partition info, if commandline partition was used */
	if (mtd_parts && (mtd_parts != palmtc_partitions))
		kfree (mtd_parts);

	if (palmtc_map.virt) {
		iounmap((void *)palmtc_map.virt);
		palmtc_map.virt = 0;
	}
}


module_init(palmtc_mtd_init);
module_exit(palmtc_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("MTD map for Palm Tungsten|C");
