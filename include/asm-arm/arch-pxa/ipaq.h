#include <asm/arch-sa1100/h3600.h>
#include <asm/hardware/ipaq-ops.h>

#ifdef CONFIG_MACH_H3900
#include <asm/arch/h3900-gpio.h>
#endif

#ifndef __ASSEMBLER__

extern void ipaq_backlight_power (int on);
extern void ipaq_map_io (void);

struct egpio_irq_info {
	int egpio_nr;
	int gpio_nr; /* GPIO_GPIO(n) */
	int irq;  /* IRQ_GPIOn */
};

#endif
