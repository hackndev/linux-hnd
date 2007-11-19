/*
 *
 * Definitions for HTC Blueangel
 *
 * Copyright 2004 Xanadux.org
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author: w4xy@xanadux.org
 *
 * Heavily based on h3900_asic.h
 *
 */

#ifndef _INCLUDE_BLUEANGEL_ASIC_H_ 
#define _INCLUDE_BLUEANGEL_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/****************************************************/
/*
 * This ASIC is at CS3# + 0x01800000
 */

/* Bank A */
#define GPIOA_BT_PWR1_ON	 0
#define GPIOA_BT_IRQ		 1	/* Input  */

#define GPIOA_WLAN_PWR1          3
#define GPIOA_WLAN_RESET         4
#define GPIOA_WLAN_PWR2          5
#define GPIOA_EXT_MIC_PWR_ON	 6
#define GPIOA_MIC_PWR_ON	 7
#define GPIOA_SND_A8		 8
#define GPIOA_SND_A9		 9
#define GPIOA_EARPHONE_PWR_ON	10	/* Output */
#define GPIOA_PWM0		11	/* ALT */
#define GPIOA_PWM1		12	/* ALT */

#define GPIOA_CONTROL_CX_ON	15

/* Bank B */

#define GPIOB_EARPHONE_N	 0	/* Input  */

#define GPIOB_LCD_PWR6_ON	 2
#define GPIOB_LCD_PWR1_ON	 3
#define GPIOB_LCD_PWR2_ON	 4
#define GPIOB_LCD_PWR3_ON	 5
#define GPIOB_FL_PWR_ON        	 6  /* Frontlight power on */
#define GPIOB_PHONEL_PWR_ON	 7
#define GPIOB_CAM_PWR_ON	 8
#define GPIOB_VIBRA_PWR_ON	 9
#define GPIOB_CHARGER_EN 	10

#define GPIOB_BT_PWR2_ON	12
#define GPIOB_WLAN_PWR3         13

#define GPIOB_SPK_PWR_ON	15

/* Bank C */

#define GPIOC_LED_RED 		 0	/* ALT */
#define GPIOC_LED_GREEN 	 1	/* ALT */
#define GPIOC_LED2 		 2	/* ALT, unused */
#define GPIOC_SPI_RXD 		 3	/* ALT */
#define GPIOC_SPI_TXD 		 4	/* ALT */
#define GPIOC_SPI_CLK 		 5	/* ALT */
#define GPIOC_SPI_CS 		 6	/*  */
#define GPIOC_CAM_RESET 	 7	/*  */

#define GPIOC_USB_PUEN 		10
#define GPIOC_CHARGER_MODE      11
#define GPIOC_HEADPHONE_IN 	12
#define GPIOC_KEYBL_PWR_ON	13
#define GPIOC_LCD_PWR4_ON	14
#define GPIOC_LCD_PWR5_ON	15

/* Bank D */
#define GPIOD_HEADSET_DETECT	 0
#define GPIOD_BTN_HEADSET_N	 1
#define GPIOD_OWM_IRQ		 2
#define GPIOD_AC_CHARGER_N	 3
#define GPIOD_QKBD_IRQ_N	 4
#define GPIOD_BTN_WWW_N		 5
#define GPIOD_KBD_RESET		 6
#define GPIOD_BTN_OK_N		 7
#define GPIOD_BTN_WINDOWS_N	 8
#define GPIOD_BTN_RECORD_N	 9
#define GPIOD_BTN_CAMERA_N	10
#define GPIOD_BTN_VOL_UP_N	11
#define GPIOD_BTN_VOL_DOWN_N	12
#define GPIOD_SD_CARD_SENSE	13
#define GPIOD_SD_WRITE_PROTECT	14
#define GPIOD_BTN_MAIL_N	15

#define BLUEANGEL_OWM_IRQ		(ASIC3_GPIOD_IRQ_BASE + GPIOD_OWM_IRQ)
#define BLUEANGEL_HEADSET_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_HEADSET_N)
#define BLUEANGEL_QKBD_IRQ		(ASIC3_GPIOD_IRQ_BASE + GPIOD_QKBD_IRQ_N)
#define BLUEANGEL_WWW_BTN_IRQ		(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_WWW_N)
#define BLUEANGEL_OK_BTN_IRQ		(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_OK_N)
#define BLUEANGEL_WINDOWS_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_WINDOWS_N)
#define BLUEANGEL_RECORD_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_RECORD_N)
#define BLUEANGEL_CAMERA_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_CAMERA_N)
#define BLUEANGEL_VOL_UP_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_VOL_UP_N)
#define BLUEANGEL_VOL_DOWN_BTN_IRQ	(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_VOL_DOWN_N)
#define BLUEANGEL_MAIL_BTN_IRQ		(ASIC3_GPIOD_IRQ_BASE + GPIOD_BTN_MAIL_N)

extern struct platform_device blueangel_asic3;

#endif  // _INCLUDE_BLUEANGEL_ASIC_H_ 
