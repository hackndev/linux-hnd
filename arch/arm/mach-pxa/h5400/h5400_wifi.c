/*
 * Driver for iPAQ H5400 internal USB OHCI host controller
 *
 * Copyright 2003 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2003-03-19   Jamey Hicks        Created file.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <linux/slab.h>
#define CONFIG_PCI
#include <linux/pci.h> /* pci_pool defs */
#include <linux/leds.h>

#include <asm/arch/hardware.h>
#include <asm/irq.h>

#include <asm/arch-pxa/h5400.h>
#include <asm/arch-pxa/h5400-asic.h>

#include <linux/mfd/samcop_base.h>

typedef struct ohci ohci_t;

extern int hc_add_ohci(struct pci_dev *dev, int irq, void *mem_base, unsigned long flags,
				 ohci_t **ohci, const char *name, const char *slot_name);
extern void hc_remove_ohci(ohci_t *ohci);

static void 
h5400_ohci_remove (struct device *dev)
{
#if 0
#warning port to kernel 2.6
	if (dev->driver_data) {
		hc_remove_ohci (dev->driver_data);
		dev->driver_data = NULL;
	}
#endif

	samcop_set_gpio_b (dev->parent, SAMCOP_GPIO_GPB_RF_POWER_ON | SAMCOP_GPIO_GPB_WLAN_POWER_ON, 0);
	led_trigger_event_shared(h5400_radio_trig, LED_OFF);
}

static int 
h5400_ohci_probe (struct device *dev)
{
	struct platform_device *sdev;
	int irq;
	void *mem_base;
	unsigned long flags = 0;
	int result = 0;

	sdev = to_platform_device (dev);
	mem_base = (void *)sdev->resource[1].start;
	irq = sdev->resource[2].start;

	/* make sure the clocks are enabled */
	samcop_clock_enable (dev->parent, SAMCOP_CPM_CLKCON_USBHOST_CLKEN, 1);
	samcop_clock_enable (dev->parent, SAMCOP_CPM_CLKCON_UCLK_EN, 0);

	/* both needed? */
	samcop_set_gpio_b (dev->parent, SAMCOP_GPIO_GPB_RF_POWER_ON | SAMCOP_GPIO_GPB_WLAN_POWER_ON, 
			   SAMCOP_GPIO_GPB_RF_POWER_ON | SAMCOP_GPIO_GPB_WLAN_POWER_ON);

	led_trigger_event_shared(h5400_radio_trig, LED_FULL);
#if 0
#warning port to kernel 2.6
	msleep (100);

	result = hc_add_ohci (pcidev, irq, mem_base, flags, (ohci_t **)&dev->driver_data, "h5400-ohci", "asic");
	if (result)
		h5400_ohci_remove (dev);
#endif

	return result;
}

static int
h5400_ohci_suspend (struct device *dev, pm_message_t msg)
{
	return 0;
}

static int
h5400_ohci_resume (struct device *dev)
{
	return 0;
}

struct device_driver h5400_usb_device_driver = {
	.name     = "h5400 wifi",
	.probe    = h5400_ohci_probe,
	.shutdown = h5400_ohci_remove,
	.suspend  = h5400_ohci_suspend,
	.resume   = h5400_ohci_resume
};

static int __init
h5400_ohci_init (void)
{
	return driver_register (&h5400_usb_device_driver);
}

static void __exit
h5400_ohci_exit (void)
{
	driver_unregister (&h5400_usb_device_driver);
}

module_init(h5400_ohci_init);
module_exit(h5400_ohci_exit);
