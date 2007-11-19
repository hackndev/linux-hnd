/*
 * System-on-Chip (SoC) driver for MediaQ 1100/1132 chips.
 *
 * Copyright (C) 2003 Andrew Zabolotny <anpaza@mail.ru>
 * Portions copyright (C) 2003 Keith Packard
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/soc-old.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mfd/mq11xx.h>
#include <asm/io.h>

#ifdef MQ_IRQ_MULTIPLEX
#  include <asm/irq.h>
#  include <asm/mach/irq.h>
#endif

#if 0
#  define debug(s, args...) printk (KERN_INFO s, ##args)
#else
#  define debug(s, args...)
#endif
#define debug_func(s, args...) debug ("%s: " s, __FUNCTION__, ##args)

/* Bitmasks for distinguishing resources for different sets of MediaQ chips */
#define MQ_MASK_1100		(1 << CHIP_MEDIAQ_1100)
#define MQ_MASK_1132		(1 << CHIP_MEDIAQ_1132)
#define MQ_MASK_1168		(1 << CHIP_MEDIAQ_1168)
#define MQ_MASK_1178		(1 << CHIP_MEDIAQ_1178)
#define MQ_MASK_1188		(1 << CHIP_MEDIAQ_1188)
#define MQ_MASK_ALL		(MQ_MASK_1100 | MQ_MASK_1132 | MQ_MASK_1168 | \
				 MQ_MASK_1178 | MQ_MASK_1188)

struct mq_block
{
	platform_device_id id;
	const char *name;
	unsigned start, end;
	unsigned mask;
};

#define MQ_SUBDEVS_COUNT	ARRAY_SIZE(mq_blocks)
#define MQ_SUBDEVS_REAL_COUNT	(MQ_SUBDEVS_COUNT - 2)
static const struct mq_block mq_blocks[] = {
	{
		/* Graphics Controller */
		{ MEDIAQ_11XX_FB_DEVICE_ID },
		"mq11xx_fb",
		0x100, 0x17f,
		MQ_MASK_ALL
	},
	{
		/* Graphics Engine -- tied to framebuffer subdevice */
		{ MEDIAQ_11XX_FB_DEVICE_ID },
		NULL,
		0x200, 0x27f,
		MQ_MASK_ALL
	},
	{
		/* Palette Registers -- tied to framebuffer subdevice */
		{ MEDIAQ_11XX_FB_DEVICE_ID },
		NULL,
		0x800, 0xbff,
		MQ_MASK_ALL
	},
	{
		/* Flat Panel control interface */
		{ MEDIAQ_11XX_FP_DEVICE_ID },
		"mq11xx_lcd",
		0x600, 0x7ff,
		MQ_MASK_ALL
	},
	{
		/* USB Function subdevice */
		{ MEDIAQ_11XX_UDC_DEVICE_ID },
		"mq11xx_udc",
		0x1000, 0x107f,
		MQ_MASK_ALL
	},
	{
		/* USB Host subdevice */
		{ MEDIAQ_11XX_UHC_DEVICE_ID },
		"mq11xx_uhc",
		0x500, 0x5ff,
		MQ_MASK_1132 | MQ_MASK_1188
	},
	{
		/* Serial Port Interface */
		{ MEDIAQ_11XX_SPI_DEVICE_ID },
		"mq11xx_spi",
		0x300, 0x37f,
		MQ_MASK_1132
	},
	{
		/* Synchronous Serial Channel (I2S) */
		{ MEDIAQ_11XX_I2S_DEVICE_ID },
		"mq11xx_i2s",
		0x280, 0x2ff,
		MQ_MASK_1132
	},
};

static struct {
	int id;
	int chip;
	const char *name;
} mq_device_id [] =
{
	{ MQ_PCI_DEVICEID_1100, CHIP_MEDIAQ_1100, "1100" },
	{ MQ_PCI_DEVICEID_1132, CHIP_MEDIAQ_1132, "1132" },
	{ MQ_PCI_DEVICEID_1168, CHIP_MEDIAQ_1168, "1168" },
	{ MQ_PCI_DEVICEID_1178, CHIP_MEDIAQ_1178, "1178" },
	{ MQ_PCI_DEVICEID_1188, CHIP_MEDIAQ_1188, "1188" }
};

/* Keep free block list no less than this value (minimal slab is 32 bytes) */
#define MEMBLOCK_MINCOUNT	(32 / sizeof (struct mq_freemem_list))
/* Align MediaQ memory blocks by this amount (MediaQ DMA limitation) */
#define MEMBLOCK_ALIGN		64

struct mq_freemem_list {
	u32 addr;
	u32 size;
} __attribute__((packed));

struct mq_data
{
	/* The exported parameters */
	struct mediaq11xx_base base;
	/* MediaQ initialization register data */
	struct mediaq11xx_init_data *mq_init;
	/* Number of subdevices */
	int ndevices;
	/* The list of subdevices */
	struct platform_device *devices;
	/* MediaQ interrupt number */
	int irq_nr;
	/* Count the number of poweron requests to every subdevice */
	u8 power_on [MQ_SUBDEVS_REAL_COUNT];
	/* Number of free block descriptors */
	int nfreeblocks, maxfreeblocks;
	/* The list of free blocks */
	struct mq_freemem_list *freelist;
	/* The lock on the free blocks list */
	spinlock_t mem_lock;
	/* The amount of RAM at the beginning of address space that
	   is not accessible through the unsynced window */
	int unsynced_ram_skip;
};

/* Pointer to initialized devices.
 * This is a bad practice, but if you don't need too much from the
 * MediaQ device (such as turning on/off a couple of GPIOs) you can
 * use the mq_get_device (int index) to get a pointer to one of
 * available MediaQ device structures.
 */
#define MAX_MQ11XX_DEVICES	8
static struct mediaq11xx_base *mq_device_list [MAX_MQ11XX_DEVICES];
static spinlock_t mq_device_list_lock = SPIN_LOCK_UNLOCKED;

#define to_mq_data(n) container_of(n, struct mq_data, base)

/* For PCI & DragonBall register mapping is as described in the specs sheet;
 * for other microprocessor buses some addresses are tossed (what a perversion).
 * Worse than that, on MQ1168 and later, the registers are always as normal.
 */
#if defined __arm__
// || defined NEC41xx || defined HITACHI 77xx
#define MQ_MASK_REGSET1		(MQ_MASK_1168 | MQ_MASK_1178 | MQ_MASK_1188)
#define MQ_MASK_REGSET2		(MQ_MASK_1100 | MQ_MASK_1132)
#else
#define MQ_MASK_REGSET1		MQ_MASK_ALL
#define MQ_MASK_REGSET2		0
#endif

