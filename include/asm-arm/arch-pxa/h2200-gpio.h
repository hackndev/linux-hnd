/* 
 * include/asm-arm/arch-pxa/h2200-gpio.h
 * History:
 *
 * 2003-12-08 Jamey Hicks		Copied over h22xx gpio definitions and beginnings of h22xx (hamcop) asic declarations
 * 2004-01-26 Michael Opdenacker	Replaced "_H5200_GPIO_H_" by "_H2200_GPIO_H_"
 * 					Added definition for IRQ_GPIO_H2200_ASIC_INT (like in h5400-gpio.h)
 * 2004-02-05 Koen Kooi			Added definition for IRQ_NR_H2200_CF_DETECT_N
 */

#ifndef _H2200_GPIO_H_
#define _H2200_GPIO_H_

#define GET_H2200_GPIO(gpio) \
	(GPLR(GPIO_NR_H2200_ ## gpio) & GPIO_bit(GPIO_NR_H2200_ ## gpio))

#define SET_H2200_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_H2200_ ## gpio) = GPIO_bit(GPIO_NR_H2200_ ## gpio); \
else \
	GPCR(GPIO_NR_H2200_ ## gpio) = GPIO_bit(GPIO_NR_H2200_ ## gpio); \
} while (0)

#define SET_H2200_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_H2200_ ## gpio ## _N) = GPIO_bit(GPIO_NR_H2200_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_H2200_ ## gpio ## _N) = GPIO_bit(GPIO_NR_H2200_ ## gpio ## _N); \
} while (0)

#define H2200_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_H2200_ ## gpio)

#define GPIO_NR_H2200_POWER_BUTTON_N	0 /* really? */
#define GPIO_NR_H2200_RESET_BUTTON_N	1 /* really? */
#define GPIO_NR_H2200_BATT_DOOR_N	2
#define GPIO_NR_H2200_USB_DETECT_N	3
/* the following pin is connected to DCD signal on FFUART that is available
   even when RS232 transceiver is powered off by GPIO80 (0-off; 1-active) */
#define GPIO_NR_H2200_RS232_DCD		4
#define GPIO_NR_H2200_ASIC_INT_N	5 
#define GPIO_NR_H2200_CIR_SIR_N		6 /* high for 512Mb nand flash, low for 256Mb NAND flash (judging from the bootloader, the comment is correct, not the name) */
#define GPIO_NR_H2200_CF_INT_N		7
#define GPIO_NR_H2200_SD_DETECT_N	8
#define GPIO_NR_H2200_CF_DETECT_N	9
#define GPIO_NR_H2200_BT_WAKE		10 /* wakeup signal to bluetooth module */
/* 11 is 3.6MHz out; wince configures as output, alt func 1 */
#define GPIO_NR_H2200_AC_IN_N		12 /* AC adapter inserted */
#define GPIO_NR_H2200_POWER_ON_N	13 /* power on key signal input ? */
#define GPIO_NR_H2200_MQ1178_IRQ_N	14 /* interrupt request from mq11xx */
/* 15 is CS1# */
/* 16 is PWM0 if configured as an output, alt func 2 */
#define GPIO_NR_H2200_BACKLIGHT_ON	17
/* 18 is ext memory rdy, alternate function 2 */
#define GPIO_NR_H2200_CPU_200MHZ	19
/* 20 is dreq0 */
#define GPIO_NR_H2200_CHG_EN		21 /* connected to MAX1898 charger */
/* 22 video related??? */
#define GPIO_NR_H2200_CF_RESET		23
#define GPIO_NR_H2200_RS232_CIR_N	24 /* rs232 or CIR control signal selection: 1->RS232, 0->CIR */
#define GPIO_NR_H2200_CF_ADD_EN_N	25 /* !enable address to be driven to CF bus */
#define GPIO_NR_H2200_CF_POWER_EN	26 /* enable power to CF device */
#define GPIO_NR_H2200_CPU_400MHZ	27 /* see table below */
/* 
 * GPIO19	GPIO27	CPUSPEED
 * 0		0	300MHz
 * 1		0	400MHz
 * 0		1	200MHz
 * 1		1	reserved
 */

/* 28 - 32 are AC97/I2S */

#define GPIO_NR_H2200_USB_PULL_UP_N	33

/* 34 - 41 FFUART, MMC (unused) */
/* 42 - 45 BTUART/HWUART */
/* 46 - 47 STUART/ICP */
/* 48 - 51 HWUART/memory controller */
/* 52 - 57 MMC (unused)/memory controller */
/* 58 - 59 LCD controller (unused) */

#define GPIO_NR_H2200_MQ1178_VDD	60 /* MediaQ 1178 VDD: Nominally 0 (?) */
#define GPIO_NR_H2200_SD_POWER_EN	61
#define GPIO_NR_H2200_BOOTLOADER_DETECT_N 62 /* boot loader detect input: Nominally 0*/
#define GPIO_NR_H2200_CF_BUFF_EN	63 /* enable CF buffers */
#define GPIO_NR_H2200_IR_ON_N		64
#define GPIO_NR_H2200_TDA_MODE		65 /* speaker mode control */
#define GPIO_NR_H2200_MIC_ON_N		66
#define GPIO_NR_H2200_MQ1178_POWER_ON	67 /* PDWN#/GPIO4 on MediaQ 1178: Nominally 1 */
#define GPIO_NR_H2200_MQ1178_RESET_N	68 /* Nominally 1 */
#define GPIO_NR_H2200_SPEAKER_ON	69
#define GPIO_NR_H2200_CODEC_ON		70
#define GPIO_NR_H2200_CODEC_RESET	71
#define GPIO_NR_H2200_CIR_RESET		72
#define GPIO_NR_H2200_CIR_POWER_ON	73 /* consumer IR power on */
#define GPIO_NR_H2200_BT_RESET_N	74 /* bluetooth module reset */
#define GPIO_NR_H2200_BT_IDENT		75 /* 0 -> SMART, 1->zeevo module */
#define GPIO_NR_H2200_BT_POWER_ON	76
#define GPIO_NR_H2200_BT_IRQ		77 /* wakeup signal from bluetooth to cpu */
/* 78 is CS2# */
#define GPIO_NR_H2200_SDRAM_128MB_N	79 /* 0 -> 128Mbit, 1 -> 256Mbit */
#define GPIO_NR_H2200_RS232_ON		80
/* 81 ??? */
/* 82 ??? */
/* 83 ??? */

#define IRQ_GPIO_H2200_ASIC_INT		IRQ_GPIO(GPIO_NR_H2200_ASIC_INT_N)

#endif /* _H2200_GPIO_H */
