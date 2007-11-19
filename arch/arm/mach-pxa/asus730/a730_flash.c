/*
 * Flash support for Asus MyPal A730(W)
 * Initial version by Michal Sroczynski
 * 
 * Uses code from mphysmap.c (by Jörn Engel)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/mtd/map.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>

#include <asm/arch/asus730-init.h>
#include <asm/arch/asus730-gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach/map.h>
#include <asm/arch/udc.h>
#include <asm/arch/audio.h>

#include <linux/lcd.h>
#include <linux/backlight.h>

#include <linux/fb.h>
#include <asm/arch/pxafb.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/mach/flash.h>

#include "../generic.h"

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>

#define CFG_MTD_MULTI_PHYSMAP_1_NAME "1st32MB"
#define CFG_MTD_MULTI_PHYSMAP_1_START 0x00000000
#define CFG_MTD_MULTI_PHYSMAP_1_LEN 0x2000000
#define CFG_MTD_MULTI_PHYSMAP_1_WIDTH 2
#define CFG_MTD_MULTI_PHYSMAP_2_NAME "2nd32MB"
#define CFG_MTD_MULTI_PHYSMAP_2_START 0xc000000
#define CFG_MTD_MULTI_PHYSMAP_2_LEN 0x2000000
#define CFG_MTD_MULTI_PHYSMAP_2_WIDTH 2

/*
 * Asus firmware size: 0x0x2BC0000 at offset 0x40000.
 * 0x02C00000
 * 0x02c1ffd0
 * 0x02c5ffd0
 * 0x02c7a060
 * 0x02CA0000
 * 0x02d2ad10
 */

/*static struct mtd_partition a730_partitions[] = {
	{
		.name		= "boot flash",
		.size		= SZ_32M,
		.offset		= PXA_CS0_PHYS,
		.mask_flags	= MTD_WRITEABLE,  // force read-only
	},
	{
		.name		= "flash disk",
		.size		= SZ_32M,
		.offset		= PXA_CS3_PHYS,
		.mask_flags	= MTD_WRITEABLE,  // force read-only
	}
	//in reality there are 2 partitions
};*/
static struct mtd_partition a730_partitions[] = {
    {
        .name           = "bootloader",
        .offset         = 0x00000000,
        .size           = 0x00040000,
        .mask_flags     = MTD_WRITEABLE,
    },
    {
        .name           = "ramdisk",
        .offset         = 0x00040000,//MTDPART_OFS_NXTBLK
        .size           = 0x2bc0000,
        .mask_flags     = MTD_WRITEABLE,
    },
    {
        .name           = "flashdisk",
        .size           = MTDPART_SIZ_FULL,
        .offset         = 0x2bc0000 + 0x40000,
        .mask_flags     = MTD_WRITEABLE,
    },
};

DECLARE_MUTEX(map_mutex);

static struct map_info mphysmap_static_maps[] = {
        {
                .name           = "1st32MB",
                .phys           = PXA_CS0_PHYS,
                .size           = SZ_32M,
                .bankwidth      = 2,
        },
        {
                .name           = "2nd32MB",
                .phys           = PXA_CS3_PHYS,
                .size           = SZ_32M,
                .bankwidth      = 2,
        },
};

static struct mtd_info *mtds[ARRAY_SIZE(mphysmap_static_maps)];
static struct mtd_info *mtds_concat = NULL;
static int mtds_found = 0;

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = {"cmdlinepart", "RedBoot", NULL};
#endif
static int mphysmap_map_device(struct map_info *map)
{
        static const char *rom_probe_types[] = {"cfi_probe", "jedec_probe", "map_rom", NULL};
        const char **type;
        struct mtd_info* mtd = NULL;

        map->virt = ioremap(map->phys, map->size);
        if (!map->virt) return -EIO;

        simple_map_init(map);
        
        type = rom_probe_types;
        for(; !mtd && *type; type++)
        {
                mtd = do_map_probe(*type, map);
        }

        if (!mtd)
        {
                iounmap(map->virt);
                return -ENXIO;
        }

        map->map_priv_1 = (unsigned long)mtd;
        mtd->owner = THIS_MODULE;

        mtds[mtds_found] = mtd;
	mtds_found++;

	return 0;
}