static struct mediaq11xx_init_data mqInitValid = {
    .DC = {
	/* dc00 */		0,
	/* dc01 */		MQ_MASK_ALL,
	/* dc02 */		MQ_MASK_ALL,
	/* dc03 */		0,
	/* dc04 */		MQ_MASK_ALL,
	/* dc05 */		MQ_MASK_ALL,
    },
    .CC = {
	/* cc00 */		MQ_MASK_ALL,
	/* cc01 */		MQ_MASK_ALL,
	/* cc02 */		MQ_MASK_ALL,
	/* cc03 */		MQ_MASK_ALL,
	/* cc04 */		MQ_MASK_ALL,
    },
    .MIU = {
	/* mm00 */		MQ_MASK_ALL,
	/* mm01 */		MQ_MASK_ALL,
	/* mm02 */		MQ_MASK_ALL,
	/* mm03 */		MQ_MASK_ALL,
	/* mm04 */		MQ_MASK_ALL,
	/* mm05 */		MQ_MASK_1168 | MQ_MASK_1178 | MQ_MASK_1188,
	/* mm06 */		MQ_MASK_1168 | MQ_MASK_1178 | MQ_MASK_1188,
    },
    .GC = {
	/* gc00 */		MQ_MASK_ALL,
	/* gc01 */		MQ_MASK_ALL,
	/* gc02 */		MQ_MASK_ALL,
	/* gc03 */		MQ_MASK_ALL,
	/* gc04 */		MQ_MASK_ALL,
	/* gc05 */		MQ_MASK_ALL,
	/* gc06 NOT SET */	0,
	/* gc07 NOT SET */	0,
	/* gc08 */		MQ_MASK_ALL,
	/* gc09 */		MQ_MASK_ALL,
	/* gc0a */		MQ_MASK_ALL,
	/* gc0b */		MQ_MASK_ALL,
	/* gc0c */		MQ_MASK_ALL,
	/* gc0d */		MQ_MASK_ALL,
	/* gc0e */		MQ_MASK_ALL,
	/* gc0f NOT SET */	0,
	/* gc10 */		MQ_MASK_ALL,
	/* gc11 */		MQ_MASK_ALL,
	/* gc12 NOT SET */	0,
	/* gc13 NOT SET */	0,
	/* gc14 */		MQ_MASK_ALL,
	/* gc15 */		MQ_MASK_ALL,
	/* gc16 */		MQ_MASK_ALL,
	/* gc17 */		MQ_MASK_ALL,
	/* gc18 */		MQ_MASK_ALL,
	/* gc19 */		MQ_MASK_ALL,
	/* gc1a */		MQ_MASK_ALL,
    },
    /* FP */
    .FP = {
	/* fp00 */		MQ_MASK_ALL,
	/* fp01 */		MQ_MASK_ALL,
	/* fp02 */		MQ_MASK_ALL,
	/* fp03 */		MQ_MASK_ALL,
	/* fp04 */		MQ_MASK_ALL,
	/* fp05 */		MQ_MASK_ALL,
	/* fp06 */		MQ_MASK_ALL,
	/* fp07 */		MQ_MASK_ALL,
	/* fp08 */		MQ_MASK_ALL,
	/* fp09 NOT SET */	0,
	/* fp0a */		MQ_MASK_ALL,
	/* fp0b */		MQ_MASK_ALL,
	/* fp0c */		MQ_MASK_1168 | MQ_MASK_1178 | MQ_MASK_1188,
	/* fp0d NOT SET */	0,
	/* fp0e NOT SET */	0,
	/* fp0f */		MQ_MASK_ALL,
	/* fp10 */		MQ_MASK_ALL,
	/* fp11 */		MQ_MASK_ALL,
	/* fp12 */		MQ_MASK_ALL,
	/* fp13 */		MQ_MASK_ALL,
	/* fp14 */		MQ_MASK_ALL,
	/* fp15 */		MQ_MASK_ALL,
	/* fp16 */		MQ_MASK_ALL,
	/* fp17 */		MQ_MASK_ALL,
	/* fp18 */		MQ_MASK_ALL,
	/* fp19 */		MQ_MASK_ALL,
	/* fp1a */		MQ_MASK_ALL,
	/* fp1b */		MQ_MASK_ALL,
	/* fp1c */		MQ_MASK_ALL,
	/* fp1d */		MQ_MASK_ALL,
	/* fp1e */		MQ_MASK_ALL,
	/* fp1f */		MQ_MASK_ALL,
	/* fp20 */		MQ_MASK_REGSET1,
	/* fp21 */		MQ_MASK_REGSET1,
	/* fp22 */		MQ_MASK_REGSET1,
	/* fp23 */		MQ_MASK_REGSET1,
	/* fp24 */		MQ_MASK_REGSET1,
	/* fp25 */		MQ_MASK_REGSET1,
	/* fp26 */		MQ_MASK_REGSET1,
	/* fp27 */		MQ_MASK_REGSET1,
	/* fp28 */		MQ_MASK_REGSET1,
	/* fp29 */		MQ_MASK_REGSET1,
	/* fp2a */		MQ_MASK_REGSET1,
	/* fp2b */		MQ_MASK_REGSET1,
	/* fp2c */		MQ_MASK_REGSET1,
	/* fp2d */		MQ_MASK_REGSET1,
	/* fp2e */		MQ_MASK_REGSET1,
	/* fp2f */		MQ_MASK_REGSET1,
	/* fp30 */		MQ_MASK_ALL,
	/* fp31 */		MQ_MASK_ALL,
	/* fp32 */		MQ_MASK_ALL,
	/* fp33 */		MQ_MASK_ALL,
	/* fp34 */		MQ_MASK_ALL,
	/* fp35 */		MQ_MASK_ALL,
	/* fp36 */		MQ_MASK_ALL,
	/* fp37 */		MQ_MASK_ALL,
	/* fp38 */		MQ_MASK_ALL,
	/* fp39 */		MQ_MASK_ALL,
	/* fp3a */		MQ_MASK_ALL,
	/* fp3b */		MQ_MASK_ALL,
	/* fp3c */		MQ_MASK_ALL,
	/* fp3d */		MQ_MASK_ALL,
	/* fp3e */		MQ_MASK_ALL,
	/* fp3f */		MQ_MASK_ALL,
	/* fp40 */		0,
	/* fp41 */		0,
	/* fp42 */		0,
	/* fp43 */		0,
	/* fp44 */		0,
	/* fp45 */		0,
	/* fp46 */		0,
	/* fp47 */		0,
	/* fp48 */		0,
	/* fp49 */		0,
	/* fp4a */		0,
	/* fp4b */		0,
	/* fp4c */		0,
	/* fp4d */		0,
	/* fp4e */		0,
	/* fp4f */		0,
	/* fp50 */		0,
	/* fp51 */		0,
	/* fp52 */		0,
	/* fp53 */		0,
	/* fp54 */		0,
	/* fp55 */		0,
	/* fp56 */		0,
	/* fp57 */		0,
	/* fp58 */		0,
	/* fp59 */		0,
	/* fp5a */		0,
	/* fp5b */		0,
	/* fp5c */		0,
	/* fp5d */		0,
	/* fp5e */		0,
	/* fp5f */		0,
	/* fp60 */		0,
	/* fp61 */		0,
	/* fp62 */		0,
	/* fp63 */		0,
	/* fp64 */		0,
	/* fp65 */		0,
	/* fp66 */		0,
	/* fp67 */		0,
	/* fp68 */		0,
	/* fp69 */		0,
	/* fp6a */		0,
	/* fp6b */		0,
	/* fp6c */		0,
	/* fp6d */		0,
	/* fp6e */		0,
	/* fp6f */		0,
	/* fp70 */		MQ_MASK_REGSET2,
	/* fp71 */		MQ_MASK_REGSET2,
	/* fp72 */		MQ_MASK_REGSET2,
	/* fp73 */		MQ_MASK_REGSET2,
	/* fp74 */		MQ_MASK_REGSET2,
	/* fp75 */		MQ_MASK_REGSET2,
	/* fp76 */		MQ_MASK_REGSET2,
	/* fp77 */		MQ_MASK_REGSET2,
    },
    .GE = {
	/* ge00 NOT SET */	0,
	/* ge01 NOT SET */	0,
	/* ge02 NOT SET */	0,
	/* ge03 NOT SET */	0,
	/* ge04 NOT SET */	0,
	/* ge05 NOT SET */	0,
	/* ge06 NOT SET */	0,
	/* ge07 NOT SET */	0,
	/* ge08 NOT SET */	0,
	/* ge09 NOT SET */	0,
	/* ge0a */		MQ_MASK_ALL,
	/* ge0b */		MQ_MASK_ALL,
	/* ge0c NOT SET */      0x0,
	/* ge0d NOT SET */      0x0,
	/* ge0e NOT SET */      0x0,
	/* ge0f NOT SET */      0x0,
	/* ge10 NOT SET */      0x0,
	/* ge11 NOT SET */      MQ_MASK_1168 | MQ_MASK_1178 | MQ_MASK_1188,
	/* ge12 NOT SET */      0x0,
	/* ge13 NOT SET */      0x0,
    },
    .SPI = {
      /* sp00 */		MQ_MASK_1132,
      /* sp01 */		0,
      /* sp02 NOT SET */	0,
      /* sp03 NOT SET */	0,
      /* sp04 */		MQ_MASK_1132,
      /* sp05 NOT SET */	0,
      /* sp06 NOT SET */	0,
      /* sp07 */		MQ_MASK_1132,
      /* sp08 */		MQ_MASK_1132,
    },
};

