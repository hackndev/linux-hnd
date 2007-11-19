/*
 * include/asm/arm/arch-pxa/htcuniversal-asic.h
 *
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see below:
 * 
 * include/asm/arm/arch-pxa/hx4700-asic.h
 *      Copyright (c) 2004 SDG Systems, LLC
 *
 */

#ifndef _HTCUNIVERSAL_ASIC_H_
#define _HTCUNIVERSAL_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/* ASIC3 */

#define HTCUNIVERSAL_ASIC3_GPIO_PHYS	PXA_CS4_PHYS
#define HTCUNIVERSAL_ASIC3_MMC_PHYS	PXA_CS3_PHYS

/* TODO: some information is missing here */

/* ASIC3 GPIO A bank */

#define GPIOA_I2C_EN	 	 	 0	/* Output */
#define GPIOA_SPK_PWR1_ON	 	 1	/* Output */
#define GPIOA_AUDIO_PWR_ON	 	 2	/* Output */
#define GPIOA_EARPHONE_PWR_ON	 	 3	/* Output */

#define GPIOA_UNKNOWN4			 4	/* Output */
#define GPIOA_BUTTON_BACKLIGHT_N	 5	/* Input  */
#define GPIOA_SPK_PWR2_ON	 	 6	/* Output */
#define GPIOA_BUTTON_RECORD_N		 7	/* Input  */

#define GPIOA_BUTTON_CAMERA_N		 8	/* Input  */
#define GPIOA_UNKNOWN9			 9	/* Output */
#define GPIOA_FLASHLIGHT 		10	/* Output */
#define GPIOA_COVER_ROTATE_N 		11	/* Input  */

#define GPIOA_TOUCHSCREEN_N		12	/* Input  */
#define GPIOA_VOL_UP_N  		13	/* Input  */
#define GPIOA_VOL_DOWN_N 		14	/* Input  */
#define GPIOA_LCD_PWR5_ON 		15	/* Output */

/* ASIC3 GPIO B bank */

#define GPIOB_BB_READY			 0	/* Input */
#define GPIOB_CODEC_PDN			 1	/* Output */
#define GPIOB_UNKNOWN2			 2	/* Input  */
#define GPIOB_BB_UNKNOWN3		 3	/* Input  */

#define GPIOB_BT_IRQ			 4	/* Input  */
#define GPIOB_CLAMSHELL_N		 5	/* Input  */
#define GPIOB_LCD_PWR3_ON		 6	/* Output */
#define GPIOB_BB_ALERT			 7	/* Input  */

#define GPIOB_BB_RESET2			 8	/* Output */
#define GPIOB_EARPHONE_N		 9	/* Input  */
#define GPIOB_MICRECORD_N		10	/* Input  */
#define GPIOB_NIGHT_SENSOR		11	/* Input  */

#define GPIOB_UMTS_DCD			12	/* Input  */
#define GPIOB_UNKNOWN13			13	/* Input  */
#define GPIOB_CHARGE_EN 		14	/* Output */
#define GPIOB_USB_PUEN 			15	/* Output */

/* ASIC3 GPIO C bank */

#define GPIOC_LED_BTWIFI  	   	 0	/* Output */
#define GPIOC_LED_RED		   	 1	/* Output */
#define GPIOC_LED_GREEN			 2	/* Output */
#define GPIOC_BOARDID3  		 3	/* Input  */

#define GPIOC_WIFI_IRQ_N  		 4	/* Input  */
#define GPIOC_WIFI_RESET  		 5	/* Output */
#define GPIOC_WIFI_PWR1_ON  		 6	/* Output */
#define GPIOC_BT_RESET  		 7	/* Output */

#define GPIOC_UNKNOWN8  		 8	/* Output */
#define GPIOC_LCD_PWR1_ON		 9	/* Output */
#define GPIOC_LCD_PWR2_ON 		10	/* Output */
#define GPIOC_BOARDID2  		11	/* Input  */

#define GPIOC_BOARDID1			12	/* Input  */
#define GPIOC_BOARDID0  		13	/* Input  */
#define GPIOC_BT_PWR_ON			14	/* Output */
#define GPIOC_CHARGE_ON			15	/* Output */

