/*
 *  lab/modules/labcopynand.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Based on nandwrite.c from mtd-utils, which is
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *   		  2003 Thomas Gleixner (tglx@linutronix.de)
 *
 *  Bootloader port to Linux Kernel, November 18, 2003
 *
 *  nand target for LAB "copy" command
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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
#include <linux/lab/copy.h>
#include <linux/jffs2.h>
#include "crc.h"

#define cpu_to_je16(x) ((jint16_t){x})
#define cpu_to_je32(x) ((jint32_t){x})
#define cpu_to_jemode(x) ((jmode_t){os_to_jffs2_mode(x)})

#define je16_to_cpu(x) ((x).v16)
#define je32_to_cpu(x) ((x).v32)
#define jemode_to_cpu(x) (jffs2_to_os_mode((x).m))

// oob layouts to pass into the kernel as default
static struct nand_oobinfo none_oobinfo = { 
	useecc: 0,
};


/* I'm thinking that I'll have this a base target of nand:
 * You will pass :-seperated arguments
 * Arguments possible:
 *  :noecc:
 *  :yaffs:
 *  :jffs2:
 *  :autoplace:
 *  :oob:
 *  :start=0xfoobar:
 * Mandatory to end with a flash number.
 * Example: copy ymodem: nand:jffs2:1
 */

static int putcheck(char* fname)
{
	char arg[64]; // buffer overrun
	char *argp;
	
	while (*fname != '\0')
	{
		argp=arg;
		while (*fname != '\0' && *fname != ':')
		{
			*argp = *fname;
			fname++;
			argp++;
		}
		if (*fname)
			fname++;
		*argp = '\0';
		if (arg[0] >= '0' && arg[0] <= '9' && arg[1] == '\0')
			continue;	// got a partition number, that's ok
		if (!strcmp(arg, "jffs2"))
			continue;	// jffs2, that's ok
		if (!strcmp(arg, "yaffs"))
			continue;	// yaffs, that's ok
		if (!strcmp(arg, "noecc"))
			continue;	// noecc, that's ok
		if (!strcmp(arg, "oob"))
			continue;	// OOB data, that's ok
		if (!strncmp(arg, "start=", 6))
			continue;	// some sort of start address, that's ok
		if (!strcmp(arg, "wince"))
			continue;
		if (!strcmp(arg, "erase"))
			continue;
		if (!strcmp(arg, "autoplace"))
			continue;
		lab_printf("Unknown argument: %s\r\n", arg);
		return 0;
	}
	return 1;	// if we're not dead yet we're ok
}

static void erase_callback(struct erase_info *ei)
{
	wait_queue_head_t* wq = (wait_queue_head_t*)ei->priv;
	wake_up(wq);
}