static int
mq11xx_loadvals (char *name, int chipmask, volatile u32 *reg,
		 u32 *src, u32 *valid, int n)
{
	int t;

	for (t = 0; t < n; t++){
		if (valid[t] & chipmask) {
			int tries;
			for (tries = 0; tries < 10; tries++) {
				reg[t] = src[t];
				if (reg[t] == src[t])
					break;
				mdelay (1);
			}
			if (tries == 10) {
				debug ("mq11xx_loadvals %s%02x %08x FAILED (got %08x)\n", name, t, src[t], reg[t]);
				return -ENODEV;
			}
		}
	}
	return 0;
}

static int
mq11xx_init (struct mq_data *mqdata)
{
	int i, chipmask;
	u16 endian;

	if (mqdata->mq_init->set_power)
		mqdata->mq_init->set_power(1);

	/* Set up chip endianness - see mq docs for notes on special care
	   when writing to this register */
	endian = mqdata->mq_init->DC [0] & 3;
	endian = endian | (endian << 2);
	endian = endian | (endian << 4);
	endian = endian | (endian << 8);
	*((u16 *)&mqdata->base.regs->DC.config_0) = endian;
	/* First of all, enable the oscillator clock */
	mqdata->base.regs->DC.config_1 = mqdata->mq_init->DC [1];
	/* Wait for oscillator to run - 30ms doesnt suffice */
	mdelay (100);
	/* Enable power to CPU Configuration module */
	mqdata->base.regs->DC.config_2 = mqdata->mq_init->DC [2];
	mdelay (10);
	/* Needed for MediaQ 1178/88 on h2200. */
	mqdata->base.regs->DC.config_8 = 0x86098609;
	/* Enable the MQ1132 MIU */
	mqdata->base.regs->MIU.miu_0 |= MQ_MIU_ENABLE;
	/* Disable the interrupt controller */
	mqdata->base.regs->IC.control = 0;

	/* See if it's actually a MediaQ chip */
	if (mqdata->base.regs->PCI.vendor_id != MQ_PCI_VENDORID) {
unkchip:	printk (KERN_ERR "%s:%d Unknown device ID (%04x:%04x)\n",
			__FILE__, __LINE__,
			mqdata->base.regs->PCI.vendor_id,
			mqdata->base.regs->PCI.device_id);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE (mq_device_id); i++)
		if (mqdata->base.regs->PCI.device_id == mq_device_id [i].id) {
			mqdata->base.chip = mq_device_id [i].chip;
			mqdata->base.chipname = mq_device_id [i].name;
			break;
		}

	if (!mqdata->base.chipname)
		goto unkchip;

	chipmask = 1 << mqdata->base.chip;

#define INIT_REGS(id) \
	if (mq11xx_loadvals (#id, chipmask, mqdata->base.regs->id.a, \
		mqdata->mq_init->id, mqInitValid.id, \
		ARRAY_SIZE (mqdata->mq_init->id))) \
		return -ENODEV

	INIT_REGS (DC);
	INIT_REGS (CC);
	INIT_REGS (MIU);

	/* XXX Move this stuff to mq1100fb driver some day */
	INIT_REGS (GC);
	INIT_REGS (FP);
	INIT_REGS (GE);

#undef INIT_REGS

	return 0;
}

