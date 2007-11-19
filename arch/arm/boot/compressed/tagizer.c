/*
 * LAB -- Linux As Bootloader
 * Build the tag list required to boot a new Linux kernel.
 *
 * Copyright (C) 2003 Joshua Wise
 */

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <linux/kernel.h>
#include <linux/lab/boot_flags.h>

#define SZ_16M			(16 * 1024 * 1024)
#define SZ_32M			(32 * 1024 * 1024)
#define SZ_64M			(64 * 1024 * 1024)
#define SZ_128M			(128 * 1024 * 1024)

#ifdef CONFIG_ARCH_PXA
#define DRAM_MAX_BANK_SIZE	SZ_64M
#define DRAM_BANK(x)		(0xA0000000 + ((x) * DRAM_MAX_BANK_SIZE))
#define DRAM_BANKS		4
#define TAGBASE			0xA0000100
#define DEFCMDLINE		"cachepolicy=writeback console=tty0"
#endif

#ifdef CONFIG_ARCH_SA1100
#define DRAM_MAX_BANK_SIZE	SZ_128M
#define DRAM_BANK(x)		(0xC0000000 + ((x) * DRAM_MAX_BANK_SIZE))
#define DRAM_BANKS		2
#define TAGBASE			0xC0000100
#define DEFCMDLINE		""
#endif

#ifdef CONFIG_LAB_DEBUG
#define dbg(str)	c_out_str (str)
#define dbghex2(x)	c_out_hex2 (x)
#define dbghex8(x)	c_out_hex8 (x)
extern void c_out_str (char *str);
extern void c_out_hex2 (u32 x);
extern void c_out_hex8 (u32 x);
#else
#define dbg(str)
#define dbghex2(x)
#define dbghex8(x)
#endif

static long mem_sizes [] = {
    SZ_128M,
    SZ_64M,
    SZ_32M,
    SZ_16M,
};

__inline__ int _strlen (char *string)
{
	char *cur = string;
	while (*cur)
		cur++;
	return cur - string;
}

__inline__ int _strcpy (char *dest, char *src)
{
	int len = 0;
	while ((*dest++ = *src++))
		len++;
	return len;
}

struct tag *probe_ram_size (struct tag *t)
{
	int i, bank, found;
	long mem_size;
	volatile long *dram_base;
	long mem_saves [ARRAY_SIZE (mem_sizes)];
	long mem_save0;

	for (bank = 0; bank < DRAM_BANKS; bank++) {
		dram_base = (volatile long *)DRAM_BANK (bank);

		dbg ("BANK");
		dbghex2 (bank);
		dbg ("@");
		dbghex8 ((u32)dram_base);
		dbg(":");

		mem_save0 = dram_base [0];
		dram_base [0] = DRAM_MAX_BANK_SIZE;
		if (dram_base [0] != DRAM_MAX_BANK_SIZE) {
			dram_base [0] = mem_save0;
			goto noram;
		}

		/* probe, saving dram value in mem_saves */
		for (i = 0; i < ARRAY_SIZE (mem_sizes); i++) {
			mem_size = mem_sizes [i];
			if (mem_size < DRAM_MAX_BANK_SIZE) {
				mem_saves [i] = dram_base [mem_size >> 2];
				dram_base [mem_size >> 2] = mem_size;
			}
		}

		/* now see if we read back a valid size */
		found = 0;
		mem_size = dram_base [0];
		for (i = 0; i < ARRAY_SIZE (mem_sizes); i++)
			if (mem_size == mem_sizes [i])
			{
				/* verify real quick */
				dram_base[0] = 0xFFFFFFFF;
				if (dram_base[0] != 0xFFFFFFFF)
				{
					dbg ("FAKEOUT-");
					goto noram;
				}
				dram_base[0] = 0xAAAAAAAA;
				if (dram_base[0] != 0xAAAAAAAA)
				{
					dbg ("FAKEOUT-");
					goto noram;
				}
				dram_base[0] = 0x55555555;
				if (dram_base[0] != 0x55555555)
				{
					dbg ("FAKEOUT-");
					goto noram;
				}
				dram_base[0] = 0x00000000;
				if (dram_base[0] != 0x00000000)
				{
					dbg ("FAKEOUT-");
					goto noram;
				}
				dram_base[mem_sizes[i] >> 2] = 0xFFFFFFFF;
				if (dram_base[0] == 0xFFFFFFFF)
				{
					dbg ("SIZEFAULT-");
					goto noram;
				}
				found = 1;
			}
		dram_base [0] = mem_save0;

		/* now restore the locations we poked */
		for (i = 0; i < ARRAY_SIZE (mem_sizes); i++)
			if (mem_sizes [i] < mem_size)
				dram_base [mem_sizes [i] >> 2] = mem_saves [i];

		if (found) {
			dbg ("0x");
			dbghex2 (mem_size >> 20);
			dbg ("MB ");
			t->hdr.tag = ATAG_MEM;
			t->hdr.size = tag_size (tag_mem32);
			t->u.mem.size  = mem_size;
			t->u.mem.start = (unsigned long)dram_base;
			t = tag_next (t);
		} else {
noram:			/* Aiee! No memory! */
			dbg ("NORAM ");
		}
	}

	return t;
}

void tagize (u32 bootflags, char *arch_cmdline)
{
	struct tag *t;
	char kernel_cmdline [256];
	int cmdline_len;
	
	dbg("  base");
	t = (struct tag *)TAGBASE;
	t->hdr.tag = ATAG_CORE;
	t->hdr.size = tag_size (tag_core);
	t->u.core.flags = 0;
	t->u.core.pagesize = 0x00001000;
	t->u.core.rootdev = 0x00FF;
	t = tag_next (t);
	dbg("\r\n");
	

	dbg("  command line: ");
	/* Build command line */
	cmdline_len = _strcpy (kernel_cmdline, DEFCMDLINE);
	if (*arch_cmdline) {
		kernel_cmdline [cmdline_len++] = ' ';
		cmdline_len += _strcpy (kernel_cmdline + cmdline_len,
					arch_cmdline);
	}
	if (bootflags & BF_ACTION_BUTTON) {
		kernel_cmdline [cmdline_len++] = ' ';
		cmdline_len += _strcpy (kernel_cmdline + cmdline_len,
					"forcecli");
	}
	
	dbg (kernel_cmdline);
	
	t->hdr.tag = ATAG_CMDLINE;
	t->hdr.size = sizeof (struct tag_header) + _strlen (kernel_cmdline) + 1;
	_strcpy (t->u.cmdline.cmdline, kernel_cmdline);
	t = tag_next (t);
	dbg ("\r\n");

	dbg ("  RAM: ");
	/* Build the MEM tag */
	t = probe_ram_size (t);
	dbg ("\r\n");
	
	t->hdr.tag = ATAG_NONE;
	t->hdr.size = 0;
	dbg ("  done\r\n");
}	
