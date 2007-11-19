/* 
 * include/asm-arm/arch-pxa/htcathena-gpio.h
 */
#ifndef _HTCATHENA_GPIO_H_
#define _HTCATHENA_GPIO_H_

#include <asm/arch/pxa-regs.h>

#define GET_HTCATHENA_GPIO(gpio) \
	(GPLR(GPIO_NR_HTCATHENA_ ## gpio) & GPIO_bit(GPIO_NR_HTCATHENA_ ## gpio))

#define SET_HTCATHENA_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_HTCATHENA_ ## gpio) = GPIO_bit(GPIO_NR_HTCATHENA_ ## gpio); \
else \
	GPCR(GPIO_NR_HTCATHENA_ ## gpio) = GPIO_bit(GPIO_NR_HTCATHENA_ ## gpio); \
} while (0)

#define SET_HTCATHENA_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_HTCATHENA_ ## gpio ## _N) = GPIO_bit(GPIO_NR_HTCATHENA_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_HTCATHENA_ ## gpio ## _N) = GPIO_bit(GPIO_NR_HTCATHENA_ ## gpio ## _N); \
} while (0)

#define HTCATHENA_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_HTCATHENA_ ## gpio)

#define GPIO_NR_HTCATHENA_KEY_POWER       0
#define GPIO_NR_HTCATHENA_CPLD1_EXT_INT	 14
#define GPIO_NR_HTCATHENA_KEY_VOL_UP   	 17
#define GPIO_NR_HTCATHENA_KEY_CAMERA   	 93
#define GPIO_NR_HTCATHENA_KEY_RECORD   	 94
#define GPIO_NR_HTCATHENA_KEY_WWW      	 95
#define GPIO_NR_HTCATHENA_KEY_SEND     	 98
#define GPIO_NR_HTCATHENA_KEY_END      	 99
#define GPIO_NR_HTCATHENA_KEY_VOL_DOWN	102
#define GPIO_NR_HTCATHENA_KEY_RIGHT   	103
#define GPIO_NR_HTCATHENA_KEY_UP      	104
#define GPIO_NR_HTCATHENA_KEY_LEFT    	105
#define GPIO_NR_HTCATHENA_KEY_DOWN    	106
#define GPIO_NR_HTCATHENA_KEY_ENTER   	107

#endif /* _HTCATHENA_GPIO_H */
