/*
 * GPIOs and interrupts for Palm Tungsten|T5 Handheld Computer
 *
 * Based on palmld-gpio.h by Alex Osborne
 * 
 * Authors: Marek Vasut <marek.vasut@gmail.com>
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

/*
 *   WORK IN PROGRESS
 */

#ifndef _INCLUDE_PALMTT5_GPIO_H_ 

#define _INCLUDE_PALMTT5_GPIO_H_

/* GPIOs */
#define GPIO_NR_PALMTT5_GPIO_RESET		1

#define GPIO_NR_PALMTT5_POWER_DETECT		90
#define GPIO_NR_PALMTT5_HOTSYNC_BUTTON_N	10
#define GPIO_NR_PALMTT5_EARPHONE_DETECT		107
#define GPIO_NR_PALMTT5_SD_DETECT_N		14

/* KEYPAD */
#define GPIO_NR_PALMTT5_KP_MKIN3		97	/* left, right */
#define GPIO_NR_PALMTT5_KP_MKIN0		100	/* center, power, home */
#define GPIO_NR_PALMTT5_KP_MKIN1		101	/* web, contact, calendar */
#define GPIO_NR_PALMTT5_KP_MKIN2		102	/* up, down */

#define GPIO_NR_PALMTT5_KP_MKOUT0		103	/* up, power, right, calendar */
#define GPIO_NR_PALMTT5_KP_MKOUT1		104	/* home, contact */
#define GPIO_NR_PALMTT5_KP_MKOUT2		105	/* center, down, left, web */

#define GPIO_NR_PALMTT5_KP_MKIN3_MD		(GPIO_NR_PALMTT5_KP_MKIN3 | GPIO_ALT_FN_3_IN)
#define GPIO_NR_PALMTT5_KP_MKIN0_MD		(GPIO_NR_PALMTT5_KP_MKIN0 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMTT5_KP_MKIN1_MD		(GPIO_NR_PALMTT5_KP_MKIN1 | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMTT5_KP_MKIN2_MD		(GPIO_NR_PALMTT5_KP_MKIN2 | GPIO_ALT_FN_1_IN)

#define GPIO_NR_PALMTT5_KP_MKOUT0_MD		(GPIO_NR_PALMTT5_KP_MKOUT0 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMTT5_KP_MKOUT1_MD		(GPIO_NR_PALMTT5_KP_MKOUT1 | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMTT5_KP_MKOUT2_MD		(GPIO_NR_PALMTT5_KP_MKOUT2 | GPIO_ALT_FN_2_OUT)

/* TOUCHSCREEN */
#define GPIO_NR_PALMTT5_WM9712_IRQ		27

/* IRDA */
#define GPIO_NR_PALMTT5_ICP_RXD			46	/* Infrared receive  pin */
#define GPIO_NR_PALMTT5_ICP_TXD			47	/* Infrared transmit pin */
#define GPIO_NR_PALMTT5_IR_DISABLE		40	/* connected to SD pin of tranceiver (TFBS4710?) ? */

#define GPIO_NR_PALMTT5_ICP_RXD_MD		(GPIO_NR_PALMTT5_ICP_RXD | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMTT5_ICP_TXD_MD		(GPIO_NR_PALMTT5_ICP_TXD | GPIO_ALT_FN_2_OUT)

/* USB */
#define GPIO_NR_PALMTT5_USB_DETECT		13
#define GPIO_NR_PALMTT5_USB_POWER		95
#define GPIO_NR_PALMTT5_USB_PULLUP		93

/* LCD/BACKLIGHT */
#define GPIO_NR_PALMTT5_BL_POWER		84
#define GPIO_NR_PALMTT5_LCD_POWER		96

/* BLUETOOTH */
#define GPIO_NR_PALMTT5_BT_POWER		17
#define GPIO_NR_PALMTT5_BT_RXD			42
#define GPIO_NR_PALMTT5_BT_TXD			43
#define GPIO_NR_PALMTT5_BT_CTS			44
#define GPIO_NR_PALMTT5_BT_RTS			45
#define GPIO_NR_PALMTT5_BT_RESET		83

#define GPIO_NR_PALMTT5_BT_RXD_MD		(GPIO_NR_PALMTT5_BT_RXD | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMTT5_BT_TXD_MD		(GPIO_NR_PALMTT5_BT_TXD | GPIO_ALT_FN_2_OUT)
#define GPIO_NR_PALMTT5_BT_UART_CTS_MD		(GPIO_NR_PALMTT5_BT_CTS | GPIO_ALT_FN_1_IN)
#define GPIO_NR_PALMTT5_BT_UART_RTS_MD		(GPIO_NR_PALMTT5_BT_RTS | GPIO_ALT_FN_2_OUT)

/* INTERRUPTS */

#define IRQ_GPIO_PALMTT5_SD_DETECT_N		IRQ_GPIO(GPIO_NR_PALMTT5_SD_DETECT_N)

#define IRQ_GPIO_PALMTT5_WM9712_IRQ		IRQ_GPIO(GPIO_NR_PALMTT5_WM9712_IRQ)

#define IRQ_GPIO_PALMTT5_USB_DETECT		IRQ_GPIO(GPIO_NR_PALMTT5_USB_DETECT)

#define IRQ_GPIO_PALMTT5_GPIO_RESET		IRQ_GPIO(GPIO_NR_PALMTT5_GPIO_RESET)


/* Utility macros */

#define GET_PALMTT5_GPIO(gpio) \
	        (GPLR(GPIO_NR_PALMTT5_ ## gpio) & GPIO_bit(GPIO_NR_PALMTT5_ ## gpio))

#define SET_PALMTT5_GPIO(gpio, setp) \
	do { \
		if (setp) \
		        GPSR(GPIO_NR_PALMTT5_ ## gpio) = GPIO_bit(GPIO_NR_PALMTT5_ ## gpio); \
		else \
		        GPCR(GPIO_NR_PALMTT5_ ## gpio) = GPIO_bit(GPIO_NR_PALMTT5_ ## gpio); \
	} while (0)

#define SET_PALMTT5_GPIO_N(gpio, setp) \
	do { \
		if (setp) \
		        GPCR(GPIO_NR_PALMTT5_ ## gpio) = GPIO_bit(GPIO_NR_PALMTT5_ ## gpio); \
		else \
		        GPSR(GPIO_NR_PALMTT5_ ## gpio) = GPIO_bit(GPIO_NR_PALMTT5_ ## gpio); \
	} while (0)


#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define SET_GPIO(gpio, setp) \
		do { \
		    if (setp) \
			GPSR(gpio) = GPIO_bit(gpio); \
		    else \
			GPCR(gpio) = GPIO_bit(gpio); \
		} while (0)

#endif
