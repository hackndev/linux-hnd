/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright 2006 (C) Vincent Benony
 *
 *  This module tries to simulate a simple block device using the Disk On Chip
 *  of the Asus A620 PDA. In order to avoid writing into flash memory, the
 *  driver stores all write commands into a diff structure in memory.
 *  List of modified pages can be retrieved by ioctl to easily implement a simple
 *  backup/restore program.
 *  The structure of the filesystem stored in the DoC is very similar to cramfs.
 *  It starts with two ints (the size of an uncompressed blocks and the number of blocks)
 *  and then a lookup table saying for each compressed block it's offset into the file
 *  and it's size. Compressed data blocks follow this header.
*/

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/mtd/doc2000.h>
#include <linux/mtd/nand.h>
#include <linux/zlib.h>

#define FF_MAJOR		123		/* not very portable, but we perfectly know the use case... */
#define FF_NAME			"ff"

#define KERNEL_SECTOR_SIZE	512		/* kernel constant */
#define DIFF_GROW		128		/* a diff page contains 128 modified sectors */
#define DOCMP_BASE		0xEF800000	/* DoC chip base address on A620 */
#define SIGNATURE		0x53555341	/* "ASUS" signature */

#define UNCOMPRESSED_BLOCK_SIZE	16384

static struct gendisk *disk   = NULL;
static int skip               = 113;		/* number of erase units on DoC to skip in ASUS sections */
static int sectors            = 102400;		/* total number of sectors, here 50Mo */
static int nb_heads           = 4;
static int nb_sect_per_tracks = 16;		/* so nb_cylinders = sectors / (nb_heads * nb_sect_per_tracks) */
static int sector_size        = KERNEL_SECTOR_SIZE;

typedef unsigned char uchar;
typedef struct {
	int	offset, size;
} ENTRY;

static z_stream	stream;
static int	last_readed_logical;
static int	nb_compressed_blocks;
static char	last_logical[UNCOMPRESSED_BLOCK_SIZE];
static char	compressed[UNCOMPRESSED_BLOCK_SIZE<<1];
static uchar	temp[522];

uchar	*full_dict;
static ENTRY *compressed_directory;

module_param(skip, int, 0);
module_param(sectors, int, 0);

short	*sector2unit;				/* used to locate the erase unit containing a DoC sector */
char	**diff_tables;				/* directory of allocated pages for diff */
char	**diff_sector;				/* pointers for each sector on his diff */
int	nb_diff_tables;				/* total number of entries in directory */
int	nb_used;				/* number of diff used in the last allocated entry */
int	max_allocatable, registered_blkdev;

static struct request_queue * queue;
spinlock_t lock;

volatile uchar  *doc8  = (uchar  *)DOCMP_BASE;
volatile ushort *doc16 = (ushort *)DOCMP_BASE;

inline uchar DoC_reg_read(int addr)
{
	return doc8[addr];
}

inline void DoC_reg_write(int addr, uchar v)
{
	doc8[addr] = v;
}

inline ushort DoC_reg_read16(int addr)
{
	//if (addr & 1) printk("A620 DoC : DoC_reg_read16 with an odd address !\n");
	return doc16[addr>>1];
}

inline void DoC_reg_write16(int addr, ushort v)
{
	//if (addr & 1) printk("A620 DoC : DoC_reg_write16 with an odd address !\n");
	doc16[addr>>1] = v;
}

void DoC_SendCommand(uchar command)
{
	DoC_reg_write(DoC_Mplus_FlashCmd,      command);
	DoC_reg_write(DoC_Mplus_WritePipeTerm, command);
	DoC_reg_write(DoC_Mplus_WritePipeTerm, command);
}

void DoC_SetAddress(ushort page, uchar offset)
{
	uchar	page_h, page_l;

	page_h = (uchar)(page >> 8);
	page_l = (uchar)(page & 255);

	DoC_reg_write(DoC_Mplus_FlashAddress, offset);
	DoC_reg_write(DoC_Mplus_FlashAddress, page_l);
	DoC_reg_write(DoC_Mplus_FlashAddress, page_h);

	DoC_reg_write(DoC_Mplus_WritePipeTerm, 0);
	DoC_reg_write(DoC_Mplus_WritePipeTerm, 0);
}

