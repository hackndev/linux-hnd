/*
 * linux/include/asm-arm/arch-s3c2410/clock.h
 *
 * Written by Roman Moravcik <roman.moravcik@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __ASM_ARM_CLOCK_H
#define __ASM_ARM_CLOCK_H

#include <asm/plat-s3c24xx/clock.h>

#define clk_register(clk)	s3c24xx_register_clock(clk)
#define clk_unregister(clk)	s3c24xx_unregister_clock(clk)

#endif /* __ASM_ARM_CLOCK_H */