static void
mq_release (struct device *dev)
{
}

/* This is kind of ugly and complex... well, this is the way GPIOs are
 * programmed on MediaQ... what a mess...
 */
static int
mq_set_GPIO (struct mediaq11xx_base *zis, int num, int state)
{
	u32 andmask = 0, ormask = 0;

	debug ("mq_set_GPIO (num:%d state:%x)\n", num, state);

	if ((num <= 7) || ((num >= 20) && (num <= 25))) {
		andmask = 4;
		if (state & MQ_GPIO_CHMODE) {
			andmask |= 3;
			if (state & MQ_GPIO_IN)
				ormask |= 1;
			if (state & MQ_GPIO_OUT)
				ormask |= 2;
		}
		if (state & MQ_GPIO_0)
			andmask |= 8;
		if (state & MQ_GPIO_1)
			ormask |= 8;
		if (num <= 7) {
			andmask <<= num * 4;
			ormask <<= num * 4;
			zis->regs->CC.gpio_control_0 =
				(zis->regs->CC.gpio_control_0 & ~andmask) | ormask;
		} else {
			andmask <<= (num - 18) * 4;
			ormask <<= (num - 18) * 4;
			zis->regs->CC.gpio_control_1 =
				(zis->regs->CC.gpio_control_1 & ~andmask) | ormask;
		}
	} else if (num >= 50 && num <= 55) {
		int shft;

		/* Uhh.... what a mess */
		if (state & MQ_GPIO_CHMODE) {
			shft = (num <= 53) ? num - 48 : num - 45;
			andmask |= 3 << shft;
			if (state & MQ_GPIO_OUT)
				ormask |= (1 << (shft * 2));
			if (state & MQ_GPIO_IN)
				ormask |= (2 << (shft * 2));
		}

		if (num == 50)
			shft = 3;
		else if (num <= 53)
			shft = (num - 51);
		else
			shft = (num - 38);

		if (state & MQ_GPIO_0)
			andmask |= (1 << shft);
		if (state & MQ_GPIO_1)
			ormask |= (1 << shft);
		if ((state & MQ_GPIO_PULLUP) && (num <= 53))
			ormask |= 0x1000 << (num - 50);
		zis->regs->SPI.gpio_mode =
			(zis->regs->SPI.gpio_mode & ~andmask) | ormask;
	} else if (num >= 60 && num <= 66) {
		int shft = (num - 60);
		if (state & MQ_GPIO_0)
			andmask |= (1 << shft);
		if (state & MQ_GPIO_1)
			ormask |= (1 << shft);
		shft *= 2;
		if (state & MQ_GPIO_CHMODE) {
			andmask |= (0x300 << shft);
			if (state & MQ_GPIO_OUT)
				ormask |= (0x100 << shft);
			if (state & MQ_GPIO_IN)
				ormask |= (0x200 << shft);
		}
		zis->regs->SPI.blue_gpio_mode =
			(zis->regs->SPI.blue_gpio_mode & ~andmask) | ormask;
	} else
		return -ENODEV;

	return 0;
}

static int
mq_get_GPIO (struct mediaq11xx_base *zis, int num)
{
	u32 val;

	if (num <= 7)
		val = zis->regs->CC.gpio_control_0 & (8 << (num * 4));
	else if ((num >= 20) && (num <= 25))
		val = zis->regs->CC.gpio_control_1 & (8 << ((num - 18) * 4));
	else if (num == 50)
		val = zis->regs->SPI.gpio_mode & 0x8;
	else if ((num >= 51) && (num <= 53))
		val = zis->regs->SPI.gpio_mode & (0x1 << (num - 51));
	else if ((num >= 54) && (num <= 55))
		val = zis->regs->SPI.gpio_mode & (0x1 << (num - 38));
	else if (num >= 60 && num <= 66)
		val = zis->regs->SPI.blue_gpio_mode & (1 << (num - 60));
	else
		val = 0;

	if (val) val = 1;

	return val;
}

static int
global_power (struct mq_data *mqdata)
{
	int i;
	for (i = 0; i < sizeof (mqdata->power_on); i++)
		if (mqdata->power_on [i])
			return 1;
	return 0;
}

static void
mq_power_on (struct mediaq11xx_base *zis, int subdev_id)
{
	/* The device IDs lower 4 bits contains the subdevice index
	 * (counting from 1). Take care to keep all MEDIAQ_11XX_XXX
	 * contstants with sequential values in their lower 4 bits!
	 */
	unsigned idx = (subdev_id & 0x0f) - 1;
	struct mq_data *mqdata = to_mq_data (zis);

	debug_func ("subdev:%x curstate:%d\n", subdev_id, mqdata->power_on [idx]);

	/* Check if MediaQ is not totally turned off */
	if (!global_power (mqdata)) {
		debug ("-*- Global POWER ON to the MediaQ chip -*-\n");
		mq11xx_init (mqdata);
	}

	if (!mqdata->power_on [idx])
		switch (subdev_id) {
		case MEDIAQ_11XX_FB_DEVICE_ID:
			zis->regs->GC.control |= MQ_GC_CONTROL_ENABLE;
			/* Enable the graphics engine power */
			zis->regs->DC.config_5 |= MQ_CONFIG_GE_ENABLE;
			break;
		case MEDIAQ_11XX_FP_DEVICE_ID:
			/* Enable flat panel pin outputs */
			//zis->regs->FP.pin_control_1 &= ~MQ_FP_DISABLE_FLAT_PANEL_PINS;
			zis->regs->FP.output_control = mqdata->mq_init->FP [2];
			break;
		case MEDIAQ_11XX_UDC_DEVICE_ID:
			/* Enable power to USB function */
			zis->regs->DC.config_5 |= MQ_CONFIG_UDC_CLOCK_ENABLE;
			break;
		case MEDIAQ_11XX_UHC_DEVICE_ID:
			/* Enable power to USB host */
			zis->regs->DC.config_5 |= MQ_CONFIG_UHC_CLOCK_ENABLE;
			break;
		case MEDIAQ_11XX_SPI_DEVICE_ID:
		case MEDIAQ_11XX_I2S_DEVICE_ID:
			/* There's no explicit way to do it */
			break;
		}
	mqdata->power_on [idx]++;
}

