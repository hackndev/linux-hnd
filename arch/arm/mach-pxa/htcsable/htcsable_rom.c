/*
 * Pseudo-device to access IPL memory.
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
#define WINDOW_SIZE      0x800

static struct map_info sable_map = {
		.name =		"IPL Flash",
		.size =         WINDOW_SIZE,
		.phys =         FLASH_ADDR1,
		.bankwidth =	4,
};

static struct mtd_info *mymtd;

static int sable_rom_init (void)
{
	int retval = 0;

	if (!(machine_is_hw6900() || machine_is_htcbeetles()))
		return -ENODEV;

	sable_map.virt = ioremap(sable_map.phys, sable_map.size);
	if (!sable_map.virt) {
		printk(KERN_WARNING "Failed to ioremap %s\n", sable_map.name);
		return -ENOMEM;
	}
        sable_map.cached = ioremap_cached(sable_map.phys, sable_map.size);
	if (!sable_map.cached)
		printk(KERN_WARNING "Failed to ioremap cached %s\n", sable_map.name);
	simple_map_init(&sable_map);

	printk(KERN_NOTICE "Probing %s at physical address 0x%08lx (%d-bit bankwidth)\n",
			sable_map.name, sable_map.phys,
			sable_map.bankwidth * 8);

	mymtd = do_map_probe("cfi_probe", &sable_map);
	printk("mymtd=%p\n", mymtd);
	if (mymtd)
		add_mtd_device(mymtd);
	return retval;
}

static void sable_rom_exit (void)
{
	del_mtd_device(mymtd);

	map_destroy(mymtd);
	iounmap((void *)sable_map.virt);
	if (sable_map.cached)
		iounmap(sable_map.cached);
}

module_init (sable_rom_init);
module_exit (sable_rom_exit);