#define CDSN_CTRL_FR_B_MASK	(CDSN_CTRL_FR_B0 | CDSN_CTRL_FR_B1)

void DoC_WaitForReady(void)
{
	const int	max_iter = 10000;
	int		it = 0;

	DoC_reg_read(DoC_Mplus_NOP);
	DoC_reg_read(DoC_Mplus_NOP);

	while (((DoC_reg_read(DoC_Mplus_FlashControl) & CDSN_CTRL_FR_B_MASK) != CDSN_CTRL_FR_B_MASK) && (it++ < max_iter));

	if (it >= max_iter) printk("A620 DoC : DoC_WaitForReady aborted loop\n");
}

void DoC_InitRead(void)
{
	DoC_reg_read(DoC_Mplus_ReadPipeInit);
	DoC_reg_read(DoC_Mplus_ReadPipeInit);
}

void DoC_ReadData8(uchar *data, int len)
{
	while(len--) *data++ = DoC_reg_read(DoC_Mil_CDSN_IO);
}

void DoC_ReadData16(ushort *data, int len)
{
	len >>= 1;
	while(len--) *data++ = DoC_reg_read16(DoC_Mil_CDSN_IO);
}

void DoC_TerminateRead(void)
{
	DoC_reg_read(DoC_Mplus_LastDataRead);
	DoC_reg_read(DoC_Mplus_LastDataRead1);
}

void DoC_SendReset(void)
{
	DoC_SendCommand(NAND_CMD_RESET);
}

void DoC_SendReadData(void)
{
	DoC_SendCommand(NAND_CMD_READ0);
}

void DoC_SendReadOOB(void)
{
	DoC_SendCommand(NAND_CMD_READOOB);
}

char *ff_allocate_diff(int sector)
{
	if (sector < 0 || sector >= sectors) return NULL;

	/* if this sector was already modified */
	if (diff_sector[sector] == NULL)
	{
		/* check if we have enough space into the current global diff table */
		if (nb_diff_tables == 0 || nb_used >= DIFF_GROW)
		{
			if (nb_diff_tables >= max_allocatable)
			{
				printk(FF_NAME ": too many entries in directory\n");
				return NULL;
			}
			diff_tables[nb_diff_tables++] = (char *)vmalloc(KERNEL_SECTOR_SIZE * DIFF_GROW);
			if (diff_tables[nb_diff_tables-1] == NULL)
			{
				printk(FF_NAME ": ran out of memory while allocating an entry of diff directory\n");
				return NULL;
			}
			nb_used = 0;
		}

		diff_sector[sector] = diff_tables[nb_diff_tables-1] + KERNEL_SECTOR_SIZE * (nb_used++);
	}

	return diff_sector[sector];
}

char DoC_Probe(void)
{
	char	mode;
	uchar	c1, c2, c3;

	mode = 0x1d;
	DoC_reg_write(DoC_Mplus_DOCControl,   mode);
	DoC_reg_write(DoC_Mplus_CtrlConfirm, ~mode);

	// Test the toggle register
	c1 = DoC_reg_read(DoC_Mplus_Toggle);
	c2 = DoC_reg_read(DoC_Mplus_Toggle);
	c3 = DoC_reg_read(DoC_Mplus_Toggle);

	//printk("Probing : 0x%02x 0x%02x 0x%02x\n", c1, c2, c3);
	if ((c1 & DOC_TOGGLE_BIT) == (c2 & DOC_TOGGLE_BIT)) {printk("Probe DoCM+ failed !\n"); return 0;}
	if ((c2 & DOC_TOGGLE_BIT) == (c3 & DOC_TOGGLE_BIT)) {printk("Probe DoCM+ failed !\n"); return 0;}
	if ((c1 & DOC_TOGGLE_BIT) != (c3 & DOC_TOGGLE_BIT)) {printk("Probe DoCM+ failed !\n"); return 0;}

	printk(FF_NAME ": DoC found\n");

	return 1;
}