/* ASIC3 GPIO D bank */

#define GPIOD_KEY_OK_N			 0	/* Input  */
#define GPIOD_KEY_RIGHT_N		 1	/* Input  */
#define GPIOD_KEY_LEFT_N		 2	/* Input  */
#define GPIOD_KEY_DOWN_N		 3	/* Input  */

#define GPIOD_KEY_UP_N  		 4	/* Input  */
#define GPIOD_SDIO_DET			 5	/* Input  */
#define GPIOD_WIFI_PWR2_ON		 6	/* Output */
#define GPIOD_HW_REBOOT			 7	/* Output */

#define GPIOD_BB_RESET1			 8	/* Output */
#define GPIOD_UNKNOWN9			 9	/* Output */
#define GPIOD_VIBRA_PWR_ON		10	/* Output */
#define GPIOD_WIFI_PWR3_ON		11	/* Output */

#define GPIOD_FL_PWR_ON  		12	/* Output */
#define GPIOD_LCD_PWR4_ON		13	/* Output */
#define GPIOD_BL_KEYP_PWR_ON		14	/* Output */
#define GPIOD_BL_KEYB_PWR_ON		15	/* Output */

extern struct platform_device htcuniversal_asic3;

/* ASIC3 GPIO A bank */

#define GPIO_I2C_EN	 	 	0*16+GPIOA_I2C_EN
#define GPIO_SPK_PWR1_ON	 	0*16+GPIOA_SPK_PWR1_ON
#define GPIO_AUDIO_PWR_ON	 	0*16+GPIOA_AUDIO_PWR_ON
#define GPIO_EARPHONE_PWR_ON	 	0*16+GPIOA_EARPHONE_PWR_ON

#define GPIO_UNKNOWNA4			0*16+GPIOA_UNKNOWN4
#define GPIO_BUTTON_BACKLIGHT_N		0*16+GPIOA_BUTTON_BACKLIGHT_N
#define GPIO_SPK_PWR2_ON	 	0*16+GPIOA_SPK_PWR2_ON
#define GPIO_BUTTON_RECORD_N		0*16+GPIOA_BUTTON_RECORD_N

#define GPIO_BUTTON_CAMERA_N		0*16+GPIOA_BUTTON_CAMERA_N
#define GPIO_UNKNOWNA9			0*16+GPIOA_UNKNOWN9
#define GPIO_FLASHLIGHT 		0*16+GPIOA_FLASHLIGHT
#define GPIO_COVER_ROTATE_N 		0*16+GPIOA_COVER_ROTATE_N

#define GPIO_TOUCHSCREEN_N		0*16+GPIOA_TOUCHSCREEN_N
#define GPIO_VOL_UP_N  			0*16+GPIOA_VOL_UP_N
#define GPIO_VOL_DOWN_N 		0*16+GPIOA_VOL_DOWN_N
#define GPIO_LCD_PWR5_ON 		0*16+GPIOA_LCD_PWR5_ON

/* ASIC3 GPIO B bank */			

#define GPIO_BB_READY			1*16+GPIOB_BB_READY
#define GPIO_CODEC_PDN			1*16+GPIOB_CODEC_PDN
#define GPIO_UNKNOWNB2			1*16+GPIOB_UNKNOWN2
#define GPIO_BB_UNKNOWN3		1*16+GPIOB_BB_UNKNOWN3

#define GPIO_BT_IRQ			1*16+GPIOB_BT_IRQ
#define GPIO_CLAMSHELL_N		1*16+GPIOB_CLAMSHELL_N
#define GPIO_LCD_PWR3_ON		1*16+GPIOB_LCD_PWR3_ON
#define GPIO_BB_ALERT			1*16+GPIOB_BB_ALERT

#define GPIO_BB_RESET2			1*16+GPIOB_BB_RESET2
#define GPIO_EARPHONE_N			1*16+GPIOB_EARPHONE_N
#define GPIO_MICRECORD_N		1*16+GPIOB_MICRECORD_N
#define GPIO_NIGHT_SENSOR		1*16+GPIOB_NIGHT_SENSOR

