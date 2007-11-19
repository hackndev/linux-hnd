/*
 *  linux/arch/arm/mach-pxa/cpu-pxa.c
 *
 *  Copyright (C) 2002,2003 Intrinsyc Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 *   31-Jul-2002 : Initial version [FB]
 *   29-Jan-2003 : added PXA255 support [FB]
 *   20-Apr-2003 : ported to v2.5 (Dustin McIntire, Sensoria Corp.)
 *   11-Jan-2006 : v2.6, support for PXA27x processor up to 624MHz (Bill Reese, Hewlett Packard)
 *
 * Note:
 *   This driver may change the memory bus clock rate, but will not do any
 *   platform specific access timing changes... for example if you have flash
 *   memory connected to CS0, you will need to register a platform specific
 *   notifier which will adjust the memory access strobes to maintain a
 *   minimum strobe width.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpufreq.h>

#include <asm/hardware.h>

#include <asm/arch/pxa-regs.h>

/*
 * This comes from generic.h in this directory.
 */
extern unsigned int get_clk_frequency_khz(int info);

#define DEBUG  0

#ifdef DEBUG
  static unsigned int freq_debug = DEBUG;
  module_param(freq_debug, int, 0644);
  MODULE_PARM_DESC(freq_debug, "Set the debug messages to on=1/off=0");
#else
  #define freq_debug  0
#endif

typedef struct
{
  unsigned int khz;     /* CPU frequency                                   */
  unsigned int membus;  /* memory bus frequency                            */
  unsigned int cccr;    /* new CCLKCFG setting                             */
  unsigned int div2;    /* alter memory controller settings to divide by 2 */
  unsigned int cclkcfg; /* new CCLKCFG setting                             */
} pxa_freqs_t;

/* Define the refresh period in mSec for the SDRAM and the number of rows */
#define SDRAM_TREF          64      /* standard 64ms SDRAM */
#if defined(CONFIG_MACH_H4700) || defined(CONFIG_ARCH_H2200) || defined(CONFIG_MACH_MAGICIAN)
#define SDRAM_ROWS          8192    /* hx4700 uses 2 64Mb DRAMs, 8912 rows */
#else
#define SDRAM_ROWS          4096    /* 64MB=8192 32MB=4096 */
#endif
#define MDREFR_DRI(x)       (((x*SDRAM_TREF/SDRAM_ROWS - 31)/32))

#define CCLKCFG_TURBO       0x1
#define CCLKCFG_FCS         0x2
#define CCLKCFG_HALFTURBO   0x4
#define CCLKCFG_FASTBUS     0x8
#ifdef CONFIG_ARCH_H5400
/* H5400's SAMCOP chip does not like when K2DB2 touched */
#define MDREFR_DB2_MASK     (MDREFR_K1DB2)
#else
#define MDREFR_DB2_MASK     (MDREFR_K2DB2 | MDREFR_K1DB2)
#endif
#define MDREFR_DRI_MASK     0xFFF
#define PXA25x_CCLKCFG      CCLKCFG_TURBO | CCLKCFG_FCS

/*
 * For the PXA27x:
 * Control variables are A, L, 2N for CCCR; B, HT, T for CLKCFG.
 *
 * A = 0 => memory controller clock from table 3-7,
 * A = 1 => memory controller clock = system bus clock
 * Run mode frequency   = 13 MHz * L
 * Turbo mode frequency = 13 MHz * L * N
 * System bus frequency = 13 MHz * L / (B + 1)
 * System initialized by bootldr to:
 *
 * In CCCR:
 * A = 1
 * L = 16         oscillator to run mode ratio
 * 2N = 6         2 * (turbo mode to run mode ratio)
 *
 * In CCLKCFG:
 * B = 1          Fast bus mode
 * HT = 0         Half-Turbo mode
 * T = 1          Turbo mode
 *
 * For now, just support some of the combinations in table 3-7 of
 * PXA27x Processor Family Developer's Manual to simplify frequency
 * change sequences.
 *
 * Specify 2N in the PXA27x_CCCR macro, not N!
 */