static void
mq_power_off (struct mediaq11xx_base *zis, int subdev_id)
{
	unsigned idx = (subdev_id & 0x0f) - 1;
	struct mq_data *mqdata = to_mq_data (zis);

	debug_func ("subdev:%x curstate:%d\n", subdev_id, mqdata->power_on [idx]);

	if (!mqdata->power_on [idx]) {
		printk (KERN_ERR "mq11xx: mismatch power on/off request count for subdevice %x\n",
			subdev_id);
		return;
	}

	switch (subdev_id) {
	case MEDIAQ_11XX_FB_DEVICE_ID:
		zis->regs->GC.control &= ~MQ_GC_CONTROL_ENABLE;
		/* Disable the graphics engine power */
		zis->regs->DC.config_5 &= ~MQ_CONFIG_GE_ENABLE;
		break;
	case MEDIAQ_11XX_FP_DEVICE_ID:
		/* Disable flat panel pin outputs */
		zis->regs->FP.output_control = 0;
		//zis->regs->FP.pin_control_1 |= MQ_FP_DISABLE_FLAT_PANEL_PINS;
		break;
	case MEDIAQ_11XX_UDC_DEVICE_ID:
		/* Disable power to USB function */
		zis->regs->DC.config_5 &= ~MQ_CONFIG_UDC_CLOCK_ENABLE;
		break;
	case MEDIAQ_11XX_UHC_DEVICE_ID:
		/* Disable power to USB host */
		zis->regs->DC.config_5 &= ~MQ_CONFIG_UHC_CLOCK_ENABLE;
		break;
	case MEDIAQ_11XX_SPI_DEVICE_ID:
	case MEDIAQ_11XX_I2S_DEVICE_ID:
		/* There's no explicit way to do it */
		break;
	}

	mqdata->power_on [idx]--;

	/* Check if we have to totally power off MediaQ */
	if (!global_power (mqdata)) {
		debug ("-*- Global POWER OFF to MediaQ chip -*-\n");
		/* Disable the MQ1132 MIU */
		zis->regs->MIU.miu_0 &= ~MQ_MIU_ENABLE;
		/* Disable power to CPU Configuration module */
		zis->regs->DC.config_2 &= ~MQ_CONFIG_CC_MODULE_ENABLE;
		/* Disable the oscillator clock */
		zis->regs->DC.config_1 &= ~MQ_CONFIG_18_OSCILLATOR;

		if (mqdata->mq_init->set_power)
			mqdata->mq_init->set_power(0);
	}
}

static void
mq_set_power (struct mediaq11xx_base *zis, int subdev_id, int state)
{
	if (state)
		mq_power_on (zis, subdev_id);
	else
		mq_power_off (zis, subdev_id);
}

static int
mq_get_power (struct mediaq11xx_base *zis, int subdev_id)
{
	unsigned idx = (subdev_id & 0x0f) - 1;
	struct mq_data *mqdata = to_mq_data (zis);
	return mqdata->power_on [idx];
}

/********************************************* On-chip memory management ******/

/**
 * Since the internal chip RAM may be shared amongst several subdevices
 * (framebuffer, graphics engine, SPI, UDC and other), we have to implement
 * a general memory management mechanism to prevent conflicts between drivers
 * trying to use this resource.
 *
 * We could use the "chained free blocks" scheme used in many simple memory
 * managers, where a { free block size; next free block; } structure is
 * stuffed directly into the free block itself, to conserve memory. However,
 * this mechanism is less reliable since it is susceptible to overwrites
 * past the bounds of an allocated block (because it breaks the free blocks
 * chain). Of course, a buggy driver is always a bad idea, but we should try
 * to be as reliable as possible. Thus, the free block chain is kept in kernel
 * memory (in the device-specific structure), and everything else is done like
 * in the mentioned scheme.
 */

static void
mq_setfreeblocks (struct mq_data *mqdata, int nblocks)
{
	void *temp;
	int newmax;

	/* Increase maximum number of free memory descriptors in power-of-two
	   steps. This is due to the fact, that Linux allocates memory in
	   power-of-two chunks, and the minimal chunk (slab) size is 32b.
	   Allocating anything but power-of-two sized memory blocks is just
	   a waste of memory. In fact, in most circumstances we have a small
	   number of free block descriptors. */
	newmax = (nblocks > 0) ? (1 << fls (nblocks - 1)) : 1;
	if (likely (newmax < MEMBLOCK_MINCOUNT))
		newmax = MEMBLOCK_MINCOUNT;

	if (mqdata->maxfreeblocks != newmax) {
		int nfb = mqdata->nfreeblocks;
		if (nfb > nblocks) nfb = nblocks;
		temp = kmalloc (newmax * sizeof (struct mq_freemem_list),
				GFP_KERNEL);
		memcpy (temp, mqdata->freelist, nfb * sizeof (struct mq_freemem_list));
		kfree (mqdata->freelist);
		mqdata->freelist = temp;
		mqdata->maxfreeblocks = newmax;
	}

	mqdata->nfreeblocks = nblocks;
}

/* Check if memory block is suitable for uses other than graphics. */
static int mq_suitable_ram (struct mq_data *mqdata, int fbn, unsigned size)
{
	int n, delta;

	delta = mqdata->unsynced_ram_skip - mqdata->freelist [fbn].addr;
	if (delta <= 0)
		return 1;

	if (mqdata->freelist [fbn].size < size + delta)
		return 0;

	/* Ok, split the free block into two */
	n = mqdata->nfreeblocks;
	mq_setfreeblocks (mqdata, n + 1);
	mqdata->freelist [n].addr = mqdata->freelist [fbn].addr;
	mqdata->freelist [n].size = delta;
	mqdata->freelist [fbn].addr += delta;
	mqdata->freelist [fbn].size -= delta;

	return 1;
}

/* Not too optimal (just finds the first free block of equal or larger size
 * than requested) but usually there are a few blocks in MediaQ RAM anyway...
 * usually a block of 320x240x2 bytes is eaten by the framebuffer, and we
 * have just 256k total.
 */
