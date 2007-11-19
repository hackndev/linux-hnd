/* 
 * include/asm-arm/arch-pxa/aximx50-gpio.h
 * History:
 *
 * 2007-01-25 Pierre Gaufillet          Creation 
 *
 */

#ifndef _X50_GPIO_H_
#define _X50_GPIO_H_

#include <asm/arch/pxa-regs.h>

#define GET_X50_GPIO(gpio) \
	(GPLR(GPIO_NR_X50_ ## gpio) & GPIO_bit(GPIO_NR_X50_ ## gpio))

#define SET_X50_GPIO(gpio, setp) \
do { \
if (setp) \
	GPSR(GPIO_NR_X50_ ## gpio) = GPIO_bit(GPIO_NR_X50_ ## gpio); \
else \
	GPCR(GPIO_NR_X50_ ## gpio) = GPIO_bit(GPIO_NR_X50_ ## gpio); \
} while (0)

#define SET_X50_GPIO_N(gpio, setp) \
do { \
if (setp) \
	GPCR(GPIO_NR_X50_ ## gpio ## _N) = GPIO_bit(GPIO_NR_X50_ ## gpio ## _N); \
else \
	GPSR(GPIO_NR_X50_ ## gpio ## _N) = GPIO_bit(GPIO_NR_X50_ ## gpio ## _N); \
} while (0)

#define X50_IRQ(gpio) \
	IRQ_GPIO(GPIO_NR_X50_ ## gpio)

/*********************************************************************/

#define GPIO_NR_X50_AC_IN_N         11                      /* Input = 1 when externally powered */
#define GPIO_NR_X50_BACKLIGHT_ON    17                      /* Tied to PWM0 when Alt function == 2 */
#define GPIO_NR_X50_PEN_IRQ_N       94                      /* Input = 0 when stylus down */


#endif /* _X50_GPIO_H */

