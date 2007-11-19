/*
 * palmtc-gpio.h
 *
 * Authors: Holger Bocklet <bitz.email@gmx.net>
 *
 */

#ifndef _PALMTC_GPIO_H_
#define _PALMTC_GPIO_H_

#include <asm/arch/pxa-regs.h>

/* Palm Tungsten C GPIOs */
#define GPIO_NR_PALMTC_EARPHONE_DETECT 		2
#define GPIO_NR_PALMTC_CRADLE_DETECT_N		4
#define GPIO_NR_PALMTC_USB_DETECT		5
#define GPIO_NR_PALMTC_HOTSYNC_BUTTON		7
#define GPIO_NR_PALMTC_SD_DETECT		12	// low->high when out, high->low when inserted
#define GPIO_NR_PALMTC_BL_POWER			16
#define GPIO_NR_PALMTC_USB_POWER		36

//#define GPIO_NR_PALMLD_STD_RXD			46	/* IRDA */
//#define GPIO_NR_PALMLD_STD_TXD			47

#define IRQ_GPIO_PALMTC_SD_DETECT	IRQ_GPIO(GPIO_NR_PALMTC_SD_DETECT)

/* Utility macros */

#define GET_PALMTC_GPIO(gpio) \
                (GPLR(GPIO_NR_PALMTC_ ## gpio) & GPIO_bit(GPIO_NR_PALMTC_ ## gpio))

#define SET_PALMTC_GPIO(gpio, setp) \
        do { \
                if (setp) \
                        GPSR(GPIO_NR_PALMTC_ ## gpio) = GPIO_bit(GPIO_NR_PALMTC_ ## gpio); \
                else \
                        GPCR(GPIO_NR_PALMTC_ ## gpio) = GPIO_bit(GPIO_NR_PALMTC_ ## gpio); \
        } while (0)

#define SET_PALMTC_GPIO_N(gpio, setp) \
        do { \
                if (setp) \
                        GPCR(GPIO_NR_PALMTC_ ## gpio) = GPIO_bit(GPIO_NR_PALMTC_ ## gpio); \
                else \
                        GPSR(GPIO_NR_PALMTC_ ## gpio) = GPIO_bit(GPIO_NR_PALMTC_ ## gpio); \
        } while (0)

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

#define SET_GPIO(gpio, setp) \
                do { \
                  if (setp) \
                    GPSR(gpio) = GPIO_bit(gpio); \
                  else \
                    GPCR(gpio) = GPIO_bit(gpio); \
                } while (0)

#define SET_GPIO_N(gpio, setp) \
                do { \
                  if (setp) \
                    GPCR(gpio) = GPIO_bit(gpio); \
                  else \
                    GPSR(gpio) = GPIO_bit(gpio); \
                } while (0)

#endif /* _PALMTC_GPIO_H_ */
