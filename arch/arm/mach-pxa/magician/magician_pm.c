/*
 * Power management driver for HTC Magician (suspend/resume code)
 *
 * Copyright (c) 2006 Philipp Zabel
 *
 * Based on: hx4700_core.c
 * Copyright (c) 2005 SDG Systems, LLC
 *
 */

#include <linux/module.h>
#include <linux/mfd/htc-egpio.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include <asm/io.h>
#include <asm/gpio.h>

#include <asm/arch/pm.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/magician.h>

static int htc_bootloader = 0;	/* Is the stock HTC bootloader installed? */
static u32 save[4];
static u32 save2[13];

#ifdef CONFIG_PM
static int magician_suspend(struct platform_device *dev, pm_message_t state)
{
/*
	PGSR0 = 0x4034CA1E;
	PGSR1 = 0x00034102;
	PGSR2 = 0x0009C000;
	PGSR3 = 0x00780000;
*/
	PGSR0 |= 0x4034c00c; /* 2,3,11(GSM_OUT1),14(SSPSFRM2),15(nCS1),18,20(nSDCS2),21(nSDCS3),30(nCHARGE_EN) */
	PGSR1 |= 0x00030102; /* 33(nCS5),40(GSM_OUT2),48,49(nPWE) */
	PGSR2 |= 0x0009C000; /* 78(nCS2),79(nCS3),80(nCS4),83(nIR_EN) */
	PGSR3 |= 0x00100000; /* 116 */

	/*see 3.6.2.3 */
//      PCFR = PCFR_GPROD|PCFR_DC_EN|PCFR_GPR_EN|PCFR_OPDE;
	/* |PCFR_FP|PCFR_PI2CEN; */
	/*haret: 00001071 */
	PCFR = 0x00001071;

//hx4700:
//      PSLR=0xc8000000 /*| (2 << 2)*/ /* SL_PI = 2, PI power domain active, clocks running;*/
	/* haret: cc000000 */
	PSLR = 0xcc000000;

	return 0;
}

static int magician_resume(struct platform_device *dev)
{
	u32 tmp;

	printk("resume:\n");
	printk("CCCR = %08x\n", CCCR);
	asm("mrc\tp14, 0, %0, c6, c0, 0":"=r"(tmp));
	printk("CLKCFG = %08x\n", tmp);
	printk("MSC0 = %08x\n", MSC0);
	printk("MSC1 = %08x\n", MSC1);
	printk("MSC2 = %08x\n", MSC2);

	return 0;
}
#else
#define magician_suspend NULL
#define magician_resume NULL
#endif

static void magician_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	int i;
	u32 csum, tmp, *p;

	/* Save the 13 words at 0xa0038000. */
	for (p = phys_to_virt(0xa0038000), i = 0; i < 13; i++)
		save2[i] = p[i];

	/* Save the first four words at 0xa0000000. */
	for (p = phys_to_virt(0xa0000000), i = 0; i < 4; i++)
		save[i] = p[i];

	/* Set the first four words at 0xa0000000 to:
	 * resume address; MMU control; TLB base addr; domain id */
	p[0] = resume_addr;

      asm("mrc\tp15, 0, %0, c1, c0, 0":"=r"(tmp));
	p[1] = tmp & ~(0x3987);	/* mmu off */

      asm("mrc\tp15, 0, %0, c2, c0, 0":"=r"(tmp));
	p[2] = tmp;		/* Shouldn't matter, since MMU will be off. */

      asm("mrc\tp15, 0, %0, c3, c0, 0":"=r"(tmp));
	p[3] = tmp;		/* Shouldn't matter, since MMU will be off. */

	/* Set PSPR to the checksum the HTC bootloader wants to see. */
	for (csum = 0, i = 0; i < 52; i++) {
		tmp = p[i] & 0x1;
		tmp = tmp << 31;
		tmp |= tmp >> 1;
		csum += tmp;
	}

	PSPR = csum;
}

static void magician_pxa_ll_pm_resume(void)
{
	int i;
	u32 *p;

	/* Restore the first four words at 0xa0000000. */
	for (p = phys_to_virt(0xa0000000), i = 0; i < 4; i++)
		p[i] = save[i];

	/* Restore the 13 words at 0xa0038000. */
	for (p = phys_to_virt(0xa0038000), i = 0; i < 13; i++)
		p[i] = save2[i];

	/* XXX Do we need to flush the cache? */
}

struct pxa_ll_pm_ops magician_ll_pm_ops = {
	.suspend = magician_pxa_ll_pm_suspend,
	.resume = magician_pxa_ll_pm_resume,
};

