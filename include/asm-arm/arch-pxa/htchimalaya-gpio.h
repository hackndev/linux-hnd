/*
 *
 * Definitions for HTC Himalaya
 *
 * Copyright 2004 Xanadux.org
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author: w4xy@xanadux.org
 *
 */

#ifndef _HIMALAYA_GPIO_H_
#define _HIMALAYA_GPIO_H_

#define GPIO_NR_HIMALAYA_POWER_BUTTON_N	(0)   /* Also known as the "off button" - INPUT */
#define GPIO_NR_HIMALAYA_WARM_RST_N	(1)   /* external circuit to differentiate warm/cold resets - INPUT*/

#define GPIO_NR_HIMALAYA_USB_DETECT_N	(3)
#define GPIO_NR_HIMALAYA_RS232_IN	(4)
#define GPIO_NR_HIMALAYA_PEN		(5)
#define GPIO_NR_HIMALAYA_BUTTON		(7)
#define GPIO_NR_HIMALAYA_ASIC3_IRQ	(8)
#define GPIO_NR_HIMALAYA_SD_IRQ_N 	 11
#define GPIO_NR_HIMALAYA_UART_POWER     (21)
#define GPIO_NR_HIMALAYA_USB_PULLUP_N	(60)
#define GPIO_NR_HIMALAYA_CHARGER_EN	(63)

#define IRQ_NR_HIMALAYA_USB_DETECT_N	IRQ_GPIO(GPIO_NR_HIMALAYA_USB_DETECT_N)
#define IRQ_NR_HIMALAYA_RS232_IN	IRQ_GPIO(GPIO_NR_HIMALAYA_RS232_IN)
#define IRQ_NR_HIMALAYA_PEN		IRQ_GPIO(GPIO_NR_HIMALAYA_PEN)
#define IRQ_NR_HIMALAYA_BUTTON		IRQ_GPIO(GPIO_NR_HIMALAYA_BUTTON)
#define IRQ_NR_HIMALAYA_ASIC3		IRQ_GPIO(GPIO_NR_HIMALAYA_ASIC3_IRQ)

/* ATI Imageon 3220 Graphics */
#define HIMALAYA_ATI_W3220_PHYS	PXA_CS2_PHYS

#endif /* _HIMALAYA_GPIO_H_ */
