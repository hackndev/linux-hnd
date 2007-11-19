/* labmtd.c
 * MTD support in a module for LAB - Linux As Bootldr.
 *
 * Author: Joshua Wise
 *
 * Modified: 2004, Sep   Szabolcs Gyurko
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

#define MAX_PARTITIONS 6

static struct mtd_partition* partitions[MAX_PARTITIONS];
static int partitionsused[MAX_PARTITIONS];

void lab_partition_show(int argc,const char** argv);
void labmtd_add_partition(char* name, unsigned int addr, unsigned int size);

int labmtd_init(void)
{
	int i;
	struct mtd_info *minfo;
	
	lab_addcommand("partition", lab_partition_show, "Same as partition show");
	lab_addsubcommand("partition", "show", lab_partition_show, "Lists all defined partitions");
	for(i=0; i<MAX_PARTITIONS; i++)	partitionsused[i]=0;

/* Static MTD partitioning */
#if 0
#ifndef CONFIG_MTD_CMDLINE_PARTS 
	/* Since shamcop_nand defines a static partition I have to delete it now (in case of HAMCOP Nand) */
	minfo = get_mtd_device(NULL, 0);
	if (minfo) {
		if (!strcmp(minfo->name, "HAMCOP NAND Flash")) {
#warning "Very nasty hack...."
			printk("XXXX: Nasty hack in %s:%d", __FILE__, __LINE__);
			del_mtd_partitions(minfo);		
		}
	}
	
	labmtd_add_partition("lab", 0x00000000, 0x00040000);
	labmtd_add_partition("root", 0x00040000, 0x01FC0000);
#endif	
#endif /* 0*/
	
	return 0;
}

void labmtd_cleanup(void)
{
	lab_delcommand("partition");
}

void labmtd_add_partition(char* name, unsigned int addr, unsigned int size)
{
	int i,part;
	struct mtd_info* minfo;
	
	for (i=0; i < MAX_PARTITIONS; i++)
	{
		if (!partitionsused[i])
			break;
	}
	
	if (i == MAX_PARTITIONS)
	{
		printk(KERN_ERR "labmtd: out of partitions!\n");
		return;
	}
	
	part=i;
	partitionsused[part]=1;
	partitions[part] = kmalloc(sizeof(struct mtd_partition), GFP_KERNEL);
	partitions[part]->name = name;
	partitions[part]->size = size;
	partitions[part]->offset = addr;
	partitions[part]->mask_flags = 0;
	
	minfo = get_mtd_device(NULL, 0);
	if (!minfo)
		printk(KERN_ERR "labmtd: failed to get_mtd_device()??\n");
	add_mtd_partitions(minfo, partitions[part], 1);
}

void lab_partition_show(int argc,const char** argv)
{
	int i;
	
	for (i = 0; i < MAX_PARTITIONS; i++) {
		if (!partitionsused[i])
			continue;
		lab_printf("%s\r\n"
			   "Size:    %lX\r\n"
			   "Offset:  %lX\r\n",
			   partitions[i]->name,
			   partitions[i]->size, partitions[i]->offset);
	}
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB MTD Interface Module");
MODULE_LICENSE("GPL");
module_init(labmtd_init);
module_exit(labmtd_cleanup);