static u32
mq_alloc (struct mediaq11xx_base *zis, unsigned size, int gfx)
{
	struct mq_data *mqdata = to_mq_data (zis);
	int i;

	size = (size + MEMBLOCK_ALIGN - 1) & ~(MEMBLOCK_ALIGN - 1);

	spin_lock (&mqdata->mem_lock);

	for (i = mqdata->nfreeblocks - 1; i >= 0; i--) {
		if ((mqdata->freelist [i].size >= size) &&
		    (gfx || mq_suitable_ram (mqdata, i, size))) {
			u32 addr = mqdata->freelist [i].addr;
			mqdata->freelist [i].size -= size;
			if (mqdata->freelist [i].size)
				mqdata->freelist [i].addr += size;
			else {
				memcpy (mqdata->freelist + i, mqdata->freelist + i + 1,
					(mqdata->nfreeblocks - 1 - i) * sizeof (struct mq_freemem_list));
				mq_setfreeblocks (mqdata, mqdata->nfreeblocks - 1);
			}
			spin_unlock (&mqdata->mem_lock);
			return addr;
		}
	}

	spin_unlock (&mqdata->mem_lock);

	return (u32)-1;
}

static void
mq_free (struct mediaq11xx_base *zis, u32 addr, unsigned size)
{
	int i, j;
	u32 eaddr;
	struct mq_data *mqdata = to_mq_data (zis);

	size = (size + MEMBLOCK_ALIGN - 1) & ~(MEMBLOCK_ALIGN - 1);
	eaddr = addr + size;

	spin_lock (&mqdata->mem_lock);

	/* Look for a free block that starts at the end of the block to free */
	for (i = mqdata->nfreeblocks - 1; i >= 0; i--)
		if (mqdata->freelist [i].addr == eaddr) {
			mqdata->freelist [i].size += size;
			mqdata->freelist [i].addr = addr;
			/* Now look for a block that ends where we start */
			for (j = mqdata->nfreeblocks - 1; j >= 0; j--) {
				if (mqdata->freelist [j].addr + mqdata->freelist [j].size == addr) {
					/* Ok, concatenate the two free blocks */
					mqdata->freelist [i].addr = mqdata->freelist [j].addr;
					mqdata->freelist [i].size += mqdata->freelist [j].size;
					memcpy (mqdata->freelist + j, mqdata->freelist + j + 1,
						(mqdata->nfreeblocks - 1 - j) * sizeof (struct mq_freemem_list));
					mq_setfreeblocks (mqdata, mqdata->nfreeblocks - 1);
					break;
				}
			}
			spin_unlock (&mqdata->mem_lock);
			return;
		}

	for (i = mqdata->nfreeblocks - 1; i >= 0; i--)
		if (mqdata->freelist [i].addr + mqdata->freelist [i].size == addr) {
			mqdata->freelist [i].size += size;
			spin_unlock (&mqdata->mem_lock);
			return;
		}

	/* Ok, we have to add a new free block entry */
	i = mqdata->nfreeblocks;
	mq_setfreeblocks (mqdata, i + 1);
	mqdata->freelist [i].addr = addr;
	mqdata->freelist [i].size = size;

	spin_unlock (&mqdata->mem_lock);
}

/**************************** IRQ handling using interrupt multiplexing ******/

#ifdef MQ_IRQ_MULTIPLEX

static void
mq_irq_demux (unsigned int irq, struct irq_desc *desc)
{
	u32 mask, orig_mask;
	int i, mq_irq, timeout;
	struct mq_data *mqdata;

	mqdata = desc->handler_data;

	mqdata->base.regs->IC.control &= ~MQ_INTERRUPT_CONTROL_INTERRUPT_ENABLE;

	debug ("mq_irq_demux ENTER\n");

	/* An IRQ is a expensive operation, thus we're handling here
	 * as much interrupt sources as we can (rather than returning
	 * from interrupt just to get caught once again). On the other
	 * hand, we can't process too much of them since some IRQ may be
	 * continuously triggered. Thus, we'll set an arbitrary upper limit
	 * on the number of interrupts we can handle at once. Note that,
	 * for example, if PCMCIA IRQ is connected to MediaQ, this number
	 * can be very large, e.g. one interrupt for every read sector
	 * from a memory card.
	 */
	timeout = 1024;

	/* Read the IRQ status register */
	while (timeout-- &&
	       (mask = orig_mask = mqdata->base.regs->IC.interrupt_status)) {
		irq = 0;
		while ((i = ffs (mask))) {
			irq += i - 1;
			mq_irq = mqdata->base.irq_base + irq;
			desc = irq_desc + mq_irq;
			debug ("mq_irq_demux (irq:%d)\n", mq_irq);
			desc->handle_irq(mq_irq, desc);
			mask >>= i;
		}
	}

	if (!timeout)
		printk (KERN_ERR "mq11xx: IRQ continuously triggered, mask %08x\n", mask);

	debug ("mq_irq_demux LEAVE\n");

	mqdata->base.regs->IC.control |= MQ_INTERRUPT_CONTROL_INTERRUPT_ENABLE;
}

/* Acknowledge, clear _AND_ disable the interrupt. */
static void
mq_irq_ack (unsigned int irq)
{
	struct mq_data *mqdata = get_irq_chip_data(irq);
	u32 mask = 1 << (irq - mqdata->base.irq_base);
	/* Disable the IRQ */
	mqdata->base.regs->IC.interrupt_mask &= ~mask;
	/* Acknowledge the IRQ */
	mqdata->base.regs->IC.interrupt_status = mask;
}

static void
mq_irq_mask (unsigned int irq)
{
	struct mq_data *mqdata = get_irq_chip_data(irq);
	u32 mask = 1 << (irq - mqdata->base.irq_base);
	mqdata->base.regs->IC.interrupt_mask &= ~mask;
}

static void
mq_irq_unmask (unsigned int irq)
{
	struct mq_data *mqdata = get_irq_chip_data(irq);
	u32 mask = 1 << (irq - mqdata->base.irq_base);
	mqdata->base.regs->IC.interrupt_mask |= mask;
}

static int
mq_irq_type (unsigned int irq, unsigned int type)
{
	struct mq_data *mqdata = get_irq_chip_data(irq);
	int mask;

	if ((irq < mqdata->base.irq_base + IRQ_MQ_GPIO_0) ||
	    (irq > mqdata->base.irq_base + IRQ_MQ_GPIO_2))
		return 0;

	mask = MQ_INTERRUPT_CONTROL_GPIO_0_INTERRUPT_POLARITY <<
		(irq - mqdata->base.irq_base - IRQ_MQ_GPIO_0);

	if (type & (__IRQT_HIGHLVL | __IRQT_RISEDGE))
		mqdata->base.regs->IC.control |= mask;
	else if (type & (__IRQT_LOWLVL | __IRQT_FALEDGE))
		mqdata->base.regs->IC.control &= ~mask;

	return 0;
}

