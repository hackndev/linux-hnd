/*
 * LED interface for Himalaya, the HTC PocketPC.
 *
 * License: GPL
 *
 * Author: Luke Kenneth Casson Leighton, Copyright(C) 2004
 *
 * Copyright(C) 2004, Luke Kenneth Casson Leighton.
 *
 * History:
 *
 * 2004-02-19	Luke Kenneth Casson Leighton	created.
 *
 */
 
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <asm/mach-types.h>


#define FLASH_ADDR1      0x00000000
#define FLASH_ADDR2      0x00000000
#define WINDOW_SIZE     32*1024*1024

static struct map_info blueangel_map = {
		.name =		"PXA Flash 1",
		.size =         WINDOW_SIZE,
		.phys =         FLASH_ADDR1,
		.bankwidth =	4,
};

static struct mtd_info *mymtd;

static int blueangel_rom_init (void)
{
	int retval = 0;

#if defined(CONFIG_MACH_BLUEANGEL)
	if (! machine_is_blueangel() )
		return -ENODEV;
#elif defined(CONFIG_MACH_HIMALAYA)
	if (! machine_is_himalaya() )
		return -ENODEV;
#endif

	blueangel_map.virt = ioremap(blueangel_map.phys, blueangel_map.size);
	if (!blueangel_map.virt) {
		printk(KERN_WARNING "Failed to ioremap %s\n", blueangel_map.name);
		return -ENOMEM;
	}
        blueangel_map.cached = ioremap_cached(blueangel_map.phys, blueangel_map.size);
	if (!blueangel_map.cached)
		printk(KERN_WARNING "Failed to ioremap cached %s\n", blueangel_map.name);
	simple_map_init(&blueangel_map);

	printk(KERN_NOTICE "Probing %s at physical address 0x%08lx (%d-bit bankwidth)\n",
			blueangel_map.name, blueangel_map.phys,
			blueangel_map.bankwidth * 8);

	mymtd = do_map_probe("cfi_probe", &blueangel_map);
	printk("mymtd=%p\n", mymtd);
	if (mymtd)
		add_mtd_device(mymtd);
	return retval;
}

static void blueangel_rom_exit (void)
{
	del_mtd_device(mymtd);

	map_destroy(mymtd);
	iounmap((void *)blueangel_map.virt);
	if (blueangel_map.cached)
		iounmap(blueangel_map.cached);
}

module_init (blueangel_rom_init);
module_exit (blueangel_rom_exit);

MODULE_LICENSE("GPL");
