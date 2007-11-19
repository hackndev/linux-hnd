/*
 *
 * Definitions for SAMCOP Companion Chip
 *
 * Copyright 2002, 2003, 2004, 2005 Hewlett-Packard Company.
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
 * Author: Jamey Hicks
 *
 */

#ifndef _INCLUDE_IPAQ_SAMCOP_H_ 
#define _INCLUDE_IPAQ_SAMCOP_H_

#include <linux/mfd/samcop_adc.h>
#include <linux/gpiodev.h>


#define SAMCOP_REGISTER(x,y)					\
     (_SAMCOP_ ## x ## _Base + _SAMCOP_ ## x ## _ ## y)

#define _SAMCOP_SRAM_Base               0x00000
#define SAMCOP_SRAM_SIZE                32768

#define _SAMCOP_USBHOST_Base            0x20000

#define _SAMCOP_IC_Base                 0x30000

#define _SAMCOP_IC_SRCPND               0x00000
#define _SAMCOP_IC_INTMSK               0x00008
#define _SAMCOP_IC_PRIORITY             0x0000c
#define _SAMCOP_IC_INTPND               0x00010
#define _SAMCOP_IC_DREQ                 0x00018

#define SAMCOP_IC_SRCPND		SAMCOP_REGISTER(IC, SRCPND) /* raw pending interrupts */
#define SAMCOP_IC_INTMSK		SAMCOP_REGISTER(IC, INTMSK) /* interrupt mask */
#define SAMCOP_IC_PRIORITY		SAMCOP_REGISTER(IC, PRIORITY) /* interrupt priority control */
#define SAMCOP_IC_INTPND		SAMCOP_REGISTER(IC, INTPND) /* pending interrupts, accounting for mask */
#define SAMCOP_IC_DREQ			SAMCOP_REGISTER(IC, DREQ)   /* DREQ control */

#define _SAMCOP_IC_INT_GPIO		25       
#define _SAMCOP_IC_INT_WAKEUP3  	24       
#define _SAMCOP_IC_INT_ADC  		23       
#define _SAMCOP_IC_INT_ADCTS		22
#define _SAMCOP_IC_INT_UTXD1		21
#define _SAMCOP_IC_INT_UTXD0		20
#define _SAMCOP_IC_INT_WAKEUP2		19
#define _SAMCOP_IC_INT_ACCEL		18
#define _SAMCOP_IC_INT_USBH		17
#define _SAMCOP_IC_INT_URXD1		16
#define _SAMCOP_IC_INT_URXD0		15
#define _SAMCOP_IC_INT_WAKEUP1		14
#define _SAMCOP_IC_INT_SD_WAKEUP	13
#define _SAMCOP_IC_INT_SD		12
#define _SAMCOP_IC_INT_DMA1		11
#define _SAMCOP_IC_INT_DMA0		10
#define _SAMCOP_IC_INT_UERR1		 9
#define _SAMCOP_IC_INT_UERR0		 8
#define _SAMCOP_IC_INT_ONEWIRE		 7
#define _SAMCOP_IC_INT_FCD		 6
#define _SAMCOP_IC_INT_PCMCIA		 5
#define _SAMCOP_IC_INT_RECORD		 4
#define _SAMCOP_IC_INT_APPBUTTON4	 3
#define _SAMCOP_IC_INT_APPBUTTON3	 2
#define _SAMCOP_IC_INT_APPBUTTON2	 1
#define _SAMCOP_IC_INT_APPBUTTON1	 0

#define IRQ_SAMCOP_BUTTON_MASK		((1 << _SAMCOP_IC_INT_APPBUTTON1)|(1 << _SAMCOP_IC_INT_APPBUTTON2)\
						|(1 << _SAMCOP_IC_INT_APPBUTTON3)|(1 << _SAMCOP_IC_INT_APPBUTTON4)\
						|(1 << _SAMCOP_IC_INT_RECORD))