#define PXA27x_CCCR(A, L, N2) (A << 25 | N2 << 7 | L)
#define PXA27x_CCLKCFG(B, HT, T) (B << 3 |  HT << 2 | CCLKCFG_FCS | T)

#define PXA25x_CCCR(L, M, N) (L << 0 | M << 5 | N << 7)

/*
 * Valid frequency assignments
 */
static pxa_freqs_t pxa2xx_freqs[] =
{
    /* CPU   MEMBUS  CCCR                  DIV2 */
#if defined(CONFIG_PXA25x)
#if defined(CONFIG_MACH_T3XSCALE)
    {133000, 99500,  0x123, 1, PXA25x_CCLKCFG}, // 133 * 1 * 1
    {199000, 99500,  0x1a3, 0, PXA25x_CCLKCFG}, // 133 * 1 * 1.5
    {266000, 99500,  0x143, 0, PXA25x_CCLKCFG}, // 133 * 2 * 1
    {399000, 99500,  0x1c3, 0, PXA25x_CCLKCFG}, // 133 * 2 * 1.5
  //{441000, 99500,  0x1c4, 0, PXA25x_CCLKCFG}, // 147 * 2 * 1.5 FIXME: doesn't work?
  //{472000, 99500,  0x242, 0, PXA25x_CCLKCFG}, // 118 * 2 * 2 FIXME: is that stabe enough?  
#else
#if defined(CONFIG_PXA25x_ALTERNATE_FREQS)
    { 99500, 99500,  PXA25x_CCCR(1, 1, 2), 1, PXA25x_CCLKCFG}, /* run=99,  turbo= 99, PXbus=50, SDRAM=50 */
    {199100, 99500,  PXA25x_CCCR(1, 1, 4), 0, PXA25x_CCLKCFG}, /* run=99,  turbo=199, PXbus=50, SDRAM=99 */
    {298500, 99500,  PXA25x_CCCR(1, 1, 6), 0, PXA25x_CCLKCFG}, /* run=99,  turbo=287, PXbus=50, SDRAM=99 */
    {298600, 99500,  PXA25x_CCCR(1, 2, 3), 0, PXA25x_CCLKCFG}, /* run=199, turbo=287, PXbus=99, SDRAM=99 */
    {398100, 99500,  PXA25x_CCCR(1, 2, 4), 0, PXA25x_CCLKCFG}  /* run=199, turbo=398, PXbus=99, SDRAM=99 */
#else
    { 99500,  99500, PXA25x_CCCR(1, 1, 2), 1, PXA25x_CCLKCFG}, /* run= 99, turbo= 99, PXbus=50,  SDRAM=50 */
    {132700, 132700, PXA25x_CCCR(3, 1, 2), 1, PXA25x_CCLKCFG}, /* run=133, turbo=133, PXbus=66,  SDRAM=66 */
    {199100,  99500, PXA25x_CCCR(1, 2, 2), 0, PXA25x_CCLKCFG}, /* run=199, turbo=199, PXbus=99,  SDRAM=99 */
    {265400, 132700, PXA25x_CCCR(3, 2, 2), 1, PXA25x_CCLKCFG}, /* run=265, turbo=265, PXbus=133, SDRAM=66 */
    {331800, 165900, PXA25x_CCCR(5, 2, 2), 1, PXA25x_CCLKCFG}, /* run=331, turbo=331, PXbus=166, SDRAM=83 */
    {398100,  99500, PXA25x_CCCR(1, 3, 2), 0, PXA25x_CCLKCFG}  /* run=398, turbo=398, PXbus=196, SDRAM=99 */
#endif
#endif
#elif defined(CONFIG_PXA27x)
    {104000, 104000, PXA27x_CCCR(1,  8, 2), 0, PXA27x_CCLKCFG(1, 0, 1)},
    {156000, 104000, PXA27x_CCCR(1,  8, 6), 0, PXA27x_CCLKCFG(1, 1, 1)},
    {208000, 208000, PXA27x_CCCR(0, 16, 2), 1, PXA27x_CCLKCFG(0, 0, 1)},
    {312000, 208000, PXA27x_CCCR(1, 16, 3), 1, PXA27x_CCLKCFG(1, 0, 1)},
    {416000, 208000, PXA27x_CCCR(1, 16, 4), 1, PXA27x_CCLKCFG(1, 0, 1)},
    {520000, 208000, PXA27x_CCCR(1, 16, 5), 1, PXA27x_CCLKCFG(1, 0, 1)},
    {624000, 208000, PXA27x_CCCR(1, 16, 6), 1, PXA27x_CCLKCFG(1, 0, 1)}
#endif
};
#define NUM_FREQS (sizeof(pxa2xx_freqs)/sizeof(pxa_freqs_t))

