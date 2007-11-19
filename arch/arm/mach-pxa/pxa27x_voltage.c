/* Helper module for the cpufreq. It is responsible for changing core
 * voltage on CPU through Intel(R) PXA27x EMTS compliant PMIC. Commands
 * sent to dumb PMIC through PWR_I2C sequencer integrated in PXA27x.
 * That driver do not expose PWR_I2C as general purpose I2C bus and
 * so far it supports single-byte commands only.
 *
 * Copyright (c) 2006  Michal Sroczynski <msroczyn@elka.pw.edu.pl> 
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * History:
 * 2006-02-04 : Michal Sroczynski <msroczyn@elka.pw.edu.pl> 
 * 	initial driver for Asus730
 * 2006-06-05 : Anton Vorontsov <cbou@mail.ru>
 *      hx4700 port, various changes
 * 2006-12-06 : Anton Vorontsov <cbou@mail.ru>
 *      Convert to the generic PXA driver.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa27x_voltage.h>

#ifdef DEBUG
#define dbg(msg...) do {printk("pxa27x_voltage:%s: ", __FUNCTION__);\
	                    printk(msg);} while(0);
#else
#define dbg(msg...)
#endif

struct freq2mv_table {
	 unsigned int khz;
	 unsigned int mv;
};

static struct freq2mv_table freq2mv_table[] = {
	//  kHz,   mV
	{104000,  900},
	{156000, 1050},
	{208000, 1100},
	{312000, 1250},
	{416000, 1350},
	{520000, 1450},
	{624000, 1550}
};

#define MAX_VOLTAGE_DROP (freq2mv_table[ARRAY_SIZE(freq2mv_table)-1].mv - \
                          freq2mv_table[0].mv)

static inline int freq2mv(int khz)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(freq2mv_table); i++) {
		if (freq2mv_table[i].khz == khz)
			return freq2mv_table[i].mv;
	}
	// Return maximal voltage we know if no freq2mv record found
	printk(KERN_WARNING "pxa27x_voltage: unknown freq: %d\n", khz);
	return freq2mv_table[ARRAY_SIZE(freq2mv_table)-1].mv;
}


/* This is Errata 28 workaround, see Intel Specification Update
 * POWER MANAGER: Core hangs during voltage change when there are
 * outstanding transactions on the bus */
static void start_voltage_change(void)
{
	unsigned int unused;
	unsigned long flags;
	local_irq_save(flags);
	__asm__ __volatile__("                                                \n\
	b       1f                                                            \n\
	nop                                                                   \n\
	.align  5                                                             \n\
1:                                                                            \n\
	ldr     r0, [%1]                   @ APB register read and compare    \n\
	cmp     r0, #0                     @ fence for pending slow apb reads \n\
	                                                                      \n\
	mov     r0, #0x8                   @ VC bit for PWRMODE               \n\
	movs    r1, #1                     @ don't execute mcr on 1st pass    \n\
	                                                                      \n\
retry:                                                                        \n\
	ldreq   r3, [%2]                   @ only stall on the 2nd pass       \n\
	cmpeq   r3, #0                     @ cmp causes fence on mem transfers\n\
	cmp     r1, #0                     @ is this the 2nd pass?            \n\
	mcreq   p14, 0, r0, c7, c0, 0      @ write to PWRMODE on 2nd pass only\n\
	                                                                      \n\
	@ Read VC bit until it is 0, indicates that the VoltageChange is done.\n\
	@ On first pass, we never set the VC bit, so it will be clear already.\n\
VoltageChange_loop:                                                           \n\
	mrc     p14, 0, r3, c7, c0, 0                                         \n\
	tst     r3, #0x8                                                      \n\
	bne     VoltageChange_loop                                            \n\
	                                                                      \n\
	subs    r1, r1, #1                                                    \n\
	beq     retry"
		:"=&r"(unused)
		:"r"(&CCCR), "r"(UNCACHED_ADDR)
		:"r0", "r1", "r3");
	local_irq_restore(flags);
	return;
}

static int fill_ramp(int old_mv, int new_mv,
                     struct pxa27x_voltage_chip *chip)
{
	int delta;
	int sign;
	int steps;
	int i;

	delta = new_mv - old_mv;
	if (!delta) return 0;

	sign  = abs(delta)/delta;
	steps = abs(delta)/chip->step;

	if (sign > 0 && old_mv + chip->step*steps < new_mv) steps++;
	else if (!steps) return 0;
	