#define IRQ_SAMCOP_PCMCIA_MASK		(1 << _SAMCOP_IC_INT_PCMCIA)
#define IRQ_SAMCOP_FCD_MASK		(1 << _SAMCOP_IC_INT_FCD)
#define IRQ_SAMCOP_ONEWIRE_MASK		(1 << _SAMCOP_IC_INT_ONEWIRE)
#define IRQ_SAMCOP_UERR_MASK		((1 << _SAMCOP_IC_INT_UERR0)|(1 << _SAMCOP_IC_INT_UERR1))
#define IRQ_SAMCOP_DMA_MASK		((1 << _SAMCOP_IC_INT_DMA0)|(1 << _SAMCOP_IC_INT_DMA1))
#define IRQ_SAMCOP_SD_MASK		((1 << _SAMCOP_IC_INT_SD)|(1 << _SAMCOP_IC_INT_SD_WAKEUP))
#define IRQ_SAMCOP_URXD_MASK		((1 << _SAMCOP_IC_INT_URXD0)|(1 << _SAMCOP_IC_INT_URXD1))
#define IRQ_SAMCOP_USBH_MASK		(1 << _SAMCOP_IC_INT_USBH)
#define IRQ_SAMCOP_ACCEL_MASK		(1 << _SAMCOP_IC_INT_ACCEL)
#define IRQ_SAMCOP_WAKEUP_MASK		((1 << _SAMCOP_IC_INT_WAKEUP2)|(1 << _SAMCOP_IC_INT_WAKEUP3))
#define IRQ_SAMCOP_UTXD_MASK		((1 << _SAMCOP_IC_INT_UTXD0)|(1 << _SAMCOP_IC_INT_UTXD1))
#define IRQ_SAMCOP_ADCTS_MASK		(1 << _SAMCOP_IC_INT_ADCTS)
#define IRQ_SAMCOP_ADC_MASK		(1 << _SAMCOP_IC_INT_ADC)
#define IRQ_SAMCOP_GPIO_MASK		(1 << _SAMCOP_IC_INT_GPIO)

#define _SAMCOP_DMAC_Base               0x40000

/********* Clock and Power Management **********/

#define _SAMCOP_CPM_Base                0x50000
#define _SAMCOP_CPM_LockTime            0x00000 /* defaults to 0x0fff */
#define _SAMCOP_CPM_PllControl          0x00004 /* defaults to 0x00028080 */
#define _SAMCOP_CPM_ClockControl        0x00008 /* defaults to 0x0ffe */
#define _SAMCOP_CPM_ClockSleep          0x0000c

#define SAMCOP_CPM_LockTime		SAMCOP_REGISTER(CPM, LockTime)
#define SAMCOP_CPM_PllControl		SAMCOP_REGISTER(CPM, PllControl)
#define SAMCOP_CPM_ClockControl		SAMCOP_REGISTER(CPM, ClockControl)
#define SAMCOP_CPM_ClockSleep		SAMCOP_REGISTER(CPM, ClockSleep)

/* upll lock time count value for uclk, ltime > 150us */
#define SAMCOP_CPM_LOCKTIME_MASK	((1 << 11) -1) 
#define SAMCOP_CPM_PLLCON_MDIV_SHIFT	12 /* main divider control */
#define SAMCOP_CPM_PLLCON_MDIV_WIDTH	8
#define SAMCOP_CPM_PLLCON_PDIV_SHIFT	4 /* pre-divider control */
#define SAMCOP_CPM_PLLCON_PDIV_WIDTH	6
#define SAMCOP_CPM_PLLCON_SDIV_SHIFT	0 /* post divider control */
#define SAMCOP_CPM_PLLCON_SDIV_WIDTH	2

#define SAMCOP_CPM_CLKCON_1WIRE_CLKEN	(1 << 11)
#define SAMCOP_CPM_CLKCON_MISC_CLKEN	(1 << 10)
#define SAMCOP_CPM_CLKCON_LED_CLKEN	(1 << 9)
#define SAMCOP_CPM_CLKCON_UART1_CLKEN	(1 << 8)
#define SAMCOP_CPM_CLKCON_UART2_CLKEN	(1 << 7)
#define SAMCOP_CPM_CLKCON_ADC_CLKEN	(1 << 6)
#define SAMCOP_CPM_CLKCON_SD_CLKEN	(1 << 5)
#define SAMCOP_CPM_CLKCON_FCD_CLKEN	(1 << 4)
#define SAMCOP_CPM_CLKCON_GPIO_CLKEN	(1 << 3)
#define SAMCOP_CPM_CLKCON_DMAC_CLKEN	(1 << 2)
#define SAMCOP_CPM_CLKCON_USBHOST_CLKEN	(1 << 1)
#define SAMCOP_CPM_CLKCON_UCLK_EN	(1 << 0)

