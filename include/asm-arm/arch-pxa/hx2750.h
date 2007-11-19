/*
 * Hardware specific definitions for iPAQ hx2750
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_HX2750_H
#define __ASM_ARCH_HX2750_H  1

/*
 * HX2750 (Non Standard) GPIO Definitions
 */

#define HX2750_GPIO_KEYPWR      (0)    /* Power button */
#define HX2750_GPIO_BATTCOVER1  (9)    /* Battery Cover Switch */
#define HX2750_GPIO_CF_IRQ      (11)   /* CF IRQ? */
#define HX2750_GPIO_USBCONNECT  (12)   /* USB Connected? */
#define HX2750_GPIO_CF_DETECT   (13)   /* CF Card Detect? */
#define HX2750_GPIO_EXTPWR      (14)   /* External Power Detect */
#define HX2750_GPIO_BATLVL      (15)   /* Battery Level Detect */
#define HX2750_GPIO_CF_PWR      (17)   /* CF Power? */
#define HX2750_GPIO_SR_STROBE   (18)   /* Shift Register Strobe */
#define HX2750_GPIO_CHARGE      (22)   /* Charging Enable (active low) */
#define HX2750_GPIO_TSC2101_SS  (24)   /* TSC2101 SS# */
#define HX2750_GPIO_BTPWR       (27)   /* Bluetooth Power? */
#define HX2750_GPIO_BATTCOVER2  (33)   /* Battery Cover Switch */
#define HX2750_GPIO_SD_DETECT   (38)   /* MMC Card Detect */
#define HX2750_GPIO_SR_CLK1     (52)   /* Shift Register Clock */
#define HX2750_GPIO_SR_CLK2     (53)   
#define HX2750_GPIO_CF_WIFIIRQ  (54)   /* CF Wifi IRQ? */
#define HX2750_GPIO_GPIO_DIN    (88)   /* Shift Register Data */
#define HX2750_GPIO_KEYLEFT     (90)   /* Left button */
#define HX2750_GPIO_KEYRIGHT    (91)   /* Right button */
#define HX2750_GPIO_KEYCAL      (93)   /* Calander button */
#define HX2750_GPIO_KEYTASK     (94)   /* Task button */
#define HX2750_GPIO_KEYSIDE     (95)   /* Side button */
#define HX2750_GPIO_KEYENTER    (96)   /* Enter Button*/
#define HX2750_GPIO_KEYCON      (97)   /* Contacts button */
#define HX2750_GPIO_KEYMAIL     (98)   /* Mail button */
#define HX2750_GPIO_BIOPWR      (99)   /* BIO Reader Power? */
#define HX2750_GPIO_KEYUP       (100)  /* Up button */
#define HX2750_GPIO_KEYDOWN     (101)  /* Down button*/
#define HX2750_GPIO_SD_READONLY (103)  /* MMC/SD Write Protection */
#define HX2750_GPIO_LEDMAIL     (106)  /* Green Mail LED */
#define HX2750_GPIO_HP_JACK     (108)  /* Head Phone Jack Present */
#define HX2750_GPIO_PENDOWN     (117)  /* Touch Screen Pendown */


//#define HX2750_GPIO_     () /* */

/*
 * HX2750 Interrupts
 */
#define HX2750_IRQ_GPIO_EXTPWR      IRQ_GPIO(HX2750_GPIO_EXTPWR)
#define HX2750_IRQ_GPIO_SD_DETECT	IRQ_GPIO(HX2750_GPIO_SD_DETECT)
#define HX2750_IRQ_GPIO_PENDOWN     IRQ_GPIO(HX2750_GPIO_PENDOWN)
#define HX2750_IRQ_GPIO_CF_DETECT   IRQ_GPIO(HX2750_GPIO_CF_DETECT)
#define HX2750_IRQ_GPIO_CF_IRQ      IRQ_GPIO(HX2750_GPIO_CF_IRQ)
#define HX2750_IRQ_GPIO_CF_WIFIIRQ  IRQ_GPIO(HX2750_GPIO_CF_WIFIIRQ)

/*
 * HX2750 Extra GPIO Definitions
 */

#define HX2750_EGPIO_WIFI_PWR	(1 << 11)  /* Triggers gpio 54 - Wifi enable? */
#define HX2750_EGPIO_LCD_PWR	(1 << 10)  /* LCD Power */
#define HX2750_EGPIO_BL_PWR     (1 << 9)   /* Backlight LED Power */
#define HX2750_EGPIO_8          (1 << 8)   /* */
#define HX2750_EGPIO_7          (1 << 7)   /* */
#define HX2750_EGPIO_SD_PWR     (1 << 6)   /* SD Power? */
#define HX2750_EGPIO_TSC_PWR    (1 << 5)   /* TS Power */
#define HX2750_EGPIO_4          (1 << 4)   /* */
#define HX2750_EGPIO_CF1_RESET  (1 << 3)   /* Triggers gpio 54 - CF1 Reset? */
#define HX2750_EGPIO_2          (1 << 2)   /* */
#define HX2750_EGPIO_1          (1 << 1)   /* */
#define HX2750_EGPIO_PWRLED     (1 << 0)   /* Red Power LED */


void hx2750_set_egpio(unsigned int gpio);
void hx2750_clear_egpio(unsigned int gpio);


#endif /* __ASM_ARCH_HX2750_H  */

