#include <asm/arch/irqs.h>
#include <linux/gpiodev2.h>

/*
CS3#    0x0C000000, 1 ;HTC Asic3 chip select
CS4#    0x10000000, 1 ;SD I/O controller
CS5#    0x14000000, 1 ;DBG LED REGISTER
        0x15000000, 1 ;DBG LAN REGISTER
        0x16000000, 1 ;DBG PPSH REGISTER */

static inline int h4000_machine_is_h4000(void)
{
	return machine_is_h4000() || machine_is_h4300();
}

#define H4000_ASIC3_IRQ_BASE IRQ_BOARD_START
#define H4000_ASIC3_GPIO_BASE GPIO_BASE_INCREMENT

/* ads7846 custom (multiplexed) pins */
#define AD7846_PIN_CUSTOM_VBAT AD7846_PIN_CUSTOM_START
#define AD7846_PIN_CUSTOM_IBAT AD7846_PIN_CUSTOM_START + 1
#define AD7846_PIN_CUSTOM_VBACKUP AD7846_PIN_CUSTOM_START + 2
#define AD7846_PIN_CUSTOM_TBAT AD7846_PIN_CUSTOM_START + 3
#define AD7846_PIN_CUSTOM_UNK AD7846_PIN_CUSTOM_START + 7

/* Backlight */
#define H4000_MAX_INTENSITY 0x3ff
