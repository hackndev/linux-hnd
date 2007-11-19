/*
 * lablsblk.c
 * An lsblk module for LAB
 *
 * Copyright (c) 2006 Paul Sokolovsky
 * Based on genhd.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/genhd.h>
#include <linux/seq_file.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

extern struct seq_operations partitions_op;

static void lab_cmd_lsblk(int argc,const char** argv);

int lablsblk_init(void)
{
	lab_addcommand("lsblk", lab_cmd_lsblk, "List block devices");
	return 0;
}

void lablsblk_cleanup(void)
{
	lab_delcommand("lsblk");
}

static void lab_cmd_lsblk(int argc, const char** argv)
{
	struct gendisk *sgp;
	char buf[BDEVNAME_SIZE];
	loff_t pos = 0;

	if (argc != 1) {
		lab_puts("lsblk: syntax: lsblk\r\n");
		return;
	}

	for (sgp = partitions_op.start(NULL, &pos); sgp; sgp = partitions_op.next(NULL, sgp, &pos)) {
	        int n;
		lab_printf("%4d  %4d %10llu %s\r\n", sgp->major, sgp->first_minor, 
			(unsigned long long)get_capacity(sgp) >> 1,
			disk_name(sgp, 0, buf));
		for (n = 0; n < sgp->minors - 1; n++) {
			if (!sgp->part[n])
				continue;
			if (sgp->part[n]->nr_sects == 0)
				continue;
			lab_printf("%4d  %4d %10llu %s\r\n",
				sgp->major, n + 1 + sgp->first_minor,
				(unsigned long long)sgp->part[n]->nr_sects >> 1 ,
				disk_name(sgp, n + 1, buf));
		}
	}

	partitions_op.stop(NULL, NULL);

}

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("LAB lsblk Module");
MODULE_LICENSE("GPL");
module_init(lablsblk_init);
module_exit(lablsblk_cleanup);