#define SAMCOP_CPM_CLKSLEEP_UCLK_ON	(1 << 2)
#define SAMCOP_CPM_CLKSLEEP_HALF_CLK	(1 << 1)
#define SAMCOP_CPM_CLKSLEEP_SLEEP	(1 << 0)

/********* ADC **********/
#define _SAMCOP_ADC_Base                0x80000

/********* UART **********/
#define _SAMCOP_UART_Base                 0x90000

/********* MISC **********/
#define _SAMCOP_MISC_Base                 0xa0000

/********* OWM **********/
#define _SAMCOP_OWM_Base                  0xa0100

/********* Fingerprint Scanner Interface **********/
#define _SAMCOP_FSI_Base		  0xb0000

/********* Secure Digital Interface **********/
#define _SAMCOP_SDI_Base                  0xb0030

/********* GPIO **********/
#define _SAMCOP_GPIO_Base                 0xc0000

#define _SAMCOP_GPIO_GPA_CON1	0x0000 /* reset value is 0xaaaaaaaa */
#define _SAMCOP_GPIO_GPA_CON2	0x0004 /* reset value is 0x0000002a */
#define _SAMCOP_GPIO_GPA_DAT	0x0008 /* gpio A data register, reset value is 0x00000000 */
#define _SAMCOP_GPIO_GPA_PUP	0x000c /* pull up disable register, reset value is 0x00080000 */

#define SAMCOP_GPIO_GPA_CON1	SAMCOP_REGISTER(GPIO, GPA_CON1)
#define SAMCOP_GPIO_GPA_CON2	SAMCOP_REGISTER(GPIO, GPA_CON2)
#define SAMCOP_GPIO_GPA_DAT	SAMCOP_REGISTER(GPIO, GPA_DAT) /* gpio A data register */
#define SAMCOP_GPIO_GPA_PUP	SAMCOP_REGISTER(GPIO, GPA_PUP) /* pull up disable register */
#define SAMCOP_GPIO_GPA_CON(n)		(SAMCOP_GPIO_GPA_CON1 + (4 * n))

#define _SAMCOP_GPIO_GPB_CON	0x0010
#define _SAMCOP_GPIO_GPB_DAT	0x0014 
#define _SAMCOP_GPIO_GPB_PUP	0x0018

#define SAMCOP_GPIO_GPB_CON	SAMCOP_REGISTER(GPIO, GPB_CON)
#define SAMCOP_GPIO_GPB_DAT	SAMCOP_REGISTER(GPIO, GPB_DAT) /* gpio B data register */
#define SAMCOP_GPIO_GPB_PUP	SAMCOP_REGISTER(GPIO, GPB_PUP) /* gpio B pull up disable register */

#define _SAMCOP_GPIO_GPC_CON	0x001c
#define _SAMCOP_GPIO_GPC_DAT	0x0020 
#define _SAMCOP_GPIO_GPC_PUP	0x0024

#define SAMCOP_GPIO_GPC_CON	SAMCOP_REGISTER(GPIO, GPC_CON)
#define SAMCOP_GPIO_GPC_DAT	SAMCOP_REGISTER(GPIO, GPC_DAT) /* gpio C data register */
#define SAMCOP_GPIO_GPC_PUP	SAMCOP_REGISTER(GPIO, GPC_PUP) /* gpio C pull up disable register */

#define _SAMCOP_GPIO_GPD_CON	0x0028
#define _SAMCOP_GPIO_GPD_DAT	0x002c 
#define _SAMCOP_GPIO_GPD_PUP	0x0030

#define SAMCOP_GPIO_GPD_CON	SAMCOP_REGISTER(GPIO, GPD_CON)
#define SAMCOP_GPIO_GPD_CON_RESET	0xaaaa
#define SAMCOP_GPIO_GPD_DAT	SAMCOP_REGISTER(GPIO, GPD_DAT) /* gpio D data register */
#define SAMCOP_GPIO_GPD_PUP	SAMCOP_REGISTER(GPIO, GPD_PUP) /* gpio D pull up disable register */

