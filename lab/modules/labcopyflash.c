/*
 *  lab/modules/labcopyflash.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, September 08, 2003
 *
 *  flash module for LAB "copy" command.
 */

#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/mtd/mtd.h>
#include <linux/lab/lab.h>
#include <linux/lab/copy.h>

static int flashcheck(char* filename)
{
	struct mtd_info *mtd;
	
	if (filename[1] != '\0')
		return 0;
	if (filename[0] < '0')
		return 0;
	if (filename[0] > '9')
		return 0;
	mtd = get_mtd_device(NULL, filename[0]-'0');
	if (!mtd)
		return 0;

	if (mtd->type == MTD_NANDFLASH)
	{
		lab_puts("As a precaution, flash: cannot write NAND flash.\r\n"
		         "Try nand:jffs2: for that.\r\n");
		put_mtd_device(mtd);
		return 0;
	}

	if (mtd->type == MTD_ABSENT)
	{
		put_mtd_device(mtd);
		return 0;
	}
	put_mtd_device(mtd);
	
	return 1;
}

static unsigned char* get(int* count, char* filename)
{
	struct mtd_info *mtd;
	void* ptr;
	
	if (filename[1] != '\0')
		return 0;
	if (filename[0] < '0')
		return 0;
	if (filename[0] > '9')
		return 0;
	mtd = get_mtd_device(NULL, filename[0]-'0');
	if (!mtd)
		return 0;
	if (mtd->type == MTD_ABSENT)
	{
		put_mtd_device(mtd);
		return 0;
	}
	
	*count = mtd->size;
	
	ptr = vmalloc(mtd->size);
	if (!ptr)
	{
		printk("lab: AIIIIIIIIIIIIIIIIIGHHH!! out of memory!!\n");
		printk("lab: I guess we just can't allocate 0x%08x bytes???\n", mtd->size);
		put_mtd_device(mtd);
		return 0;
 	}
	
	printk("lab: reading %08x bytes from flash\n", mtd->size);
	if (MTD_READ(mtd, 0, mtd->size, count, ptr))
	{
		printk("lab: uh. couldn't read from flash.\n");
		put_mtd_device(mtd);
		vfree(ptr);
		return 0;
	}
	
	put_mtd_device(mtd);
	return (unsigned char*)ptr;
}

static void erase_callback(struct erase_info *ei)
{
	wait_queue_head_t* wq = (wait_queue_head_t*)ei->priv;
	wake_up(wq);
}

static int put(int count, unsigned char* data, char* filename)
{
	struct mtd_info* mtd;
	unsigned int addr;
	struct erase_info erase;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wq;
	int ret;
	
	if (filename[1] != '\0')
		return 0;
	if (filename[0] < '0')
		return 0;
	if (filename[0] > '9')
		return 0;
	mtd = get_mtd_device(NULL, filename[0]-'0');
	if (!mtd)
		return 0;
	if (mtd->type == MTD_ABSENT)
	{
		put_mtd_device(mtd);
		return 0;
	}
	
	if (count > mtd->size)
	{
		lab_puts("Aiee! Too big!\r\n");
		put_mtd_device(mtd);
		return 0;
	}
	
	/* unlock */
	lab_puts("Unprotecting flash:\t");
	addr = 0;
	while (addr < mtd->size)
	{
		lab_printf("\rUnprotecting flash:\t%08X / %08X", addr, mtd->size);
		
		if (!mtd->unlock)
		{
			lab_puts("\rUnprotecting flash:\tunnecessary?                          \r\n");
			goto unprodone;
		}
		ret = (*(mtd->unlock))(mtd, addr, mtd->erasesize);
		if (ret)
		{
			lab_printf("\rUnprotect failure:\t%08X / %08X\tErrno: %d\r\n", addr, mtd->size, ret);
			put_mtd_device(mtd);
			return 0;
		}
		
		addr += mtd->erasesize;
	};
	lab_puts("\rUnprotecting flash:\tdone                              \r\n");

unprodone:	
	init_waitqueue_head(&wq);
	
	erase.mtd = mtd;
	erase.callback = erase_callback;
	erase.priv = (u_long)&wq;
	
	/* erase */
	addr = 0;
	while (addr < mtd->size)
	{
		lab_printf("\rErasing flash:\t\t%08X / %08X", addr, mtd->size);
		
		erase.addr = addr;
		erase.len = mtd->erasesize;
		
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wq, &wait);
		
		ret = MTD_ERASE(mtd, &erase);
		if (ret) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wq, &wait);
			lab_printf("\rErase failure:\t%08X / %08X\tErrno: %d\r\n", addr, mtd->size, ret);
			return 0;
		}
		schedule();
		remove_wait_queue(&wq, &wait);
		
		addr += mtd->erasesize;
	}
	lab_puts("\rErasing flash:\t\tdone                   \r\n");
	
	addr = 0;
	while (count)
	{
		int rlen;
		int wcount;
		
		wcount = (count > mtd->erasesize) ? mtd->erasesize : count;

		lab_printf("\rWriting flash:\t\t%08X / %08X", addr, addr+count);
		
		ret = MTD_WRITE(mtd, addr, wcount, &rlen, data);
		if (!ret) {
			addr += rlen;
			count -= rlen;
			data += rlen;
		} else {
			lab_puts("error\r\n");
			put_mtd_device(mtd);
			return 0;
		};
	}
	lab_puts("\rWriting flash:\t\tdone                   \r\n");	
	put_mtd_device(mtd);

	return 1;
}

int  labcopyflash_init(void)
{
	lab_copy_addsrc ("flash",flashcheck,get);
	lab_copy_adddest("flash",flashcheck,put);
	
	return 0;
}

void labcopyflash_cleanup(void)
{	
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy flash module");
MODULE_LICENSE("GPL");
module_init(labcopyflash_init);
module_exit(labcopyflash_cleanup);
