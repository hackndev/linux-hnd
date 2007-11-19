/*
 * Driver interface to the NAND Flash controller on the Samsung HAMCOP
 * Companion chip.
 *
 * HAMCOP: iPAQ H22xx
 *	   Specifications: http://www.handhelds.org/platforms/hp/ipaq-h22xx/
 *
 * Copyright © 2004 -2006 Matt Reimer
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author:  Matt Reimer <mreimer@vpop.net>
 *          March 2004
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/mfd/hamcop_base.h>
#include <asm/hardware/ipaq-hamcop.h>


static int write_enable = 1;
module_param(write_enable, uint, 0);
MODULE_PARM_DESC(write_enable, "Enable writes to NAND flash");

static struct nand_ecclayout nand_hw_eccoob = {
	.eccbytes	= 3,
	.eccpos		= { 6, 7, 8 },
	.oobfree	= { {0, 5}, {9, 7} },
};


static struct mtd_partition h2200_partitions[] = {
	{ name: "HTC first-stage bootstrap",
		mask_flags: MTD_WRITEABLE,
		offset: 0,
		size: 16 * 1024 },
	{ name: "HTC rescue bootloader",
		mask_flags: MTD_WRITEABLE,
		offset: MTDPART_OFS_NXTBLK,
		size: 144 * 1024 },
	{ name: "second-stage loader",
		offset: MTDPART_OFS_NXTBLK,
		size: 1280 * 1024 },
	{ name: "root fs",
		offset: MTDPART_OFS_NXTBLK,
		size: MTDPART_SIZ_FULL }
};
#define NUM_PARTITIONS (sizeof(h2200_partitions) / sizeof(h2200_partitions[0]))

static const char *probes[] = { "cmdlinepart", NULL };

struct hamcop_nand_data {
	struct clk	   *clk;
	struct semaphore   lock;

	struct device      *parent;
	void               *map;

	struct mtd_info	   *mtd;
	struct mtd_partition *mtd_partitions;

	int	cle; /* command latch enable status */
	int	ale; /* address latch enable status */
};


static inline void
hamcop_nand_write_register(struct hamcop_nand_data *nand, u32 reg, u16 val)
{
	__raw_writew(val, reg + nand->map);
}

static inline u16
hamcop_nand_read_register(struct hamcop_nand_data *nand, u32 reg)
{
	return __raw_readw(reg + nand->map);
}

static void
hamcop_nand_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	struct hamcop_nand_data *nand = (struct hamcop_nand_data *)(this->priv);

	if (nand->cle)
		hamcop_nand_write_register(nand, _HAMCOP_NF_CMMD, byte);
	else if (nand->ale) 
		hamcop_nand_write_register(nand, _HAMCOP_NF_ADDR0, byte);
	else
		hamcop_nand_write_register(nand, _HAMCOP_NF_DATA, byte);
}

static void
hamcop_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int bitmask) 
{
	struct nand_chip *this = mtd->priv;
	struct hamcop_nand_data *nand = (struct hamcop_nand_data *)(this->priv);
	u16 cfg;

	nand->cle = bitmask & NAND_CLE ? 1 : 0;
	nand->ale = bitmask & NAND_ALE ? 1 : 0;

	if (bitmask & NAND_NCE) {
		cfg = hamcop_nand_read_register(nand, _HAMCOP_NF_CONT0);

		hamcop_nand_write_register(nand, _HAMCOP_NF_CONT0,
					   cfg & ~HAMCOP_NF_CONT0_NFNCE);
	} else {
		cfg = hamcop_nand_read_register(nand, _HAMCOP_NF_CONT0);
		hamcop_nand_write_register(nand, _HAMCOP_NF_CONT0,
					   cfg | HAMCOP_NF_CONT0_NFNCE);
	}

	if (cmd != NAND_CMD_NONE)
		hamcop_nand_write_byte(mtd, cmd);
}

static int
hamcop_nand_device_ready(struct mtd_info *mtd)
{
	register struct nand_chip *this = mtd->priv;
	struct hamcop_nand_data *nand = (struct hamcop_nand_data *)(this->priv);

	return (hamcop_nand_read_register(nand, _HAMCOP_NF_STAT1) &
		HAMCOP_NF_STAT1_FLASH_RNB) ? 1 : 0;
}

static void
hamcop_nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	readsb(this->IO_ADDR_R, buf, len);
}

static void
hamcop_nand_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	writesb(this->IO_ADDR_W, buf, len);
}


