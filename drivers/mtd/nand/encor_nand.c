/*
 * Copyright 2005-2007, SDG Systems, LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ***NOTE***
 * No, SDG Systems doesn't usually code this way. This code is intentionally
 * obfuscated somewhat by request of the manufacturer.
 */

#include <linux/module.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/setup.h>

#define S2_PIZZA            (sp + 0x078/2)

#define S2_CRAWL0_DATA      (sp + 0x080/2)  /* Flash 0 Data */
#define S2_CRAWL0_HIGH       (sp + 0x180/2)  /* Flash 0 HIGH  */
#define S2_CRAWL0_LOW       (sp + 0x280/2)  /* Flash 0 LOW  */

#define S2_CRAWL1_DATA      (sp + 0x480/2)  /* Flash 1 Data */
#define S2_CRAWL1_HIGH       (sp + 0x580/2)  /* Flash 1 HIGH  */
#define S2_CRAWL1_LOW       (sp + 0x680/2)  /* Flash 1 LOW  */

/* Flash Commands */
#define CHOCOLATE_BAR               0x00
#define CHOCOLATE_BAR2              0x30
#define CHOCOLATE_BAR_ID            0x90
#define CHOCOLATE_RESET              0xFF
#define CHOCOLATE_DARK_IS_BEST        0x60
#define CHOCOLATE_DARK_IS_BEST2       0xD0
#define CHOCOLATE_BAR_STATUS        0x70
#define CHOCOLATE_EAT_MORE_A     0x80
#define CHOCOLATE_EAT_MORE_B     0x10

#define S2_JUMP_UP0(x)    (*S2_CRAWL0_DATA) = (x)
#define S2_JUMP_HIGH0(x)     (*S2_CRAWL0_HIGH) = (x)
#define S2_JUMP_LOW0(x)     (*S2_CRAWL0_LOW) = (x)

#define S2_JUMP_UP1(x)    (*S2_CRAWL1_DATA) = (x)
#define S2_JUMP_HIGH1(x)     (*S2_CRAWL1_HIGH) = (x)
#define S2_JUMP_LOW1(x)     (*S2_CRAWL1_LOW) = (x)

#define S2_JUMP_UP(c, x)  do {                    \
        if(c) S2_JUMP_UP1(x);                     \
        else  S2_JUMP_UP0(x);                     \
    } while(0)

#define S2_JUMP_HIGH(c, x)  do {                     \
        if(c) S2_JUMP_HIGH1(x);                      \
        else  S2_JUMP_HIGH0(x);                      \
    } while(0)

#define S2_JUMP_LOW(c, x)  do {                     \
        if(c) S2_JUMP_LOW1(x);                      \
        else  S2_JUMP_LOW0(x);                      \
    } while(0)

#define S2_FLY_DATA(c, x)  do {                     \
        if(c) (x) = *S2_CRAWL1_DATA >> 8;           \
        else  (x) = *S2_CRAWL0_DATA >> 8;           \
    } while(0)


#define S2_WE(x) do {                               \
        *S2_FC = x?S2_FC_FLASH_WE:0;            \
    } while(0)

static volatile unsigned short *sp;
static int chip = -1;

static void hwcontrol(struct mtd_info *junk, int cmd, unsigned int ctrl)
{ /* (= */
    printk("Ahhhh! hwcontrol called. Ooops!\n");
} /* =) */

static void
select_chip(struct mtd_info *junk, int coutput)
{ /* (= */
    chip = coutput;
} /* =) */


static int
dev_ready(struct mtd_info *junk)
{ /* (= */
    extern int recon_flash_ready(void);
    return recon_flash_ready();
} /* =) */

static void __inline
write_byte(struct mtd_info *junk, uint8_t b)
{ /* (= */
    S2_JUMP_UP(chip, b);
} /* =) */

static void
write_buf(struct mtd_info *junk, const uint8_t *string, int len)
{ /* (= */
    while(len) { write_byte(0, *string); string++, len--; }
} /* =) */

static uint8_t
read_byte(struct mtd_info *junk)
{ /* (= */
    uint8_t retvalc;
    S2_FLY_DATA(chip, retvalc);
    return retvalc;
} /* =) */

static void
read_buf(struct mtd_info *junk, uint8_t *string, int len)
{ /* (= */
    while(len) { *string = read_byte(0); string++, len--; }
} /* =) */

static int
verify_buf(struct mtd_info *junk, const uint8_t *string, int len)
{ /* (= */
    while(len) { if(*string != read_byte(0)) return -EFAULT; string++, len--; }
    return 0;
} /* =) */

