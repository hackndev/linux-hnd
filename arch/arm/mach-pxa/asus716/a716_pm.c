/*
 *  Asus MyPal A716 PM glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *  Copyright (C) 2005 Jacek Migdal
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/apm-emulation.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>

#include <asm/arch/serial.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxa-regs.h>
#include <../drivers/pcmcia/soc_common.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/arch/irda.h>
#include <asm/arch/pxa-pm_ll.h>

static u32 *addr_a2000400;
static u32 *addr_a2000404;
static u32 *addr_a2000408;
static u32 *addr_a200040c;
static u32 save_a2000400;
static u32 save_a2000404;
static u32 save_a2000408;
static u32 save_a200040c;

static void a716_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	save_a2000400 = *addr_a2000400;
	save_a2000404 = *addr_a2000404;
	save_a2000408 = *addr_a2000408;
	save_a200040c = *addr_a200040c;

	*addr_a2000400 = 0xe3a00101; // mov r0, #0x40000000
	*addr_a2000404 = 0xe380060f; // orr r0, r0, #0x0f000000
	*addr_a2000408 = 0xe3800008; // orr r0, r0, #8
	*addr_a200040c = 0xe590f000; // ldr pc, [r0]
}

static void a716_pxa_ll_pm_resume(void)
{
	*addr_a2000400 = save_a2000400;
	*addr_a2000404 = save_a2000404;
	*addr_a2000408 = save_a2000408;
	*addr_a200040c = save_a200040c;
}

static struct pxa_ll_pm_ops a716_ll_pm_ops = {
	.suspend = a716_pxa_ll_pm_suspend,
	.resume  = a716_pxa_ll_pm_resume,
};

static void a716_ll_pm_init(void) {
	addr_a2000400 = phys_to_virt(0xa2000400);
	addr_a2000404 = phys_to_virt(0xa2000404);
	addr_a2000408 = phys_to_virt(0xa2000408);
	addr_a200040c = phys_to_virt(0xa200040c);

	pxa_pm_set_ll_ops(&a716_ll_pm_ops);
}								

static int a716_battery_power;
struct completion a716_battery_thread_comp;
static int thread_must_end;

signed long a716_ssp_putget(ulong data);

#define CTRL_START  0x80
#define CTRL_VBAT   0x24
#define CTRL_PD0    0x01

#define A716_MAIN_BATTERY_MAX 1676 // ~ sometimes it's more or less (> 1700 - AC)
#define A716_MAIN_BATTERY_MIN 1347
#define A716_MAIN_BATTERY_RANGE (A716_MAIN_BATTERY_MAX - A716_MAIN_BATTERY_MIN)

static void a716_battery(void)
{
	int sample;

	sample = a716_ssp_putget(CTRL_PD0 | CTRL_START | CTRL_VBAT); // main battery: min - 1347, max - 1676 (1700 AC)
	if (sample == -ETIMEDOUT)
		return;
	a716_ssp_putget(CTRL_START | CTRL_VBAT);

	sample = ((sample - A716_MAIN_BATTERY_MIN) * 100) / A716_MAIN_BATTERY_RANGE;
	if (sample > 100)
		a716_battery_power = 100;
	else
		a716_battery_power = sample;

	if (a716_battery_power < 10 && !(GPLR(GPIO_NR_A716_AC_DETECT) & GPIO_bit(GPIO_NR_A716_AC_DETECT)))
		a716_gpo_set(GPO_A716_POWER_LED_RED);
	else
		a716_gpo_clear(GPO_A716_POWER_LED_RED);

	//printk("battery: %d\n", a716_battery_power);
}

static int a716_battery_thread(void *arg)
{
	daemonize("kbattery");
	set_user_nice(current, 5);

	while (!kthread_should_stop()) {
		if (thread_must_end == 1)
			break;
		try_to_freeze();

		a716_battery();
		schedule_timeout_interruptible(HZ / 2);
	}

	complete_and_exit(&a716_battery_thread_comp, 0);
}

typedef void (*apm_get_power_status_t)(struct apm_power_info*);

static void a716_apm_get_power_status(struct apm_power_info *info)
{
	info->battery_life = a716_battery_power;

	if (!(GPLR(GPIO_NR_A716_AC_DETECT) & GPIO_bit(GPIO_NR_A716_AC_DETECT)))
		info->ac_line_status = APM_AC_OFFLINE;
	else
		info->ac_line_status = APM_AC_ONLINE;

	if (a716_battery_power > 50)
		info->battery_status = APM_BATTERY_STATUS_HIGH;
	else if (a716_battery_power < 10)
		info->battery_status = APM_BATTERY_STATUS_CRITICAL;
	else
		info->battery_status = APM_BATTERY_STATUS_LOW;

	info->time = 0;
	info->units = 0;
}

static int a716_pm_probe(struct platform_device *pdev)
{
	a716_ll_pm_init();

	a716_battery_power = 100;

	thread_must_end = 0;
	apm_get_power_status = a716_apm_get_power_status;
	init_completion(&a716_battery_thread_comp);
	kernel_thread(a716_battery_thread, NULL, 0);

	return 0;
}

static int a716_pm_remove(struct platform_device *pdev)
{
	apm_get_power_status = NULL;
	thread_must_end = 1;
	wait_for_completion(&a716_battery_thread_comp);

	a716_battery_power = 100;

	return 0;
}

static struct platform_driver a716_pm_driver = {
        .driver         = {
                .name           = "a716-pm",
        },
        .probe          = a716_pm_probe,
        .remove         = a716_pm_remove,
};

static int __init a716_pm_init(void)
{
        if (!machine_is_a716())
                return -ENODEV;

        return platform_driver_register(&a716_pm_driver);
}

static void __exit a716_pm_exit(void)
{
        platform_driver_unregister(&a716_pm_driver);
}

module_init(a716_pm_init)
module_exit(a716_pm_exit)

MODULE_AUTHOR("Pawel Kolodziejski, Jacek Migdal");
MODULE_DESCRIPTION("Asus MyPal A716 PM glue driver");
MODULE_LICENSE("GPL");
