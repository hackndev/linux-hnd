/* eraseall.c -- erase the whole of a MTD device

   Copyright (C) 2000 Arcom Control System Ltd  

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA  

   $Id$
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/fd.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>
#include "crc.h"

#define cpu_to_je16(x) ((jint16_t){x})
#define cpu_to_je32(x) ((jint32_t){x})
#define cpu_to_jemode(x) ((jmode_t){os_to_jffs2_mode(x)})

#define je16_to_cpu(x) ((x).v16)
#define je32_to_cpu(x) ((x).v32)
#define jemode_to_cpu(x) (jffs2_to_os_mode((x).m))

void lab_cmd_erasemtd(int argc, const char **argv);

#define PROGRAM "eraseall"
#define VERSION "0.1.2"

static const char *exe_name;
static const char *mtd_device;
static int quiet;		/* true -- don't output progress */
static int jffs2;		// fornmat for jffs2 usage

static int process_options (int argc, const char *argv[]);
static void display_help (void);
static void display_version (void);
static struct jffs2_unknown_node cleanmarker;

int laberasemtd_init(void)
{
	lab_addcommand("erasemtd", lab_cmd_erasemtd, "Erases an MTD device");
	return 0;
}

void laberasemtd_cleanup(void)
{
	lab_delcommand("erasemtd");
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB erasemtd Module");
MODULE_LICENSE("GPL");
module_init(laberasemtd_init);
module_exit(laberasemtd_cleanup);

static void erase_callback(struct erase_info *ei)
{
	wait_queue_head_t* wq = (wait_queue_head_t*)ei->priv;
	wake_up(wq);
}

void lab_cmd_erasemtd (int argc, const char **argv)
{
	struct mtd_info* mtd;
	struct erase_info erase;
	int clmpos = 0, clmlen = 8;
	int isNAND;
	int retlen;
	
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wq;
	

	if (!process_options(argc, argv))
		return;
		
	if (mtd_device[1] != '\0')
	{
		lab_puts("Bad mtd_device\r\n");
		return;
	}
	
	if ((mtd = get_mtd_device(NULL, mtd_device[0] - '0')) == NULL)
	{
		lab_puts("Failed to get MTD device\r\n");
		return;
	}
	
	init_waitqueue_head(&wq);

	erase.mtd = mtd;
	erase.len = mtd->erasesize;
	erase.priv = (u_long)&wq;
	erase.callback = erase_callback;
	isNAND = mtd->type == MTD_NANDFLASH ? 1 : 0;

	if (jffs2) {
		cleanmarker.magic = cpu_to_je16 (JFFS2_MAGIC_BITMASK);
		cleanmarker.nodetype = cpu_to_je16 (JFFS2_NODETYPE_CLEANMARKER);
		if (!isNAND)
			cleanmarker.totlen = cpu_to_je32 (sizeof (struct jffs2_unknown_node));
		else
		{
			/* Check for autoplacement */
			if (mtd->oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
				/* Get the position of the free bytes */
				if (!mtd->oobinfo.oobfree[0][1]) {
					lab_printf(" Eeep. Autoplacement selected and no empty space in oob\n");
					put_mtd_device(mtd);
					return;
				}
				clmpos = mtd->oobinfo.oobfree[0][0];
				clmlen = mtd->oobinfo.oobfree[0][1];
				if (clmlen > 8)
					clmlen = 8;
			} else {
				/* Legacy mode */
				switch (mtd->oobsize) {
				case 8:
					clmpos = 6;
					clmlen = 2;
					break;
				case 16:
					clmpos = 8;
					clmlen = 8;
					break;
				case 64:
					clmpos = 16;
					clmlen = 8;
					break;
				}
			}
			cleanmarker.totlen = cpu_to_je32(8); /* NAND code expects this to be 8 ! */
		}
		cleanmarker.hdr_crc =  cpu_to_je32 (crc32 (0, &cleanmarker,  sizeof (struct jffs2_unknown_node) - 4));
	}

	for (erase.addr = 0; erase.addr < mtd->size; erase.addr += mtd->erasesize) {

		if (!quiet) {
			lab_printf("\rErasing %d Kibyte @ %x -- %2d %% complete.",
			     mtd->erasesize / 1024, erase.addr,
			     erase.addr * 100 / mtd->size);
		}
		
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wq, &wait);
		if (MTD_ERASE(mtd, &erase)) {
			lab_printf("\r\n%s: %s: MTD Erase failure\r\n", exe_name, mtd_device);
			remove_wait_queue(&wq, &wait);
			set_current_state(TASK_RUNNING);
			continue;
		}
		schedule();
		remove_wait_queue(&wq, &wait);
		set_current_state(TASK_RUNNING);
		
		// format for JFFS2
		if (!jffs2) 
			continue;
				
		// write cleanmarker	
		if (isNAND) {
			if (MTD_WRITEOOB(mtd, erase.addr + clmpos, clmlen, &retlen, (unsigned char*)&cleanmarker)) {
				lab_printf("\r\n%s: %s: MTD writeoob failure\r\n", exe_name, mtd_device);
				continue;
			}
		} else {
			if (MTD_WRITE(mtd, erase.addr, sizeof(cleanmarker), &retlen, (unsigned char*)&cleanmarker))
			{
				lab_printf("\r\n%s: %s: MTD write failure\r\n", exe_name, mtd_device);
				continue;
			}
		}
		if (!quiet)
			lab_printf (" Cleanmarker written at %x.", erase.addr);
	}
	if (!quiet)
		lab_puts("\r\n");

	put_mtd_device(mtd);
	return;
}


int process_options (int argc, const char *argv[])
{
	exe_name = argv[0];
	jffs2=0;
	quiet=0;
	if (argc == 3)
	{
		if (!strcmp(argv[1], "-j"))
			jffs2 = 1;
		else if (!strcmp(argv[1], "-q"))
			quiet = 1;
		else {
			display_help();
			return 0;
		}
	}
	if (argc <= 2)
	{
		if ((argc-1) && !strcmp(argv[1], "-h"))
		{
			display_help();
			return 1;
		} else if ((argc-1) && !strcmp(argv[1], "-v")) {
			display_version();
			return 1;
		}
	}

	mtd_device = argv[argc-1];
	return 1;
}

void display_help (void)
{
	lab_printf("Usage: %s [OPTION] MTD_DEVICE\r\n"
	       "Erases all of the specified MTD device.\r\n"
	       "\r\n"
	       "  -j        format the device for jffs2\r\n"
	       "  -q        don't display progress messages\r\n"
	       "  -h        display this help and exit\r\n"
	       "  -v        output version information and exit\r\n",
	       exe_name);
}


void display_version (void)
{
	lab_puts(PROGRAM " " VERSION "\r\n"
	       "\r\n"
	       "Copyright (C) 2000 Arcom Control Systems Ltd\r\n"
	       "\r\n"
	       PROGRAM " comes with NO WARRANTY\r\n"
	       "to the extent permitted by law.\r\n"
	       "\r\n"
	       "You may redistribute copies of " PROGRAM "\r\n"
	       "under the terms of the GNU General Public Licence.\r\n"
	       "See the file `COPYING' for more information.\r\n");
}
