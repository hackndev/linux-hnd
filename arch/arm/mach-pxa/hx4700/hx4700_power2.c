/*
 * Power driver for iPAQ hx4700
 *
 * Copyright 2007 Anton Vorontsov <cbou@mail.ru>
 * Copyright 2005 SDG Systems, LLC
 * Copyright 2005 Aric D. Blumer
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pda_power.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>
#include <asm/arch/hx4700-core.h>

#define DRIVER_NAME "hx4700_power"

static void hx4700_set_charge(int flags)
{
	SET_HX4700_GPIO_N(CHARGE_EN, flags & PDA_POWER_CHARGE_AC);
	SET_HX4700_GPIO(USB_CHARGE_RATE, flags & PDA_POWER_CHARGE_USB);
	return;
}

static int hx4700_is_ac_online(void)
{
	return !(asic3_get_gpio_status_d(&hx4700_asic3.dev) &
	        (1<<GPIOD_AC_IN_N));
}

static int hx4700_is_usb_online(void)
{
	return !(asic3_get_gpio_status_d(&hx4700_asic3.dev) &
	        (1<<GPIOD_USBC_DETECT_N));
}

static char *hx4700_supplicants[] = {
	"ds2760-battery.0", "backup-battery"
};

static struct pda_power_pdata hx4700_power_pdata = {
	.is_ac_online = hx4700_is_ac_online,
	.is_usb_online = hx4700_is_usb_online,
	.set_charge = hx4700_set_charge,
	.supplied_to = hx4700_supplicants,
	.num_supplicants = ARRAY_SIZE(hx4700_supplicants),
};

static struct resource hx4700_power_resourses[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
	},
	[1] = {
		.name = "usb",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
	},
};

static void hx4700_null_release(struct device *dev)
{
	return; /* current platform_device api is weird */
}

static struct platform_device hx4700_power_pdev = {
	.name = "pda-power",
	.id = -1,
	.resource = hx4700_power_resourses,
	.num_resources = ARRAY_SIZE(hx4700_power_resourses),
	.dev = {
		.platform_data = &hx4700_power_pdata,
		.release = hx4700_null_release,
	},
};

static int hx4700_power_init(void)
{
	int ret;
	unsigned int ac_irq, usb_irq;

	ac_irq = asic3_irq_base(&hx4700_asic3.dev) + ASIC3_GPIOD_IRQ_BASE
		+ GPIOD_AC_IN_N;

	usb_irq = asic3_irq_base(&hx4700_asic3.dev) + ASIC3_GPIOD_IRQ_BASE
		+ GPIOD_USBC_DETECT_N;

	hx4700_power_pdev.resource[0].start = ac_irq;
	hx4700_power_pdev.resource[0].end = ac_irq;
	hx4700_power_pdev.resource[1].start = usb_irq;
	hx4700_power_pdev.resource[1].end = usb_irq;

	ret = platform_device_register(&hx4700_power_pdev);
	if (ret)
		printk(DRIVER_NAME ": registration failed\n");

	return ret;
}

static void hx4700_power_exit(void)
{
	platform_device_unregister(&hx4700_power_pdev);
	return;
}

module_init(hx4700_power_init);
module_exit(hx4700_power_exit);
MODULE_LICENSE("GPL");
