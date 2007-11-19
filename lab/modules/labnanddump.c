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

void lab_cmd_nanddump(int argc, const char **argv);
                             
int labnanddump_init(void)
{
	lab_addcommand("nanddump", lab_cmd_nanddump, "Dumps contents of NAND flash");
	return 0;
}

void labnanddump_cleanup(void)
{
	lab_delcommand("nanddump");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB nanddump Module");
MODULE_LICENSE("GPL");
module_init(labnanddump_init);
module_exit(labnanddump_cleanup);


/*
 * Main program
 */
void lab_cmd_nanddump(int argc, const char **argv)
{
	unsigned char readbuf[512];
	unsigned char oobbuf[16];
	unsigned long ofs;
	struct mtd_info* mtd = NULL;
	int i, ofd, bs, start_addr, end_addr, pretty_print;
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	unsigned char pretty_buf[120];
	int retlen;
	
	/* Make sure enough arguments were passed */ 
	if (argc < 3 || argv[1][1] != '\0') {
		lab_printf("usage: %s <mtd number> <dumpname> [start addr] [length]\r\n", argv[0]);
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

	/* Open output file for writing */
	if ((ofd = sys_open(argv[2], O_WRONLY | O_CREAT, 0644)) == -1) {
		lab_puts("Couldn't open outfile\r\n");
		put_mtd_device(mtd);
		return;
	}

	/* Initialize start/end addresses and block size */
	start_addr = 0;
	end_addr = mtd->size;
	bs = mtd->oobblock;

	/* See if start address and length were specified */
	if (argc == 4) {
		start_addr = simple_strtoul(argv[3], NULL, 0) & ~(bs - 1);
		end_addr = mtd->size;
	} else if (argc == 5) {
		start_addr = simple_strtoul(argv[3], NULL, 0) & ~(bs - 1);
		end_addr = (simple_strtoul(argv[3], NULL, 0) + simple_strtoul(argv[4], NULL, 0)) & ~(bs - 1);
	}

	/* Ask user if they would like pretty output */
	lab_puts("Would you like formatted output? ");
	if (tolower(pretty_buf[0] = lab_getc_seconds(NULL, 0)) != 'y')
		pretty_print = 0;
	else
		pretty_print = 1;
	lab_putc(pretty_buf[0]);

	/* Print informative message */
	lab_printf("\r\nDumping data starting at 0x%08x and ending at 0x%08x...\r\n",
	           start_addr, end_addr);
	lab_printf("OOB size: %d. OOB block: %d\r\n",
	           mtd->oobsize, mtd->oobblock);

	/* Dump the flash contents */
	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {
		struct nand_oobinfo oobsel;

		oobsel.useecc = 0;
		/* Read page data and exit on failure */
//		if (MTD_READECC(mtd, ofs, bs, &retlen, readbuf, NULL, &oobsel)) {
		if (MTD_READECC(mtd, ofs, bs, &retlen, readbuf, &oobbuf, NULL)) {
			lab_puts("Error in mtdread\r\n");
//			put_mtd_device(mtd);
//			sys_close(ofd);
//			return;
		}

		/* Write out page data */
		if (pretty_print) {
			for (i = 0; i < bs; i += 16) {
				sprintf(pretty_buf,
					"0x%08x: %02x %02x %02x %02x %02x %02x %02x "
					"%02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					(unsigned int) (ofs + i),  readbuf[i],
					readbuf[i+1], readbuf[i+2],
					readbuf[i+3], readbuf[i+4],
					readbuf[i+5], readbuf[i+6],
					readbuf[i+7], readbuf[i+8],
					readbuf[i+9], readbuf[i+10],
					readbuf[i+11], readbuf[i+12],
					readbuf[i+13], readbuf[i+14],
					readbuf[i+15]);
				sys_write(ofd, pretty_buf, 60);
			}
		} else
			sys_write(ofd, readbuf, bs);

		/* Read OOB data and exit on failure */
		oob.start = ofs;
		if ((ofs & 0xFFFF) == 0x0)
			lab_printf("Dumping %lx\r", ofs);
#if 0
		if (MTD_READOOB(mtd, ofs, mtd->oobsize, &retlen, oobbuf)) {
			lab_puts("ioctl(MEMREADOOB) failed\r\n");
			put_mtd_device(mtd);
			sys_close(ofd);
			return;
		}
#endif
		/* Write out OOB data */
		if (pretty_print) {
			if (mtd->oobsize == 16) {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
					"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					oobbuf[0], oobbuf[1], oobbuf[2],
					oobbuf[3], oobbuf[4], oobbuf[5],
					oobbuf[6], oobbuf[7], oobbuf[8],
					oobbuf[9], oobbuf[10], oobbuf[11],
					oobbuf[12], oobbuf[13], oobbuf[14],
					oobbuf[15]);
				sys_write(ofd, pretty_buf, 60);
			} else {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
					"%02x %02x\n",
					oobbuf[0], oobbuf[1], oobbuf[2],
					oobbuf[3], oobbuf[4], oobbuf[5],
					oobbuf[6], oobbuf[7]);
				sys_write(ofd, pretty_buf, 48);
			}
		} else
			sys_write(ofd, oobbuf, mtd->oobsize);
	}

	/* Close the output file and MTD device */
	put_mtd_device(mtd);
	sys_close(ofd);
	lab_puts("\n");

	/* Exit happy */
	return;
}
