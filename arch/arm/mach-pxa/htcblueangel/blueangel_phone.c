
/* Interface driver for HTC Blueangel Phone Modem
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-04-21   Todd Blumer             Created.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/asic3_base.h>
#include <linux/leds.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include <asm/arch/htcblueangel-asic.h>

DEFINE_LED_TRIGGER(radio_trig);

static int phone_power = 0;

static void
blueangel_phone_configure( int state )
{
        if (state) {
                printk("Phone modem: Powering up\n");
                STISR=0;
                GPCR(GPIO58_LDD_0) = GPIO_bit(GPIO58_LDD_0);
                mdelay(10);
                GPSR(GPIO58_LDD_0) = GPIO_bit(GPIO58_LDD_0);
                mdelay(100);
                GPCR(GPIO58_LDD_0) = GPIO_bit(GPIO58_LDD_0);
                mdelay(10);
                GPSR(GPIO62_LDD_4) = GPIO_bit(GPIO62_LDD_4);
                mdelay(10);
                GPCR(GPIO59_LDD_1) = GPIO_bit(GPIO59_LDD_1);
                mdelay(10);
                GPSR(GPIO59_LDD_1) = GPIO_bit(GPIO59_LDD_1);
                mdelay(10);
                GPCR(GPIO59_LDD_1) = GPIO_bit(GPIO59_LDD_1);
                led_trigger_event(radio_trig, LED_FULL);

        }
        else
        {
                printk("Phone modem: Powering Down\n");
                GPCR(GPIO58_LDD_0) = GPIO_bit(GPIO58_LDD_0);
                GPCR(GPIO62_LDD_4) = GPIO_bit(GPIO62_LDD_4);
                GPSR(GPIO64_LDD_6) = GPIO_bit(GPIO64_LDD_6);
                GPSR(GPIO65_LDD_7) = GPIO_bit(GPIO65_LDD_7);
                led_trigger_event(radio_trig, LED_OFF);
        }
}

static ssize_t phone_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", phone_power);
}

static ssize_t phone_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned int power;

        power = simple_strtoul(buf, NULL, 10);

        phone_power = power != 0 ? 1 : 0;

        blueangel_phone_configure(phone_power);

        return count;
}

static DEVICE_ATTR(phone_power, 0644, phone_show, phone_store);

static int
blueangel_phone_probe( struct platform_device *pdev )
{
        printk("Blueangel: Initializing phone modem hardware.\n");

        blueangel_phone_configure(phone_power);

        device_create_file(&pdev->dev, &dev_attr_phone_power);

        led_trigger_register_simple("phone", &radio_trig);

        return 0;
}

static int
blueangel_phone_remove( struct platform_device *pdev )
{

        device_remove_file(&pdev->dev, &dev_attr_phone_power);

        led_trigger_unregister_simple(radio_trig);

        return 0;
}

static struct platform_driver phone_driver = {
        .driver = {
                .name     = "blueangel-phone",
        },
        .probe    = blueangel_phone_probe,
        .remove   = blueangel_phone_remove,
};

static int __init
blueangel_phone_init( void )
{
        printk(KERN_NOTICE "HTC Blueangel Phone Modem Driver\n");
        return platform_driver_register( &phone_driver );
}
static void __exit
blueangel_phone_exit( void )
{
        platform_driver_unregister( &phone_driver );
}

module_init( blueangel_phone_init );
module_exit( blueangel_phone_exit );

MODULE_AUTHOR("");
MODULE_DESCRIPTION("HTC Blueangel Phone Modem Support Driver");
MODULE_LICENSE("GPL");