static void
hamcop_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	u16 cfg;
	register struct nand_chip *this = mtd->priv;
	struct hamcop_nand_data *nand = (struct hamcop_nand_data *)(this->priv);

	cfg = hamcop_nand_read_register(nand, _HAMCOP_NF_CONT0);
	hamcop_nand_write_register(nand, _HAMCOP_NF_CONT0,
				   cfg | HAMCOP_NF_CONT0_INITECC);
}

static int
hamcop_nand_correct_data(struct mtd_info *mtd, u_char *dat, u_char *read_ecc,
			 u_char *calc_ecc)
{
	if (read_ecc[0] == calc_ecc[0] &&
	    read_ecc[1] == calc_ecc[1] &&
	    read_ecc[2] == calc_ecc[2]) 
		return 0;

	/* XXX We curently have no method for correcting the error. */

	return -1;
}

static int
hamcop_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
			  u_char *ecc_code)
{
	struct nand_chip *this = mtd->priv;
	struct hamcop_nand_data *nand = (struct hamcop_nand_data *)(this->priv);
	u16 ecc;

	ecc = hamcop_nand_read_register(nand, _HAMCOP_NF_ECCL0);
	ecc_code[0] = ecc & 0xff;
	ecc_code[1] = (ecc >> 8) & 0xff;
	ecc_code[2] = hamcop_nand_read_register(nand, _HAMCOP_NF_ECCL1) & 0xff;

        return 0;
}


static void 
hamcop_nand_up(struct hamcop_nand_data *nand)
{
	clk_enable(nand->clk);

	/* Enable pullups. */
	hamcop_set_spucr(nand->parent, HAMCOP_GPIO_SPUCR_PU_fIO, 0);

	/* Set flash memory timing.
	 * PocketPC uses 0x1213. */
	hamcop_nand_write_register(nand, _HAMCOP_NF_CONF0,
				   HAMCOP_NF_CONF0_TACLS_1  |
				   HAMCOP_NF_CONF0_TWRPH0_2 |
				   HAMCOP_NF_CONF0_TWRPH1_1);

	/* No RnB interrupts. */
	hamcop_nand_write_register(nand, _HAMCOP_NF_CONT1, 0);

	/* NAND Flash controller enable */
	hamcop_nand_write_register(nand, _HAMCOP_NF_CONT0,
				   HAMCOP_NF_CONT0_WP_EN |
				   HAMCOP_NF_CONT0_MODE_NFMODE);

	/* Chip Enable -> RESET -> Wait for Ready -> Chip Disable */
	hamcop_nand_write_register(nand, _HAMCOP_NF_CMMD, NAND_CMD_RESET);

	hamcop_nand_read_register(nand, _HAMCOP_NF_STAT1);

	while (!(hamcop_nand_read_register(nand, _HAMCOP_NF_STAT1) &
		HAMCOP_NF_STAT1_FLASH_RNB));

	hamcop_nand_write_register(nand, _HAMCOP_NF_CONT0,
				   HAMCOP_NF_CONT0_WP_EN |
				   HAMCOP_NF_CONT0_NFNCE |
				   HAMCOP_NF_CONT0_MODE_NFMODE);
}

static void 
hamcop_nand_down(struct hamcop_nand_data *nand)
{
	/* XXX What about outstanding I/O? */

        /* Until the clock enable/disable functions do refcounting,
	 * DON'T disable the NAND clock because it must be on for resume
	   to work, since the bootloader SRAM is powered by the NAND clock. */
        /* clk_disable(nand->clk); */

	/* Disable pullups. */
	hamcop_set_spucr(nand->parent, HAMCOP_GPIO_SPUCR_PU_fIO,
			 HAMCOP_GPIO_SPUCR_PU_fIO);
}


#ifdef CONFIG_PM

/* XXX What if I/O is in progress? */
static int 
hamcop_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hamcop_nand_data *nand = platform_get_drvdata(pdev);

	//down (&nand->lock);  // No interruptions
	hamcop_nand_down(nand);

	return 0;
}

/* XXX Should we set MODE etc.? */
static int
hamcop_nand_resume(struct platform_device *pdev)
{
	struct hamcop_nand_data *nand = platform_get_drvdata(pdev);

	hamcop_nand_up(nand);
	//up (&nand->lock);

	return 0;
}
#endif