static struct irq_chip mq_irq_chip = {
	.ack		= mq_irq_ack,
	.mask		= mq_irq_mask,
	.unmask		= mq_irq_unmask,
	.set_type	= mq_irq_type,
};

static int mq_irq_init(struct mq_data *mqdata, 
	struct mediaq11xx_init_data *init_data)
{
	int i;

	/* Disable all IRQs */
	mqdata->base.regs->IC.control = 0;
	mqdata->base.regs->IC.interrupt_mask = 0;
	/* Clear IRQ status */
	mqdata->base.regs->IC.interrupt_status = 0xffffffff;

	mqdata->base.irq_base = init_data->irq_base;
	if (!mqdata->base.irq_base) {
		printk(KERN_ERR "mq11xx: uninitialized irq_base!\n");
		return -ENOMEM;
	}

	printk("mq11xx: using irq %d-%d on irq %d\n", mqdata->base.irq_base,
		mqdata->base.irq_base + MQ11xx_NUMIRQS - 1, mqdata->irq_nr);

	for (i = 0; i < MQ11xx_NUMIRQS; i++) {
		int irq = mqdata->base.irq_base + i;
		set_irq_flags (irq, IRQF_VALID);
		set_irq_chip (irq, &mq_irq_chip);
		set_irq_handler(irq, handle_level_irq);
		set_irq_chip_data(irq, mqdata);
	}

	i = (mqdata->base.regs->IC.control & MQ_INTERRUPT_CONTROL_INTERRUPT_POLARITY);
	set_irq_chained_handler (mqdata->irq_nr, mq_irq_demux);
	set_irq_type (mqdata->irq_nr, i ? IRQT_RISING : IRQT_FALLING);
	set_irq_data (mqdata->irq_nr, mqdata);

	/* Globally enable IRQs from MediaQ */
	mqdata->base.regs->IC.control |= MQ_INTERRUPT_CONTROL_INTERRUPT_ENABLE;

	return 0;
}

static void
mq_irq_free (struct mq_data *mqdata)
{
	/* Disable all IRQs */
	mqdata->base.regs->IC.control = 0;
	mqdata->base.regs->IC.interrupt_mask = 0;
	/* Clear IRQ status */
	mqdata->base.regs->IC.interrupt_status = 0xffffffff;

	set_irq_handler(mqdata->irq_nr, handle_edge_irq);
}
#endif

int
mq_device_enum (struct mediaq11xx_base **list, int list_size)
{
	int i, j;

	if (!list_size)
		return 0;

	spin_lock (&mq_device_list_lock);
	for (i = j = 0; i < MAX_MQ11XX_DEVICES; i++)
		if (mq_device_list [i]) {
			list [j++] = mq_device_list [i];
			if (j >= list_size)
				break;
		}
	spin_unlock (&mq_device_list_lock);

	return j;
}
EXPORT_SYMBOL (mq_device_enum);

int
mq_driver_get (void)
{
	return try_module_get (THIS_MODULE) ? 0 : -ENXIO;
}
EXPORT_SYMBOL (mq_driver_get);

void
mq_driver_put (void)
{
	module_put (THIS_MODULE);
}
EXPORT_SYMBOL (mq_driver_put);

/*
 * Resources specified in resource table for this driver are:
 * 0: Synchronous RAM address (physical base + 0 on ARM arch)
 * 1: Asynchronous RAM address (physical base + 256K+2K on ARM arch)
 * 2: Registers address (physical base + 256K on ARM arch)
 * 3: The MediaQ IRQ number
 *
 * Also the platform_data field of the device should contain a pointer to
 * a mq11xx_platform_data structure, which contains the platform-specific
 * initialization data for MediaQ chip, the name of framebuffer driver
 * and the name of LCD driver.
 */
static int
mq_initialize (struct device *dev, int num_resources,
	       struct resource *resource, int instance)
{
	int i, j, k, rc, chipmask;
	struct mq_data *mqdata;
	struct mediaq11xx_init_data *init_data =
		(struct mediaq11xx_init_data *)dev->platform_data;

	if (!init_data || num_resources != 4) {
		printk (KERN_ERR "mq11xx_base: Incorrect platform resources!\n");
		return -ENODEV;
	}

	mqdata = kmalloc (sizeof (struct mq_data), GFP_KERNEL);
	if (!mqdata)
		return -ENOMEM;
	memset (mqdata, 0, sizeof (struct mq_data));
	dev_set_drvdata (dev, mqdata);

#define IOREMAP(v, n, el) \
	mqdata->base.v = ioremap (resource[n].start, \
		resource[n].end - resource[n].start + 1); \
	if (!mqdata->base.v) goto el; \
	mqdata->base.paddr_##v = resource[n].start;

	IOREMAP (gfxram, 0, err0);
	IOREMAP (ram, 1, err1);
	IOREMAP (regs, 2, err2);

#undef IOREMAP

	/* Check how much RAM is accessible through the unsynced window */
	mqdata->unsynced_ram_skip =
		(resource [0].end - resource [0].start) -
		(resource [1].end - resource [1].start);
	mqdata->base.ram -= mqdata->unsynced_ram_skip;
	mqdata->base.paddr_ram -= mqdata->unsynced_ram_skip;

	mqdata->ndevices = MQ_SUBDEVS_REAL_COUNT;
	mqdata->devices = kmalloc (mqdata->ndevices *
		(sizeof (struct platform_device)), GFP_KERNEL);
	if (!mqdata->devices)
		goto err3;

	mqdata->mq_init = init_data;
	if (mq11xx_init (mqdata)) {
		printk (KERN_ERR "MediaQ device initialization failed!\n");
		return -ENODEV;
	}

	mqdata->irq_nr = resource[3].start;
	mqdata->base.set_GPIO = mq_set_GPIO;
	mqdata->base.get_GPIO = mq_get_GPIO;
	mqdata->base.set_power = mq_set_power;
	mqdata->base.get_power = mq_get_power;
	mqdata->base.alloc = mq_alloc;
	mqdata->base.free = mq_free;

	/* Initialize memory manager */
	spin_lock_init (&mqdata->mem_lock);
	mqdata->nfreeblocks = 1;
	mqdata->maxfreeblocks = MEMBLOCK_MINCOUNT;
	mqdata->freelist = kmalloc (MEMBLOCK_MINCOUNT * sizeof (struct mq_freemem_list),
				    GFP_KERNEL);
	mqdata->freelist [0].addr = 0;
	mqdata->freelist [0].size = MQ11xx_FB_SIZE;

#if defined MQ_IRQ_MULTIPLEX
	if ((mqdata->irq_nr != -1) && mq_irq_init(mqdata, init_data))
		goto err4;
#endif