static struct cpufreq_frequency_table pxa2xx_freq_table[NUM_FREQS+1];

/* Return the memory clock rate for a given cpu frequency. */
int pxa_cpufreq_memclk(int cpu_khz)
{
	int i;
	int freq_mem = 0;

	for (i = 0; i < NUM_FREQS; i++) {
		if (pxa2xx_freqs[i].khz == cpu_khz) {
			freq_mem = pxa2xx_freqs[i].membus;
			break;
		}
	}

	return freq_mem;
}
EXPORT_SYMBOL(pxa_cpufreq_memclk);


/* find a valid frequency point */
static int pxa_verify_policy(struct cpufreq_policy *policy)
{
    int ret;

    ret=cpufreq_frequency_table_verify(policy, pxa2xx_freq_table);

    if(freq_debug) {
        printk("Verified CPU policy: %dKhz min to %dKhz max\n",
            policy->min, policy->max);
    }

    return ret;
}

static int pxa_set_target(struct cpufreq_policy *policy,
                 unsigned int target_freq,
                 unsigned int relation)
{
    int idx;
    cpumask_t cpus_allowed, allowedcpuset;
    int cpu = policy->cpu;
    struct cpufreq_freqs freqs;
    unsigned long flags;
    unsigned int unused;
    unsigned int preset_mdrefr, postset_mdrefr, cclkcfg;

    if(freq_debug) {
      printk ("CPU PXA: target freq %d\n", target_freq);
      printk ("CPU PXA: relation %d\n", relation);
    }

    /*
     * Save this threads cpus_allowed mask.
     */
    cpus_allowed = current->cpus_allowed;

    /*
     * Bind to the specified CPU.  When this call returns,
     * we should be running on the right CPU.
     */
    cpus_clear (allowedcpuset);
    cpu_set (cpu, allowedcpuset);
    set_cpus_allowed(current, allowedcpuset);
    BUG_ON(cpu != smp_processor_id());

    /* Lookup the next frequency */
    if (cpufreq_frequency_table_target(policy, pxa2xx_freq_table,
                                       target_freq, relation, &idx)) {
      return -EINVAL;
    }

    freqs.old = policy->cur;
    freqs.new = pxa2xx_freqs[idx].khz;
    freqs.cpu = policy->cpu;
    if(freq_debug) {
        printk(KERN_INFO "Changing CPU frequency to %d Mhz, (SDRAM %d Mhz)\n",
            freqs.new/1000, (pxa2xx_freqs[idx].div2) ?
            (pxa2xx_freqs[idx].membus/2000) :
            (pxa2xx_freqs[idx].membus/1000));
    }

    /*
     * Tell everyone what we're about to do...
     * you should add a notify client with any platform specific
     * Vcc changing capability
     */
    cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

    /* Calculate the next MDREFR.  If we're slowing down the SDRAM clock
     * we need to preset the smaller DRI before the change.  If we're speeding
     * up we need to set the larger DRI value after the change.
     */
#ifndef CONFIG_MACH_T3XSCALE
    preset_mdrefr = postset_mdrefr = MDREFR;
    if((MDREFR & MDREFR_DRI_MASK) > MDREFR_DRI(pxa2xx_freqs[idx].membus)) {
        preset_mdrefr = (preset_mdrefr & ~MDREFR_DRI_MASK) |
                        MDREFR_DRI(pxa2xx_freqs[idx].membus);
    }
    postset_mdrefr = (postset_mdrefr & ~MDREFR_DRI_MASK) |
                    MDREFR_DRI(pxa2xx_freqs[idx].membus);

    /* If we're dividing the memory clock by two for the SDRAM clock, this
     * must be set prior to the change.  Clearing the divide must be done
     * after the change.
     */
    if(pxa2xx_freqs[idx].div2) {
      /*
       * Potentially speeding up memory clock, so slow down the memory
       * before speeding up the clock.
       */
      preset_mdrefr  |= MDREFR_DB2_MASK | MDREFR_K0DB4;
      preset_mdrefr  &= ~MDREFR_K0DB2;

      postset_mdrefr |= MDREFR_DB2_MASK | MDREFR_K0DB4;
      postset_mdrefr &= ~MDREFR_K0DB2;
    } else {
      /*
       * Potentially slowing down memory clock.  Wait until after the change
       * to speed up the memory.
       */
      postset_mdrefr &= ~MDREFR_DB2_MASK;
      postset_mdrefr &= ~MDREFR_K0DB4;
      postset_mdrefr |= MDREFR_K0DB2;
    }
#endif
    cclkcfg = pxa2xx_freqs[idx].cclkcfg;

    if (freq_debug) {
      printk (KERN_INFO "CPU PXA writing 0x%08x to CCCR\n",
              pxa2xx_freqs[idx].cccr);
      printk (KERN_INFO "CPU PXA writing 0x%08x to CCLKCFG\n",
              pxa2xx_freqs[idx].cclkcfg);
      printk (KERN_INFO "CPU PXA writing 0x%08x to MDREFR before change\n",
              preset_mdrefr);
      printk (KERN_INFO "CPU PXA writing 0x%08x to MDREFR after change\n",
              postset_mdrefr);
    }

    local_irq_save(flags);

    /* Set new the CCCR */
    CCCR = pxa2xx_freqs[idx].cccr;

    /*
     * Should really set both of PMCR[xIDAE] while changing the core frequency
     */

    /*
     * TODO: On the PXA27x: If we're setting half-turbo mode and changing the
     * core frequency at the same time we must split it up into two operations.
     * The current values in the pxa2xx_freqs table don't do this, so the code
     * is unimplemented.
     */
#ifdef CONFIG_MACH_T3XSCALE
    __asm__ __volatile__("                                  \
        /*ldr r4, [%1] ;*/  /* load MDREFR */                   \
        b   2f ;                                            \
        .align  5 ;                                         \
1:                                                          \
        /*str %3, [%1] ;*/          /* preset the MDREFR */     \
        mcr p14, 0, %2, c6, c0, 0 ; /* set CCLKCFG[FCS] */  \
        /*str %4, [%1] ;*/          /* postset the MDREFR */    \
                                                            \
        b   3f       ;                                      \
2:      b   1b       ;                                      \
3:      nop          ;                                      \
        "                                                                            
        : "=&r" (unused)                                                             
        : "r" (&MDREFR), "r" (cclkcfg),
          "r" (preset_mdrefr), "r" (postset_mdrefr)             
        : "r4", "r5");
#else
    __asm__ __volatile__("                                  \
        ldr r4, [%1] ;  /* load MDREFR */                   \
        b   2f ;                                            \
        .align  5 ;                                         \
1:                                                          \
        str %3, [%1] ;          /* preset the MDREFR */     \
        mcr p14, 0, %2, c6, c0, 0 ; /* set CCLKCFG[FCS] */  \
        str %4, [%1] ;          /* postset the MDREFR */    \
                                                            \
        b   3f       ;                                      \
2:      b   1b       ;                                      \
3:      nop          ;                                      \
        "
        : "=&r" (unused)
        : "r" (&MDREFR), "r" (cclkcfg), \
          "r" (preset_mdrefr), "r" (postset_mdrefr)
        : "r4", "r5");
#endif
    local_irq_restore(flags);

    if (freq_debug) {
      printk (KERN_INFO "CPU PXA Frequency change successful\n");
      printk (KERN_INFO "CPU PXA new CCSR 0x%08x\n", CCSR);
    }

    /*
     * Restore the CPUs allowed mask.
     */
    set_cpus_allowed(current, cpus_allowed);

    /*
     * Tell everyone what we've just done...
     * you should add a notify client with any platform specific
     * SDRAM refresh timer adjustments
     */
    cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

    return 0;
}

