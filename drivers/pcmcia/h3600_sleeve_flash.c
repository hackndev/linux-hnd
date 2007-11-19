/*
 * drivers/pcmcia/h3600_generic
 *
 * PCMCIA implementation routines for H3600 iPAQ standards sleeves
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/i2c.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/pcmcia.h>
#include <asm/arch/h3600-sleeve.h>
#include <asm/arch/linkup-l1110.h>
#include <asm/arch/backpaq.h>




struct mtd_partition pcmcia_sleeve_flash_partitions[] = {
	{
		name: "PCMCIA Sleeve Flash",
		offset: 0x00000000,
		size: 0  /* will expand to the end of the flash */
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

#define WINDOW_ADDR 0xf1000000 /* static memory bank 1 */

static int offset_multiplier = 1; /* 2 for rev-A backpaqs -- don't ask -- Jamey 2/12/2001 */

static __u8 sa1100_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(WINDOW_ADDR + ofs*offset_multiplier);
}

static __u16 sa1100_read16(struct map_info *map, unsigned long ofs)
{
        __u16 d = *(__u16 *)(WINDOW_ADDR + ofs*offset_multiplier);
        if (debug >= 1) printk(__FUNCTION__ " ofs=%08lx (adr=%p) => d=%#x\n", ofs, (__u16 *)(WINDOW_ADDR + ofs*offset_multiplier), d);
	return d;
}

static __u32 sa1100_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(WINDOW_ADDR + ofs*offset_multiplier);
}

static void sa1100_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
        int i;
        if (0) printk(__FUNCTION__ " from=%08lx offset_multiplier=%d\n", WINDOW_ADDR + from, offset_multiplier);
        if (offset_multiplier == 1) {
                memcpy(to, (void *)(WINDOW_ADDR + from), len);
        } else {
                for (i = 0; i < len; i += 2) {
                        *(short *)(to++) = *(short *)(WINDOW_ADDR + from*offset_multiplier + i*offset_multiplier);
                }
        }
}

static void sa1100_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(WINDOW_ADDR + adr*offset_multiplier) = d;
}

static void sa1100_write16(struct map_info *map, __u16 d, unsigned long adr)
{
        if (debug >= 1) printk(__FUNCTION__ " adr=%08lx d=%x\n", WINDOW_ADDR + adr*offset_multiplier, d);
	*(__u16 *)(WINDOW_ADDR + adr*offset_multiplier) = d;
}

static void sa1100_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(WINDOW_ADDR + adr*offset_multiplier) = d;
}

static void sa1100_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
        int i;
        if (0) printk(__FUNCTION__ " to=%08lx offset_multiplier=%d\n", WINDOW_ADDR + to, offset_multiplier);
        if (offset_multiplier == 1) {
                memcpy((void *)(WINDOW_ADDR + to), from, len);
        } else {
                for (i = 0; i < len; i += 2) {
                        *(short *)(WINDOW_ADDR + to*offset_multiplier + i*offset_multiplier) = *(short *)(from + i);
                }
        }
}

static void pcmcia_set_vpp(struct map_info *map, int vpp)
{
	/* ??? Is this correct? Why not set flash program mode? */
	assign_h3600_egpio( IPAQ_EGPIO_OPT_RESET, vpp );
}

static struct map_info pcmcia_sleeve_flash_map = {
	name:		"PCMCIA Sleeve Flash ",
	read8:		sa1100_read8,
	read16:		sa1100_read16,
	read32:		sa1100_read32,
	copy_from:	sa1100_copy_from,
	write8:		sa1100_write8,
	write16:	sa1100_write16,
	write32:	sa1100_write32,
	copy_to:	sa1100_copy_to,
        size:           0x02000000,
        set_vpp:        NULL,
        buswidth:       2
};

static struct mtd_info *pcmcia_sleeve_mtd = NULL;