extern struct platform_device egpio; /* defined in magician.c */

static int magician_pxa_pm_enter(suspend_state_t state)
{
	int resume = 0;

	printk(KERN_NOTICE "magician_pm suspend\n");

	pxa_pm_enter(state);

	do {
		printk(KERN_NOTICE "magician_pm wakeup, PEDR = 0x%08x\n", PEDR);

		resume = 1;
		if (PEDR == 0) { /* wince clears PEDR for CPLD interrupts */

			int irq = htc_egpio_get_wakeup_irq(&egpio.dev);

			switch(irq) {
			case IRQ_MAGICIAN_SD:
				printk("no resume for SD irq\n");
				resume = 0;
				break;
			case IRQ_MAGICIAN_EP:
				printk("no resume for EP irq\n");
				resume = 0;
				break;
			case IRQ_MAGICIAN_BT:
				printk("got BT irq, resume\n");
				break;
			case IRQ_MAGICIAN_AC:
				printk("no resume for AC irq\n");
				// update LED state here
				resume = 0;
			}

		}

		PEDR = 0xffffffff;

		if (resume == 0) {
			printk(KERN_NOTICE "magician_pm back to sleep (AC = %d, USB= %d)\n",
				gpio_get_value(EGPIO_MAGICIAN_CABLE_STATE_AC),
				gpio_get_value(EGPIO_MAGICIAN_CABLE_STATE_USB));

			pxa_pm_enter(state);
		}
	} while (resume == 0);

	printk(KERN_NOTICE "magician_pm resume\n");

	return 0;
}

struct pm_ops magician_pm_ops = {
	.pm_disk_mode	= PM_DISK_FIRMWARE,
	.prepare	= pxa_pm_prepare,
	.enter		= magician_pxa_pm_enter,
	.finish		= pxa_pm_finish,
};

extern struct pm_ops pxa_pm_ops;

static int magician_pm_probe(struct platform_device *dev)
{
	u32 *bootldr;
	int i;
	u32 tmp;

	printk(KERN_NOTICE "HTC Magician power management driver\n");

	/* Is the stock HTC bootloader installed? */

	bootldr = (u32 *) ioremap(PXA_CS0_PHYS, 1024 * 1024);
	i = 0x0004156c / 4;

	if (bootldr[i] == 0xe59f1534 &&	/* ldr r1, [pc, #1332] ; power base */
	    bootldr[i + 1] == 0xe5914008 &&	/* ldr r4, [r1, #8]    ; PSPR */
	    bootldr[i + 2] == 0xe1320004) {	/* teq r2, r4 */

		printk("Stock HTC bootloader detected\n");
		htc_bootloader = 1;
		pxa_pm_set_ll_ops(&magician_ll_pm_ops);
	}

	iounmap(bootldr);

	printk("CCCR = %08x\n", CCCR);
	asm("mrc\tp14, 0, %0, c6, c0, 0":"=r"(tmp));
	printk("CLKCFG = %08x\n", tmp);
	printk("MSC0 = %08x\n", MSC0);
	printk("MSC1 = %08x\n", MSC1);
	printk("MSC2 = %08x\n", MSC2);

	/* rtc driver should set PWER |= PWER_RTC
	   phone driver will set PWER |= PWER_GPIO10
	   cpld driver will set  PWER |= PWER_GPIO13 */
	PWER |= PWER_GPIO0 | PWER_GPIO1;
	PFER |= PWER_GPIO0 | PWER_GPIO1;
	PRER |= PWER_GPIO0 | PWER_GPIO1;


	pxa_pm_ops.enter = magician_pxa_pm_enter;

	return 0;
}

static int magician_pm_remove(struct platform_device *pdev)
{
//	pm_set_ops(NULL);
	pxa_pm_ops.enter = pxa_pm_enter;

	return 0;
}

struct platform_driver magician_pm_driver = {
	.probe   = magician_pm_probe,
	.remove  = magician_pm_remove,
	.suspend = magician_suspend,
	.resume  = magician_resume,
	.driver  = {
		.name = "magician-pm",
	}
};

static int __init magician_pm_init(void)
{
	return platform_driver_register(&magician_pm_driver);
}

static void __exit magician_pm_exit(void)
{
	platform_driver_unregister(&magician_pm_driver);
}

module_init(magician_pm_init);
module_exit(magician_pm_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("HTC Magician power management driver");
MODULE_LICENSE("GPL");
