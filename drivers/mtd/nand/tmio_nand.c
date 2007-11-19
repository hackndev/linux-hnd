/*
 *  drivers/mtd/nand/nand_tmio.c
 *
 * (c) Ian Molton and Sebastian Carlier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is a driver for the NAND flash controller on the TMIO
 * (Toshiba Multio IO Controller) as found in the toshiba e750 and e800 PDAs.
 *
 * This Driver should support the TC6393XB SoC 'smartmedia' controller.
 *
 * This is alpha quality. This chip did not mesh well with the linux
 * NAND flash base code as it has some funny requirements as to when you can
 * write bytes to the data register (eg. you need to write the address as a
 * 32 bit word).
 *
 * I have used the default mtd NAND read byte and write byte functions since
 * the few places they are used seem to be OK with this. The exception was the
 * read_id code, which had to be split off as an ovverideable function thanks
 * to his chip requiring a word (32bit) read, not two byte accesses, doh!)
 *
 * This chip has a hard ECC unit, but this appears to be buggy, at least for
 * reads. This may be solved by reading halfwords, but thats a hunch I havent
 * tried yet.
 *
 * Oh, also - this code assumes all buffers are a multiple of 4 bytes (1 word)
 * long (due to the use of word read/writes in {read,write,verify}_buf).
 *
 *                                          -Ian Molton <spyro@f2s.com>
 *  Revision history:
 *    23/08/2004     First working version
 * 
 *  TO DO list
 *    Do full chip initialisation (rather than rely on winCE)
 *    Fix hwECC if poss.
 *    Make use of chip 'ready' interrupt.
 *
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/soc-old.h>
#include <asm/io.h>
#include <asm/sizes.h>

#include <linux/mfd/tmio_nand.h>
#include "tmio_nand.h"

/* 
 *	hardware specific access to control-lines
 */
static void tmio_read_id(struct mtd_info *mtd, int *maf_id, int *dev_id) {
	struct nand_chip *this = mtd->priv;
	unsigned long id;

	/* Send the command for reading device ID */
	this->cmdfunc (mtd, NAND_CMD_READID, 0x00, -1);

	id = readl(this->IO_ADDR_R);
	*maf_id = id & 0xff;
	*dev_id = (id >> 8) & 0xff;
}

static void tmio_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16*)buf;
	int i;

	BUG_ON(len & 0x1);

	len >>= 1;

	for (i=0; i < len; i++){
		p[i] = readw(this->IO_ADDR_R);
	}
}

static void tmio_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16*)buf;
	int i;

	BUG_ON(len & 0x1);

	len >>= 1;

	for (i=0; i < len; i++)
		writew(p[i], this->IO_ADDR_W);
}

static int tmio_verify_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16*)buf;
	int i;

	BUG_ON(len & 0x1);

	len >>= 1;

	for (i=0; i<len; i++)
		if (p[i] != readw(this->IO_ADDR_R))
			return -EFAULT;

	return 0;
}

static unsigned char stat;
#if 0
// HARDWARE ECC DOESNT WORK YET (hardware bug?)
static void tmio_enable_hwecc(struct mtd_info *mtd, int mode)
{
        struct nand_chip *this = mtd->priv;
	// Reset ECC
	writeb(stat | 0x60, this->IO_ADDR_W + TMIO_MODE_REG);
	readb(this->IO_ADDR_R);
	// Enable ECC
	writeb(stat | 0x20, this->IO_ADDR_W + TMIO_MODE_REG);
}

static int tmio_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
                                 unsigned char *ecc_code)
{
        struct nand_chip *this = mtd->priv;
	unsigned char data_1[4], data_2[4];

	//FIXME: not big-endian safe

	// Read ECC result
	writeb(stat | 0x40, this->IO_ADDR_W + TMIO_MODE_REG);
	*(u32*)data_1 = readl(this->IO_ADDR_R);
	*(u32*)data_2 = readl(this->IO_ADDR_R);
	// Disable ECC
	writeb(stat, this->IO_ADDR_W + TMIO_MODE_REG);
	ecc_code[0] = data_2[0];
	ecc_code[1] = data_1[3];
	ecc_code[2] = data_2[1];
	ecc_code[3] = data_1[1];
	ecc_code[4] = data_1[0];
	ecc_code[5] = data_1[2];
	return 0;
}
#endif

