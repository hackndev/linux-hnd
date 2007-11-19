/*
 *
 * Definitions for H3600 Handheld Computer
 *
 * Copyright 2000 Compaq Computer Corporation.
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
 */

#include <asm/arch/ipaqsa.h>

/* GPIO[2:9] used by LCD on H3600, used as GPIO on H3100 */
#define GPIO_H3100_BT_ON		GPIO_GPIO (2)
#define GPIO_H3100_GPIO3		GPIO_GPIO (3)
#define GPIO_H3100_QMUTE		GPIO_GPIO (4)
#define GPIO_H3100_LCD_3V_ON		GPIO_GPIO (5)
#define GPIO_H3100_AUD_ON		GPIO_GPIO (6)
#define GPIO_H3100_AUD_PWR_ON		GPIO_GPIO (7)
#define GPIO_H3100_IR_ON		GPIO_GPIO (8)
#define GPIO_H3100_IR_FSEL		GPIO_GPIO (9)