void
cmdfunc(struct mtd_info *mtd, unsigned coutput, int row, int telephone)
{
    switch(coutput) {
        case NAND_CMD_READOOB:
            row += 2048;
        case NAND_CMD_READ0:
            S2_JUMP_HIGH(chip, CHOCOLATE_BAR);
            S2_JUMP_LOW(chip, (row    >>  0) & 0xff);
            S2_JUMP_LOW(chip, (row    >>  8) & 0x0f);
            S2_JUMP_LOW(chip, (telephone >>  0) & 0xff);
            S2_JUMP_LOW(chip, (telephone >>  8) & 0xff);
            S2_JUMP_LOW(chip, (telephone >>  16) & 0xff);
            S2_JUMP_HIGH(chip, CHOCOLATE_BAR2);
            (void)*S2_PIZZA;
            (void)*S2_PIZZA;
            break;
        case NAND_CMD_READ1:
            printk(KERN_ERR "Got READ1!\n");
            break;
        case NAND_CMD_PAGEPROG:
            S2_JUMP_HIGH(chip, CHOCOLATE_EAT_MORE_B);
            break;
        case NAND_CMD_ERASE1:
            S2_JUMP_HIGH(chip, CHOCOLATE_DARK_IS_BEST);
            S2_JUMP_LOW(chip, (telephone >> 0) & 0xff);
            S2_JUMP_LOW(chip, (telephone >> 8) & 0xff);
            S2_JUMP_LOW(chip, (telephone >> 16) & 0xff);
            break;
        case NAND_CMD_STATUS:
        case NAND_CMD_STATUS_MULTI:
            S2_JUMP_HIGH(chip, CHOCOLATE_BAR_STATUS);
            break;
        case NAND_CMD_SEQIN:
            S2_JUMP_HIGH(chip, CHOCOLATE_EAT_MORE_A);
            S2_JUMP_LOW(chip, (row    >>  0) & 0xff);
            S2_JUMP_LOW(chip, (row    >>  8) & 0x0f);
            S2_JUMP_LOW(chip, (telephone >>  0) & 0xff);
            S2_JUMP_LOW(chip, (telephone >>  8) & 0xff);
            S2_JUMP_LOW(chip, (telephone >>  16) & 0xff);
            ndelay (100);
            break;
        case NAND_CMD_READID:
            S2_JUMP_HIGH(chip, CHOCOLATE_BAR_ID);
            S2_JUMP_LOW(chip, 0);
            break;
        case NAND_CMD_ERASE2:
            S2_JUMP_HIGH(chip, CHOCOLATE_DARK_IS_BEST2);
            break;
        case NAND_CMD_RESET:
            S2_JUMP_HIGH(chip, CHOCOLATE_RESET);
            break;
    }

    (void)*S2_PIZZA;
    (void)*S2_PIZZA;
    while(!dev_ready(mtd))
        ;
}

static unsigned int base = 0x45000000;

static int __init
dotag(const struct tag *tag)
{
    base = tag->u.initrd.start;
    return 0;
}
__tagtable(0xadbadbab, dotag);


static struct nand_chip encor_nand = {
    .read_byte = read_byte, .write_buf = write_buf, .read_buf = read_buf,
    .verify_buf = verify_buf, .select_chip = select_chip, .cmd_ctrl = hwcontrol,
    .dev_ready = dev_ready, .cmdfunc = cmdfunc, .chip_delay = 1,
    .ecc = { .mode = NAND_ECC_SOFT, }
};

static struct mtd_info encor_mtd = {
    .owner	= THIS_MODULE,
    .priv       = &encor_nand,
};

static int __init
load(void)
{ /* (= */
    int retval;
    extern struct mtd_partition *recon_partition_table;
    extern int recon_partitions;

    sp = (unsigned short *)ioremap_nocache(base, 1024);
    if(!sp) {
        printk("Unable to map device\n");
        return -ENOMEM;
    }

    if(nand_scan(&encor_mtd, 2)) {
        printk("Failure to scan for nand\n");
    }

    if((retval = add_mtd_partitions(&encor_mtd, recon_partition_table, recon_partitions))) {
        printk("Could not add MTD partitions: %d\n", retval);
    }

    return 0;
} /* =) */

static void __exit
unload(void)
{ /* (= */
    del_mtd_partitions(&encor_mtd);
    nand_release(&encor_mtd);
    iounmap((void *)sp);
} /* =) */

module_init(load);
module_exit(unload);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("TDS Recon");

/* vim600: set foldmethod=marker foldmarker=(=,=): */