static int pxa_cpufreq_init(struct cpufreq_policy *policy)
{
    cpumask_t cpus_allowed, allowedcpuset;
    unsigned int cpu = policy->cpu;
    int i;
    unsigned int cclkcfg;

    cpus_allowed = current->cpus_allowed;

    cpus_clear (allowedcpuset);
    cpu_set (cpu, allowedcpuset);
    set_cpus_allowed(current, allowedcpuset);
    BUG_ON(cpu != smp_processor_id());

    /* set default governor and cpuinfo */
    policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
    policy->cpuinfo.transition_latency = 1000; /* FIXME: 1 ms, assumed */
    policy->cur = get_clk_frequency_khz(0); /* current freq */

    /* Generate the cpufreq_frequency_table struct */
    for(i=0;i<NUM_FREQS;i++) {
        pxa2xx_freq_table[i].frequency = pxa2xx_freqs[i].khz;
        pxa2xx_freq_table[i].index = i;
    }
    pxa2xx_freq_table[i].frequency = CPUFREQ_TABLE_END;

    /*
     * Set the policy's minimum and maximum frequencies from the tables
     * just constructed.  This sets cpuinfo.mxx_freq, min and max.
     */
    cpufreq_frequency_table_cpuinfo (policy, pxa2xx_freq_table);

    set_cpus_allowed(current, cpus_allowed);
    printk(KERN_INFO "PXA CPU frequency change support initialized\n");

    if (freq_debug) {
      printk (KERN_INFO "PXA CPU initial CCCR 0x%08x\n", CCCR);
      asm
	(
	 "mrc p14, 0, %0, c6, c0, 0 ; /* read CCLKCFG from CP14 */ "
	 : "=r" (cclkcfg) :
	 );
      printk ("PXA CPU initial CCLKCFG 0x%08x\n", cclkcfg);
      printk ("PXA CPU initial MDREFR  0x%08x\n", MDREFR);
    }

    return 0;
}