#define _SAMCOP_GPIO_GPE_CON	0x0034
#define _SAMCOP_GPIO_GPE_DAT	0x0038 

#define SAMCOP_GPIO_GPE_CON	SAMCOP_REGISTER(GPIO, GPE_CON)
#define SAMCOP_GPIO_GPE_CON_RESET	0x00000007
#define SAMCOP_GPIO_GPE_DAT	SAMCOP_REGISTER(GPIO, GPE_DAT) /* gpio E data register */

#define _SAMCOP_GPIO_CK8MCON	0x003c
#define _SAMCOP_GPIO_SPCR	0x0040
#define _SAMCOP_GPIO_INT1	0x0044
#define _SAMCOP_GPIO_INT2	0x0048
#define _SAMCOP_GPIO_INT3	0x004c
#define _SAMCOP_GPIO_FLTCONFIG1	0x0050
#define _SAMCOP_GPIO_FLTCONFIG2	0x0054
#define _SAMCOP_GPIO_FLTCONFIG3	0x0058
#define _SAMCOP_GPIO_FLTCONFIG4	0x005c
#define _SAMCOP_GPIO_FLTCONFIG5	0x0060
#define _SAMCOP_GPIO_FLTCONFIG6	0x0064
#define _SAMCOP_GPIO_FLTCONFIG7	0x0068
#define _SAMCOP_GPIO_MON	0x006c
#define _SAMCOP_GPIO_ENINT1	0x0070
#define _SAMCOP_GPIO_ENINT2	0x0074
#define _SAMCOP_GPIO_INTPND	0x0078

#define SAMCOP_GPIO_CK8MCON		SAMCOP_REGISTER(GPIO, CK8MCON)
#define SAMCOP_GPIO_SPCR		SAMCOP_REGISTER(GPIO, SPCR)
#define SAMCOP_GPIO_INT1		SAMCOP_REGISTER(GPIO, INT1) /* gpio interrupt config 1 */
#define SAMCOP_GPIO_INT2		SAMCOP_REGISTER(GPIO, INT2) /* gpio interrupt config 2 */
#define SAMCOP_GPIO_INT3		SAMCOP_REGISTER(GPIO, INT3) /* gpio interrupt config 3 */
#define SAMCOP_GPIO_FLTCONFIG1		SAMCOP_REGISTER(GPIO, FLTCONFIG1) /* gpio filter config 1 */
#define SAMCOP_GPIO_FLTCONFIG2		SAMCOP_REGISTER(GPIO, FLTCONFIG2) /* gpio filter config 2 */
#define SAMCOP_GPIO_FLTCONFIG3		SAMCOP_REGISTER(GPIO, FLTCONFIG3) /* gpio filter config 3 */
#define SAMCOP_GPIO_FLTCONFIG4		SAMCOP_REGISTER(GPIO, FLTCONFIG4) /* gpio filter config 4 */
#define SAMCOP_GPIO_FLTCONFIG5		SAMCOP_REGISTER(GPIO, FLTCONFIG5) /* gpio filter config 5 */
#define SAMCOP_GPIO_FLTCONFIG6		SAMCOP_REGISTER(GPIO, FLTCONFIG6) /* gpio filter config 6 */
#define SAMCOP_GPIO_FLTCONFIG7		SAMCOP_REGISTER(GPIO, FLTCONFIG7) /* gpio filter config 7 */
#define SAMCOP_GPIO_FLTDAT		SAMCOP_REGISTER(GPIO, FLTDAT) 	/* gpio filtered gpio level */
#define SAMCOP_GPIO_ENINT1		SAMCOP_REGISTER(GPIO, ENINT1) /* gpio interrupt enable 1 */
#define SAMCOP_GPIO_ENINT2		SAMCOP_REGISTER(GPIO, ENINT2) /* gpio interrupt enable 2 */
#define SAMCOP_GPIO_INTPND		SAMCOP_REGISTER(GPIO, INTPND) /* gpio interrupt pending */

#define SAMCOP_GPIO_SPCR_SDPUCR		(1 << 10)


#define _SAMCOP_LED_Base		0xc1000
#define _SAMCOP_LED_LEDCON0		0x00000
#define _SAMCOP_LED_LEDCON1		0x00004
#define _SAMCOP_LED_LEDCON2		0x00008
#define _SAMCOP_LED_LEDCON3		0x0000c
#define _SAMCOP_LED_LEDCON4		0x00010
#define _SAMCOP_LED_LEDPS		0x00014

