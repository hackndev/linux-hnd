/*
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2002-08-23   Jamey Hicks        GPIO and IRQ support for iPAQ H5400
 *
 */

#ifndef SAMCOP_BASE_H
#define SAMCOP_BASE_H

extern void samcop_set_gpio_a (struct device *dev, u32 mask, u32 bits);
extern void samcop_set_gpio_b (struct device *dev, u32 mask, u32 bits);
extern void samcop_set_gpio_c (struct device *dev, u32 mask, u32 bits);
extern void samcop_set_gpio_d (struct device *dev, u32 mask, u32 bits);
extern void samcop_set_gpio_e (struct device *dev, u32 mask, u32 bits);

extern u32 samcop_get_gpio_a (struct device *dev);
extern u32 samcop_get_gpio_b (struct device *dev);
extern u32 samcop_get_gpio_c (struct device *dev);
extern u32 samcop_get_gpio_d (struct device *dev);
extern u32 samcop_get_gpio_e (struct device *dev);

extern void samcop_reset_fcd (struct device *dev);

extern void samcop_set_spcr (struct device *dev, u32 mask, u32 bits);

struct samcop_platform_data
{
	struct gpiodev_ops gpiodev_ops;

	/* Standard MFD properties */
	int irq_base;
	int gpio_base;
	struct platform_device **child_devs;
	int num_child_devs;

	u32 clocksleep;
	u32 pllcontrol;
	struct samcop_adc_platform_data samcop_adc_pdata;
	struct {
		u32 gpacon1, gpacon2, gpadat, gpaup;
		
		/* These are not configurable yet - there are no functions to set them
		 * in samcop_base.c 
		 */

		/*u32 gpbcon, gpbdat, gpbup;
		u32 gpccon, gpcdat, gpcup;
		u32 gpdcon, gpddat, gpdup;
		u32 gpecon, gpedat;*/
		u32 gpioint[3];
		u32 gpioflt[7];
		u32 gpioenint[2];
	} gpio_pdata;
};

#endif
