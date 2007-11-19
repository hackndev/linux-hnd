/*
 * NAND flash driver for Grayhill Duramax H/HG
 *
 * Copyright 2007 SDG Systems, LLC
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author:  Matt Reimer <mreimer@sdgsystems.com>
 *          March 2007
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/gpio.h>
#include <asm/io.h>

#include <asm/arch/hardware.h>
#include <asm/arch/ghi270-nand.h>
#include <asm/arch/ghi270.h>


/* latch bits */
#define GHI270_NAND_nWP	(1 << 0)
#define GHI270_NAND_ALE (1 << 1)
#define GHI270_NAND_nCE	(1 << 2)
#define GHI270_NAND_CLE	(1 << 3)


struct ghi270_nand_host {
	struct nand_chip	nand_chip;
	struct mtd_info		mtd;
	struct mtd_partition	*mtd_partitions;
	void __iomem		*io_base;
	struct ghi270_nand_data	*board;
};

static void
ghi270_nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	unsigned char bits;

	if (ctrl & NAND_CTRL_CHANGE) {

		bits = GHI270_NAND_nWP;
		if (ctrl & NAND_NCE) {
			bits &= ~GHI270_NAND_nCE;
		}

		if (ctrl & NAND_CLE) {
			bits |= GHI270_NAND_CLE;
		}

		if (ctrl & NAND_ALE) {
			bits |= GHI270_NAND_ALE;
		}

		__raw_writel(bits, chip->IO_ADDR_W + 4);
	}

	if (cmd != NAND_CMD_NONE) {
		__raw_writel(cmd & 0xff, chip->IO_ADDR_W);
	}
}

static int
ghi270_nand_dev_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct ghi270_nand_host *host = chip->priv;
	return gpio_get_value(host->board->rnb_gpio) ? 1 : 0;
}

static void
ghi270_nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	readsb(chip->IO_ADDR_R, buf, len);
}

static void
ghi270_nand_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	writesb(chip->IO_ADDR_W, buf, len);
}


static int __init
ghi270_nand_probe(struct platform_device *pdev)
{
	struct ghi270_nand_host *host;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	int err;

	host = kzalloc(sizeof(struct ghi270_nand_host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->io_base = ioremap(pdev->resource[0].start,
				pdev->resource[0].end - pdev->resource[0].start + 1);
	if (host->io_base == NULL) {
		printk(KERN_ERR "ghi270_nand: ioremap failed\n");
		kfree(host);
		return -EIO;
	}

	/* Link the private data with the MTD structure. */
	mtd = &host->mtd;
	nand_chip = &host->nand_chip;
	host->board = pdev->dev.platform_data;

	nand_chip->priv = host;
	mtd->priv = nand_chip;
	mtd->owner = THIS_MODULE;

        /* Configure the RnB GPIO. */
	gpio_direction_input(host->board->rnb_gpio);

	/* Set up callbacks, etc. */
	nand_chip->IO_ADDR_R	= host->io_base;
	nand_chip->IO_ADDR_W	= host->io_base;
	nand_chip->cmd_ctrl	= ghi270_nand_cmd_ctrl;
	nand_chip->dev_ready	= ghi270_nand_dev_ready;
	nand_chip->read_buf	= ghi270_nand_read_buf;
	nand_chip->write_buf	= ghi270_nand_write_buf;
	nand_chip->chip_delay	= 50; 	/* 12 us command delay time */
	nand_chip->ecc.mode	= NAND_ECC_SOFT;

	platform_set_drvdata(pdev, host);

	/* Scan to find existence of the device. */
	if ((err = nand_scan(mtd, 1))) {
		printk(KERN_NOTICE "No ghi270 NAND device.\n");
		goto err_out;
	}

#ifdef CONFIG_MTD_PARTITIONS
	if ((!host->board->partitions) || (host->board->n_partitions == 0)) {
		printk(KERN_ERR "ghi270_nand: No parititions defined, "
				"or unsupported device.\n");
		err = ENXIO;
		goto release;
	}

	err = add_mtd_partitions(mtd, host->board->partitions,
				 host->board->n_partitions);
#else
	err = add_mtd_device(mtd);
#endif

	if (!err)
		return 0;

release:
	nand_release(mtd);

err_out:
	platform_set_drvdata(pdev, NULL);
	iounmap(host->io_base);
	kfree(host);
	return err;
}

static int __devexit
ghi270_nand_remove(struct platform_device *pdev)
{
	struct ghi270_nand_host *host = platform_get_drvdata(pdev);
	struct mtd_info *mtd = &host->mtd;

	nand_release(mtd);

	iounmap(host->io_base);

	kfree(host);

	return 0;
}

struct platform_driver ghi270_nand_driver = {
	.probe		= ghi270_nand_probe,
	.remove		= ghi270_nand_remove,
	.driver		= {
		.name   = "ghi270-nand",
		.owner	= THIS_MODULE,
	},
};

static int __init ghi270_nand_init(void)
{
	return platform_driver_register(&ghi270_nand_driver);
}

 void __exit ghi270_nand_cleanup(void)
{
	platform_driver_unregister(&ghi270_nand_driver);
}

module_init(ghi270_nand_init);
module_exit(ghi270_nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("Grayhill Duramax H/HG NAND flash driver");
/* vim600: set noexpandtab sw=8 : */
