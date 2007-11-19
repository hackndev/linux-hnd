/* 
 * Copyright 2006 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Battery driver for HP iPAQ RX3000
 *
 * Based on rx3000_battery.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pda_power.h>
#include <linux/mfd/asic3_base.h>

#include <asm/io.h>

#include <asm/hardware/ipaq-asic3.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/rx3000-asic3.h>

extern struct platform_device s3c_device_asic3;

static void rx3000_null_release(struct device *dev)
{
	return; /* current platform_device api is weird */
}

/* External power */

static void rx3000_set_charge(int flags)
{
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA13,
	                     flags ? ASIC3_GPA13 : 0);
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA0,
	                     flags ? ASIC3_GPA0 : 0);
	return;
}

static int rx3000_is_ac_online(void)
{
	return !s3c2410_gpio_getpin(S3C2410_GPF2);
}

static int rx3000_is_usb_online(void)
{
	return !s3c2410_gpio_getpin(S3C2410_GPG5);
}

static struct resource rx3000_power_resourses[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ,
		.start = IRQ_EINT2,
		.end = IRQ_EINT2,
	},
	[1] = {
		.name = "usb",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
		.start = IRQ_EINT13,
		.end = IRQ_EINT13,
	},
};

static char *rx3000_supplicants[] = {
	"ds2760-battery.0"
};

static struct pda_power_pdata rx3000_power_pdata = {
	.is_ac_online = rx3000_is_ac_online,
	.is_usb_online = rx3000_is_usb_online,
	.set_charge = rx3000_set_charge,
	.supplied_to = rx3000_supplicants,
	.num_supplicants = ARRAY_SIZE(rx3000_supplicants),
};

static struct platform_device rx3000_power = {
	.name = "pda-power",
	.id = -1,
	.resource = rx3000_power_resourses,
	.num_resources = ARRAY_SIZE(rx3000_power_resourses),
	.dev = {
		.platform_data = &rx3000_power_pdata,
		.release = rx3000_null_release,
	},
};

/* Battery */

static int rx3000_battery_probe(struct platform_device *pdev)
{
	int ret;

	ret = platform_device_register(&rx3000_power);
	if (ret) {
		printk(KERN_ERR "rx3000_battery: pda-power failed\n");
		goto power_failed;
	}

	goto success;

power_failed:
success:
	return ret;
}

static int rx3000_battery_remove(struct platform_device *pdev)
{
	platform_device_unregister(&rx3000_power);
	return 0;
}

static struct platform_driver rx3000_battery_driver = {
        .driver         = {
                .name   = "rx3000-battery",
        },
        .probe          = rx3000_battery_probe,
        .remove         = rx3000_battery_remove,
};

static int __init rx3000_battery_init(void)
{
        printk(KERN_INFO "iPAQ RX3000 Battery Driver\n");
        return platform_driver_register(&rx3000_battery_driver);
}

static void __exit rx3000_battery_exit(void)
{
        platform_driver_unregister(&rx3000_battery_driver);
}

module_init(rx3000_battery_init);
module_exit(rx3000_battery_exit);

MODULE_DESCRIPTION("Battery monitor for HP iPAQ RX3000");
MODULE_AUTHOR("Roman Moravcik, <roman.moravcik@gmail.com>");
MODULE_LICENSE("GPL");