#define SAMCOP_LED_CONTROL(_n)		SAMCOP_REGISTER(LED, LEDCON0 + (4 * (_n)))
#define SAMCOP_LED_LEDCON0		SAMCOP_REGISTER(LED, LEDCON0)
#define SAMCOP_LED_LEDCON1		SAMCOP_REGISTER(LED, LEDCON1)
#define SAMCOP_LED_LEDCON2		SAMCOP_REGISTER(LED, LEDCON2)
#define SAMCOP_LED_LEDCON3		SAMCOP_REGISTER(LED, LEDCON3)
#define SAMCOP_LED_LEDCON4		SAMCOP_REGISTER(LED, LEDCON4)
#define SAMCOP_LED_LEDPS		SAMCOP_REGISTER(LED, LEDPS)

/********* PCMCIA and Expansion Pack Interface **********/
#define _SAMCOP_PCMCIA_Base             0xd0000
#define _SAMCOP_PCMCIA_TDW              0x00000 /* data width, defaults to 0x01 */
#define _SAMCOP_PCMCIA_EPS              0x00004 /* status, defaults to 0x00 */
#define _SAMCOP_PCMCIA_CC               0x00008 /* control, defaults to 0 */
#define _SAMCOP_PCMCIA_IC               0x0000c /* interrupt enable, defaults to 0 */
#define _SAMCOP_PCMCIA_IM               0x00010 /* interrupt edge config, defaults to 0x25a5 */
#define _SAMCOP_PCMCIA_IP               0x00014 /* interrupt pending */
#define _SAMCOP_PCMCIA_MISC             0x00018 /* usb mask, defaults to 1 */ 

#define SAMCOP_PCMCIA_TDW		SAMCOP_REGISTER(PCMCIA, TDW)
#define SAMCOP_PCMCIA_EPS		SAMCOP_REGISTER(PCMCIA, EPS)
#define SAMCOP_PCMCIA_CC		SAMCOP_REGISTER(PCMCIA, CC)
#define SAMCOP_PCMCIA_IC		SAMCOP_REGISTER(PCMCIA, IC)
#define SAMCOP_PCMCIA_IM		SAMCOP_REGISTER(PCMCIA, IM)
#define SAMCOP_PCMCIA_IP		SAMCOP_REGISTER(PCMCIA, IP)

#define SAMCOP_PCMCIA_TDW_CS23_16BIT	(1 << 0)
#define SAMCOP_PCMCIA_TDW_CS4_16BIT	(1 << 1)
#define SAMCOP_PCMCIA_TDW_BIG_ENDIAN	(1 << 2)

#define SAMCOP_PCMCIA_EPS_CD0_N         (1 << 0) /* cd0 interrupt */	 
#define SAMCOP_PCMCIA_EPS_CD1_N         (1 << 1) /* cd1 interrupt */	 
#define SAMCOP_PCMCIA_EPS_IRQ0_N        (1 << 2) /* irq0 interrupt */	 
#define SAMCOP_PCMCIA_EPS_IRQ1_N        (1 << 3) /* irq1 interrupt */	 
#define SAMCOP_PCMCIA_EPS_ODET0_N       (1 << 4) /* odet0 interrupt */	 
#define SAMCOP_PCMCIA_EPS_ODET1_N       (1 << 5) /* odet1 interrupt */	 
#define SAMCOP_PCMCIA_EPS_BATT_FLT	(1 << 6) /* batt_flt interrupt */ 

#define SAMCOP_PCMCIA_CC_RESET		(1 << 0)

/* shift left by twice the interrupt number */
#define SAMCOP_PCMCIA_IM_NONE    0
#define SAMCOP_PCMCIA_IM_FALLING 1
#define SAMCOP_PCMCIA_IM_RISING  2
#define SAMCOP_PCMCIA_IM_BOTH    3

#define SAMCOP_PCMCIA_MISC_USB_MASK   0 /* enables usb host master address masking when 1 */

#define SAMCOP_MAP_SIZE	0x100000

#define SAMCOP_IC_IRQ_START		0
#define SAMCOP_IC_IRQ_COUNT		26

