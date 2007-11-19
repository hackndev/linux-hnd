/*
 *
 * Definitions for H3900 Handheld Computer
 *
 * Copyright 2003 Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian
 *
 */

#ifndef _INCLUDE_H3900_ASIC_H_ 
#define _INCLUDE_H3900_ASIC_H_

#include <linux/mfd/asic3_base.h>

#define H3900_ASIC2_PHYS	(PXA_CS5_PHYS + 0x1000000)
#define H3900_ASIC3_PHYS	(PXA_CS5_PHYS + 0x800000)
#define H3900_ASIC3_SD_PHYS	PXA_CS5_PHYS

#define H3900_ASIC3_IRQ_BASE IRQ_BOARD_START
#define H3900_ASIC2_IRQ_BASE (H3900_ASIC3_IRQ_BASE + ASIC3_NR_IRQS)

/* these gpio's are on GPIO_B */

#define GPIO3_IR_ON_N          (1 << 0)   /* Apply power to the IR Module */
#define GPIO3_LCD_9V_ON        (1 << 1)
#define GPIO3_RS232_ON         (1 << 2)   /* Turn on power to the RS232 chip ? */
#define GPIO3_LCD_NV_ON        (1 << 3)
#define GPIO3_CH_TIMER         (1 << 4)   /* Charger */
#define GPIO3_LCD_5V_ON        (1 << 5)   /* Enables LCD_5V */
#define GPIO3_LCD_ON           (1 << 6)   /* Enables LCD_3V */
#define GPIO3_LCD_PCI          (1 << 7)   /* Connects to PDWN on LCD controller */
#define GPIO3_TEST_POINT_170   (1 << 8)
#define GPIO3_CIR_CTL_PWR_ON   (1 << 9)
#define GPIO3_AUD_RESET        (1 << 10)
#define GPIO3_BT_PWR_ON        (1 << 11)  /* Bluetooth power on */
#define GPIO3_SPK_ON           (1 << 12)  /* Built-in speaker on */
#define GPIO3_FL_PWR_ON        (1 << 13)  /* Frontlight power on */
#define GPIO3_AUD_PWR_ON       (1 << 14)  /* All audio power */
#define GPIO3_TEST_POINT_123   (1 << 15)

#define ASIC3GPIO_INIT_DIR		0xFFFF				// initial status, sleep direction
#define ASIC3GPIO_INIT_OUT		0x8200				// Strain 2001.12.15
#define ASIC3GPIO_BATFALT_OUT		0x8001
#define ASIC3GPIO_SLEEP_OUT		0x8001
#define ASIC3CLOCK_INIT			0x0

#ifndef _ASM_ONLY
#include <linux/device.h>

extern struct platform_device h3900_asic2;
extern struct platform_device h3900_asic3;
#endif

#endif  // _INCLUDE_H3900_ASIC_H_ 