static int put(int count, unsigned char* buf, char* fname)
{
	struct jffs2_unknown_node cleanmarker;

	int imglen, pagelen, blockstart = -1, rdat;
	struct mtd_info *mtd = NULL;
	struct nand_oobinfo *oobsel;
	struct erase_info eraseinf;
	
	unsigned char writebuf[512];
	unsigned char oobbuf[16];
	
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wq;

	int 	mtdoffset = 0;
	int 	quiet = 0;
	int	writeoob = 0;
	int	jffs2 = 0;
	int	noecc = 0;
	int	wince = 0;
	int	erase = 0;
	int	autoplace = 0;
	
	int	retries = 0;
	
	int	err;
	int	blockalign = 1;
	
	char arg[64]; // buffer overrun
	char *argp;
	
	while (*fname != '\0')
	{
		argp=arg;
		while (*fname != '\0' && *fname != ':')
		{
			*argp = *fname;
			fname++;
			argp++;
		}
		if (*fname)
			fname++;
		*argp = '\0';
		if (arg[0] >= '0' && arg[0] <= '9' && arg[1] == '\0') {
			if (mtd) {
				lab_puts("Can't specify more than one MTD!\r\n");
				return 0;
			}
			mtd = get_mtd_device(NULL, arg[0]-'0');
			if (!mtd) {
				lab_puts("Couldn't get MTD device.\r\n");
				return 0;
			}
			if (mtd->type != MTD_NANDFLASH) {
				lab_puts("This MTD is not NAND flash.\r\n");
				put_mtd_device(mtd);
				return 0;
			}
			
		} else if (!strcmp(arg, "jffs2"))
			jffs2 = 1;	// jffs2, that's ok
		else if (!strcmp(arg, "noecc"))
			noecc = 1;	// noecc, that's ok
		else if (!strcmp(arg, "oob"))
			writeoob = 1;	// OOB data, that's ok
		else if (!strcmp(arg, "erase"))
			erase = 1;
		else if (!strcmp(arg, "autoplace"))
			autoplace = 1;
		else if (!strcmp(arg, "wince"))
		{
			wince = 1;
			noecc = 1;
			writeoob = 1;
			jffs2 = 0;
			erase = 1;
		} else if (!strncmp(arg, "start=", 6))
			continue;	// some sort of start address, that's ok
		else {
			lab_printf("Unknown argument: %s\r\n", arg);
			return 0;
		}
	}
	
	if (!mtd)
	{
		lab_puts("You forgot to specify an MTD device!\r\n");
		return 0;
	}

	rdat = count;

	/* Make sure device page sizes are valid */
	if (!(mtd->oobsize == 16 && mtd->oobblock == 512) &&
	    !(mtd->oobsize == 8 && mtd->oobblock == 256)) {
		lab_puts("What an oddly sized MTD device you have. I don't think it's NAND flash, although MTD seems to disagree.\r\n");
		put_mtd_device(mtd);
		mtd = NULL;
		return 0;
	}
	
	oobsel = NULL;	/* default oob */
	
	// write without ecc ?
	if (noecc)
		oobsel = &none_oobinfo;

   	imglen = count;

	pagelen = mtd->oobblock + ((writeoob == 1) ? mtd->oobsize : 0);
	// Check, if file is pagealigned
	if ( (imglen % pagelen) != 0) {
		lab_puts ("Input file is not page aligned. I'm not going to give up now, but you might run into problems later.\r\n");
//		put_mtd_device(mtd);
//		mtd = NULL;
//		return 0;
	}
	
	// Check, if length fits into device
	if ( ((imglen / pagelen) * mtd->oobblock) > (mtd->size - mtdoffset)) {
		lab_puts ("Input file does not fit into device\r\n");
		put_mtd_device(mtd);
		mtd = NULL;
		return 0;
	}
	
	init_waitqueue_head(&wq);
	
	eraseinf.mtd = mtd;
	eraseinf.callback = erase_callback;
	eraseinf.priv = (u_long)&wq;
	eraseinf.len = mtd->erasesize;
	
	blockstart = -1;
	
	/* Get data from input and write to the device */
	while(mtdoffset < mtd->size) {
		int retlen;
		
		// new eraseblock , check for bad block
		while (blockstart != (mtdoffset & (~mtd->erasesize + 1))) {
			unsigned long offs;
			int baderaseblock;
			
			blockstart = mtdoffset & (~mtd->erasesize + 1);
			offs = blockstart;
		        baderaseblock = 0;
			if (!quiet)
				lab_printf("Writing data to block %x\r", blockstart);
		   
		        /* Check all the blocks in an erase block for bad blocks */
			do {
				if (MTD_READOOB(mtd, offs, mtd->oobsize, &retlen, oobbuf)) {
					lab_puts("\nOOB read failure!\r\n");
					put_mtd_device(mtd);
					mtd = NULL;
					return 0;
				}
				if ((oobbuf[5] != 0xff) && !wince) {
					baderaseblock = 1;
				   	if (!quiet)
						lab_printf("\r\nBad block at %x, %u block(s) from %x will be skipped\r\n", (int) offs, blockalign, blockstart);
				}
			   
				if (baderaseblock) {		   
					mtdoffset = blockstart + mtd->erasesize;
				}
			        offs +=  mtd->erasesize / blockalign ;
			} while ( offs < blockstart + mtd->erasesize );
		}

		if (erase && (blockstart != (mtdoffset & (~mtd->erasesize + 1))))
		{
			eraseinf.addr = blockstart;
			set_current_state(TASK_INTERRUPTIBLE);
			add_wait_queue(&wq, &wait);
			
		eraseretry:		
			if (MTD_ERASE(mtd, &eraseinf)) {
				lab_printf("\r\nErase failure:\t%08X / %08X\r\n", blockstart, mtd->size); 
				if (retries < 10) {
					retries++;
					lab_puts("Retrying...");
					goto eraseretry;
				}
				lab_puts("Maximum retries exceeded, giving up. Sorry it didn't work out.\r\n");
				set_current_state(TASK_RUNNING);
				remove_wait_queue(&wq, &wait);
				put_mtd_device(mtd);
				mtd = NULL;
				return 0;
			}
			schedule();
			remove_wait_queue(&wq, &wait);
		}

		/* Read Page Data from input file */
		if (rdat <= 0)
			break;

		if ((rdat < mtd->oobblock) && writeoob) {
			lab_puts("Out of data. Oops.\r\n");
			put_mtd_device(mtd);
			mtd = NULL;
			return 0;
		}
		memcpy(writebuf, buf, (rdat < mtd->oobblock) ? rdat : mtd->oobblock);
		buf += mtd->oobblock;
		rdat -= mtd->oobblock;
		
		if (erase && jffs2 && (blockstart != (mtdoffset & (~mtd->erasesize + 1))))
		{
			int clmpos, clmlen;
			
			cleanmarker.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
			cleanmarker.nodetype = cpu_to_je16(JFFS2_NODETYPE_CLEANMARKER);
			if (!mtd->oobinfo.oobfree[0][1]) {
				lab_printf(" Eeep. Autoplacement selected and no empty space in oob\n");
				put_mtd_device(mtd);
				return 0;
			}
			clmpos = mtd->oobinfo.oobfree[0][0];
			clmlen = mtd->oobinfo.oobfree[0][1];
			if (clmlen > 8)
				clmlen = 8;
			cleanmarker.totlen = cpu_to_je32(8); /* NAND code expects this to be 8 ! */
			cleanmarker.hdr_crc =  cpu_to_je32 (crc32 (0, &cleanmarker,  sizeof (struct jffs2_unknown_node) - 4));
			memcpy((void*)(oobbuf+clmpos), (void*)&cleanmarker, clmlen);
		} else {
			if (MTD_READOOB(mtd, blockstart, mtd->oobsize, &retlen, oobbuf)) {
				lab_puts("\nOOB read failure!\r\n");
				put_mtd_device(mtd);
				mtd = NULL;
				return 0;
			}
		}
writeretry:
		if ((err = MTD_WRITEECC(mtd, mtdoffset, mtd->oobblock, &retlen, writebuf, oobbuf, oobsel))) {
			lab_printf("\nMTD_WRITEECC failed for offset %08x! (%d!)\r\n", mtdoffset, err);
			if (retries < 10)
			{
				retries++;
				lab_puts("Retrying...");
				goto writeretry;
			}
			lab_puts("Maximum retries exceeded, giving up. Sorry it didn't work out.\r\n");
			put_mtd_device(mtd);
			mtd = NULL;
			return 0;
		}
		
		if (writeoob) {
			if (rdat < mtd->oobsize) {
				lab_puts("Out of data for OOB. Oops.\r\n");
				put_mtd_device(mtd);
				mtd = NULL;
				return 0;
			}
			memcpy(oobbuf, buf, mtd->oobsize);
			buf += mtd->oobsize;
			rdat -= mtd->oobsize;
			
			/* Write OOB data first, as ecc will be placed in there*/
			if (MTD_WRITEOOB(mtd, mtdoffset, mtd->oobsize, &retlen, oobbuf))
			{
				lab_puts("\nOOB write failure!\r\n");
			}
			imglen -= mtd->oobsize;
		}

		imglen -= mtd->oobblock;
		mtdoffset += mtd->oobblock;
	}

	lab_puts("\r\nAll done. Closing the MTD device.");
	/* Close the output file and MTD device */
	put_mtd_device(mtd);
	mtd = NULL;
	lab_puts("\r\n");

	if (imglen > 0) {
		lab_puts("Data did not fit into device, due to bad blocks\r\n");
		return 0;
	}

	/* Return happy */
	return 1;
}

int  labcopynand_init(void)
{
	lab_copy_adddest("nand",putcheck,put);
	
	return 0;
}

void labcopynand_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy nand module");
MODULE_LICENSE("GPL");
module_init(labcopynand_init);
module_exit(labcopynand_cleanup);
