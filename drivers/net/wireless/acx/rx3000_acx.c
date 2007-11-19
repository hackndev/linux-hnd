/*
 * WLAN (TI TNETW1100B) support in the HP iPAQ RX3000
 *
 * Copyright (c) 2006 SDG Systems, LLC
 * Copyright (c) 2006 Roman Moravcik
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Based on hx4700_acx.c
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dpm.h>
#include <linux/leds.h>

#include <asm/hardware.h>

#include <asm/arch/regs-gpio.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/rx3000.h>
#include <asm/arch/rx3000-asic3.h>
#include <asm/io.h>

#include "acx_hw.h"

extern struct platform_device s3c_device_asic3;

static int rx3000_wlan_start(void)
{
	DPM_DEBUG("rx3000_acx: Turning on\n");
	asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB3, ASIC3_GPB3);
	mdelay(20);
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC13, ASIC3_GPC13);
	mdelay(20);
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC11, ASIC3_GPC11);
	mdelay(100);
	asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB3, ASIC3_GPB3);
	mdelay(20);
	s3c2410_gpio_cfgpin(S3C2410_GPA15, S3C2410_GPA15_nGCS4);        
	mdelay(100);
	s3c2410_gpio_setpin(S3C2410_GPA11, 0);
	mdelay(50);
	s3c2410_gpio_setpin(S3C2410_GPA11, 1);
	led_trigger_event_shared(rx3000_radio_trig, LED_FULL);
	return 0;
}

static int rx3000_wlan_stop(void)
{
	DPM_DEBUG("rx3000_acx: Turning off\n");
	s3c2410_gpio_setpin(S3C2410_GPA15, 1);
	s3c2410_gpio_cfgpin(S3C2410_GPA15, S3C2410_GPA15_OUT);
	asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB3, 0);
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC13, 0);
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC11, 0);
	led_trigger_event_shared(rx3000_radio_trig, LED_OFF);
	return 0;
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= RX3000_PA_WLAN,
		.end	= RX3000_PA_WLAN + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EINT16,
		.end	= IRQ_EINT16,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct acx_hardware_data acx_data = {
	.start_hw	= rx3000_wlan_start,
	.stop_hw	= rx3000_wlan_stop,
};

static struct platform_device acx_device = {
	.name	= "acx-mem",
	.dev	= {
		.platform_data = &acx_data,
	},
	.num_resources	= ARRAY_SIZE(acx_resources),
	.resource	= acx_resources,
};

static int __init rx3000_wlan_init(void)
{
	printk("rx3000_wlan_init: acx-mem platform_device_register\n");
	return platform_device_register(&acx_device);
}


static void __exit rx3000_wlan_exit(void)
{
	platform_device_unregister(&acx_device);
}

module_init(rx3000_wlan_init);
module_exit(rx3000_wlan_exit);

MODULE_AUTHOR("Todd Blumer <todd@sdgsystems.com>, Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("WLAN driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");

