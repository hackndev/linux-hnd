/* 
 * Copyright 2007 Roderick Taylor <myopiate@gmail.com>
 * Copyright 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Serial port driver for HP iPAQ RX3000
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <linux/mfd/asic3_base.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/rx3000-asic3.h>

extern struct platform_device s3c_device_asic3;

static void rx3000_serial_power(int power)
{
	if (power) {
		DPM_DEBUG("rx3000_serial: Turning on\n");
		/* configure uart1 gpios */
		s3c2410_gpio_cfgpin(S3C2410_GPH4, S3C2410_GPH4_TXD1);
		s3c2410_gpio_cfgpin(S3C2410_GPH5, S3C2410_GPH5_RXD1);

		/* power on */
    		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB2, ASIC3_GPB2);
	} else {
		DPM_DEBUG("rx3000_serial: Turning off\n");
		/* power off */
    		asic3_set_gpio_out_b(&s3c_device_asic3.dev, ASIC3_GPB2, 0);

		/* configure uart1 gpios */
		s3c2410_gpio_cfgpin(S3C2410_GPH4, S3C2410_GPH4_OUTP);
		s3c2410_gpio_cfgpin(S3C2410_GPH5, S3C2410_GPH5_OUTP);
	}
}

static irqreturn_t rx3000_serial_cable(int irq, void *dev_id)
{
	int status;
	
	status = s3c2410_gpio_getpin(S3C2410_GPF6);

	rx3000_serial_power(status);

	return IRQ_HANDLED;
}

static int rx3000_serial_probe(struct platform_device *pdev)
{
	int ret;

	/* configure serial cable detection pin */
	s3c2410_gpio_cfgpin(S3C2410_GPF6, S3C2410_GPF6_EINT6);
	
	ret = request_irq(IRQ_EINT6, rx3000_serial_cable, IRQF_DISABLED | IRQF_TRIGGER_RISING |
			  IRQF_TRIGGER_FALLING, "serial_cable", NULL);

	if (ret < 0)
    	    return -EINTR;


        return 0;
}

static int rx3000_serial_remove(struct platform_device *pdev)
{
        free_irq(IRQ_EINT6, NULL);
	s3c2410_gpio_cfgpin(S3C2410_GPF6, S3C2410_GPF6_INP);

	rx3000_serial_power(0);
        return 0;
}

#ifdef CONFIG_PM
static int rx3000_serial_suspend(struct platform_device *pdev, pm_message_t state)
{
	disable_irq(IRQ_EINT6);

	rx3000_serial_power(0);
	return 0;
}

static int rx3000_serial_resume(struct platform_device *pdev)
{
	enable_irq(IRQ_EINT6);
	
	if (s3c2410_gpio_getpin(S3C2410_GPF6))
		rx3000_serial_power(1);

	return 0;
}
#endif

static struct platform_driver rx3000_serial_driver = {
        .driver         = {
                .name   = "rx3000-serial",
        },
        .probe          = rx3000_serial_probe,
        .remove         = rx3000_serial_remove,
#ifdef CONFIG_PM
	.suspend	= rx3000_serial_suspend,
	.resume		= rx3000_serial_resume,
#endif
};

static int __init rx3000_serial_init(void)
{
        platform_driver_register(&rx3000_serial_driver);
        return 0;
}

static void __exit rx3000_serial_exit(void)
{
        platform_driver_unregister(&rx3000_serial_driver);
}

module_init(rx3000_serial_init);
module_exit(rx3000_serial_exit);

MODULE_AUTHOR("Roderick Taylor <myopiate@gmail.com>, Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("Serial port driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
