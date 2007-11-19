/*
 * linux/include/asm-arm/arch-s3c2410/rx3000-asic3.h
 *
 * Written by Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#ifndef __ASM_ARCH_RX3000_ASIC3_H
#define __ASM_ARCH_RX3000_ASIC3_H "rx3000-asic3.h"

#include <asm/hardware/ipaq-asic3.h>

/* GPIOA */
#define ASIC3_GPA0			(1 << 0)  /* charger enable, 0 = disable, 1 = enable */
#define ASIC3_GPA1			(1 << 1)  /* audio mute, 0 = mute, 1 = unmute */
#define ASIC3_GPA2			(1 << 2)  /* audio reset, 0 = disable, 1 = enable */
#define ASIC3_GPA3			(1 << 3)  /* usb d+ pullup, 0 = disable, 1 = enable */
#define ASIC3_GPA13			(1 << 13) /* charger mode, 0 = slow, 1 = fast */
#define ASIC3_GPA15			(1 << 15) /* bluetooth clock 32kHz, 0 = disable, 1 = enable */

/* GPIOB */
#define ASIC3_GPB1			(1 << 1)  /* backup battery charger enable, 0 = enable, 1 = disable */
#define ASIC3_GPB2			(1 << 2)  /* rs232 level convertor supply, 0 = off, 1 = on */
#define ASIC3_GPB3			(1 << 3)  /* wlan supply, 0 = off, 1 = on */
#define ASIC3_GPB10			(1 << 10) 
#define ASIC3_GPB11			(1 << 11) /* lcd supply, 0 = off, 1 = on */
#define ASIC3_GPB12			(1 << 12) /* lcd supply, 0 = off, 1 = on */

/* GPIOC */
#define ASIC3_GPC0			(1 << 0)  /* green led */
#define ASIC3_GPC1			(1 << 1)  /* red led */
#define ASIC3_GPC2			(1 << 2)  /* blue led */
#define ASIC3_GPC4			(1 << 4)  /* camera supply, 0 = off, 1 = on */
#define ASIC3_GPC5			(1 << 5)  /* camera supply, 0 = off, 1 = on */
#define ASIC3_GPC7			(1 << 7)  /* audio supply, 0 = off, 1 = on */
#define ASIC3_GPC9			(1 << 9)  /* lcd supply, 0 = off, 1 = on */
#define ASIC3_GPC10			(1 << 10) /* lcd supply, 0 = off, 1 = on */
#define ASIC3_GPC11			(1 << 11) /* wlan supply, 0 = off, 1 = on */
#define ASIC3_GPC12			(1 << 12) /* bluetooth supply, 0 = off, 1 = on */
#define ASIC3_GPC13			(1 << 13) /* wlan supply, 0 = off, 1 = on */

/* GPIOD */
#define ASIC3_GPD0      		(1 << 0)  /* right button */
#define ASIC3_GPD1       		(1 << 1)  /* down button */
#define ASIC3_GPD2	        	(1 << 2)  /* left button */
#define ASIC3_GPD3		        (1 << 3)  /* up button */

/* ASIC3 IRQs */
#define IRQ_ASIC3_EINT0 	        (ASIC3_GPIOD_IRQ_BASE + 0) /* right button */
#define IRQ_ASIC3_EINT1 		(ASIC3_GPIOD_IRQ_BASE + 1) /* down button */
#define IRQ_ASIC3_EINT2			(ASIC3_GPIOD_IRQ_BASE + 2) /* left button */
#define IRQ_ASIC3_EINT3			(ASIC3_GPIOD_IRQ_BASE + 3) /* up button */

/* ASIC3 LEDS */
#define ASIC3_LED0       		(0) /* green led */
#define ASIC3_LED1	                (1) /* red led */
#define ASIC3_LED2              	(2) /* blue led */

#endif  // __ASM_ARCH_RX3000_ASIC3_H
