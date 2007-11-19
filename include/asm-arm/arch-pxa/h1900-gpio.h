/*
 *
 * Definitions for HP iPAQ Handheld Computer
 *
 * Copyright 2003 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 */

#ifndef _H1900_GPIO_H_
#define _H1900_GPIO_H_

/* INPUT and OUTPUT are just what joshua notes wince sets 'em up as. */

#define GPIO_NR_H1900_POWER_BUTTON_N	(0)   /* Also known as the "off button" - INPUT */
#define GPIO_NR_H1900_WARM_RST_N	(1)   /* external circuit to differentiate warm/cold resets - INPUT*/
#define GPIO_NR_H1900_SD_DETECT_N	(2)   /* INPUT */
#define GPIO_NR_H1900_CHARGING		(3)   /* charger active signal INPUT */
#define GPIO_NR_H1900_AC_IN_N		(4)   /* detect ac adapter */
#define GPIO_NR_H1900_BATTERY_DOOR_N	(5)   /* whether door is present */ 
// gpio 6 not connected ?
#define GPIO_NR_H1900_USB_DETECT2_N	(7)  /* Seems to detect USB also.*/
#define GPIO_NR_H1900_APP_BUTTON0_N	(8)  /* app button 0 */
#define GPIO_NR_H1900_ASIC_IRQ_1_N	(9)  /* ASIC IRQ? */
#define GPIO_NR_H1900_SD_IRQ_N		(10)   
#define GPIO_NR_H1900_MBAT_IN		(11) /* battery present */
#define GPIO_NR_H1900_ASIC3_IRQ		(12) /* I can't get this to fire. - OUTPUT */
#define GPIO_NR_H1900_ASIC_IRQ_2_N	(13) /* ASIC IRQ? */
#define GPIO_NR_H1900_USB_DETECT_N	(14) /* This seems to be USB_DETECT_N */
#define GPIO_NR_H1900_RECORD_BUTTON_N   (15) /* not used for CS1#. unsure if correct. - OUTPUT*/
#define GPIO_NR_H1900_LCD_PWM		(16) /* OUTPUT */
#define GPIO_NR_H1900_SOFT_RESET_N	(17) /* OUTPUT - take it low, then hit power, and the device downloads brownware*/
// 18 is RDY (need to set GAFR)  
#define GPIO_NR_H1900_UP_BUTTON_N	(19) /* INPUT */
#define GPIO_NR_H1900_APP_BUTTON2_N	(20) /* always zero? - OUTPUT */
#define GPIO_NR_H1900_ACTION_BUTTON_N	(21) /* INPUT */
// 22 unused	
#define GPIO_NR_H1900_SPI_CLK		(23) /* SPI/Microwire things */
#define GPIO_NR_H1900_SPI_CS_N		(24) /* Used for touchscreen */ 
#define GPIO_NR_H1900_SPI_TXD           (25) 
#define GPIO_NR_H1900_SPI_RXD           (26) 
#define GPIO_NR_H1900_PEN_IRQ_N		(27) /* detect stylus down */

/* IIS to UDA1380 */
#define GPIO_NR_H1900_AC97_BITCLK       (28)
#define GPIO_NR_H1900_AC97_SDATA_IN     (29)
#define GPIO_NR_H1900_AC97_SDATA_OUT    (30)
#define GPIO_NR_H1900_AC97_SYNC         (31)

#define GPIO_NR_H1900_SYS_CLK		(32) /* check connection of this */
// 33 is CS5#, (need to set GAFR)

#define GPIO_NR_H1900_LCD_SOMETHING     (34) /* it seems we have to set this */
#define GPIO_NR_H1900_FFUART_CTS_N      (35)
#define GPIO_NR_H1900_FL_PWR_EN         (36) /* front light.  have to steal this from serial driver */
#define GPIO_NR_H1900_FLASH_VPEN        (37) /* have to steal this from serial driver. */
#define GPIO_NR_H1900_LCD_PCI		(38) /* LCD ??. have to steal this from serial driver. */
#define GPIO_NR_H1900_FFUART_TXD        (39)
#define GPIO_NR_H1900_FW_LOW_N          (40) /* output, something to do with reset, steal from serial */
#define GPIO_NR_H1900_FFUART_RTS_N      (41)

#define GPIO_NR_H1900_CHARGER_EN        (42)
#define GPIO_NR_H1900_DOWN_BUTTON_N     (43)
#define GPIO_NR_H1900_LEFT_BUTTON_N     (44)
#define GPIO_NR_H1900_RIGHT_BUTTON_N    (45)

#define GPIO_NR_H1900_IR_RXD            (46)
#define GPIO_NR_H1900_IR_TXD            (47)

#define GPIO_NR_H1900_VGL_EN            (48) /* lcd -7V supply */

// 49 is PWE# (need to set GAFR)

#define GPIO_NR_H1900_LCD_DVDD_EN       (50) /* 2.5V to LCD */
#define GPIO_NR_H1900_LCD_TYPE          (51)
#define GPIO_NR_H1900_SPEAKER_EN        (52) /* speaker amplifier power */
#define GPIO_NR_H1900_CODEC_AUDIO_RESET (53)
#define GPIO_NR_H1900_ASIC3_RESET_N     (54)
#define GPIO_NR_H1900_FLASH_BUSY_N      (55)
#define GPIO_NR_H1900_KB_DETECT_N       (56)
#define GPIO_NR_H1900_HEADPHONE_IN_N    (57) /* headphone detect? */

// 58 through 77 is LCD signals (need to set GAFR)

// 78 is CS2# (need to set GAFR) /* nand flash */
// 79 is CS3# (need to set GAFR)
// 80 is CS4# (need to set GAFR) /* asic3 is on CS4# ? */

#endif /* _H1900_GPIO_H_ */
