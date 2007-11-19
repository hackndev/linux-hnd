/*
 * Power driver for HTC blueangel
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
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>

#define DRIVER_NAME "blueangel_power"

void blueangel_set_charge_mode(int mode) {
        // While connected to usb the charger has two configurable modes, a low power < 100mA
        // and for once a higher power has be negotiated < 500mA

        if (mode) {
                asic3_set_gpio_out_c(&blueangel_asic3.dev, 1 << GPIOC_CHARGER_MODE, 1 << GPIOC_CHARGER_MODE);
        }
        else {
                asic3_set_gpio_out_c(&blueangel_asic3.dev, 1 << GPIOC_CHARGER_MODE, 0);
        }
        return;
}

static void blueangel_set_charge(int flags)
{
        printk("blueangel_set_charge: enabled=%d\n", flags);

        // Activates charging (low = enabled)
        if (flags) {
                asic3_set_gpio_out_b(&blueangel_asic3.dev, 1 << GPIOB_CHARGER_EN, 0);
        }
        else {
                asic3_set_gpio_out_b(&blueangel_asic3.dev, 1 << GPIOB_CHARGER_EN, 1 << GPIOB_CHARGER_EN);
                blueangel_set_charge_mode(0);
        }
}

static int blueangel_is_ac_online(void)
{
        return (asic3_get_gpio_status_d(&blueangel_asic3.dev) & (1<<GPIOD_AC_CHARGER_N)) == 0;
}

static int blueangel_is_usb_online(void)
{
        return (GPLR(GPIO_NR_BLUEANGEL_USB_DETECT_N) & GPIO_bit(GPIO_NR_BLUEANGEL_USB_DETECT_N)) == 0;
}

static char *blueangel_supplicants[] = {
	"ds2760-battery.0", "backup-battery"
};

static struct pda_power_pdata blueangel_power_pdata = {
	.is_ac_online = blueangel_is_ac_online,
	.is_usb_online = blueangel_is_usb_online,
	.set_charge = blueangel_set_charge,
	.supplied_to = blueangel_supplicants,
	.num_supplicants = ARRAY_SIZE(blueangel_supplicants),
};

static struct resource blueangel_power_resourses[] = {
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

static void blueangel_null_release(struct device *dev)
{
	return; /* current platform_device api is weird */
}

static struct platform_device blueangel_power_pdev = {
	.name = "pda-power",
	.id = -1,
	.resource = blueangel_power_resourses,
	.num_resources = ARRAY_SIZE(blueangel_power_resourses),
	.dev = {
		.platform_data = &blueangel_power_pdata,
		.release = blueangel_null_release,
	},
};

static int blueangel_power_init(void)
{
	int ret;
	unsigned int ac_irq, usb_irq;

	ac_irq = asic3_irq_base(&blueangel_asic3.dev) + ASIC3_GPIOD_IRQ_BASE
		+ GPIOD_AC_CHARGER_N;

	usb_irq = IRQ_GPIO(GPIO_NR_BLUEANGEL_USB_DETECT_N);

	blueangel_power_pdev.resource[0].start = ac_irq;
	blueangel_power_pdev.resource[0].end = ac_irq;
	blueangel_power_pdev.resource[1].start = usb_irq;
	blueangel_power_pdev.resource[1].end = usb_irq;

	ret = platform_device_register(&blueangel_power_pdev);
	if (ret)
		printk(DRIVER_NAME ": registration failed\n");

	return ret;
}

static void blueangel_power_exit(void)
{
	platform_device_unregister(&blueangel_power_pdev);
	return;
}

module_init(blueangel_power_init);
module_exit(blueangel_power_exit);
MODULE_LICENSE("GPL");