#define GPIO_UMTS_DCD			1*16+GPIOB_UMTS_DCD
#define GPIO_UNKNOWNB13			1*16+GPIOB_UNKNOWN13
#define GPIO_CHARGE_EN 			1*16+GPIOB_CHARGE_EN
#define GPIO_USB_PUEN 			1*16+GPIOB_USB_PUEN

/* ASIC3 GPIO C bank */			

#define GPIO_LED_BTWIFI  	   	2*16+GPIOC_LED_BTWIFI
#define GPIO_LED_RED		   	2*16+GPIOC_LED_RED
#define GPIO_LED_GREEN			2*16+GPIOC_LED_GREEN
#define GPIO_BOARDID3  			2*16+GPIOC_BOARDID3

#define GPIO_WIFI_IRQ_N  		2*16+GPIOC_WIFI_IRQ_N
#define GPIO_WIFI_RESET  		2*16+GPIOC_WIFI_RESET
#define GPIO_WIFI_PWR1_ON  		2*16+GPIOC_WIFI_PWR1_ON
#define GPIO_BT_RESET  			2*16+GPIOC_BT_RESET

#define GPIO_UNKNOWNC8  		2*16+GPIOC_UNKNOWN8
#define GPIO_LCD_PWR1_ON		2*16+GPIOC_LCD_PWR1_ON
#define GPIO_LCD_PWR2_ON 		2*16+GPIOC_LCD_PWR2_ON
#define GPIO_BOARDID2  			2*16+GPIOC_BOARDID2

#define GPIO_BOARDID1			2*16+GPIOC_BOARDID1
#define GPIO_BOARDID0  			2*16+GPIOC_BOARDID0
#define GPIO_BT_PWR_ON			2*16+GPIOC_BT_PWR_ON
#define GPIO_CHARGE_ON			2*16+GPIOC_CHARGE_ON

/* ASIC3 GPIO D bank */			

#define GPIO_KEY_OK_N			3*16+GPIOD_KEY_OK_N
#define GPIO_KEY_RIGHT_N		3*16+GPIOD_KEY_RIGHT_N
#define GPIO_KEY_LEFT_N			3*16+GPIOD_KEY_LEFT_N
#define GPIO_KEY_DOWN_N			3*16+GPIOD_KEY_DOWN_N

#define GPIO_KEY_UP_N  			3*16+GPIOD_KEY_UP_N
#define GPIO_SDIO_DET			3*16+GPIOD_SDIO_DET
#define GPIO_WIFI_PWR2_ON		3*16+GPIOD_WIFI_PWR2_ON
#define GPIO_HW_REBOOT			3*16+GPIOD_HW_REBOOT

#define GPIO_BB_RESET1			3*16+GPIOD_BB_RESET1
#define GPIO_UNKNOWND9			3*16+GPIOD_UNKNOWN9
#define GPIO_VIBRA_PWR_ON		3*16+GPIOD_VIBRA_PWR_ON
#define GPIO_WIFI_PWR3_ON		3*16+GPIOD_WIFI_PWR3_ON

#define GPIO_FL_PWR_ON  		3*16+GPIOD_FL_PWR_ON
#define GPIO_LCD_PWR4_ON		3*16+GPIOD_LCD_PWR4_ON
#define GPIO_BL_KEYP_PWR_ON		3*16+GPIOD_BL_KEYP_PWR_ON
#define GPIO_BL_KEYB_PWR_ON		3*16+GPIOD_BL_KEYB_PWR_ON

#define HTCUNIVERSAL_EGPIO_BASE	PXA_CS2_PHYS+0x02000000

#define EGPIO4_ON			 4  /* something */
#define EGPIO5_BT_3V3_ON		 5  /* Bluetooth related */
#define EGPIO6_WIFI_ON			 6  /* WLAN related*/

extern void htcuniversal_egpio_enable( u_int16_t bits );
extern void htcuniversal_egpio_disable( u_int16_t bits );

#endif /* _HTCUNIVERSAL_ASIC_H_ */

