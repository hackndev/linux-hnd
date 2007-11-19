/*
 * Battery driver for iPAQ h3900
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/apm-emulation.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>

#include <asm/mach/arch.h>
#include <asm/arch/h3900.h>
#include <asm/arch/h3900-gpio.h>

#include <linux/mfd/asic3_base.h>

static int ac_in;

static int 
h3900_ac_power (void)
{
	return (GPLR0 & GPIO_H3900_AC_IN_N) ? 0 : 1;
}

static void 
h3900_apm_get_power_status (struct apm_power_info *info)
{
	info->ac_line_status = ac_in ? APM_AC_ONLINE : APM_AC_OFFLINE;
}

static irqreturn_t
h3900_ac_in_isr (int irq, void *dev_id)
{
	ac_in = h3900_ac_power ();

	return IRQ_HANDLED;
}

static int
h3900_battery_init (void)
{
	int result;

	asic3_set_gpio_out_b(&h3900_asic3.dev, 
				   GPIO3_CH_TIMER, GPIO3_CH_TIMER);

	result = request_irq (IRQ_GPIO (GPIO_NR_H3900_AC_IN_N), h3900_ac_in_isr, 
			      SA_INTERRUPT | SA_SAMPLE_RANDOM, "AC presence", NULL);
	if (result) {
		printk (KERN_CRIT "%s: unable to grab AC in IRQ\n", __FUNCTION__);
		return result;
	}

	apm_get_power_status = h3900_apm_get_power_status;

	return 0;
}

static void
h3900_battery_exit (void)
{
	free_irq (IRQ_GPIO (GPIO_NR_H3900_AC_IN_N), NULL);
	apm_get_power_status = NULL;
}

module_init (h3900_battery_init)
module_exit (h3900_battery_exit)
