/* 
 * Copyright 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * UDC Device controler driver for HP iPAQ RX3000
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/dpm.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <linux/mfd/asic3_base.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/rx3000-asic3.h>

#include <asm/arch/udc.h>

#include <asm/plat-s3c24xx/devs.h>

extern struct platform_device s3c_device_asic3;

static void rx3000_udc_pullup(enum s3c2410_udc_cmd_e cmd)
{
        switch (cmd)
        {
                case S3C2410_UDC_P_ENABLE :
			DPM_DEBUG("rx3000_udc: Turning on port\n");        
                        asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA3, ASIC3_GPA3);
                        break;
                case S3C2410_UDC_P_DISABLE :
			DPM_DEBUG("rx3000_udc: Turning off port\n");        
                        asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA3, 0);
                        break;
                case S3C2410_UDC_P_RESET :
                        break;
                default:
                        break;
        }
}

static struct s3c2410_udc_mach_info rx3000_udc_cfg = {
        .udc_command            = rx3000_udc_pullup,
        .vbus_pin               = S3C2410_GPG5,
        .vbus_pin_inverted      = 1,
};

static int rx3000_udc_probe(struct platform_device *pdev)
{
	/* configure vbus pin */
	s3c2410_gpio_cfgpin(S3C2410_GPG5, S3C2410_GPG5_EINT13);

	s3c24xx_udc_set_platdata(&rx3000_udc_cfg);

        /* Turn off suspend on both USB ports, and switch the
         * selectable USB port to USB device mode. */

        s3c2410_modify_misccr(S3C2410_MISCCR_USBHOST |
                              S3C2410_MISCCR_USBSUSPND0 |
                              S3C2410_MISCCR_USBSUSPND1, 0x0);


        platform_device_register(&s3c_device_usbgadget);
        return 0;
}

static int rx3000_udc_remove(struct platform_device *pdev)
{
        platform_device_unregister(&s3c_device_usbgadget);
        return 0;
}

static struct platform_driver rx3000_udc_driver = {
        .driver         = {
                .name   = "rx3000-udc",
        },
        .probe          = rx3000_udc_probe,
        .remove         = rx3000_udc_remove,
};

static int __init rx3000_udc_init(void)
{
        platform_driver_register(&rx3000_udc_driver);
        return 0;
}

static void __exit rx3000_udc_exit(void)
{
        platform_driver_unregister(&rx3000_udc_driver);
}

module_init(rx3000_udc_init);
module_exit(rx3000_udc_exit);

MODULE_AUTHOR("Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("USB Device controler driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