	i = 0;
	while (steps > i) {
		old_mv += sign*chip->step;
		dbg("i:%d	mv: %d	lc:%d\n",
		    i, old_mv, steps <= i+1 ? 1 : 0);
		PCMD(i) = chip->mv2cmd(old_mv) |
		          (chip->delay ? PCMD_DCE : 0) |
		          (steps <= i+1 ? PCMD_LC : 0);
		i++;
	}

	return steps;
}

static int change_voltage(struct cpufreq_freqs *f,
                           struct pxa27x_voltage_chip *chip)
{
	int ret = 0;
	int steps = 1;

	PVCR = chip->address;
	if (chip->step) {
		PVCR = PVCR | (chip->delay << 7 & PVCR_CommandDelay);
		steps = fill_ramp(freq2mv(f->old), freq2mv(f->new), chip);
		if (steps == 0) return ret;
	}
	else PCMD(0) = chip->mv2cmd(freq2mv(f->new)) | PCMD_LC;

	start_voltage_change();

	while (PVCR & PVCR_VCSA) {
		dbg("read pointer at %d\n", PVCR >> 20);
		if (steps-- <= 0) {
			printk(KERN_WARNING "pxa27x_voltage: tired to wait, "
			       "read pointer at %d\n", PVCR >> 20);
			ret = 1;
			break;
		}
		msleep(chip->delay_ms);
	}
	return ret;
}

static int do_freq_transition(struct notifier_block *nb, unsigned long val,
                              void *data)
{
	int ret = 0;
	struct pxa27x_voltage_chip *chip = container_of(nb,
			struct pxa27x_voltage_chip, freq_transition);
	struct cpufreq_freqs *f = data;

	dbg("v=%ld cpu=%u old=%u new=%u flags=%hu\n",
	    val, f->cpu, f->old, f->new, f->flags);
	
	switch (val) {
		case CPUFREQ_PRECHANGE:
			if (f->new < f->old) break;
			ret = change_voltage(f, chip);
			break;
		case CPUFREQ_POSTCHANGE:
			if (f->new > f->old) break;
			ret = change_voltage(f, chip);
			break;
		default:
			printk(KERN_WARNING "pxa27x_voltage: "
			       "Unhandled cpufreq event\n");
			ret = 1;
			break;
	}

	return ret;
}

static int pxa27x_voltage_probe(struct platform_device *pdev)
{
	struct pxa27x_voltage_chip *chip = pdev->dev.platform_data;

	if (chip->step && (MAX_VOLTAGE_DROP + chip->step)/chip->step > 32) {
		printk(KERN_ERR "pxa27x_voltage: step too small, commands "
		       "will not fit in sequencer");
		return -EINVAL;
	}
	else if (chip->delay) {
		int i;
		if (chip->delay > 24) chip->delay = 24;
		chip->delay_ms = 2;
		for (i = 1; i < chip->delay; i++)
			chip->delay_ms *= 2;
		chip->delay_ms = chip->delay_ms/13000 + 1;
		dbg("delay_ms = %d\n", chip->delay_ms)
	}

	PCFR |= PCFR_PI2C_EN;
	pxa_set_cken(CKEN15_PWRI2C,1);
	chip->freq_transition.notifier_call = do_freq_transition;
	return cpufreq_register_notifier(&chip->freq_transition,
	                                 CPUFREQ_TRANSITION_NOTIFIER);
}

static int pxa27x_voltage_remove(struct platform_device *pdev)
{
	struct pxa27x_voltage_chip *chip = pdev->dev.platform_data;
	int ret;
	ret = cpufreq_unregister_notifier(&chip->freq_transition,
	                                  CPUFREQ_TRANSITION_NOTIFIER);
	pxa_set_cken(CKEN15_PWRI2C,0);
	return ret;
}

static struct platform_driver pxa27x_voltage_driver = {
	.probe = pxa27x_voltage_probe,
	.remove = pxa27x_voltage_remove,
	.driver = {
		.name = "pxa27x-voltage",
	},
};

static int __init pxa27x_voltage_init(void)
{
	return platform_driver_register(&pxa27x_voltage_driver);
}

static void __exit pxa27x_voltage_exit(void)
{
	platform_driver_unregister(&pxa27x_voltage_driver);
	return;
}

module_init(pxa27x_voltage_init);
module_exit(pxa27x_voltage_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn@elka.pw.edu.pl>, "
              "Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("Voltage change for PXA27x");
