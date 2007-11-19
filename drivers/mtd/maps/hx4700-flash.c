
/* iPAQ hx4700 StrataFlash Driver */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/cacheflush.h>


static void
hx4700_set_vpp(struct map_info *map, int vpp)
{
	SET_HX4700_GPIO( FLASH_VPEN, vpp );
}

/*
	Creating 5 MTD partitions on "ipaq":
	0x00000000-0x00100000 : "iPAQ boot firmware"
	0x00100000-0x00300000 : "iPAQ kernel"
	0x00300000-0x04fc0000 : "iPAQ rootfs"
	0x04fc0000-0x07fc0000 : "iPAQ home"
	0x07fc0000-0x08000000 : "Asset"
*/

#define PXA_CS_SIZE		0x04000000
#define HX4700_ASSET_SIZE	0x00040000
#define HX4700_BOOTLDR_SIZE	0x00100000	/* 1M */
#define HX4700_KERNEL_SIZE	0x00200000	/* 2M */
#define HX4700_HOME_SIZE	0x03000000	/* 48M */
#define HX4700_ROOTFS_SIZE	0x08000000 - HX4700_BOOTLDR_SIZE - HX4700_KERNEL_SIZE - HX4700_HOME_SIZE - HX4700_ASSET_SIZE

static struct mtd_partition hx4700_partitions[] = {
	{
		name:		"bootloader",
		offset:		0,
		size:		HX4700_BOOTLDR_SIZE,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, 
	{
		name:		"kernel",
		size:		HX4700_KERNEL_SIZE,
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"root",
		size:		HX4700_ROOTFS_SIZE,
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"home",
		size:		HX4700_HOME_SIZE,
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"asset",
		size:		HX4700_ASSET_SIZE,
		offset:		MTDPART_OFS_NXTBLK,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}
};

static const char *probes[] = { "cmdlinepart", NULL };

struct hx4700_mtd_info {
	struct mtd_info		*mtd[2];
	struct map_info		map[2];
	struct mtd_partition	*parts;
	struct mtd_info		*concat_mtd;
} hxflash;

static void
hx4700_inval_cache(struct map_info *map, unsigned long from, ssize_t len)
{
	flush_ioremap_region(map->phys, map->cached, from, len);
}

static int
hx4700_mtd_probe( struct platform_device *pdev )
{
	int mtd_parts, i;
	struct mtd_info *concat_mtd;
	struct map_info *map;
	char *part_type;
	int ret=0;

	platform_set_drvdata(pdev, &hxflash);

	hxflash.map[0].phys = PXA_CS0_PHYS;
	hxflash.map[1].phys = PXA_CS1_PHYS;

	hxflash.map[0].name = "hx4700-part0";
	hxflash.map[1].name = "hx4700-part1";

	for (i=0; i<2; i++) {
		map = &hxflash.map[i];
		map->set_vpp = hx4700_set_vpp;
		map->inval_cache = hx4700_inval_cache;
		map->bankwidth = 4;
		map->size = PXA_CS_SIZE;
		map->virt = ioremap_nocache( map->phys, PXA_CS_SIZE );
		map->cached = ioremap_cached( map->phys, PXA_CS_SIZE );

		simple_map_init( &hxflash.map[i] );
		printk(KERN_NOTICE
		       "Probing %s at physical address 0x%08lx"
		       " (%d-bit bankwidth)\n",
		       map->name, map->phys, map->bankwidth * 8);

		hxflash.mtd[i] = do_map_probe( "cfi_probe", &hxflash.map[i] );
		if (hxflash.mtd[i] == NULL) {
			printk( KERN_NOTICE "hx4700 MTD failed to map flash.\n" );
			ret = -ENXIO;
			goto probe_err;
		}
		hxflash.mtd[i]->owner = THIS_MODULE;
	}

	concat_mtd = mtd_concat_create( &hxflash.mtd[0], 2, "hx4700-mtd" );
	if (concat_mtd == NULL) {
		printk( KERN_NOTICE "hx4700 MTD failed to concatenate MTDs.\n" );
		ret = -ENXIO;
		goto probe_err;
	}

	mtd_parts = parse_mtd_partitions( concat_mtd, probes,
			&hxflash.parts, 0 );
	if (mtd_parts > 0)
		part_type = "command line";
	else {
		part_type = "built-in";
		hxflash.parts = hx4700_partitions;
		mtd_parts = ARRAY_SIZE( hx4700_partitions );
	}

	printk( KERN_INFO "hx4700 MTD: using %s partition definitions\n",
		part_type );
	add_mtd_partitions( concat_mtd, hxflash.parts, mtd_parts );
	hxflash.concat_mtd = concat_mtd;

	return 0;

probe_err:
	for (i=0; i<2; i++) {
		map = &hxflash.map[i];
		if (map->virt)
			iounmap( map->virt );
		if (map->cached)
			iounmap( map->cached );
	}
	return ret;
}

static int
hx4700_mtd_remove( struct platform_device *pdev )
{
	int i;
	struct map_info *map;

	for (i=0; i<2; i++) {
		map = &hxflash.map[i];
		if (map->virt)
			iounmap( map->virt );
		if (map->cached)
			iounmap( map->cached );
	}
	return 0;
}

#ifdef CONFIG_PM
static int
hx4700_mtd_suspend( struct platform_device *pdev, pm_message_t state )
{
	return hxflash.concat_mtd->suspend( hxflash.concat_mtd );
}

static int
hx4700_mtd_resume( struct platform_device *pdev )
{
	int i;
	volatile u32 *flash;

	/*
	* The HX4700 contains newer Intel Strataflash that has all blocks
	* locked on power-up.  The mtd code does not support this yet, so I am
	* putting the unlocking code here.  During resume, we cannot do it
	* from bootldr because bootldr is running from flash.  Here, we are
	* running from memory, and we can unlock the flash.
	*
	* Aric Blumer  (aric email_at_sign sdgsystems period com)
	* June 2, 2005
	*/

#define HX4700_UNLOCK_START 0x00100000
#define HX4700_UNLOCK_END   0x08000000
#define HX4700_BLOCK_SIZE   0x00040000

	flash = (u32 *) ioremap(PXA_CS0_PHYS, HX4700_UNLOCK_END);

	/* Set VPPEN. */
	GPSR2 = GPIO_bit(GPIO_NR_HX4700_FLASH_VPEN);

	/* We skip to 1MB offset so that we never unlock the bootldr code. */
	for (i = HX4700_UNLOCK_START / 4; i < HX4700_UNLOCK_END / 4;
	     i += HX4700_BLOCK_SIZE / 4) {
		flash[i] = 0x00600060;
		flash[i] = 0x00d000d0;
		flash[i] = 0x00ff00ff;
	}

	/* Clear VPPEN. */
	GPCR2 = GPIO_bit(GPIO_NR_HX4700_FLASH_VPEN);

	/* Clear the status register. */
	flash[HX4700_UNLOCK_START / 4] = 0x00500050;

	iounmap(flash);

	hxflash.concat_mtd->resume( hxflash.concat_mtd );
	return 0;
}

#else
#define hx4700_mtd_resume	NULL
#define hx4700_mtd_suspend	NULL
#endif

static struct platform_driver hx4700_mtd_driver = {
	.driver = {
		.name = "hx4700-flash",
	},
	.probe		= hx4700_mtd_probe,
	.remove 	= hx4700_mtd_remove,
	.suspend	= hx4700_mtd_suspend,
	.resume		= hx4700_mtd_resume,
};

static int
hx4700_mtd_init( void )
{
	return platform_driver_register( &hx4700_mtd_driver );
}

static void
hx4700_mtd_exit( void )
{
	platform_driver_unregister( &hx4700_mtd_driver );
}

module_init( hx4700_mtd_init );
module_exit( hx4700_mtd_exit );

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("HP iPAQ hx4700 flash driver");

