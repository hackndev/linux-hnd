/*
 *  * include/asm-arm/arch-pxa/palmz72-gpio.h
 *   *
 *    * Authors: Alex Osborne <bobofdoom@gmail.com>
 *     *	 Jan Herman <2hp@seznam.cz>
 *      */


#ifndef _PALMZ72_GPIO_H_
#define _PALMZ72_GPIO_H_

#include <asm/arch/pxa-regs.h>

/* Keypad */

#define GPIO_NR_PALMZ72_KP_MKIN0		100 	/* center, camera, power */
#define GPIO_NR_PALMZ72_KP_MKIN1		101 	/* calendar, contacts, music */
#define GPIO_NR_PALMZ72_KP_MKIN2		102 	/* up, down,  */
#define GPIO_NR_PALMZ72_KP_MKIN3		97 	/* left, right,  */
#define GPIO_NR_PALMZ72_KP_DKIN7		13      /* Voice button */


#define GPIO_NR_PALMZ72_KP_MKOUT0	103  // up, right, calendar, power
#define GPIO_NR_PALMZ72_KP_MKOUT1	104  // contacts, camera, 
#define GPIO_NR_PALMZ72_KP_MKOUT2	105  // down, left, center, music, 

#define GPIO_NR_PALMZ72_KP_MKIN0_MD 	(GPIO_NR_PALMZ72_KP_MKIN0 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_KP_MKIN1_MD 	(GPIO_NR_PALMZ72_KP_MKIN1 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_KP_MKIN2_MD 	(GPIO_NR_PALMZ72_KP_MKIN2 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_KP_MKIN3_MD 	(GPIO_NR_PALMZ72_KP_MKIN3 | GPIO_ALT_FN_3_IN)

#define GPIO_NR_PALMZ72_KP_MKOUT0_MD (GPIO_NR_PALMZ72_KP_MKOUT0 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMZ72_KP_MKOUT1_MD (GPIO_NR_PALMZ72_KP_MKOUT1 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMZ72_KP_MKOUT2_MD (GPIO_NR_PALMZ72_KP_MKOUT2 | GPIO_ALT_FN_2_OUT)


/* LED */

#define GPIO_NR_PALMZ72_LED		88
#define GPIO_NR_PALMZ72_LED_MD		(GPIO_NR_PALMZ72_LED | GPIO_ALT_FN_1_OUT)


/* IRDA */

#define GPIO_NR_PALMZ72_IR_SD				49
#define GPIO_NR_PALMZ72_ICP_RXD			46	
#define GPIO_NR_PALMZ72_ICP_TXD			47

#define GPIO_NR_PALMZ72_ICP_RXD_MD	(GPIO_NR_PALMZ72_ICP_RXD | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_ICP_TXD_MD	(GPIO_NR_PALMZ72_ICP_TXD | GPIO_ALT_FN_2_OUT)

/* Bluetooth */

#define GPIO_NR_PALMZ72_BT_RXD				42
#define GPIO_NR_PALMZ72_BT_TXD				43
#define GPIO_NR_PALMZ72_BT_CTS				44
#define GPIO_NR_PALMZ72_BT_RTS				45
#define GPIO_NR_PALMZ72_BT_RESET			83
#define GPIO_NR_PALMZ72_BT_POWER			17 

#define GPIO_NR_PALMZ72_BT_RXD_MD			(GPIO_NR_PALMZ72_BT_RXD | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_BT_TXD_MD			(GPIO_NR_PALMZ72_BT_TXD | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMZ72_BT_UART_CTS_MD			(GPIO_NR_PALMZ72_BT_CTS | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMZ72_BT_UART_RTS_MD			(GPIO_NR_PALMZ72_BT_RTS | GPIO_ALT_FN_2_OUT)


/* Wolfson WM9712 */

#define GPIO_NR_PALMZ72_WM9712_IRQ		27

/* USB */

#define GPIO_NR_PALMZ72_USB_DETECT               15
#define GPIO_NR_PALMZ72_USB_POWER                95 
#define GPIO_NR_PALMZ72_USB_PULLUP               12
#define IRQ_GPIO_PALMZ72_USB_DETECT              IRQ_GPIO(GPIO_NR_PALMZ72_USB_DETECT)

/* Power */

#define GPIO_NR_PALMZ72_POWER_DETECT		0 //battery low level indicator??
#define GPIO_NR_PALMZ72_LCD_POWER		96

/* SD/MMC */

#define GPIO_NR_PALMZ72_SD_DETECT_N		14
#define IRQ_GPIO_PALMZ72_SD_DETECT_N		IRQ_GPIO(GPIO_NR_PALMZ72_SD_DETECT_N)		

/* Backlight */

#define GPIO_NR_PALMZ72_BL_POWER		20

/* Others */

#define GPIO_NR_PALMZ72_GPIO_RESET		1
#define IRQ_GPIO_PALMZ72_GPIO_RESET 		IRQ_GPIO(GPIO_NR_PALMZ72_GPIO_RESET)

#define GPIO_NR_PALMZ72_SCREEN			19  //screen on when this GPIO is 1


/* Utility macros */

#define GET_PALMZ72_GPIO(gpio) \
	        (GPLR(GPIO_NR_PALMZ72_ ## gpio) & GPIO_bit(GPIO_NR_PALMZ72_ ## gpio))

#define SET_PALMZ72_GPIO(gpio, setp) \
	do { \
		if (setp) \
		        GPSR(GPIO_NR_PALMZ72_ ## gpio) = GPIO_bit(GPIO_NR_PALMZ72_ ## gpio); \
		else \
		        GPCR(GPIO_NR_PALMZ72_ ## gpio) = GPIO_bit(GPIO_NR_PALMZ72_ ## gpio); \
	} while (0)

#define SET_PALMZ72_GPIO_N(gpio, setp) \
	do { \
		if (setp) \
		        GPCR(GPIO_NR_PALMZ72_ ## gpio) = GPIO_bit(GPIO_NR_PALMZ72_ ## gpio); \
		else \
		        GPSR(GPIO_NR_PALMZ72_ ## gpio) = GPIO_bit(GPIO_NR_PALMZ72_ ## gpio); \
	} while (0)


#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define IRQ_GPIO_PALMZ72_WM9712_IRQ	IRQ_GPIO(GPIO_NR_PALMZ72_WM9712_IRQ)

#define SET_GPIO(gpio, setp) \
		do { \
		  if (setp) \
		    GPSR(gpio) = GPIO_bit(gpio); \
		  else \
		    GPCR(gpio) = GPIO_bit(gpio); \
		} while (0)

#endif
/* _PALMZ72_GPIO_H_ */
