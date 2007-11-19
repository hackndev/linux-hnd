/* labnanddump.c
 * A nanddump module for LAB
 *
 * Derived from nanddump.c from mtdutils, which is
 * Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                    Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This utility dumps the contents of raw NAND chips or NAND
 *   chips contained in DoC devices. NOTE: If you are using raw
 *   NAND chips, disable NAND ECC in your kernel.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/fd.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

void lab_cmd_nandcheck(int argc, const char **argv);
                             
int labnandcheck_init(void)
{
	lab_addcommand("nandcheck", lab_cmd_nandcheck, "Dumps contents of NAND flash");
	return 0;
}

void labnandcheck_cleanup(void)
{
	lab_delcommand("nandcheck");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB nanddump Module");
MODULE_LICENSE("GPL");
module_init(labnandcheck_init);
module_exit(labnandcheck_cleanup);


#define BLOCKSIZE 17*1024

/*
 * Main program
 */
void lab_cmd_nandcheck(int argc, const char **argv)
{
	static unsigned char readbuf[BLOCKSIZE];
	static unsigned char oobbuf[BLOCKSIZE/512*16];
	unsigned long ofs;
	struct mtd_info* mtd = NULL;
	int bs, oobbs, start_addr, end_addr;
	int retlen;
	
	/* Make sure enough arguments were passed */ 
	if (argc < 2 || argv[1][1] != '\0') {
		lab_printf("usage: %s <mtd number> [start addr] [length]\r\n", argv[0]);
		return;
	}

	/* Open MTD device */
	if ((mtd = get_mtd_device(NULL, argv[1][0]-'0')) == NULL) {
		lab_puts("Couldn't open flash\r\n");
		return;
	}

	if (mtd->type != MTD_NANDFLASH) {
		lab_puts("This MTD is not NAND flash. I can't dump this - sorry.\r\n");
		put_mtd_device(mtd);
		return;
	}

	/* Make sure device page sizes are valid */
	if (!(mtd->oobsize == 16 && mtd->oobblock == 512) &&
	    !(mtd->oobsize == 8  && mtd->oobblock == 256)) {
		lab_puts("Unknown flash (not normal NAND)\r\n");
		put_mtd_device(mtd);
		return;
	}

	/* Initialize start/end addresses and block size */
	start_addr = 0;
	end_addr = mtd->size;
	bs = BLOCKSIZE;
	oobbs = BLOCKSIZE/mtd->oobblock*mtd->oobsize;

	/* See if start address and length were specified */
	if (argc == 3) {
		start_addr = simple_strtoul(argv[2], NULL, 0) & ~(bs - 1);
		end_addr = mtd->size;
	} else if (argc == 4) {
		start_addr = simple_strtoul(argv[2], NULL, 0) & ~(bs - 1);
		end_addr = (simple_strtoul(argv[2], NULL, 0) + simple_strtoul(argv[3], NULL, 0)) & ~(bs - 1);
	}

	/* Print informative message */
	lab_printf("\r\nChecking data starting at 0x%08x and ending at 0x%08x...\r\n",
	           start_addr, end_addr);
	lab_printf("Block size: %d\r\n", bs);
	if ((start_addr % bs) || (end_addr % bs))
	{
		lab_printf("Sizes not a multiple of blocksize\r\n");
		put_mtd_device(mtd);
		return;
	}

	/* Dump the flash contents */
	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {
		struct nand_oobinfo oobsel;
		int i,j;

		oobsel.useecc = 0;
		/* Read page data and exit on failure */
		if (MTD_READECC(mtd, ofs, bs, &retlen, readbuf, NULL, &oobsel))
			lab_puts("Error in mtdread\r\n");
		for (i = ofs, j = 0; i < (ofs + bs); i += mtd->oobblock, j++)
			if (MTD_READOOB(mtd, i, mtd->oobsize, &retlen, oobbuf+(j*mtd->oobsize)))
				lab_puts("Error in readoob\r\n");

		for (i = 0; i < oobbs; i += mtd->oobsize)
			if (oobbuf[i+5] != 0xFF)
			{
				lab_printf("\r\nofs %08x contains a bad block (0x%02x); let's move on OK?\r\n", ofs, oobbuf[i*mtd->oobsize+5]);
				goto endloop;
			}

		/* Read OOB data and exit on failure */
		if ((ofs & 0xFFFF) == 0x0)
			lab_printf("Checking %lx\r", ofs);
		
		for (i = 0; i < bs; i++)
			if (readbuf[i] != 0xFF)
				lab_printf("\r\nOffset: %08x:     Addr: %05x: byte: %02x\r\n", ofs, i, readbuf[i]);
		
		for (i = 0; i < oobbs; i++)
			if (oobbuf[i] != 0xFF)
				lab_printf("\r\nOffset: %08x: OOB Addr: %05x: byte: %02x\r\n", ofs, i, oobbuf[i]);
	endloop:;

	}

	/* Close the output file and MTD device */
	put_mtd_device(mtd);
	lab_puts("\r\n");

	/* Exit happy */
	return;
}
