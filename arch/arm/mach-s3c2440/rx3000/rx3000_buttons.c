/* 
 * Copyright 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Buttons driver for HP iPAQ RX3000
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
#include <linux/input_pda.h>
#include <linux/gpio_keys.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/hardware/asic3_keys.h>

#include <linux/mfd/asic3_base.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/rx3000-asic3.h>

extern struct platform_device s3c_device_asic3;

static struct gpio_keys_button rx3000_gpio_keys_table[] = {
        {KEY_POWER,     S3C2410_GPF0,   1,      "Power button"}, /* power*/
        {_KEY_RECORD,   S3C2410_GPF7,   1,      "Record button"}, /* camera */
        {_KEY_CALENDAR, S3C2410_GPG0,   1,      "Calendar button"}, /* play */
        {_KEY_CONTACTS, S3C2410_GPG2,   1,      "Contacts button"}, /* person */
        {_KEY_MAIL,     S3C2410_GPG3,   1,      "Mail button"}, /* nevo */
        {_KEY_HOMEPAGE, S3C2410_GPG7,   1,      "Home button"}, /* arrow */
        {KEY_ENTER,     S3C2410_GPG9,   1,      "Ok button"}, /* ok */
        {SW_HEADPHONE_INSERT, S3C2410_GPG11,  1,      "Headphone switch", EV_SW}, /* headphone switch */
};

static struct gpio_keys_platform_data rx3000_gpio_keys_data = {
        .buttons        = rx3000_gpio_keys_table,
        .nbuttons       = ARRAY_SIZE(rx3000_gpio_keys_table),
};

static struct platform_device rx3000_keys_gpio = {
	.name           = "gpio-keys",
	.dev		= {
	.platform_data  = &rx3000_gpio_keys_data,
    	 }
};

static struct asic3_keys_button rx3000_asic3_keys_table[] = {
        {KEY_LEFT,      IRQ_ASIC3_EINT2,        1,      "Left button"}, /* left */
        {KEY_RIGHT,     IRQ_ASIC3_EINT0,        1,      "Right button"}, /* right */
        {KEY_UP,        IRQ_ASIC3_EINT3,        1,      "Up button"}, /* up */
        {KEY_DOWN,      IRQ_ASIC3_EINT1,        1,      "Down button"}, /* down */

};

static struct asic3_keys_platform_data rx3000_asic3_keys_data = {
        .buttons        = rx3000_asic3_keys_table,
        .nbuttons       = ARRAY_SIZE(rx3000_asic3_keys_table),
        .asic3_dev      = &s3c_device_asic3.dev,
};

static struct platform_device rx3000_keys_asic3 = {
        .name           = "asic3-keys",
        .dev 		= {
        .platform_data	= &rx3000_asic3_keys_data,
        }
};

static int rx3000_buttons_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rx3000_gpio_keys_table); i++)
    	    s3c2410_gpio_cfgpin(rx3000_gpio_keys_table[i].gpio, S3C2410_GPIO_IRQ);

	platform_device_register(&rx3000_keys_gpio);
	platform_device_register(&rx3000_keys_asic3);
	return 0;
}

static int rx3000_buttons_remove(struct platform_device *pdev)
{
	platform_device_unregister(&rx3000_keys_gpio);
	platform_device_unregister(&rx3000_keys_asic3);
	return 0;
}

static struct platform_driver rx3000_buttons_driver = {
	.driver		= {	
		.name	= "rx3000-buttons",
	},
	.probe		= rx3000_buttons_probe,
	.remove		= rx3000_buttons_remove,
};

static int __init rx3000_buttons_init(void)
{	
	platform_driver_register(&rx3000_buttons_driver);
        return 0;
}

static void __exit rx3000_buttons_exit(void)
{
	platform_driver_unregister(&rx3000_buttons_driver);
}

module_init(rx3000_buttons_init);
module_exit(rx3000_buttons_exit);

MODULE_AUTHOR("Roman Moravcik <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("Buttons driver for HP iPAQ RX3000");
MODULE_LICENSE("GPL");
