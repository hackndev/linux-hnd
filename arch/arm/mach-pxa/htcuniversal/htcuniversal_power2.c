/*
 * pda_power driver for HTC Universal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pda_power.h>
#include <linux/mfd/asic3_base.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

static void charge_on(int flags)
{
        asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_CHARGE_EN, 0);
}

static int ac_on(void)
{
 return (GET_HTCUNIVERSAL_GPIO(POWER_DET) == 0);
}

static int usb_on(void)
{
 return (GET_HTCUNIVERSAL_GPIO(USB_DET) == 0);
}

static char *supplicants[] = {
	"ds2760-battery.0", "backup-battery"
};

static struct pda_power_pdata power_pdata = {
	.is_ac_online	= ac_on,
	.is_usb_online	= usb_on,
	.set_charge	= charge_on,
	.supplied_to = supplicants,
	.num_supplicants = ARRAY_SIZE(supplicants),
};

static struct resource power_resources[] = {
	[0] = {
		.name	= "ac",
		.start	= HTCUNIVERSAL_IRQ(POWER_DET),
		.end	= HTCUNIVERSAL_IRQ(POWER_DET),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	},
	[1] = {
		.name	= "usb",
		.start	= HTCUNIVERSAL_IRQ(USB_DET),
		.end	= HTCUNIVERSAL_IRQ(USB_DET),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	},
};

static void dev_release(struct device *dev)
{
	return;
}

static struct platform_device power_dev = 
{
	.name		= "pda-power",
	.id		= -1,
	.resource	= power_resources,
	.num_resources	= ARRAY_SIZE(power_resources),
	.dev = 
	 {
		.platform_data	= &power_pdata,
		.release	= dev_release,
	 },
};

static int htcuniversal_power_init(void)
{
 return platform_device_register(&power_dev);
}

static void htcuniversal_power_exit(void)
{
	platform_device_unregister(&power_dev);

	return;
}

module_init(htcuniversal_power_init);
module_exit(htcuniversal_power_exit);

MODULE_DESCRIPTION("Power driver for HTC Universal");
MODULE_LICENSE("GPL");