static void tmio_hwcontrol(struct mtd_info *mtd, int cmd) 
{
	struct nand_chip *this = mtd->priv;

	switch(cmd) {

		case NAND_CTL_SETCLE: stat |=  TNAND_MODE_BIT_CLE; break;
		case NAND_CTL_CLRCLE: stat &= ~TNAND_MODE_BIT_CLE; break;

		case NAND_CTL_SETALE: stat |=  TNAND_MODE_BIT_ALE; break;
		case NAND_CTL_CLRALE: stat &= ~TNAND_MODE_BIT_ALE; break;

		/* we (could) enable / disable power here too. */
		/* we might want to consider an mdelay(5) here if so */
		case NAND_CTL_SETNCE: stat |=  TNAND_MODE_BIT_CE; break;
		case NAND_CTL_CLRNCE: stat &= ~TNAND_MODE_BIT_CE; break;
	}

//	printk("ctl: 0x%02x\n", stat);
	writeb(stat, this->IO_ADDR_W + TNAND_MODE_REG);
}

// This is the main hack to the default drivers - this chip is funy in
// that the address must be written as a word, not three bytes writes.

static void tmio_command (struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
	register struct nand_chip *this = mtd->priv;

	/* Begin command latch cycle */
	this->hwcontrol(mtd, NAND_CTL_SETCLE);
	/*
	 * Write out the command to the device.
	 */
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->oobblock) {
			/* OOB area */
			column -= mtd->oobblock;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			/* First 256 bytes --> READ0 */
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}
		writeb(readcmd, this->IO_ADDR_W);
	}
	writeb(command, this->IO_ADDR_W);

	/* Set ALE and clear CLE to start address cycle */
	this->hwcontrol(mtd, NAND_CTL_CLRCLE);

	if (column != -1 || page_addr != -1) {
		this->hwcontrol(mtd, NAND_CTL_SETALE);

		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			writeb(column, this->IO_ADDR_W);
		}
		if (page_addr != -1)
			writel(page_addr & 0x00ffffff, this->IO_ADDR_W);

		/* Latch in address */
		this->hwcontrol(mtd, NAND_CTL_CLRALE);
	}

	/*
	 * program and erase have their own busy handlers
	 * status and sequential in needs no delay
	 */
	switch (command) {
		case NAND_CMD_PAGEPROG:
		case NAND_CMD_ERASE1:
		case NAND_CMD_ERASE2:
		case NAND_CMD_SEQIN:
		case NAND_CMD_STATUS:
			return;
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	ndelay (100);
	/* wait until command is processed */
	while (!this->dev_ready(mtd));
}

/*
 *	read device ready pin
 */
static int tmio_device_ready(struct mtd_info *mtd)
{
	register struct nand_chip *this = mtd->priv;
	return (readb(this->IO_ADDR_R + TNAND_STATUS_REG) & TNAND_STATUS_BUSY) ? 0 : 1;
}

#ifdef CONFIG_MTD_PARTITIONS
// Nice and simple - one big partition.
static struct mtd_partition partition_a = {
	.name = "Internal NAND flash",
	.offset =  0,
	.size =  MTDPART_SIZ_FULL,
};
#endif

// WinCE uses this ECC layout (it uses SSFDC). lets play nice.
struct nand_oobinfo tmio_oobinfo  = {
	.eccpos = {14,13,15,9,8,10},
	.oobfree = {{0,4},{6,2},{11,2}},
	.eccbytes = 6,
	.useecc = MTD_NANDECC_AUTOPLACE,
};

/*
 * Main initialization routine
 */