/* XXX Needs error recovery. */
static int 
hamcop_nand_probe(struct platform_device *pdev)
{
	struct hamcop_nand_data *nand;
	struct nand_chip *this;
	struct resource *res;
	const char *part_type;
	int n_mtd_parts = 0;

	nand = kmalloc(sizeof(*nand), GFP_KERNEL);
	if (!nand)
		return -ENOMEM;
	memset(nand, 0, sizeof (*nand));

	platform_set_drvdata(pdev, nand);

	nand->parent = pdev->dev.parent;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nand->map = ioremap(res->start, res->end - res->start + 1);

	//init_MUTEX(&nand->lock);

	/* Allocate memory for MTD device structure and private data */
	nand->mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip),
			    GFP_KERNEL);
	if (!nand->mtd) {
		kfree(nand);
		return -ENOMEM;
	}

	/* Get pointer to private data. */
	this = (struct nand_chip *)(&nand->mtd[1]);

	/* Initialize structures. */
	memset((char *)nand->mtd, 0, sizeof(struct mtd_info));
	memset((char *)this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure. */
	nand->mtd->priv = this;

	nand->mtd->owner = THIS_MODULE;

	/* Get the NAND clock. */
	nand->clk = clk_get(&pdev->dev, "nand");
	if (!nand->clk) {
		printk(KERN_ERR "failed to get nand clock\n");
		/* XXX error recovery */
		return -EBUSY;
	}

	/* Enable the NAND Flash controller. */
	hamcop_nand_up(nand);

	/* Set up callbacks. */
	this->priv	  = nand;
	this->IO_ADDR_R   = nand->map + _HAMCOP_NF_DATA;
	this->IO_ADDR_W   = nand->map + _HAMCOP_NF_DATA;
	this->cmd_ctrl	  = hamcop_nand_hwcontrol;
	this->dev_ready   = hamcop_nand_device_ready;
	this->read_buf    = hamcop_nand_read_buf;
	this->write_buf   = hamcop_nand_write_buf;
	this->chip_delay  = 50; 	/* 50 us command delay time */

	this->ecc.hwctl	    = hamcop_nand_enable_hwecc;
	this->ecc.correct   = hamcop_nand_correct_data;
	this->ecc.calculate = hamcop_nand_calculate_ecc;
	this->ecc.mode      = NAND_ECC_HW;
	this->ecc.size      = 512;
	this->ecc.bytes     = 3;
	this->ecc.layout    = &nand_hw_eccoob;

	/* Scan to find existence of the device. */
	if (nand_scan(nand->mtd, 1)) {
		printk(KERN_NOTICE "No NAND device - returning -ENXIO\n");
		kfree (nand->mtd);
		nand->mtd = NULL;
		return -ENXIO; /* XXX */
	}

	n_mtd_parts = parse_mtd_partitions(nand->mtd, probes,
					   &nand->mtd_partitions, 0);
	if (n_mtd_parts > 0)
		part_type = "command line";
	else
		n_mtd_parts = 0;

	if (n_mtd_parts == 0) {
		nand->mtd_partitions = h2200_partitions;
		n_mtd_parts = NUM_PARTITIONS;
		part_type = "builtin";
	}

	/* Register the partitions */
	printk(KERN_INFO "Using %s partition definition\n", part_type);
	add_mtd_partitions(nand->mtd, nand->mtd_partitions, n_mtd_parts);
	
	return 0;
}

static int
hamcop_nand_remove(struct platform_device *pdev)
{
	struct hamcop_nand_data *nand = platform_get_drvdata(pdev);

	hamcop_nand_down(nand);
	clk_put(nand->clk);

	nand_release(nand->mtd);

	/* Free partition info, if commandline partition was used */
        if (nand->mtd_partitions && (nand->mtd_partitions != h2200_partitions))
                kfree(nand->mtd_partitions);

	iounmap(nand->map);
	kfree(nand->mtd);
	kfree(nand);

	return 0;
}

static struct platform_driver hamcop_nand_driver = {
	.driver	  = {
		.name = "hamcop nand",
	},
	.probe    = hamcop_nand_probe,
	.remove   = hamcop_nand_remove,
#ifdef CONFIG_PM
	.suspend  = hamcop_nand_suspend,
	.resume   = hamcop_nand_resume,
#endif
};

static int
hamcop_nand_init(void)
{
	return platform_driver_register(&hamcop_nand_driver);
}

static void
hamcop_nand_cleanup(void)
{
	platform_driver_unregister(&hamcop_nand_driver);
}

module_init(hamcop_nand_init);
module_exit(hamcop_nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matt Reimer <mreimer at vpop dot net>");
MODULE_DESCRIPTION("NAND flash driver for Samsung HAMCOP ASIC (used in iPAQ h2200)");
