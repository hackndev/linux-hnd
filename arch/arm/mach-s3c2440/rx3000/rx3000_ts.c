/* 
 * Copyright 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Touchscreen driver for HP iPAQ RX3000
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <asm/arch/ts.h>

#include <asm/plat-s3c24xx/devs.h>

static struct s3c2410_ts_mach_info rx3000_ts_cfg __initdata = {
        .delay = 10000,
        .presc = 49,
        .oversampling_shift = 2,
};


static int rx3000_ts_probe(struct platform_device *pdev)
{
        set_s3c2410ts_info(&rx3000_ts_cfg);
        
        platform_device_register(&s3c_device_ts);
        return 0;
}

static int rx3000_ts_remove(struct platform_device *pdev)
{
        platform_device_unregister(&s3c_device_ts);
        return 0;
}

static struct platform_driver rx3000_ts_driver = {
        .driver         = {
                .name   = "rx3000-ts",
        },
        .probe          = rx3000_ts_probe,
        .remove         = rx3000_ts_remove,
};

static int __init rx3000_ts_init(void)
{
        platform_driver_register(&rx3000_ts_driver);
        return 0;
}

static void __exit rx3000_ts_exit(void)
{
        platform_driver_unregister(&rx3000_ts_driver);
}

module_init(rx3000_ts_init);
module_exit(rx3000_ts_exit);

MODULE_AUTHOR("Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("Touchscreen driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