static int tmio_nand_probe (struct device *dev)
{
	struct platform_device *sdev = to_platform_device(dev);
	struct tmio_nand_hwconfig *hwconfig = (struct tmio_hwconfig *)dev->platform_data;
	struct mtd_info *tmio_mtd;
	struct nand_chip *tmio_chip;
	struct tmio_mtd_data *dev_data;
	int ret = -ENOMEM;

	/* Allocate memory for MTD device structure and private data */
	tmio_mtd = kmalloc(sizeof(struct nand_chip) + sizeof(struct mtd_info),
	                   GFP_KERNEL);
	if (!tmio_mtd)
		goto out;

	dev_data = kmalloc(sizeof(struct tmio_mtd_data), GFP_KERNEL);
	if(!dev_data)
		goto out;

	memset((char *)tmio_mtd, 0, sizeof(struct nand_chip) + sizeof(struct mtd_info));

	tmio_chip = (struct nand_chip *)&tmio_mtd[1];
	tmio_mtd->priv = tmio_chip;
	dev_data->mtd = tmio_mtd;
	dev_data->cnf_base = ioremap((unsigned long)sdev->resource[1].start,
	                             (unsigned long)sdev->resource[1].end -
	                             (unsigned long)sdev->resource[1].start);
	if(!dev_data->cnf_base)
		goto out;
	dev_data->ctl_base = ioremap((unsigned long)sdev->resource[0].start,
	                             (unsigned long)sdev->resource[0].end -
	                             (unsigned long)sdev->resource[0].start);
	if(!dev_data->ctl_base)
		goto out;

	dev_set_drvdata(dev, dev_data);

	/* Setup any state required by the SoC chip we're part of */
	if(hwconfig && hwconfig->hwinit)
		hwconfig->hwinit(sdev);

	/* Setup the nand control register base */
        writeb(SMCREN, dev_data->cnf_base + 0x04);
        writel((unsigned long)sdev->resource[0].start & 0xfffe,
	       dev_data->cnf_base + 0x10);

	/* Enable the clock */
        writeb(SCRUNEN, dev_data->cnf_base + 0x4c);
	/* ??? */
        writeb(0x02, dev_data->cnf_base + 0x62);

	/* Clear and disable interrupts */
	writeb(0x00, dev_data->ctl_base + TNAND_INTMASK_REG);
	writeb(0x0f, dev_data->ctl_base + TNAND_INTCTL_REG);

	/* Turn on power */
	writeb(TNAND_MODE_POWER_ON,     dev_data->ctl_base + TNAND_MODE_REG);

	/* Reset media */
	writeb(TNAND_MODE_READ_COMMAND, dev_data->ctl_base + TNAND_MODE_REG);
	writeb(NAND_CMD_RESET,          dev_data->ctl_base + TNAND_CMD_REG);

	/* Standby Mode smode*/
	writeb(TNAND_MODE_STANDBY,      dev_data->ctl_base + TNAND_MODE_REG);
	stat = TNAND_MODE_STANDBY; // Set stat to reflect the register.

	mdelay(100);

	/* Link the private data with the MTD structure */
	/* insert callbacks */
	tmio_chip->IO_ADDR_R  = dev_data->ctl_base;
	tmio_chip->IO_ADDR_W  = dev_data->ctl_base;
	tmio_chip->cmdfunc    = tmio_command;
	tmio_chip->hwcontrol  = tmio_hwcontrol;
	tmio_chip->dev_ready  = tmio_device_ready;
	tmio_chip->read_id    = tmio_read_id;
	tmio_chip->read_buf   = tmio_read_buf;
	tmio_chip->write_buf  = tmio_write_buf;
	tmio_chip->verify_buf = tmio_verify_buf;
	tmio_chip->autooob    = &tmio_oobinfo;
	tmio_chip->eccmode    = NAND_ECC_SOFT;
#if 0
	// HW ECC doesnt work (hardware bug?)
	this->enable_hwecc = tmio_enable_hwecc;
	this->calculate_ecc = tmio_calculate_ecc;
	this->correct_data = nand_correct_data;
	this->eccmode = NAND_ECC_HW6_512;
#endif

	/* Scan to find existence of the device */
	if (nand_scan (tmio_mtd, 1)) {
		ret = -ENXIO;
		goto out_unmap;
	}

	/* Allocate memory for internal data buffer */
	tmio_chip->data_buf = kmalloc (sizeof(u_char) * (tmio_mtd->oobblock + tmio_mtd->oobsize), GFP_KERNEL);
	if (!tmio_chip->data_buf) {
		ret = -ENXIO;
		goto out_unmap;
	}

#ifdef CONFIG_MTD_PARTITIONS
	/* Register the partitions */
	add_mtd_partitions(tmio_mtd, &partition_a, 1);
#endif

	/* Return happy */
	return 0;

out_unmap:
	iounmap((void*)dev_data->ctl_base);
	kfree (tmio_mtd);
out:
	return ret;
}

/*
 * Clean up routine
 */
static int tmio_nand_remove (struct device *dev)
{
	struct tmio_mtd_data *dev_data = (struct tmio_mtd*)dev_get_drvdata(dev);
	struct mtd_info *tmio_mtd = dev_data->mtd;
	struct nand_chip *this = tmio_mtd->priv;

	//FIXME - do we need to remove the partition?

	/* Unregister the device */
	del_mtd_device (tmio_mtd);

	/* Free internal data buffer */
	kfree (this->data_buf);

	iounmap((void*)dev_data->ctl_base);
	iounmap((void*)dev_data->cnf_base);

	/* Free the MTD device structure */
	kfree (tmio_mtd);

	return 0;
}

/* ------------------- device registration ----------------------- */

static platform_device_id tmio_platform_device_ids[] = {TMIO_NAND_DEVICE_ID, 0};

struct device_driver tmio_nand_device = {
	.name = "tmio_nand",
	.bus = &platform_bus_type,
	.probe = tmio_nand_probe,
	.remove = tmio_nand_remove,
};

static int __init tmio_nand_init(void)
{
	printk("tmio_nand_init()\n");
	return driver_register (&tmio_nand_device);
}

static void __exit tmio_nand_exit(void)
{
	return driver_unregister (&tmio_nand_device);
}

module_init(tmio_nand_init);
module_exit(tmio_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ian Molton and Sebastian Carlier");
MODULE_DESCRIPTION("MTD map driver for TMIO NAND controller.");