#define _IRQ_SAMCOP_BUTTON_1		(0)
#define _IRQ_SAMCOP_BUTTON_2		(1)
#define _IRQ_SAMCOP_BUTTON_3		(2)
#define _IRQ_SAMCOP_BUTTON_4		(3)
#define _IRQ_SAMCOP_RECORD		(4)
#define _IRQ_SAMCOP_PCMCIA		(5)
#define _IRQ_SAMCOP_FCD			(6)
#define _IRQ_SAMCOP_ONEWIRE		(7)
#define _IRQ_SAMCOP_UERR0		(8)
#define _IRQ_SAMCOP_UERR1		(9)
#define _IRQ_SAMCOP_DMA0		(10)
#define _IRQ_SAMCOP_DMA1		(11)
#define _IRQ_SAMCOP_SD			(12)
#define _IRQ_SAMCOP_SD_WAKEUP		(13)
#define _IRQ_SAMCOP_WAKEUP1		(14)
#define _IRQ_SAMCOP_URXD0		(15)
#define _IRQ_SAMCOP_URXD1		(16)
#define _IRQ_SAMCOP_USBH		(17)
#define _IRQ_SAMCOP_ACCEL		(18)
#define _IRQ_SAMCOP_WAKEUP2		(19)
#define _IRQ_SAMCOP_UTXD		(20)
#define _IRQ_SAMCOP_ADCTS		(22)
#define _IRQ_SAMCOP_ADC			(23)
#define _IRQ_SAMCOP_WAKEUP3		(24)
#define _IRQ_SAMCOP_GPIO		(25)

#define SAMCOP_GPIO_IRQ_START		(SAMCOP_IC_IRQ_START + SAMCOP_IC_IRQ_COUNT)
#define _IRQ_SAMCOP_GPIO_GPA4		0
#define _IRQ_SAMCOP_GPIO_GPA5		1
#define _IRQ_SAMCOP_GPIO_GPA6		2
#define _IRQ_SAMCOP_GPIO_GPA7		3
#define _IRQ_SAMCOP_GPIO_GPA8		4
#define _IRQ_SAMCOP_GPIO_GPA9		5
#define _IRQ_SAMCOP_GPIO_GPA10		6
#define _IRQ_SAMCOP_GPIO_GPA11		7
#define _IRQ_SAMCOP_GPIO_GPA18		8
#define _IRQ_SAMCOP_GPIO_GPB12		9
#define _IRQ_SAMCOP_GPIO_GPB13		10
#define _IRQ_SAMCOP_GPIO_GPB14		11
#define _IRQ_SAMCOP_GPIO_GPB15		12
#define _IRQ_SAMCOP_GPIO_GPB16		13
#define SAMCOP_GPIO_IRQ_COUNT		14

#define SAMCOP_EPS_IRQ_START		(SAMCOP_GPIO_IRQ_START + SAMCOP_GPIO_IRQ_COUNT)
#define _IRQ_SAMCOP_EPS_CD0		0
#define _IRQ_SAMCOP_EPS_CD1		1
#define _IRQ_SAMCOP_EPS_IRQ0		2
#define _IRQ_SAMCOP_EPS_IRQ1		3
#define _IRQ_SAMCOP_EPS_ODET		4
#define _IRQ_SAMCOP_EPS_BATT_FAULT	5
#define SAMCOP_EPS_IRQ_COUNT		6

#define SAMCOP_NR_IRQS			(SAMCOP_IC_IRQ_COUNT + SAMCOP_GPIO_IRQ_COUNT + SAMCOP_EPS_IRQ_COUNT)

#define SAMCOP_SDI_TIMER_MAX	0xffff

#define SAMCOP_GPIO_GPx_CON_MASK(_n)	(3 << (2 * _n))

#define SAMCOP_GPIO_GPx_CON_MODE_EXTIN	(2)