void DoC_CheckASIC(void)
{
	uchar mode;
	if ((DoC_reg_read(DoC_Mplus_DOCControl) & DOC_MODE_NORMAL) == 0)
	{
		mode = DOC_MODE_NORMAL | DOC_MODE_MDWREN;
		DoC_reg_write(DoC_Mplus_DOCControl,   mode);
		DoC_reg_write(DoC_Mplus_CtrlConfirm, ~mode);
	}
}

void ff_read_DoC_sector(int sector, char *buffer)
{
	short	page;

	DoC_CheckASIC();
	DoC_reg_write(DoC_Mplus_FlashSelect, DOC_FLASH_WP | DOC_FLASH_CE);

	DoC_SendReset();
	DoC_WaitForReady();

	page = (sector >> 1) & 31;
	DoC_SendReadData();
	DoC_SetAddress((sector2unit[sector] << 5) | page, 0);
	DoC_reg_write(DoC_Mplus_FlashControl, 0);
	DoC_WaitForReady();

	DoC_InitRead();
	if ((sector & 1) == 0)
		DoC_ReadData16((ushort *)buffer, 512);
	else
	{
		DoC_ReadData16((ushort *)temp,   522); // 522 = 512 + 10 (DATA + ECC)
		DoC_ReadData16((ushort *)buffer, 512);
	}
	DoC_TerminateRead();
}

int ff_read_DoC_signature(int unit)
{
	int	sig;

	DoC_CheckASIC();
	DoC_reg_write(DoC_Mplus_FlashSelect, DOC_FLASH_WP | DOC_FLASH_CE);

	DoC_SendReset();
	DoC_WaitForReady();

	DoC_SendReadOOB();
	DoC_SetAddress(unit << 5, 8);
	DoC_reg_write(DoC_Mplus_FlashControl, 0);
	DoC_WaitForReady();

	DoC_InitRead();
	DoC_ReadData16((ushort*)&sig, 4);
	DoC_TerminateRead();

	return sig;
}

void ff_read_logical_sector(int sector, char *buffer)
{
	int	f_sector;
	int	f_offset, l_offset;
	int	f_doc,    l_doc;
	int	block, readed, page;
	int	error;

	if (sector < 0 || sector >= sectors) return;

	// Did we have a diff ?
	if (diff_sector[sector] != NULL)
		memcpy(buffer, diff_sector[sector], KERNEL_SECTOR_SIZE);
	else
	{
		// Which compressed block corresponds to this logical sector ?
		block = sector * sector_size / UNCOMPRESSED_BLOCK_SIZE;

		// Security check
		if (block < 0 || block >= nb_compressed_blocks)
		{
			printk(FF_NAME ": warning, reading behind limits\n");
			printk(FF_NAME ": requesting bloc %d. Blocs are numbered 0 to %d\n", block, nb_compressed_blocks - 1);
			memset(buffer, 0, sector_size);
			return;
		}

		// last_readed_logical represents the number of the last uncompressed
		// block. So we have to check first if the sector we want to read is still
		// in this buffer
		if (last_readed_logical != block)
		{
			// We do not have this sector in our buffer, so first look into dict
			// to locate compressed datas, and then read them from DoC
			f_offset = compressed_directory[block].offset;
			l_offset = compressed_directory[block].size + f_offset;
			f_doc    = f_offset / sector_size;
			l_doc    = l_offset / sector_size;

			if (f_doc == l_doc)
			{
				// All on datas in the same DoC page
				ff_read_DoC_sector(f_doc, compressed);
				stream.next_in   = compressed + (f_offset % sector_size);
				stream.avail_in  = l_offset - f_offset;
			}
			else
			{
				// Compressed datas are on multiple DoC pages
				// Read first incomplete page
				ff_read_DoC_sector(f_doc, temp);
				readed = sector_size - (f_offset % sector_size);
				memcpy(compressed, temp + (sector_size - readed), readed);

				// Read full page between
				for (page = f_doc + 1; page < l_doc; page++)
				{
					ff_read_DoC_sector(page, compressed + readed);
					readed += sector_size;
				}

				// Read last incomplete page
				ff_read_DoC_sector(l_doc, temp);
				memcpy(compressed+readed, temp, l_offset % sector_size);
				readed += l_offset % sector_size;

				stream.next_in   = compressed;
				stream.avail_in  = readed;
			}

			stream.next_out  = last_logical;
			stream.avail_out = UNCOMPRESSED_BLOCK_SIZE;

			// Inflate block
			zlib_inflateInit(&stream);
			error = zlib_inflate(&stream, Z_FINISH);
			if (error != Z_STREAM_END)
			{
				printk(FF_NAME ": error reading compressed block %d\n", block);
				printk(FF_NAME ": size is %d\n", l_offset - f_offset);
				printk(FF_NAME ": zlib error %d while inflating block %d. Only %ld bytes inflated\n", error, block, stream.total_out);
				if (stream.msg)
					printk(FF_NAME ": message was \"%s\"\n", stream.msg);
			}
			zlib_inflateEnd(&stream);

			last_readed_logical = block;
		}

		// Copy sector from buffer of the last inflated block
		f_sector = last_readed_logical * UNCOMPRESSED_BLOCK_SIZE / sector_size;
		memcpy(buffer, last_logical + (sector - f_sector) * sector_size, sector_size);
	}
}

