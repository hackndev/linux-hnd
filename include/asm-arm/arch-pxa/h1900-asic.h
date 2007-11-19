/*
 *
 * Definitions for H1900 Handheld Computer
 *
 * Copyright Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 */

#ifndef _INCLUDE_H1900_ASIC_H_ 
#define _INCLUDE_H1900_ASIC_H_

#ifdef CONFIG_ARCH_H1900

#include <asm/hardware/ipaq-asic3.h>

#define GPIO3_H1900_SPEAKER_POWER	(1 << 3)
#define GPIO3_H1900_BACKLIGHT_POWER	(1 << 4)
#define GPIO3_H1900_LCD_5V		(1 << 5)
#define GPIO3_H1900_LCD_3V		(1 << 6)
#define GPIO3_H1900_CODEC_AUDIO_POWER	(1 << 7)
#define GPIO3_H1900_LCD_PCI		(1 << 8)

#define GPIO4_H1900_USB			(1 << 0)
#define GPIO4_H1900_BUTTON_RECORD	(1 << 2)
#define GPIO4_H1900_BUTTON_HOME		(1 << 3)
#define GPIO4_H1900_BUTTON_MAIL		(1 << 4)
#define GPIO4_H1900_BUTTON_CONTACTS	(1 << 5)
#define GPIO4_H1900_BUTTON_CALENDAR	(1 << 6)
#define GPIO4_H1900_RS232_POWER		(1 << 8)
#define GPIO4_H1900_MUX_SEL0		(1 << 9)
#define GPIO4_H1900_MUX_SEL1		(1 << 10)

/* MUX_SEL [0-1]: */
/* 0 -> VS_MBAT */
/* 1 -> IS_MBAT */
/* 2 -> BACKUP_BAT */
/* 3 -> TEMP_AD */

/* ASIC3 IRQs */
#define H1900_RECORD_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + 2)
#define H1900_HOME_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + 3)
#define H1900_MAIL_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + 4)
#define H1900_CONTACTS_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + 5)
#define H1900_CALENDAR_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + 6)

#define H1900_RED_LED         1
#define H1900_GREEN_LED       2
#define H1900_YELLOW_LED      3

#endif  // CONFIG_ARCH_H1900

#endif  // _INCLUDE_H1900_ASIC_H_ 