int pcmcia_sleeve_attach_flash(void (*set_vpp)(struct map_info *, int), unsigned long map_size)
{
	unsigned long msc1_sm1_config = (MSC_NonBrst | MSC_16BitStMem 
					 | MSC_RdAcc((150 + 100)/4)
					 | MSC_NxtRdAcc((150 + 100)/4)
					 | MSC_Rec(100/4));
        void *(*cfi_probe)(void *) = inter_module_get("cfi_probe");
	if ( !cfi_probe )
		return -ENXIO;

	msc1_sm1_config = 0xFFFC;
	MSC1 = ((MSC1 & 0xFFFF) /* sm0 */
		| (msc1_sm1_config << 16));

	printk(__FUNCTION__ ": setting MSC1=0x%x\n", MSC1);

	pcmcia_sleeve_flash_map.set_vpp = set_vpp;
	pcmcia_sleeve_flash_map.size    = map_size;

	pcmcia_sleeve_mtd = cfi_probe(&pcmcia_sleeve_flash_map);
	if ( pcmcia_sleeve_mtd ) {
		pcmcia_sleeve_mtd->module = THIS_MODULE;
		add_mtd_device(pcmcia_sleeve_mtd);
		return 0;
	}

	printk(" *** " __FUNCTION__ ": unable to add flash device\n");
	return -ENXIO;
}

void pcmcia_sleeve_detach_flash(void)
{
	printk(" ### " __FUNCTION__ "\n");
        if (pcmcia_sleeve_mtd != NULL) {
		printk(" ### " __FUNCTION__ " 2\n");
                del_mtd_device(pcmcia_sleeve_mtd);
		printk(" #### " __FUNCTION__ " 3\n");
                map_destroy(pcmcia_sleeve_mtd);
		printk(" ### " __FUNCTION__ " 4\n"); 
		pcmcia_sleeve_mtd = NULL;
        }
}


int old_pcmcia_sleeve_attach_flash(void (*set_vpp)(struct map_info *, int), unsigned long map_size)
{
        void *(*cfi_probe)(void *) = inter_module_get("cfi_probe");

	printk(" ### " __FUNCTION__ "\n");

        if (cfi_probe != NULL) {
                /* 
                 * assuming 150ns Flash, 30ns extra to cross the expansion ASIC and control logic 
                 * and 206 MHz CPU clock (4ns ns cycle time, up to 250MHz)
                 */
                unsigned long msc1_sm1_config = (MSC_NonBrst | MSC_16BitStMem 
                                                 | MSC_RdAcc((150 + 100)/4)
                                                 | MSC_NxtRdAcc((150 + 100)/4)
                                                 | MSC_Rec(100/4));
                msc1_sm1_config = 0xFFFC;
                MSC1 = ((MSC1 & 0xFFFF) /* sm0 */
                        | (msc1_sm1_config << 16));

                printk(__FUNCTION__ ": setting MSC1=0x%x\n", MSC1);

                pcmcia_sleeve_flash_map.set_vpp = set_vpp;
                pcmcia_sleeve_flash_map.size = map_size;

                pcmcia_sleeve_mtd = cfi_probe(&pcmcia_sleeve_flash_map);
                if (pcmcia_sleeve_mtd) {
                        pcmcia_sleeve_mtd->module = THIS_MODULE;
                        if ( add_mtd_partitions(pcmcia_sleeve_mtd, pcmcia_sleeve_flash_partitions, 
						NB_OF(pcmcia_sleeve_flash_partitions)))
				printk(" *** " __FUNCTION__ ": unable to add flash partitions\n");
                        printk(KERN_NOTICE "PCMCIA flash access initialized\n");
                        return 0;
                }
                return -ENXIO;
        } else {
                return -EINVAL;
        }

}

void old_pcmcia_sleeve_detach_flash(void)
{
	printk(" ### " __FUNCTION__ "\n");
        if (pcmcia_sleeve_mtd != NULL) {
		printk(" ### " __FUNCTION__ " 2\n");
                del_mtd_partitions(pcmcia_sleeve_mtd);
		printk(" #### " __FUNCTION__ " 3\n");
                map_destroy(pcmcia_sleeve_mtd);
		printk(" ### " __FUNCTION__ " 4\n"); 
        }
}
EXPORT_SYMBOL(pcmcia_sleeve_attach_flash);
EXPORT_SYMBOL(pcmcia_sleeve_detach_flash);

