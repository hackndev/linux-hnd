/*
 * include/asm/arm/arch-pxa/htcsable-asic.h
 *
 * Authors: Giuseppe Zompatori <giuseppe_zompatori@yahoo.it>
 *
 * based on previews work, see below:
 * 
 * include/asm/arm/arch-pxa/hx4700-asic.h
 *      Copyright (c) 2004 SDG Systems, LLC
 *
 */

#ifndef _HTCSABLE_ASIC_H_
#define _HTCSABLE_ASIC_H_

#include <asm/hardware/ipaq-asic3.h>

/* ASIC3 */

#define HTCSABLE_ASIC3_GPIO_PHYS	PXA_CS4_PHYS
#define HTCSABLE_ASIC3_MMC_PHYS		PXA_CS3_PHYS

/* TODO: some information is missing here :) */

/* ASIC3 GPIO A bank */

#define GPIOA_I2C_PWR_ON     0  /* Output */ /* lkcl: guess!!  */
#define GPIOA_CODEC_ON     1  /* Output */ /* ak4641 codec on */
#define GPIOA_HEADPHONE_PWR_ON     3  /* Output */ /* headphone power-on? */
#define GPIOA_SPKMIC_PWR2_ON     6  /* Output */ /* speaker power-on */
#define GPIOA_USB_PUEN 			7	/* Output */
#define GPIOA_LCD_PWR_5 		15	/* Output */

/* ASIC3 GPIO B bank */

#define GPIOB_GSM_DCD        0  /*  Input */ /* phone-related: not sure of name or purpose */
#define GPIOB_BB_READY       2  /*  Input */ /* phone-related: not sure of name or purpose */
#define GPIOB_ACX_IRQ_N       5 /* Input */ /* acx wifi irq ? */
#define GPIOB_USB_DETECT 			6	/* Input */
#define GPIOB_HEADPHONE       9   /* Input */ /* lkcl: guess!!! */
#define GPIOB_CODEC_RESET    10  /* Output */ /* think this is right... */
#define GPIOB_ACX_PWR_1      15 /* Output */ /* acx wifi power 1 */

/* ASIC3 GPIO C bank */

#define GPIOC_BT_RESET       7  /*  Output */
#define GPIOC_PHONE_4        12  /*  Output */ /* phone-related: not sure of name or purpose */
#define GPIOC_ACX_RESET   8  /* Output */ /* acx wifi reset */
#define GPIOC_PHONE_5       13  /*  Output */ /* phone-related: not sure of name or purpose */
#define GPIOC_LCD_PWR_1 		9	/* Output */
#define GPIOC_LCD_PWR_2 		10	/* Output */
#define GPIOC_ACX_PWR_3      15 /* Output */ /* acx wifi power 3 */


/* ASIC3 GPIO D bank */

#define GPIOD_BT_PWR_ON     6  /* Output */
#define GPIOD_PHONE_1     4  /* Output */ /* phone-related: not sure of name or purpose */
#define GPIOD_ACX_PWR_2   3  /* Output */ /* acx wifi power 2 */
#define GPIOD_PHONE_6     5  /* Output */ /* phone-related: not sure of name or purpose */
#define GPIOD_PHONE_2     8  /* Output */ /* phone-related: not sure of name or purpose */
#define GPIOD_VIBRATE     10  /* Output */ /* vibrate */
#define GPIOD_LCD_BACKLIGHT 		12	/* Output */

extern struct platform_device htcsable_asic3;

#endif /* _HTCSABLE_ASIC_H_ */

