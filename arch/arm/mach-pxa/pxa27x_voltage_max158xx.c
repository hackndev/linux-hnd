/* Driver for the Maxim 158xx PMIC, connected to PXA27x PWR_I2C.
 *
 * Copyright (c) 2006  Michal Sroczynski <msroczyn@elka.pw.edu.pl> 
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * History:
 * 2006-02-04 : Michal Sroczynski <msroczyn@elka.pw.edu.pl> 
 * 	initial driver for Asus730
 * 2006-06-05 : Anton Vorontsov <cbou@mail.ru>
 *      hx4700 port, various changes
 * 2006-12-06 : Anton Vorontsov <cbou@mail.ru>
 *      Convert to the generic PXA driver.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm/arch/pxa27x_voltage.h>

static u_int8_t max158xx_address = 0x14;
module_param(max158xx_address, byte, 0644);
MODULE_PARM_DESC(max158xx_address, "MAX158xx address on I2C bus (0x14/0x16)");

static inline u_int8_t mv2cmd (int mv)
{
	u_int8_t val = (mv - 700) / 25;
	if (val > 31) val = 31;
	return val;
}

static struct platform_device *pdev;

static int __init max158xx_init(void)
{
	int ret;
	struct pxa27x_voltage_chip chip;
	
	memset(&chip, 0, sizeof(chip));
	chip.address = max158xx_address;
	chip.mv2cmd  = mv2cmd;

	pdev = platform_device_alloc("pxa27x-voltage", -1);
	if (!pdev) return -ENOMEM;
	
	ret = platform_device_add_data(pdev, &chip, sizeof(chip));
	if (ret) goto failed;
	
	ret = platform_device_register(pdev);
	if (ret) goto failed;

	return 0;
failed:
	platform_device_put(pdev);
	return ret;
}

static void __exit max158xx_exit(void)
{
	platform_device_unregister(pdev);
	return;
}

module_init(max158xx_init);
module_exit(max158xx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn@elka.pw.edu.pl>, "
              "Anton Vorontsov <cbou@mail.ru>");
MODULE_DESCRIPTION("Driver for the Maxim 158xx PMIC");
