#include <linux/leds.h>

#define H5000_SAMCOP_IRQ_BASE IRQ_BOARD_START
#define H5000_MQ11XX_IRQ_BASE (H5000_SAMCOP_IRQ_BASE + SAMCOP_NR_IRQS)

extern void (*h5400_btuart_configure)(int state);
extern void (*h5400_hwuart_configure)(int state);

EXTERN_LED_TRIGGER_SHARED(h5400_radio_trig);
