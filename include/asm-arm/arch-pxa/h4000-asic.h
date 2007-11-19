/*
 * Definitions for H4000 Handheld Computer
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Changelog:
 * 	may 2004: initial version,  shawn anderson <sa at xmission.com>
 *
 * Work in progress... please help if you can :-), thx
 */

#ifndef _INCLUDE_H4000_ASIC_H_ 
#define _INCLUDE_H4000_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/* H4000, ASIC3 at CS3# 0x0c000000 */
#define H4000_ASIC3_PHYS PXA_CS3_PHYS
#define H4000_ASIC3_SD_PHYS 0x10000000

/* ASIC3 GPIO_A */

#define GPIOA_WLAN_LED_ON        (1<<4)
#define GPIOA_BT_LED_ON          (1<<5)
#define GPIOA_RS232_ON           6

#define GPIOA_FLASH_IRQ_N        (1<<7)

/* ASIC3 GPIO_B */

#define GPIOB_MICRO_3V3_EN       (1<<0)  /* keyboard microcontroller power on ? ; Output */
#define GPIOB_KEYBOARD_IRQ       (1<<1)  /* Input */
#define GPIOB_KEYBOARD_WAKE_UP   (1<<2)  /* Output */
// GPIOB 3 not connected
#define GPIOB_MUX_SEL2           (1<<4)
#define GPIOB_BT_WAKE_UP         (1<<5)
// GPIOB 5 is not connected
#define GPIOB_FLASH_LOCK_N       (1<<6)
#define GPIOB_ECHGING            (1<<7)  /* cradle connector pin 16 */
#define GPIOB_STATUS_CHANGE_N    (1<<8)  /* from WLAN MAC */
#define GPIOB_WLAN_SOMETHING     9  /* from WLAN MAC */
// GPIOB 9 ?
#define GPIOB_BACKLIGHT_POWER_ON (1<<10) /* Lcd Backlight */
#define GPIOB_LCD_PCI            (1<<11) /* enables LCD -6.5V */
#define GPIOB_LCD_ON             (1<<12) /* enables LCD 9V */
#define GPIOB_ELP_ACK            (1<<13) /* from WLAN MAC ; Input */
#define GPIOB_ELP_REQ            (1<<14) /* to WLAN MAC; Input */
// GPIOB 15 is not connected

/* ASIC3 GPIO_C */

#define GPIOC_TEMP_LED_ON        (1<<0)
#define GPIOC_FULL_LED_ON        (1<<1)
// GPIOC2 is not connected
#define GPIOC_KEY_RXD            (1<<3)
#define GPIOC_KEY_TXD            (1<<4)
#define GPIOC_KEY_CLK            (1<<5)
// GPIOC6 is not connected
#define GPIOC_AUD_POWER_ON       (1<<7)
#define GPIOC_LCD_N3V_EN         (1<<8)
#define GPIOC_LCD_5V_EN          (1<<9)
#define GPIOC_LCD_3V3_ON         (1<<10)
#define GPIOC_WLAN_POWER_ON      (1<<11) /* 802.11b */
#define GPIOC_BLUETOOTH_3V3_ON   (1<<12)
// GPIOC13 is not connected 
#define GPIOC_FLASH_RESET_N      (1<<14)
#define GPIOC_BLUETOOTH_RESET_N  (1<<15)

/* ASIC3 GPIO_D */

#define GPIOD_USB_PULLUP         0 /* USB pullup */
#define GPIOD_LCD_RESET_N        (1<<1)
#define GPIOD_RECORD_BUTTON_N    (1<<2)
#define GPIOD_TASK_BUTTON_N	 (1<<3)
#define GPIOD_MAIL_BUTTON_N	 (1<<4)
#define GPIOD_CONTACTS_BUTTON_N	 (1<<5)
#define GPIOD_CALENDAR_BUTTON_N	 (1<<6)
#define GPIOD_HEADPHONE_IN_N     (1<<7)
#define GPIOD_IR_ON_N            (1<<8) /* active low */
#define GPIOD_NR_IR_ON_N         (16*3 + 8) /* active low */
#define GPIOD_MUX_SEL1           (1<<9) /* mux_sel[0:2]  */
#define GPIOD_MUX_SEL0           (1<<10)
#define GPIOD_PCI                (1<<11) /* ?? */
#define GPIOD_PCO                (1<<12) /* ?? */
#define GPIOD_SPK_EN             (1<<13)
#define GPIOD_CHARGER_EN         (1<<14)
#define GPIOD_WLAN_MAC_RESET     (1<<15)


#define GPIOD_USB_ON    GPIOD_CHARGER_EN         

/* MUX_SEL[0:2]: selects analog input input to ADS7846 ADC which also handles touch panel */
/* 0 -> VS_MBAT */
/* 1 -> IS_MBAT */
/* 2 -> BACKUP_BAT */
/* 3 -> TEMP_AD */
/* 7 -> MBAT_ID */

/* ASIC3 IRQs */
#define H4000_KEYBOARD_IRQ     (ASIC3_GPIOB_IRQ_BASE+1)
#define H4000_RECORD_BTN_IRQ   (ASIC3_GPIOD_IRQ_BASE+2)
#define H4000_TASK_BTN_IRQ     (ASIC3_GPIOD_IRQ_BASE+3)
#define H4000_MAIL_BTN_IRQ     (ASIC3_GPIOD_IRQ_BASE+4)
#define H4000_CONTACTS_BTN_IRQ (ASIC3_GPIOD_IRQ_BASE+5)
#define H4000_CALENDAR_BTN_IRQ (ASIC3_GPIOD_IRQ_BASE+6)
#define H4000_HEADPHONE_IN_IRQ (ASIC3_GPIOD_IRQ_BASE+7)

#define H4000_RED_LED         1
#define H4000_GREEN_LED       2
#define H4000_YELLOW_LED      3

#endif /* _INCLUDE_H4000_ASIC_H_ */

extern struct platform_device h4000_asic3;
