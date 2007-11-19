/*
 * asus730 suspend/resume support based on code for asus716
 *
 * Use consistent with the GNU GPL is permitted, provided that this
 * copyright notice is preserved in its entirety in all copies and
 * derived works.
 *
 * Copyright (C) 2005 Michal Sroczynski
 *
 * 03 Apr 2006
 *	Serge Nikolaenko	USB Host platform init
 *				Suspend preparations
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/lcd.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/asus730-gpio.h>
#include <asm/arch/asus730-init.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>

static u32 *addr_a1440000;
static u32 save_a1440000;

static u32 *addr_a1441000;
static u32 *addr_a1441004;
static u32 *addr_a1441008;
static u32 save_a1441000;
static u32 save_a1441004;
static u32 save_a1441008;

extern void pca9535_write_output(u16);

static void a730_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	save_a1440000 = *addr_a1440000;
	save_a1441000 = *addr_a1441000;
	save_a1441004 = *addr_a1441004;
	save_a1441008 = *addr_a1441008;

	*addr_a1440000 = 0xea0003fe; // b +0x1000
	*addr_a1441000 = 0xe59f0000; // ldr r0, [pc, #0]
	*addr_a1441004 = 0xe590f000; // ldr pc,[r0]
	*addr_a1441008 = 0x40f00008; // data (PSPR addr)
	
	return;
}

static void a730_pxa_ll_pm_resume(void)
{
	*addr_a1440000 = save_a1440000;
	*addr_a1441000 = save_a1441000;
	*addr_a1441004 = save_a1441004;
	*addr_a1441008 = save_a1441008;
}

static struct pxa_ll_pm_ops a730_ll_pm_ops = {
	.suspend = a730_pxa_ll_pm_suspend,
	.resume  = a730_pxa_ll_pm_resume,
};

void a730_ll_pm_init(void) {
	addr_a1440000 = phys_to_virt(0xa1440000);
	addr_a1441000 = phys_to_virt(0xa1441000);
	addr_a1441004 = phys_to_virt(0xa1441004);
	addr_a1441008 = phys_to_virt(0xa1441008);

	pxa_pm_set_ll_ops(&a730_ll_pm_ops);
}

int a730_suspend(struct platform_device *pdev, pm_message_t state)
{
	PWER = PWER_RTC | PWER_GPIO0 | PWER_GPIO1 | PWER_GPIO10 | PWER_GPIO12 | PWER_GPIO(17) | PWER_GPIO13 | PWER_WEP1 | /*PWER_USBC |*/ (0x1 << 24 /*wakeup enable for standby or sleep*/ | (0x1 << 16 /* CF card */));
	PFER = PWER_GPIO1 | PWER_GPIO12 | PWER_GPIO10;
	PRER = PWER_GPIO0 | PWER_GPIO12;

	return 0;
}

int a730_resume(struct platform_device *pdev)
{
	/*printk("PGSR0 0x%x - 0x%x\n", PGSR0, GPSRx_SleepValue);
	printk("PGSR1 0x%x - 0x%x\n", PGSR1, GPSRy_SleepValue);
	printk("PGSR2 0x%x - 0x%x\n", PGSR2, GPSRz_SleepValue);
	
	printk("GAFR0_L 0x%x - 0x%x\n", GAFR0_L, GAFR0x_InitValue);
        printk("GAFR0_U 0x%x - 0x%x\n", GAFR0_U, GAFR1x_InitValue);
        printk("GAFR1_L 0x%x - 0x%x\n", GAFR1_L, GAFR0y_InitValue);
        printk("GAFR1_U 0x%x - 0x%x\n", GAFR1_U, GAFR1y_InitValue);
        printk("GAFR2_L 0x%x - 0x%x\n", GAFR2_L, GAFR0z_InitValue);
        printk("GAFR2_U 0x%x - 0x%x\n", GAFR2_U, GAFR1z_InitValue);
        
        printk("GPDR0 0x%x - 0x%x\n", GPDR0, GPDRx_InitValue);
        printk("GPDR1 0x%x - 0x%x\n", GPDR1, GPDRy_InitValue);
        printk("GPDR2 0x%x - 0x%x\n", GPDR2, GPDRz_InitValue);
        
        printk("GPSR0 0x%x - 0x%x\n", GPSR0, GPSRx_InitValue);
        printk("GPSR1 0x%x - 0x%x\n", GPSR1, GPSRy_InitValue);
        printk("GPSR2 0x%x - 0x%x\n", GPSR2, GPSRz_InitValue);

        printk("GPCR0 0x%x - 0x%x\n", GPCR0, ~GPSRx_InitValue);
        printk("GPCR1 0x%x - 0x%x\n", GPCR1, ~GPSRy_InitValue);
        printk("GPCR2 0x%x - 0x%x\n", GPCR2, ~GPSRz_InitValue);*/

	return 0;
}
