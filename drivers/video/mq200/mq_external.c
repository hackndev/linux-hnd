/*
 * Copyright (C) 2005 Holger Hans Peter Freyther
 *
 * Based ON:
 *
 * linux/drivers/video/mq200fb.c -- MQ-200 for a frame buffer device
 * based on linux/driver/video/pm2fb.c
 *
 * Copyright (C) 2000 Lineo, Japan
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <asm/types.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "mq200_data.h"


#if 1
#define PRINTK(args...) printk(args)
#else
#define PRINTK(args...)
#endif


/****
 * power state transition to "state".
 */
static void
power_state_transition(unsigned long register_base, int state)
{
    int i;
    writel(state, PMCSR(register_base));
    for (i = 1; ; i++) {
       udelay(100);
       if ((readl(PMCSR(register_base)) & 0x3) == state) {
	   break;
       }
    }
}


/****
 * device configuration initialization.
 */
static void
dc_reset(unsigned long register_base)
{
    union dc00r dc00r;

    /* Reset First */
    dc00r.whole       = DC_RESET;
    writel(dc00r.whole, DC00R(register_base));
    udelay(10);


    dc00r.whole = 0xEF2082A;
    writel(dc00r.whole, DC00R(register_base));
    udelay(5);
    PRINTK(CHIPNAME ": DC00R = %xx\n", readl(DC00R(register_base)));
}


/****
 * initialize memory interface unit.
 */
static void
miu_reset(unsigned long register_base)
{
    union mm00r mm00r;
    union mm01r mm01r;
    union mm02r mm02r;
    union mm03r mm03r;
    union mm04r mm04r;

    /* MIU interface control 1 */
    mm00r.whole = 0x4;
    writel(mm00r.whole, MM00R(register_base));
    udelay(5);
    writel(0, MM00R(register_base));
    udelay(50);

    /* MIU interface control 2
     * o PLL 1 output is used as memory clock source.
     */
    mm01r.whole = 0x4143e086;
    writel(mm01r.whole, MM01R(register_base));

    /* memory interface control 3 */
    mm02r.whole = 0x6d6aabff;
    writel(mm02r.whole, MM02R(register_base));

    /* memory interface control 5 */
    mm04r.whole = 0x10d;
    writel(mm04r.whole, MM04R(register_base));

    /* memory interface control 4 */
    mm03r.whole = 0x1;
    writel(mm03r.whole, MM03R(register_base));
    mdelay(10);

    /* MIU interface control 1 */
    mm00r.whole = 0x3;
    writel(mm00r.whole, MM00R(register_base));
    mdelay(50);
}

/****
 *
 */
static
void fpctrl_reset(unsigned long addr)
{
   /*
     * We're in D0 State, let us set the FPCTRL
     */
    union fp00r fp00r;
    union fp01r fp01r;
    union fp02r fp02r;
    union fp03r fp03r;
    union fp04r fp04r;
    union fp0fr fp0fr;

    fp00r.whole = 0x6320;
    writel(fp00r.whole, FP00R(addr));

    fp01r.whole = 0x20;
    writel(fp01r.whole, FP01R(addr));

    fp04r.whole = 0xBD0001;
    writel(fp04r.whole, FP04R(addr));

    /* Set Flat Panel General Purpose register first */
    fp02r.whole = 0x0;
    writel(fp02r.whole, FP02R(addr));

    fp03r.whole = 0x0;
    writel(fp03r.whole, FP03R(addr));

    fp0fr.whole = 0xA16c44;
    writel(fp0fr.whole, FP0FR(addr));


    /* Set them again */
    fp02r.whole = 0x0;
    writel(fp02r.whole, FP02R(addr));

    fp03r.whole = 0x0;
    writel(fp03r.whole, FP03R(addr));
}


/****
 * initialize power management unit.
 */
static void
pmu_reset(unsigned long register_base)
{
    union pm00r pm00r;
    union pm01r pm01r;
    union pm02r pm02r;
//    union pm06r pm06r;
//    union pm07r pm07r;

    /* power management miscellaneous control
     * o GE is driven by PLL 1 clock.
     */
    pm00r.whole = 0xc0900;
    writel(pm00r.whole, PM00R(register_base));

    /* D1 state control */
    pm01r.whole = 0x5000271;
    writel(pm01r.whole, PM01R(register_base));

    /* D2 state control */
    pm02r.whole = 0x271;
    writel(pm02r.whole, PM02R(register_base));

#if 0
    /* PLL 2 programming */
    pm06r.whole = 0xE90830;
    writel(pm06r.whole, PM06R(register_base));

    /* PLL 3 programming */
    pm07r.whole = 0xE90830;
    writel(pm07r.whole, PM07R(register_base));
#endif
}

/****
 * initialize graphics controller 1.
 */
