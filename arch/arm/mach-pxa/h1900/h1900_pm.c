/*
 * HP iPAQ h1910/h1915 Power and APM status driver
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2005-2007 Pawel Kolodziejski
 * Copyright (C) 2003 Joshua Wise
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>
#include <linux/mfd/tmio_mmc.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/apm-emulation.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>
#include <asm/arch/h1900-asic.h>
#include <asm/arch/h1900-gpio.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/irq.h>
#include <asm/types.h>

#include "../generic.h"

static int h1900_battery_power;
struct completion h1900_battery_thread_comp;
static int thread_must_end;
extern struct platform_device h1900_asic3;
#define asic3 &h1900_asic3.dev

static u32 *addr_sig;
static u32 save_sig;

signed long h1900_ssp_putget(ulong data);
void h1900_set_led(int color, int duty_time, int cycle_time);

static void h1900_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	save_sig = *addr_sig;
	*addr_sig = 0x4C494E42; // LINB signature for bootloader while resume
}

static void h1900_pxa_ll_pm_resume(void)
{
	*addr_sig = save_sig;
}

static struct pxa_ll_pm_ops h1900_ll_pm_ops = {
	.suspend = h1900_pxa_ll_pm_suspend,
	.resume  = h1900_pxa_ll_pm_resume,
};

static void h1900_ll_pm_init(void)
{
	addr_sig = phys_to_virt(0xa0a00000);
	pxa_pm_set_ll_ops(&h1900_ll_pm_ops);
}

static irqreturn_t h1900_charge(int irq, void *dev_id)
{
	int charge;

	if (!(GPLR(GPIO_NR_H1900_AC_IN_N) & GPIO_bit(GPIO_NR_H1900_AC_IN_N)) &&
			(GPLR(GPIO_NR_H1900_MBAT_IN) & GPIO_bit(GPIO_NR_H1900_MBAT_IN))  )
		charge = 1;
	else
		charge = 0;
		
	if (charge) {
		int dtime;
		
		if (GPLR(GPIO_NR_H1900_CHARGING) & GPIO_bit(GPIO_NR_H1900_CHARGING)) {
			dtime = 0x80;
		} else {
			dtime = 0x101;
		}
		asic3_set_led(asic3, 1, dtime, 0x100, 6);
		asic3_set_led(asic3, 0, dtime, 0x100, 6);
		GPSR(GPIO_NR_H1900_CHARGER_EN) = GPIO_bit(GPIO_NR_H1900_CHARGER_EN);
	} else {
		asic3_set_led(asic3, 1, 0, 0x100, 6);
		asic3_set_led(asic3, 0, 0, 0x100, 6);
		GPCR(GPIO_NR_H1900_CHARGER_EN) = GPIO_bit(GPIO_NR_H1900_CHARGER_EN);
	};

	return IRQ_HANDLED;
};

static struct irqaction h1900_charge_irq = {
	name:     "h1910/h1915 charge",
	handler:  h1900_charge,
	flags:    SA_INTERRUPT
};

#define CTRL_START  0x80
#define CTRL_AUX_N  0x64
#define CTRL_PD0    0x01

static void h1900_battery(void)
{
	int sample;

	sample = h1900_ssp_putget(CTRL_PD0 | CTRL_START | CTRL_AUX_N); // main battery: max - 4096, min - 3300
	if (sample == -ETIMEDOUT)
		return;
	h1900_ssp_putget(CTRL_START | CTRL_AUX_N);

	h1900_battery_power = ((sample - 3300) * 100) / 796;

	if (!!(GPLR(GPIO_NR_H1900_AC_IN_N) & GPIO_bit(GPIO_NR_H1900_AC_IN_N))) {
		if (h1900_battery_power > 50) {
			asic3_set_led(asic3, 1, 0, 0x100, 6);
			asic3_set_led(asic3, 0, 0, 0x100, 6);
		} else if ((h1900_battery_power < 50) && (h1900_battery_power > 10)) {
			h1900_set_led(H1900_GREEN_LED, 0x101, 0x100);
		} else if (h1900_battery_power < 10) {
			h1900_set_led(H1900_RED_LED, 0x101, 0x100);
		}
	}

	//printk("bat: %d\n", battery_power);
}

static int h1900_battery_thread(void *arg)
{
	daemonize("kbattery");
	set_user_nice(current, 5);

	while (!kthread_should_stop()) {
		if (thread_must_end == 1)
			break;
		try_to_freeze();

		h1900_battery();
		schedule_timeout_interruptible(HZ / 2);
	}

        complete_and_exit(&h1900_battery_thread_comp, 0);
}

typedef void (*apm_get_power_status_t)(struct apm_power_info*);

static void h1900_apm_get_power_status(struct apm_power_info *info)
{
	info->battery_life = h1900_battery_power;

	if (!(GPLR(GPIO_NR_H1900_AC_IN_N) & GPIO_bit(GPIO_NR_H1900_AC_IN_N)))
		info->ac_line_status = APM_AC_ONLINE;
	else
		info->ac_line_status = APM_AC_OFFLINE;

	if (GPLR(GPIO_NR_H1900_CHARGING) & GPIO_bit(GPIO_NR_H1900_CHARGING))
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	else if (!(GPLR(GPIO_NR_H1900_MBAT_IN) & GPIO_bit(GPIO_NR_H1900_MBAT_IN)))
		info->battery_status = APM_BATTERY_STATUS_NOT_PRESENT;
	else {
		if (h1900_battery_power > 50)
			info->battery_status = APM_BATTERY_STATUS_HIGH;
		else if (h1900_battery_power < 5)
			info->battery_status = APM_BATTERY_STATUS_CRITICAL;
		else
			info->battery_status = APM_BATTERY_STATUS_LOW;
	}

	info->time = 0;
	info->units = 0;
}

static int h1900_power_probe(struct platform_device *pdev)
{
	h1900_ll_pm_init();

	h1900_charge(0, NULL);

	set_irq_type(IRQ_GPIO(GPIO_NR_H1900_AC_IN_N), IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(GPIO_NR_H1900_MBAT_IN), IRQT_BOTHEDGE);
	set_irq_type(IRQ_GPIO(GPIO_NR_H1900_CHARGING), IRQT_BOTHEDGE);
	setup_irq(IRQ_GPIO(GPIO_NR_H1900_AC_IN_N), &h1900_charge_irq);
	setup_irq(IRQ_GPIO(GPIO_NR_H1900_MBAT_IN), &h1900_charge_irq);
	setup_irq(IRQ_GPIO(GPIO_NR_H1900_CHARGING), &h1900_charge_irq);

	h1900_battery_power = 100;

	thread_must_end = 0;
	apm_get_power_status = h1900_apm_get_power_status;
	init_completion(&h1900_battery_thread_comp);
	kernel_thread(h1900_battery_thread, NULL, 0);

	return 0;
}

static int h1900_power_remove(struct platform_device *pdev)
{
	asic3_set_led(asic3, 1, 0, 0x100, 6);
	asic3_set_led(asic3, 0, 0, 0x100, 6);

	disable_irq(IRQ_GPIO(GPIO_NR_H1900_AC_IN_N));
	disable_irq(IRQ_GPIO(GPIO_NR_H1900_MBAT_IN));
	disable_irq(IRQ_GPIO(GPIO_NR_H1900_CHARGING));

	apm_get_power_status = NULL;
	thread_must_end = 1;
	wait_for_completion(&h1900_battery_thread_comp);
	h1900_battery_power = 100;

	return 0;
}

static int h1900_power_suspend(struct platform_device *pdev, pm_message_t state)
{
	asic3_set_led(asic3, 1, 0, 0x100, 6);
	asic3_set_led(asic3, 0, 0, 0x100, 6);

	disable_irq(IRQ_GPIO(GPIO_NR_H1900_AC_IN_N));
	disable_irq(IRQ_GPIO(GPIO_NR_H1900_MBAT_IN));
	disable_irq(IRQ_GPIO(GPIO_NR_H1900_CHARGING));

	return 0;
}

static int h1900_power_resume(struct platform_device *pdev)
{
	enable_irq(IRQ_GPIO(GPIO_NR_H1900_AC_IN_N));
	enable_irq(IRQ_GPIO(GPIO_NR_H1900_MBAT_IN));
	enable_irq(IRQ_GPIO(GPIO_NR_H1900_CHARGING));

	return 0;
}

static struct platform_driver h1900_power_driver = {
	.driver		= {
		.name		= "h1900-power",
	},
	.probe		= h1900_power_probe,
	.remove		= h1900_power_remove,
	.suspend	= h1900_power_suspend,
	.resume		= h1900_power_resume,
};

static int __init h1900_power_init(void)
{
	if (!machine_is_h1900())
		return -ENODEV;

	return platform_driver_register(&h1900_power_driver);
}

static void __exit h1900_power_exit(void)
{
	platform_driver_unregister(&h1900_power_driver);
}

module_init(h1900_power_init);
module_exit(h1900_power_exit);

MODULE_AUTHOR("Joshua Wise, Pawel Kolodziejski");
MODULE_DESCRIPTION("HP iPAQ h1910/h1915 Power Management driver");
MODULE_LICENSE("GPL");
