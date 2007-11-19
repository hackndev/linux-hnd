/*
 *
 * Definitions for HP iPAQ3 Handheld Computer
 *
 * Copyright 2002 Hewlett-Packard Company.
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
 * Author: Jamey Hicks
 *
 */

#ifndef _INCLUDE_H5400_ASIC_H_ 
#define _INCLUDE_H5400_ASIC_H_

#include <asm/hardware/ipaq-samcop.h>

#define SAMCOP_GPIO_GPA_APPBUTTON1			(1 << 0)
#define SAMCOP_GPIO_GPA_APPBUTTON2			(1 << 1)
#define SAMCOP_GPIO_GPA_APPBUTTON3			(1 << 2)
#define SAMCOP_GPIO_GPA_APPBUTTON4			(1 << 3)
#define SAMCOP_GPIO_GPA_JOYSTICK1			(1 << 4)
#define SAMCOP_GPIO_GPA_JOYSTICK2			(1 << 5)
#define SAMCOP_GPIO_GPA_JOYSTICK3			(1 << 6)
#define SAMCOP_GPIO_GPA_JOYSTICK4			(1 << 7)
#define SAMCOP_GPIO_GPA_JOYSTICK5			(1 << 8)
#define SAMCOP_GPIO_GPA_TOGGLEUP_N			(1 << 9)
#define SAMCOP_GPIO_GPA_TOGGLEDOWN_N			(1 << 10)
#define SAMCOP_GPIO_GPA_RESET_BUTTON_N			(1 << 11)
#define SAMCOP_GPIO_GPA_RECORD_N			(1 << 12)
#define SAMCOP_GPIO_GPA_MCHG_EN				(1 << 13)
#define SAMCOP_GPIO_GPA_USB_DETECT			(1 << 14)
#define SAMCOP_GPIO_GPA_ADP_IN_STATUS			(1 << 15)
#define SAMCOP_GPIO_GPA_SD_DETECT_N			(1 << 16)
#define SAMCOP_GPIO_GPA_OPT_ON_N			(1 << 17)
#define SAMCOP_GPIO_GPA_OPT_RESET			(1 << 18)
#define SAMCOP_GPIO_GPA_UART_CLK			(1 << 19)

#define SAMCOP_GPIO_GPB_CODEC_POWER_ON			(1 << 0)
#define SAMCOP_GPIO_GPB_RF_POWER_ON			(1 << 1)
#define SAMCOP_GPIO_GPB_AUDIO_POWER_ON			(1 << 2)
#define SAMCOP_GPIO_GPB_WLAN_POWER_ON			(1 << 3) /* to 802.11 module */
#define SAMCOP_GPIO_GPB_MQ_POWER_ON			(1 << 4)
#define SAMCOP_GPIO_GPB_BLUETOOTH_3V0_ON		(1 << 5)
#define SAMCOP_GPIO_GPB_BACKLIGHT_POWER_ON		(1 << 6)
#define SAMCOP_GPIO_GPB_LCD_EN				(1 << 7)
#define SAMCOP_GPIO_GPB_CLK16M				(1 << 8) /* output, not connected */
#define SAMCOP_GPIO_GPB_CLK48M				(1 << 9) /* output, not connected */
#define SAMCOP_GPIO_GPB_RF_CTS				(1 << 10) /* input */
#define SAMCOP_GPIO_GPB_RF_RTS				(1 << 11) /* output */
#define SAMCOP_GPIO_GPB_MDL_WAKE			(1 << 12)
#define SAMCOP_GPIO_GPB_RESET_P_DN_N
#define SAMCOP_GPIO_GPB_HOST_WAKE_STATUS_N
#define SAMCOP_GPIO_GPB_MODEM_RESET_STATUS_N

#define SAMCOP_GPIO_GPC_FLASH_VPEN			(1 << 0) /* unused. */
#define SAMCOP_GPIO_GPC_FLASH_WE_N			(1 << 1) /* unused ? */

#define SAMCOP_GPIO_GPD_FCD_DATA0			(1 << 1)
#define SAMCOP_GPIO_GPD_FCD_DATA1			(1 << 3)
#define SAMCOP_GPIO_GPD_FCD_DATA2			(1 << 5)
#define SAMCOP_GPIO_GPD_FCD_DATA3			(1 << 7)
#define SAMCOP_GPIO_GPD_FCD_DATA4			(1 << 9)
#define SAMCOP_GPIO_GPD_FCD_DATA5			(1 << 11)
#define SAMCOP_GPIO_GPD_FCD_DATA6			(1 << 13)
#define SAMCOP_GPIO_GPD_FCD_DATA7			(1 << 15)

#define SAMCOP_GPIO_GPE_FCD_RESET			(1 << 0)
#define SAMCOP_GPIO_GPE_FCD_PCLK			(1 << 1)
#define SAMCOP_GPIO_GPE_FCD_TPE				(1 << 2)

#define H5400_SAMCOP_BASE				0x14000000

#include <linux/device.h>

extern struct platform_device h5400_samcop;
extern void (*h5400_set_samcop_gpio_b)(struct device *dev, u32 mask, u32 bits);
extern void h5400_pxa_ack_muxed_gpio(unsigned int irq);

#endif /* _INCLUDE_H5400_ASIC_H_ */
