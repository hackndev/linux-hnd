/*
 *
 * Definitions for HTC Himalaya
 *
 *
 */

#ifndef _INCLUDE_HIMALAYA_ASIC_H_ 
#define _INCLUDE_HIMALAYA_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

#define HIMALAYA_ASIC3_MMC_PHYS		0x0c800000
#define HIMALAYA_ASIC3_GPIO_PHYS	0x0d800000

/* these gpio's are on GPIO_A */
#define GPIOA_USB_PUEN		 3
#define GPIO_USB_PUEN		 0*16+3

#define GPIOA_MIC_PWR_ON	 9
#define GPIO_MIC_PWR_ON	 	 0*16+9

/* these gpio's are on GPIO_B */

#define GPIOB_GSM_POWER		 0   /*  */
#define GPIO_GSM_POWER		 1*16+0   /*  */
#define GPIOB_GSM_RESET		 1   /*  */
#define GPIO_GSM_RESET		 1*16+1   /*  */
                                     /* backpack */
#define GPIOB_LCD_PWR1_ON	 3   /*  */
#define GPIO_LCD_PWR1_ON	 1*16+3   /*  */
#define GPIOB_PHONEL_PWR_ON      4   /* Phone buttons backlight power on */
#define GPIO_PHONEL_PWR_ON       1*16+4   /* Phone buttons backlight power on */
	                             /* IR/serial */
#define GPIOB_LCD_PWR2_ON	 7   /*  */
#define GPIO_LCD_PWR2_ON	 1*16+7   /*  */
#define GPIOB_LCD_PWR3_ON	 8   /*  */
#define GPIO_LCD_PWR3_ON	 1*16+8   /*  */
                                     /* backpack */
#define GPIOB_FL_PWR_ON        	10   /* Frontlight power on */
#define GPIO_FL_PWR_ON        	1*16+10   /* Frontlight power on */
#define GPIOB_LCD_PWR4_ON	11   /*  */
#define GPIO_LCD_PWR4_ON	1*16+11   /*  */

#define GPIOB_SPK_PWR_ON	13   /* Built-in speaker on */
#define GPIO_SPK_PWR_ON		1*16+13   /* Built-in speaker on */
#define GPIOB_VIBRA_PWR_ON	14
#define GPIO_VIBRA_PWR_ON	1*16+14
#define GPIOB_TEST_POINT_123	15
#define GPIO_TEST_POINT_123	1*16+15

/* these gpio's are on GPIO_C */

#define GPIOC_BOARDID0		 3
#define GPIO_BOARDID0		 2*16+3
#define GPIOC_BOARDID1		 4
#define GPIO_BOARDID1		 2*16+4
#define GPIOC_BOARDID2		 5
#define GPIO_BOARDID2		 2*16+5

#define GPIOC_BTN_VOL_UP_N	14
#define GPIO_BTN_VOL_UP_N	2*16+14
#define GPIOC_BTN_VOL_DOWN_N	15
#define GPIO_BTN_VOL_DOWN_N	2*16+15

/* these gpio's are on GPIO_D */

#define GPIOD_BTN_RECORD_N	 0
#define GPIO_BTN_RECORD_N	 3*16+0
#define GPIOD_BTN_CAMERA_N	 1
#define GPIO_BTN_CAMERA_N	 3*16+1

#define GPIOD_OWM_IRQ		 3
#define GPIO_OWM_IRQ		 3*16+3
#define GPIOD_GSM_IRQ		 4
#define GPIO_GSM_IRQ		 3*16+4

#define	GPIOD_SD_WRITE_PROTECT	 9
#define	GPIO_SD_WRITE_PROTECT	 3*16+9

#define GPIOD_SD_CARD_SENSE	13
#define GPIO_SD_CARD_SENSE	3*16+13
#define GPIOD_BT_IRQ		15
#define GPIO_BT_IRQ		3*16+15

extern struct device *asic3_dev;

#endif  // _INCLUDE_HIMALAYA_ASIC_H_