#define SAMCOP_GPIO_GPx_CON_MODE(_n, _v)	\
	(SAMCOP_GPIO_GPx_CON_MODE_ ## _v << (2 * _n))

#define SAMCOP_GPIO_IRQT_MASK(_n)	(7 << (4 * _n))
#define SAMCOP_GPIO_IRQT_RISING(_n)	(2 << (4 * _n))
#define SAMCOP_GPIO_IRQT_FALLING(_n)	(4 << (4 * _n))
#define SAMCOP_GPIO_IRQT_BOTHEDGE(_n)	(6 << (4 * _n))
#define SAMCOP_GPIO_IRQT_LOW(_n)	(0 << (4 * _n))
#define SAMCOP_GPIO_IRQT_HIGH(_n)	(1 << (4 * _n))

#define SAMCOP_GPIO_IRQF_MASK(_n)		(1 << (4 * _n + 3))
#define SAMCOP_GPIO_IRQF_WIDTH_MASK(_n)		(0x1fff << (16 * _n))

#define SAMCOP_GPIO_ENINT_MASK(_n)		(1 << _n)

#define SAMCOP_GPIO_INT(n)			(SAMCOP_GPIO_INT1 + (4 * n))
#define SAMCOP_GPIO_FLTCONFIG(n)	(SAMCOP_GPIO_FLTCONFIG1 + (4 * n))
#define SAMCOP_GPIO_ENINT(n)		(SAMCOP_GPIO_ENINT1 + (4 * n))

/* GPIO bank definitions */
#define _SAMCOP_GPIO_BANK_A	0
#define _SAMCOP_GPIO_BANK_B	1
#define _SAMCOP_GPIO_BANK_C	2
#define _SAMCOP_GPIO_BANK_D	3
#define _SAMCOP_GPIO_BANK_E	4

#define _SAMCOP_GPIO(port, pin) ((_SAMCOP_GPIO_BANK_##port << 6) + pin)
#define _SAMCOP_GPIO_BIT(n)	(1<<((n)&0x1f))
#define _SAMCOP_GPIO_BANK(n)	((n) >> 6) 

/* GPIO bank A */
#define SAMCOP_GPA_APPBTN1	_SAMCOP_GPIO(A, 0)
#define SAMCOP_GPA_APPBTN2	_SAMCOP_GPIO(A, 1)
#define SAMCOP_GPA_APPBTN3	_SAMCOP_GPIO(A, 2)
#define SAMCOP_GPA_APPBTN4	_SAMCOP_GPIO(A, 3)
#define SAMCOP_GPA_JOYSTICK1	_SAMCOP_GPIO(A, 4)
#define SAMCOP_GPA_JOYSTICK2	_SAMCOP_GPIO(A, 5)
#define SAMCOP_GPA_JOYSTICK3	_SAMCOP_GPIO(A, 6)
#define SAMCOP_GPA_JOYSTICK4	_SAMCOP_GPIO(A, 7)
#define SAMCOP_GPA_JOYSTICK5	_SAMCOP_GPIO(A, 8)
#define SAMCOP_GPA_TOGGLEUP	_SAMCOP_GPIO(A, 9)
#define SAMCOP_GPA_TOGGLEDOWN	_SAMCOP_GPIO(A, 10)
#define SAMCOP_GPA_RESET	_SAMCOP_GPIO(A, 11)
#define SAMCOP_GPA_RECORD	_SAMCOP_GPIO(A, 12)
#define SAMCOP_GPA_WAKEUP1	_SAMCOP_GPIO(A, 13)
#define SAMCOP_GPA_WAKEUP2	_SAMCOP_GPIO(A, 14)
#define SAMCOP_GPA_WAKEUP3	_SAMCOP_GPIO(A, 15)
#define SAMCOP_GPA_SD_WAKEUP	_SAMCOP_GPIO(A, 16)
#define SAMCOP_GPA_OPT_ON	_SAMCOP_GPIO(A, 17)
#define SAMCOP_GPA_OPT_RESET	_SAMCOP_GPIO(A, 18)
#define SAMCOP_GPA_OPT_UARTCLK	_SAMCOP_GPIO(A, 19)

/* part of GPIO bank B */
#define SAMCOP_GPB_WAN1	_SAMCOP_GPIO(B, 12)
#define SAMCOP_GPB_WAN2	_SAMCOP_GPIO(B, 13)
#define SAMCOP_GPB_WAN3	_SAMCOP_GPIO(B, 14)
#define SAMCOP_GPB_WAN4	_SAMCOP_GPIO(B, 15)
#define SAMCOP_GPB_WAN5	_SAMCOP_GPIO(B, 16)

/* other GPIO banks - TODO */

#endif
