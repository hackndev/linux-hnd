/*
 * include/asm-arm/arch-pxa/htcalpine-gpio.h
 * History:
 *
 * 2004-12-10 Michael Opdenacker. Wrote down GPIO settings as identified by Jamey Hicks.
 *            Reused the h2200-gpio.h file as a template.
 */

#ifndef _HTCALPINE_H_
#define _HTCALPINE_H_

#include <asm/arch/pxa-regs.h>

#define GET_HTCALPINE_GPIO(gpio) \
	(GPLR(GPIO_NR_HTCALPINE_ ## gpio) & GPIO_bit(GPIO_NR_HTCALPINE_ ## gpio))

#define SET_HTCALPINE_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_HTCALPINE_ ## gpio) = GPIO_bit(GPIO_NR_HTCALPINE_ ## gpio); \
else \
	GPCR(GPIO_NR_HTCALPINE_ ## gpio) = GPIO_bit(GPIO_NR_HTCALPINE_ ## gpio); \
} while (0)

#define SET_HTCALPINE_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_HTCALPINE_ ## gpio ## _N) = GPIO_bit(GPIO_NR_HTCALPINE_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_HTCALPINE_ ## gpio ## _N) = GPIO_bit(GPIO_NR_HTCALPINE_ ## gpio ## _N); \
} while (0)

#define HTCALPINE_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_HTCALPINE_ ## gpio)

#define GPIO_NR_HTCALPINE_KEY_ON				0
#define GPIO_NR_HTCALPINE_GP_RST_N			1

#define GPIO_NR_HTCALPINE_USB_DET			11

#define GPIO_NR_HTCALPINE_USB_PUEN			27

#define GPIO_NR_HTCALPINE_LDD0			58
#define GPIO_NR_HTCALPINE_LDD1			59
#define GPIO_NR_HTCALPINE_LDD2			60
#define GPIO_NR_HTCALPINE_LDD3			61
#define GPIO_NR_HTCALPINE_LDD4			62
#define GPIO_NR_HTCALPINE_LDD5			63
#define GPIO_NR_HTCALPINE_LDD6			64
#define GPIO_NR_HTCALPINE_LDD7			65
#define GPIO_NR_HTCALPINE_LDD8			66
#define GPIO_NR_HTCALPINE_LDD9			67
#define GPIO_NR_HTCALPINE_LDD10			68
#define GPIO_NR_HTCALPINE_LDD11			69
#define GPIO_NR_HTCALPINE_LDD12			70
#define GPIO_NR_HTCALPINE_LDD13			71
#define GPIO_NR_HTCALPINE_LDD14			72
#define GPIO_NR_HTCALPINE_LDD15			73

#define GPIO_NR_HTCALPINE_LFCLK_RD			74
#define GPIO_NR_HTCALPINE_LFCLK_A0			75
#define GPIO_NR_HTCALPINE_LFCLK_WR			76
#define GPIO_NR_HTCALPINE_LBIAS			77

#define GPIO_NR_HTCALPINE_CS2_N			78
#define GPIO_NR_HTCALPINE_CS3_N			79
#define GPIO_NR_HTCALPINE_CS4_N			80
#define GPIO_NR_HTCALPINE_CIF_DD0			81
#define GPIO_NR_HTCALPINE_CIF_DD5			82


#define GPIO_NR_HTCALPINE_TOUCHPANEL_IRQ_N		115

#define GPIO_NR_HTCALPINE_I2C_SCL			117
#define GPIO_NR_HTCALPINE_I2C_SDA			118

#define GPIO_NR_HTCALPINE_I2C_SCL_MD		(117 | GPIO_ALT_FN_1_OUT)
#define GPIO_NR_HTCALPINE_I2C_SDA_MD		(118 | GPIO_ALT_FN_1_OUT)

#endif /* _HTCALPINE_H_ */
