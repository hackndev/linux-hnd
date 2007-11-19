/*
 * include/asm-arm/arch-pxa/palmte2-gpio.h
 *
 * Author: Carlos Eduardo Medaglia Dyonisio <cadu@nerdfeliz.com>
 *
 */


#ifndef _PALMTE2_GPIO_H_
#define _PALMTE2_GPIO_H_

/* Keypad */

#define GPIO_NR_PALMTE2_KP_DKIN0		5	/* Notes */
#define GPIO_NR_PALMTE2_KP_DKIN1		7	/* Tasks */
#define GPIO_NR_PALMTE2_KP_DKIN2		11	/* Calendar */
#define GPIO_NR_PALMTE2_KP_DKIN3		13	/* Contacts */
#define GPIO_NR_PALMTE2_KP_DKIN4		14	/* Center */
#define GPIO_NR_PALMTE2_KP_DKIN5		19	/* Left */
#define GPIO_NR_PALMTE2_KP_DKIN6		20	/* Right */
#define GPIO_NR_PALMTE2_KP_DKIN7		21	/* Down */
#define GPIO_NR_PALMTE2_KP_DKIN8		22	/* Up */

/* Bluetooth */

#define GPIO_NR_PALMTE2_BT_RXD			42
#define GPIO_NR_PALMTE2_BT_TXD			43
#define GPIO_NR_PALMTE2_BT_CTS			44
#define GPIO_NR_PALMTE2_BT_RTS			45
#define GPIO_NR_PALMTE2_BT_RESET		80
#define GPIO_NR_PALMTE2_BT_POWER		32

/* GPIO */

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define SET_GPIO(gpio, setp) \
		do { \
		  if (setp) \
		    GPSR(gpio) = GPIO_bit(gpio); \
		  else \
		    GPCR(gpio) = GPIO_bit(gpio); \
		} while (0)

#define SET_PALMTE2_GPIO(gpio, setp) \
	do { \
		if (setp) \
		        GPSR(GPIO_NR_PALMTE2_ ## gpio) = GPIO_bit(GPIO_NR_PALMTE2_ ## gpio); \
		else \
		        GPCR(GPIO_NR_PALMTE2_ ## gpio) = GPIO_bit(GPIO_NR_PALMTE2_ ## gpio); \
	} while (0)

#endif /* _PALMTE2_GPIO_H_ */