	for (i = j = 0; j < MQ_SUBDEVS_COUNT; i++) {
		struct platform_device *sdev = &mqdata->devices[i];
		const struct mq_block *blk = &mq_blocks[j];
		struct resource *res;
		int old_j, k;

		memset (sdev, 0, sizeof (struct platform_device));
		sdev->id = blk->id.id; /* placeholder id */
		sdev->name = blk->name;
		sdev->dev.parent = dev;
		sdev->dev.release = mq_release;
		sdev->dev.platform_data = &mqdata->base;

		/* Count number of resources */
		sdev->num_resources = 2;
		old_j = j;
		while (mq_blocks [++j].id.id == sdev->id)
			sdev->num_resources += 2;

		res = kmalloc (sdev->num_resources * sizeof (struct resource), GFP_KERNEL);
		sdev->resource = res;
		memset (res, 0, sdev->num_resources * sizeof (struct resource));

		for (j = old_j, k = 0; mq_blocks [j].id.id == sdev->id; j++) {
			blk = &mq_blocks[j];
			res[k].start = resource[2].start + blk->start;
			res[k].end = resource[2].start + blk->end;
			res[k].parent = &resource[2];
			res[k++].flags = IORESOURCE_MEM;
			res[k].start = (unsigned)mqdata->base.regs + blk->start;
			res[k].end = (unsigned)mqdata->base.regs + blk->end;
			res[k++].flags = 0;
		}

		mqdata->power_on [i] = 1;;
	}

	/* Second pass */
	chipmask = 1 << mqdata->base.chip;
	for (i = j = k = 0; j < MQ_SUBDEVS_COUNT; i++) {
		struct platform_device *sdev = &mqdata->devices[i];
		const struct mq_block *blk = &mq_blocks[j];

		while (mq_blocks [++j].id.id == sdev->id)
			;

		/* Power off all subdevices */
		mq_power_off (&mqdata->base, sdev->id);

		if (!(blk->mask & chipmask))
			continue;

		sdev->id = instance;
		rc = platform_device_register (sdev);
		if (rc) {
			printk (KERN_ERR "Failed to register MediaQ subdevice `%s', code %d\n",
				blk->name, rc);
			goto err5;
		}
	}

	printk (KERN_INFO "MediaQ %s chip detected, ", mqdata->base.chipname);
	if (mqdata->base.irq_base)
		printk ("base IRQ %d\n", mqdata->base.irq_base);
	else
		printk ("\n");

	spin_lock (&mq_device_list_lock);
	for (i = 0; i < MAX_MQ11XX_DEVICES; i++)
		if (!mq_device_list [i]) {
			mq_device_list [i] = &mqdata->base;
			break;
		}
	spin_unlock (&mq_device_list_lock);

	return 0;

err5:	while (--i >= 0) {
		platform_device_unregister (&mqdata->devices[i]);
		kfree (mqdata->devices[i].resource);
	}

#if defined MQ_IRQ_MULTIPLEX
	if (mqdata->irq_nr != -1)
		mq_irq_free (mqdata);
#endif
err4:	kfree (mqdata->devices);
err3:	iounmap ((void *)mqdata->base.regs);
err2:	iounmap ((void *)mqdata->base.ram + mqdata->unsynced_ram_skip);
err1:	iounmap (mqdata->base.gfxram);
err0:	kfree (mqdata);

	printk (KERN_ERR "MediaQ base SoC driver initialization failed\n");

	return -ENOMEM;
}

static int
mq_finalize (struct device *dev)
{
	int i;
	struct mq_data *mqdata = dev_get_drvdata (dev);

	spin_lock (&mq_device_list_lock);
	for (i = 0; i < MAX_MQ11XX_DEVICES; i++)
		if (mq_device_list [i] == &mqdata->base) {
			mq_device_list [i] = NULL;
			break;
		}
	spin_unlock (&mq_device_list_lock);

	for (i = 0; i < mqdata->ndevices; i++) {
		platform_device_unregister (&mqdata->devices[i]);
		kfree (mqdata->devices[i].resource);
	}

#if defined MQ_IRQ_MULTIPLEX
	if (mqdata->irq_nr != -1)
		mq_irq_free (mqdata);
#endif

	kfree (mqdata->devices);

	iounmap ((void *)mqdata->base.regs);
	iounmap ((void *)mqdata->base.ram + mqdata->unsynced_ram_skip);
	iounmap (mqdata->base.gfxram);

	kfree (mqdata);

	return 0;
}

/* This is the platform device handler. If MediaQ is connected to a different
 * bus type (e.g. PCI) a similar device_driver structure should be registered
 * for that bus. For now this is not implemented.
 */

static int
mq_probe (struct platform_device *pdev)
{
	return mq_initialize (&pdev->dev, pdev->num_resources, pdev->resource,
			      pdev->id);
}

static int
mq_remove (struct platform_device *pdev)
{
	return mq_finalize (&pdev->dev);
}

static void
mq_shutdown (struct platform_device *pdev)
{
	//struct mq_data *mqdata = dev_get_drvdata (dev);
}

static int
mq_suspend (struct platform_device *pdev, pm_message_t state)
{
	//struct mq_data *mqdata = dev_get_drvdata (dev);
	return 0;
}

static int
mq_resume (struct platform_device *pdev)
{
	//struct mq_data *mqdata = dev_get_drvdata (dev);
	return 0;
}

static struct platform_driver mq_device_driver = {
	.driver = {
		.name		= "mq11xx",
	},
	.probe		= mq_probe,
	.remove		= mq_remove,
	.suspend	= mq_suspend,
	.resume		= mq_resume,
	.shutdown	= mq_shutdown,
};

static int __init
mq_base_init (void)
{
	return platform_driver_register (&mq_device_driver);
}

static void __exit
mq_base_exit (void)
{
	platform_driver_unregister (&mq_device_driver);
}

#ifdef MODULE
module_init (mq_base_init);
#else   /* start early for dependencies */
subsys_initcall (mq_base_init);
#endif
module_exit (mq_base_exit)

MODULE_AUTHOR("Andrew Zabolotny <anpaza@mail.ru>");
MODULE_DESCRIPTION("MediaQ 1100/1132 base SoC support");
MODULE_LICENSE("GPL");