void ff_write_logical_sector(int sector, char *buffer)
{
	char	*table;

	if (sector < 0 || sector >= sectors) return;

	table = ff_allocate_diff(sector);
	if (table) memcpy(table, buffer, KERNEL_SECTOR_SIZE);
}

void ff_transfer(int sector, int nb, char *buffer, int write)
{
	void (*action)(int,char *);

	if (sector < 0 || sector >= sectors)   return;
	if (nb < 0 || (sector + nb) > sectors) return;

	action = write?ff_write_logical_sector:ff_read_logical_sector;

	while (nb--)
	{
		action(sector++, buffer);
		buffer += KERNEL_SECTOR_SIZE;
	}
}

int ff_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct hd_geometry geo;
	int size;

	switch(cmd)
	{
		case HDIO_GETGEO:
			size          = sectors * sector_size;
			geo.cylinders = size / (nb_heads * nb_sect_per_tracks);
			geo.heads     = nb_heads;
			geo.sectors   = nb_sect_per_tracks;
			geo.start     = 4;
			if (copy_to_user((void *)arg, &geo, sizeof(geo)))
				return -EFAULT;
			return 0;

		// Interface to query information about current state of the module
		case 0x1000:
			return nb_diff_tables;
		case 0x1001:
			return nb_used;
		case 0x1002:
			if (arg < 0 || arg >= sectors) return -EFAULT;
			return diff_sector[arg] != NULL ? 1 : 0;
	}
	return -EINVAL;
}

static struct block_device_operations operations = {
	.owner  = THIS_MODULE,
	.ioctl	= ff_ioctl
};

static void ff_request(request_queue_t *q)
{
	struct request *req;

	while ((req = elv_next_request(q)) != NULL)
	{
		if (! blk_fs_request(req))
		{
			end_request(req, 0);
			continue;
		}
		ff_transfer(req->sector, req->current_nr_sectors, req->buffer, rq_data_dir(req));
		end_request(req, 1);
	}
}

void ff_cleanup_variables(void)
{
	disk                = NULL;
	queue               = NULL;
	diff_tables         = NULL;
	diff_sector         = NULL;
	sector2unit         = NULL;
	stream.workspace    = NULL;
	full_dict           = NULL;
	registered_blkdev   = 0;
	nb_diff_tables      = 0;
	last_readed_logical = -1;
}

static void ff_cleanup(void)
{
	printk(FF_NAME ": cleanup\n");

	if (disk)
	{
		del_gendisk(disk);
		put_disk(disk);
	}

	if (registered_blkdev) unregister_blkdev(FF_MAJOR, FF_NAME);
	if (queue)             blk_cleanup_queue(queue);

	while(nb_diff_tables) vfree(diff_tables[--nb_diff_tables]);

	if (diff_sector) vfree(diff_sector);
	if (diff_tables) vfree(diff_tables);
	if (sector2unit) vfree(sector2unit);
	if (full_dict)   vfree(full_dict);

	if (stream.workspace)
	{
		zlib_inflateEnd(&stream);
		vfree(stream.workspace);
	}

	ff_cleanup_variables();
}

