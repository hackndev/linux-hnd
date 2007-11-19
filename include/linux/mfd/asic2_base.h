/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
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
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*/

#ifndef ASIC2_SHARED_H
#define ASIC2_SHARED_H

struct asic2_platform_data
{
	/* Standard MFD properties */
	int irq_base;
	int gpio_base;
	struct platform_device **child_devs;
	int num_child_devs;
};

enum ASIC_SHARED {
	ASIC_SHARED_CLOCK_EX1 = 1,  /* Bit fields */
	ASIC_SHARED_CLOCK_EX2 = 2
};

struct asic2_data;

extern void asic2_set_gpiod(struct device *dev, unsigned long mask, unsigned long bits);
extern unsigned long asic2_read_gpiod(struct device *dev);
extern unsigned int asic2_irq_base(struct device *dev);
extern void asic2_set_gpint(struct device *dev, unsigned long bit, unsigned int type, unsigned int sense);

extern void asic2_set_pwm(struct device *dev, unsigned long pwm,
			  unsigned long dutytime, unsigned long periodtime,
			  unsigned long timebase);

extern void asic2_write_register(struct device *dev, unsigned int reg, unsigned long value);
extern unsigned long asic2_read_register(struct device *dev, unsigned int reg);

extern void asic2_shared_add(struct device *dev, unsigned long *s, enum ASIC_SHARED v);
extern void asic2_shared_release(struct device *dev, unsigned long *s, enum ASIC_SHARED v);
extern void __asic2_shared_add(struct asic2_data *asic, unsigned long *s, enum ASIC_SHARED v);
extern void __asic2_shared_release(struct asic2_data *asic, unsigned long *s, enum ASIC_SHARED v);
extern void asic2_clock_enable(struct device *dev, unsigned long bit, int on);

#endif