static void mphysmap_unmap_device(struct map_info *map)
{
        struct mtd_info* mtd = (struct mtd_info*)map->map_priv_1;
#ifdef CONFIG_MTD_PARTITIONS
        struct mtd_partition* mtd_parts = (struct mtd_partition*)map->map_priv_2;
#endif
        BUG_ON(!mtd);
        if (!map->virt) return;

#ifdef CONFIG_MTD_PARTITIONS
        if (mtd_parts)
        {
                del_mtd_partitions(mtd);
                kfree(mtd_parts);
        }
        //else del_mtd_device(mtd);
#else
        //del_mtd_device(mtd);
#endif
        //map_destroy(mtd);
        iounmap(map->virt);

        map->map_priv_1 = 0;
        map->map_priv_2 = 0;
        map->virt = NULL;
}

//

static int a730_physmap_flash_probe(struct device *dev)
{
	int i;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition* mtd_parts;
	int mtd_parts_nb;
#endif
	for (i = 0; i < ARRAY_SIZE(mphysmap_static_maps); i++)
	{
		if (strcmp(mphysmap_static_maps[i].name, "") != 0 && mphysmap_static_maps[i].size != 0 && mphysmap_static_maps[i].bankwidth != 0)
		{
			mphysmap_map_device(&mphysmap_static_maps[i]);
		}
	}

	if (mtds_found > 0)
	{
		for (i = 0; i < mtds_found; i++)
		{
			mtd_parts_nb = parse_mtd_partitions(mtds[i], part_probes, &mtd_parts, 0);
			if (mtd_parts_nb > 0)
			{
				add_mtd_partitions(mtds[i], mtd_parts, mtd_parts_nb);
				//map->map_priv_2=(unsigned long)mtd_parts;
			}
			else
			{
				add_mtd_device(mtds[i]);
				//map->map_priv_2 = (unsigned long)NULL;
			}
		}
		if (mtds_found > 1)
		{
			mtds_concat = mtd_concat_create(mtds, mtds_found, "A730MTD");
			if (mtds_concat)
			{
				mtd_parts_nb = parse_mtd_partitions(mtds_concat, part_probes, &mtd_parts, 0);

				if (mtd_parts_nb > 0)
				{
					add_mtd_partitions(mtds_concat, mtd_parts, mtd_parts_nb);
					//map->map_priv_2=(unsigned long)mtd_parts;
				}
				else
				{
					add_mtd_device(mtds_concat);
					//map->map_priv_2 = (unsigned long)NULL;
				}
			}
		}
	}
	
        return 0;
}

static int a730_physmap_flash_remove(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mphysmap_static_maps); i++)
        {
                if (strcmp(mphysmap_static_maps[i].name, "") != 0 && mphysmap_static_maps[i].size != 0 && mphysmap_static_maps[i].bankwidth != 0)
                {
                        mphysmap_unmap_device(&mphysmap_static_maps[i]);
                }
        }

	if (mtds_concat)
	{
		del_mtd_device(mtds_concat);
		mtds_concat = NULL;
	}
	if (mtds_found > 0)
	{
		for (i = 0; i < mtds_found; i++)
		{
			del_mtd_device(mtds[i]);
		}
	}

        return 0;
}

static struct device_driver a730_physmap_flash_driver = {
	.probe          = a730_physmap_flash_probe,
	.remove         = a730_physmap_flash_remove,
	.name		= "a730-physmap-flash",
	.bus		= &platform_bus_type,
};

static int __init a730_flash_init(void)
{
	if (!machine_is_a730()) return -ENODEV;
	return driver_register(&a730_physmap_flash_driver);
}

static void __exit a730_flash_exit(void)
{
	driver_unregister(&a730_physmap_flash_driver);
}

late_initcall(a730_flash_init);
module_exit(a730_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serge Nikolaenko <mypal_hh@utl.ru>");
MODULE_DESCRIPTION("Flash for Asus MyPal 730");
