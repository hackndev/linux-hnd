/*
 *  linux/arch/arm/mach-pxa/looxc550/looxc550.c
 *
 *  Hardware definitions for FSC LOOX C550
 *  
 *  Copyright (c) 2007 Yurii Zolot'ko
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>

#include <asm/arch/gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/udc.h>

#include <linux/ads7846.h>
#include <linux/touchscreen-adc.h>
#include <linux/pda_power.h>

#include <asm/arch/looxc550.h>

#include "../generic.h"

/**************************** Framebuffer **************************/

static struct pxafb_mode_info looxc550_pxafb_mode_info = {
	.pixclock		= 57692,
	.xres			= 480,
	.yres			= 640,
	.bpp			= 16,
	.hsync_len		= 8,
	.left_margin		= 8,
	.right_margin		= 124,
	.vsync_len		= 2,
	.upper_margin		= 3,
	.lower_margin		= 25,
	.sync			= 0,
};

static struct pxafb_mach_info looxc550_pxafb_info = {
	.modes			= &looxc550_pxafb_mode_info,
	.num_modes      	= 1,
	.lccr0			= LCCR0_Act | LCCR0_Sngl | LCCR0_Color,
	.lccr3			= LCCR3_OutEnH | LCCR3_PixRsEdg,
};

/**************************** MCI **********************************/

static int looxc550_mci_init(struct device *dev, irq_handler_t looxc550_detect_int, void *data)
{
	int err;

	/*
	 * setup GPIO for PXA27x MMC controller
	 */

	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);

	set_irq_type(gpio_to_irq(GPIO_NR_LOOXC550_SD_DETECT)
			, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
	err = request_irq(gpio_to_irq(GPIO_NR_LOOXC550_SD_DETECT)
			, looxc550_detect_int, IRQF_DISABLED
			, "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "looxc550_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static void looxc550_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	gpio_set_value(GPIO_NR_LOOXC550_SD_POWER, ( 1 << vdd) & p_d->ocr_mask);
}

static void looxc550_mci_exit(struct device *dev, void *data)
{
	free_irq(gpio_to_irq(GPIO_NR_LOOXC550_SD_DETECT), data);
}

static int looxc550_mci_get_ro(struct device* dev)
{
	return gpio_get_value(GPIO_NR_LOOXC550_SD_RO);
}

static struct pxamci_platform_data looxc550_mci_platform_data = {
	.ocr_mask       = MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= looxc550_mci_init,
	.setpower 	= looxc550_mci_setpower,
	.exit		= looxc550_mci_exit,
	.get_ro		= looxc550_mci_get_ro
};

/****************************     UDC     **************************/

static struct pxa2xx_udc_mach_info looxc550_udc_info __initdata = {
	.gpio_pullup	= GPIO_NR_LOOXC550_USB_PULLUP
};

/*************************** Power driver **************************/

static int looxc550_is_ac_online(void)
{
	return !gpio_get_value(GPIO_NR_LOOXC550_AC_IN_N);
}

static int looxc550_is_usb_online(void)
{
	return !gpio_get_value(GPIO_NR_LOOXC550_USB_DETECT_N);
}

static void looxc550_set_charge(int flags)
{
	gpio_set_value(GPIO_NR_LOOXC550_CHARGE_EN_N
			, !(flags & PDA_POWER_CHARGE_AC));

	gpio_set_value(GPIO_NR_LOOXC550_USB_CHARGE_RATE_N
			, !(flags & PDA_POWER_CHARGE_USB));
};

static struct pda_power_pdata looxc550_power_data = {
	.is_ac_online = looxc550_is_ac_online,
	.is_usb_online = looxc550_is_usb_online,
	.set_charge = looxc550_set_charge
};

static struct resource looxc550_power_resources[] = {
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

static struct platform_device looxc550_power = {
	.name = "pda-power",
	.id = -1,
	.resource = looxc550_power_resources,
	.num_resources = ARRAY_SIZE(looxc550_power_resources),
	.dev = {
		.platform_data = &looxc550_power_data
	},
};

/**************************** Entry point **************************/
struct ads7846_ssp_platform_data looxc550_ssp_params = {
	.port = 1,
	.pd_bits = 1,
	.freq = 720000,
};
static struct platform_device ads7846_ssp = { 
	.name = "ads7846-ssp", 
	.id = -1,
	.dev = {
		.platform_data = &looxc550_ssp_params,
	}
};

struct tsadc_platform_data looxc550_ts_params = {
	.pen_gpio = GPIO_NR_LOOXC550_TOUCHPANEL_IRQ_N,
	.x_pin = "ads7846-ssp:x",
	.y_pin = "ads7846-ssp:y",
	.z1_pin = "ads7846-ssp:z1",
	.z2_pin = "ads7846-ssp:z2",
	.pressure_factor = 100000,
	.min_pressure = 2,
	.max_jitter = 8,
};
static struct resource looxc550_pen_irq = {
	.start = gpio_to_irq(GPIO_NR_LOOXC550_TOUCHPANEL_IRQ_N),
	.end = gpio_to_irq(GPIO_NR_LOOXC550_TOUCHPANEL_IRQ_N),
	.flags = IORESOURCE_IRQ,
};
static struct platform_device ads7846_ts = {
	.name = "ts-adc",
	.id = -1,
	.resource = &looxc550_pen_irq,
	.num_resources = 1,
	.dev = {
		.platform_data = &looxc550_ts_params,
	}
};

static struct platform_device looxc550_keyboard = {
	.name = "looxc550-buttons"
};

static struct platform_device *devices[] __initdata = {
	&ads7846_ssp,
	&ads7846_ts,
	&looxc550_power,
	&looxc550_keyboard
};

static void __init looxc550_init(void)
{
	set_pxa_fb_info(&looxc550_pxafb_info);
	pxa_set_mci_info(&looxc550_mci_platform_data);
	pxa_set_udc_info(&looxc550_udc_info);
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(LOOXC550, "FSC Loox C550")
	.phys_io	= 0x40000000,
	.boot_params	= 0xa0000100,	/* BLOB boot parameter setting */
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= looxc550_init,
MACHINE_END