static int ff_init(void)
{
	int	i, nb_doc_sectors;
	int	*header_ptr;
	int	unit, curr, s;

	printk(FF_NAME ": init\n");

	// Cleanup variables
	ff_cleanup_variables();

	if (!DoC_Probe())
	{
		printk(FF_NAME ": DoC not found\n");
		ff_cleanup();
		return -EIO;
	}

	// Init the zlib
	printk(FF_NAME ": init zlib\n");
	stream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (!stream.workspace)
	{
		printk(FF_NAME ": no memory for zlib workspace\n");
		ff_cleanup();
		return -ENOMEM;
	}
	stream.next_in  = NULL;
	stream.avail_in = 0;
	zlib_inflateInit(&stream);

	// Allocate diff structures
	max_allocatable = 1 + (sectors / DIFF_GROW);
	sector2unit     = (short *)vmalloc(sizeof(short)*sectors);
	diff_sector     = (char **)vmalloc(sizeof(char *)*sectors);
	diff_tables     = (char **)vmalloc(sizeof(char *)*max_allocatable);
	memset(diff_sector, 0, sizeof(char *)*sectors);

	// Read all erase units signatures of DoC
	// this is because we whant to be able to handle the case where
	// the erase units are not contiguous. All erase units used for the
	// while system image has the "ASUS" signature into it's OOB.
	printk(FF_NAME ": scanning DoC\n");

	for (unit=4, curr=0; unit<2048 && curr < skip; unit++)
		if (ff_read_DoC_signature(unit) == SIGNATURE)
			curr++;

	for (curr = 0;unit<2048 && curr < sectors; unit++)
		if (ff_read_DoC_signature(unit) == SIGNATURE)
			for (s=0; s<64; s++)
				if (curr+s < sectors)
					sector2unit[curr++] = unit;

	printk(FF_NAME ": scan done !\n");

	// Read the dictionnary of compressed blocks
	ff_read_DoC_sector(0, temp);
	header_ptr = (int *)temp;
	printk(FF_NAME ": block size are %d\n", header_ptr[0]);
	if (header_ptr[0] != UNCOMPRESSED_BLOCK_SIZE)
	{
		printk(FF_NAME ": error, block size should be %d\n", UNCOMPRESSED_BLOCK_SIZE);
		ff_cleanup();
		return -EIO;
	}
	nb_compressed_blocks = header_ptr[1];
	printk(FF_NAME ": %d compressed blocks\n", nb_compressed_blocks);
	nb_doc_sectors = (nb_compressed_blocks * sizeof(ENTRY) + 2*sizeof(int) + 511) / 512;
	full_dict = (uchar *)vmalloc(nb_doc_sectors * 512);
	for (i=0; i<nb_doc_sectors; i++)
		ff_read_DoC_sector(i, full_dict + 512*i);
	compressed_directory = (ENTRY *)(full_dict + 2*sizeof(int));

	// Register block device
	if (register_blkdev(FF_MAJOR, FF_NAME))
	{
		printk(FF_NAME ": cannot register block device\n");
		ff_cleanup();
		return -EIO;
	}
	registered_blkdev = 1;

	// Initialize queue
	spin_lock_init(&lock);
	if ((queue = blk_init_queue(ff_request, &lock)) == NULL)
	{
		printk(FF_NAME ": unable to allocate queue\n");
		ff_cleanup();
		return -EIO;
	}
	blk_queue_hardsect_size(queue, sector_size);

	// Create the generic disk
	disk              = alloc_disk(16);
	disk->major       = FF_MAJOR;
	disk->first_minor = 0;
	disk->fops        = &operations;
	disk->queue       = queue;
	strcpy(disk->disk_name, "ff0");
	set_capacity(disk, sectors);

	add_disk(disk);

	return 0;
}

module_init(ff_init);
module_exit(ff_cleanup);

MODULE_AUTHOR("Vincent Benony");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(FF_MAJOR);
