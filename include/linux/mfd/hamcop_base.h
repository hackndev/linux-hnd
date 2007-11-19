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
 * 2005-02-12   Matthew Reimer     GPIO and IRQ support for iPAQ H2200
 *
 */

#ifndef HAMCOP_BASE_H
#define HAMCOP_BASE_H

struct hamcop_platform_data
{
	/* Standard MFD properties */
	int irq_base;
	int gpio_base;
	struct platform_device **child_devs;
	int num_child_devs;

        u16 clocksleep;
        u16 pllcontrol;
};

extern void hamcop_set_gpio_a (struct device *dev, u32 mask, u16 bits);
extern void hamcop_set_gpio_b (struct device *dev, u32 mask, u16 bits);
extern void hamcop_set_gpio_c (struct device *dev, u32 mask, u16 bits);
extern void hamcop_set_gpio_d (struct device *dev, u32 mask, u16 bits);

extern u16 hamcop_get_gpio_a (struct device *dev);
extern u16 hamcop_get_gpio_b (struct device *dev);
extern u16 hamcop_get_gpio_c (struct device *dev);
extern u16 hamcop_get_gpio_d (struct device *dev);

extern void hamcop_set_gpio_a_con (struct device *dev, u16 mask, u16 bits);
extern u16  hamcop_get_gpio_a_con (struct device *dev);
extern void hamcop_set_gpio_a_int (struct device *dev, u32 mask, u32 bits);
extern u32  hamcop_get_gpio_a_int(struct device *dev);
extern void hamcop_set_gpio_a_flt (struct device *dev, int gpa_n, u16 ctrl, u16 width);

extern void hamcop_set_gpio_b_con (struct device *dev, u16 mask, u16 bits);
extern u16  hamcop_get_gpio_b_con (struct device *dev);
extern void hamcop_set_gpio_b_int (struct device *dev, u32 mask, u32 bits);
extern u32  hamcop_get_gpio_b_int(struct device *dev);
extern void hamcop_set_gpio_b_flt (struct device *dev, int gpa_n, u16 ctrl, u16 width);

extern u16  hamcop_get_gpiomon (struct device *dev);

extern void hamcop_set_spucr (struct device *dev, u16 mask, u16 bits);

unsigned int hamcop_irq_base (struct device *dev);

u8 * hamcop_get_bootloader(struct device *dev);
u8 * hamcop_sram(struct device *dev);

void hamcop_set_ifmode(struct device *dev, u16 mode);

void hamcop_set_led(struct device *dev, int led_num, int duty_time, int cycle_time);

#endif
