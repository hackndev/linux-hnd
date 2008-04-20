/*
 * GPIOs and interrupts for Palm Tungsten|T3 Handheld Computer
 *
 * Copied from on palmt5-gpio.h by Marek Vasut and T|T3 files
 * 
 * Authors: Tomas Cech <Tomas.Cech@matfyz.cz>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * this code is in a very early stage:
 * - use it at your own risk
 * - any help is encouraged and will be highly appreciated
 *
 */

#ifndef _INCLUDE_PALMTT3_GPIO_H_ 

#define _INCLUDE_PALMTT3_GPIO_H_
#include <asm/arch/pxa-regs.h>
#include <asm/arch/gpio.h>


#define T3_TPS65010_GPIO 14
#define PALMTT3_GPIO_TSC2101_SS	(24)
#define PALMTT3_GPIO_PENDOWN 	(37)
#define PALMTT3_IRQ_GPIO_PENDOWN     IRQ_GPIO(PALMTT3_GPIO_PENDOWN)

#define GPIO_NR_PALMTT3_IR_DISABLE		36


#define GPIO_NR_PALMTT3_USB_POWER		53
#define GPIO_NR_PALMTT3_PUC_USB_POWER		85
#define GPIO_NR_PALMTT3_USB_DETECT		9
#define GPIO_PALMTT3_HOTSYNC_BUTTON		12

#define GPIO_NR_PALMTT3_TPS65010_BT_POWER	4
#define GPIO_NR_PALMTT3_BT_WAKEUP		15

#define GPIO_PALMTT3_MKIN0		19
#define GPIO_PALMTT3_MKIN1		20
#define GPIO_PALMTT3_MKIN2		21
#define GPIO_PALMTT3_MKIN3		22
#define GPIO_PALMTT3_MKIN4		33

#define GPIO_PALMTT3_MKOUT0		0
#define GPIO_PALMTT3_MKOUT1		10
#define GPIO_PALMTT3_MKOUT2		11


#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define SET_GPIO(gpio, setp) \
		do { \
		  if (setp) \
		    GPSR(gpio) = GPIO_bit(gpio); \
		  else \
		    GPCR(gpio) = GPIO_bit(gpio); \
		} while (0)

#endif