static unsigned int pxa_cpufreq_get(unsigned int cpu)
{
    cpumask_t cpumask_saved;
    unsigned int cur_freq;

    cpumask_saved = current->cpus_allowed;
    set_cpus_allowed(current, cpumask_of_cpu(cpu));
    BUG_ON(cpu != smp_processor_id());

    cur_freq = get_clk_frequency_khz(0);

    set_cpus_allowed(current, cpumask_saved);

    return cur_freq;
}

static struct cpufreq_driver pxa_cpufreq_driver = {
    .verify     = pxa_verify_policy,
    .target     = pxa_set_target,
    .init       = pxa_cpufreq_init,
    .get        = pxa_cpufreq_get,
#if defined(CONFIG_PXA25x)
    .name       = "PXA25x",
#elif defined(CONFIG_PXA27x)
    .name       = "PXA27x",
#endif
};

static int __init pxa_cpu_init(void)
{
    return cpufreq_register_driver(&pxa_cpufreq_driver);
}

static void __exit pxa_cpu_exit(void)
{
    cpufreq_unregister_driver(&pxa_cpufreq_driver);
}


MODULE_AUTHOR ("Intrinsyc Software Inc.");
MODULE_DESCRIPTION ("CPU frequency changing driver for the PXA architecture");
MODULE_LICENSE("GPL");
module_init(pxa_cpu_init);
module_exit(pxa_cpu_exit);

