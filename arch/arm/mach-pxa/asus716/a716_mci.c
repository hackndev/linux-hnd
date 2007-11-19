/*
 *  Asus MyPal A716 MCI glue driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>

#include <asm/arch/mmc.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/arch/irda.h>

static int a716_mci_init(struct device *dev, irqreturn_t (*a716_detect_int)(int, void *), void *data)
{
	int err = request_irq(IRQ_GPIO(GPIO_NR_A716_SD_CARD_DETECT_N), a716_detect_int, SA_INTERRUPT, "MMC/SD CD", data);
	if (err) {
		printk(KERN_ERR "a716_mci_init: MMC/SD: can't request MMC CD IRQ\n");
		return -1;
	}
	set_irq_type(IRQ_GPIO(GPIO_NR_A716_SD_CARD_DETECT_N), IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);

	return 0;
}

static void a716_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	if ((1 << vdd) & p_d->ocr_mask) {
		a716_gpo_clear(GPO_A716_SD_POWER_N);
	} else {
		a716_gpo_set(GPO_A716_SD_POWER_N);
	}
}

static void a716_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO(GPIO_NR_A716_SD_CARD_DETECT_N), data);
}

static struct pxamci_platform_data a716_mci_platform_data = {
	.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init           = a716_mci_init,
	.setpower       = a716_mci_setpower,
	.exit           = a716_mci_exit,
};

static int a716_mci_probe(struct platform_device *pdev)
{
	pxa_set_mci_info(&a716_mci_platform_data);

	return 0;
}

static struct platform_driver a716_mci_driver = {
	.driver   = {
		.name     = "a716-mci",
	},
	.probe    = a716_mci_probe,
};

static int __init a716_mci_init_module(void)
{
	return platform_driver_register(&a716_mci_driver);
}

module_init(a716_mci_init_module);

MODULE_AUTHOR("Pawel Kolodziejski");
MODULE_DESCRIPTION("Asus MyPal A716 MCI glue driver");
MODULE_LICENSE("GPL");