static void
gc1_reset(unsigned long register_base, spinlock_t *lock )
{
    unsigned long flags;
    union gc00r gc00r;
    union gc01r gc01r;
    union gc02r gc02r;
    union gc03r gc03r;
    union gc04r gc04r;
    union gc05r gc05r;
    union gc08r gc08r;
    union gc09r gc09r;
//    union gc0er gc0er;
//    union gc11r gc11r;
    union pm00r pm00r;
    union pm06r pm06r;

    spin_lock_irqsave(lock, flags);

    /* graphics controller CRT control */
    gc01r.whole = 0x800;
    writel(gc01r.whole, GC01R(register_base));

    /* horizontal display 1 control */
    gc02r.whole = 0x320041e;
    writel(gc02r.whole, GC02R(register_base));

    /* vertical display 1 control */
    gc03r.whole = 0x2570273;
    writel(gc03r.whole, GC03R(register_base));

    /* horizontal sync 1 control */
    gc04r.whole = 0x3c70347;
    writel(gc04r.whole, GC04R(register_base));

    /* vertical sync 1 control */
    gc05r.whole = 0x25d0259;
    writel(gc05r.whole, GC05R(register_base));

    /* horizontal window 1 control */
    gc08r.whole = 0x131f0000;
    writel(gc08r.whole, GC08R(register_base));

    /* vertical window 1 control */
    gc09r.whole = 0x2570000;
    writel(gc09r.whole, GC09R(register_base));

#if 0
    /* alternate horizontal window 1 control */
    writel(0, GC0AR(register_base));

    /* alternate vertical window 1 control */
    writel(0, GC0BR(register_base));

    /* window 1 start address */
    writel(0x2004100, GC0CR(register_base));

    /* alternate window 1 start address */
    writel(0, GC0DR(register_base));

    /* window 1 stride */
    gc0er.whole = 0x5100048;
    writel(gc0er.whole, GC0ER(register_base));

    /* reserved register - ??? - */
    writel(0x31f, GC0FR(register_base));
#endif

#if 0
    /* hardware cursor 1 position */
    writel(0, GC10R(register_base));

    /* hardware cursor 1 start address and offset */
    gc11r.whole = 0x5100048;
    writel(gc11r.whole, GC11R(register_base));

    /* hardware cursor 1 foreground color */
    writel(0x00ffffff, GC12R(register_base));

    /* hardware cursor 1 background color */
    writel(0x00000000, GC13R(register_base));
#endif

    /* PLL 2 programming */
    pm06r.whole = 0xE90830;
    writel(pm06r.whole, PM06R(register_base));


    /* graphics controller 1 register
     * o GC1 clock source is PLL 2.
     * o hardware cursor is disabled.
     */
    gc00r.whole = 0x10200C8;
    writel(gc00r.whole, GC00R(register_base));

    /*
     * Enable PLL2 in the PM Register
     */
    pm00r.whole = readl(PM00R(register_base));
    pm00r.part.pll2_enbl = 0x1;
    writel(pm00r.whole, PM00R(register_base));

    spin_unlock_irqrestore(lock, flags);
}


/****
 * initialize graphics engine.
 */
static void
ge_reset(unsigned long register_base)
{
    /* drawing command register */
    writel(0, GE00R(register_base));

    /* promary width and height register */
    writel(0, GE01R(register_base));

    /* primary destination address register */
    writel(0, GE02R(register_base));

    /* primary source XY register */
    writel(0, GE03R(register_base));

    /* primary color compare register */
    writel(0, GE04R(register_base));

    /* primary clip left/top register */
    writel(0, GE05R(register_base));

    /* primary clip right/bottom register */
    writel(0, GE06R(register_base));

    /* primary source and pattern offset register */
    writel(0, GE07R(register_base));

    /* primary foreground color register/rectangle fill color depth */
    writel(0, GE08R(register_base));

    /* source stride/offset register */
    writel(0, GE09R(register_base));

    /* destination stride register and color depth */
    writel(0, GE0AR(register_base));

    /* image base address register */
    writel(0, GE0BR(register_base));
}


/****
 * initialize Color Palette 1.
 */
static void
cp1_reset(unsigned long addr_info)
{
    int i;

    for (i = 0; i < 256; i++)
       writel(0, C1xxR(addr_info, i));
}




/*
 * Below functions are called from the skeleton
 */
void mq200_external_setpal(unsigned regno, unsigned long color, unsigned long addr)
{
    writel(color,C1xxR(addr,regno));
}

void mq200_external_setqmode(struct mq200_monitor_info* info,
			     unsigned long addr, spinlock_t *lock)
{
    dc_reset(addr);     /* device configuration */

    power_state_transition(addr, 0);       /* transition to D0 state */
    pmu_reset(addr);    /* power management unit */
    miu_reset(addr);    /* memory interface unit */
    ge_reset(addr);     /* graphics engine */
    fpctrl_reset(addr); /* reset the panel settings */
    gc1_reset(addr, lock); /* graphics controller 1 */
    cp1_reset(addr);    /* color palette 1 */
    mq200_external_ondisplay(addr);
}

void mq200_external_offdisplay(unsigned long addr)
{
    /*
     * Move the MQ200 to D3 mode
     */
    power_state_transition(addr, 3);
}

/**
 * to be called after mq200_external_setqmode
 */
void mq200_external_ondisplay (unsigned long addr)
{
    /*
     * Set the framebuffer details
     */
    #warning FIX HERE AS WELL
    union gc00r gc00r;
    union fp00r fp00r;
    gc00r.whole = readl(GC00R(addr));
    fp00r.whole = readl(FP00R(addr));

    if(!(gc00r.whole & 0x1)) {
	gc00r.whole |= 1;
	writel(gc00r.whole, GC00R(addr));
    }

    fp00r.whole |= 0x01;
    writel(fp00r.whole, FP00R(addr));
}

int mq200_external_probe(unsigned long addr)
{
    union pc00r pc00r;
    if(readl(PMR(addr)) != PMR_VALUE)
       return 0;

    pc00r.whole = readl(PC00R(addr));
    printk(KERN_INFO "mq200 video driver found Vendor:%d Device:%d\n",
	   pc00r.part.device, pc00r.part.vendor);
    return 1;
}
