/*
 * HTC simple EGPIO irq and gpio extender
 */

#ifndef __HTC_EGPIO_H__
#define __HTC_EGPIO_H__

#include <linux/gpiodev.h> /* struct gpiodev_ops */

enum {
	/* Maximum number of 16 bit registers a chip may have. */
	MAX_EGPIO_REGS = 8,
	/* Number of IRQS the EGPIO chip will claim. */
	MAX_EGPIO_IRQS = 16,
};

/* Available pin types. */
enum {
	/* This pin corresponds to an input gpio */
	HTC_EGPIO_TYPE_INPUT,
	/* This pin corresponds to an irq that does not have an
	 * associated input gpio */
	HTC_EGPIO_TYPE_IRQ,
	/* This pin corresponds to an output gpio */
	HTC_EGPIO_TYPE_OUTPUT,
};

/* Information on each pin on the chip. */
struct htc_egpio_pinInfo {
	/* The bit offset of the pin (eg, 18 is the third bit of the
	 * second register). */
	int pin_nr;
	/* The type - input, irq, output */
	int type;
	/* For output pins - the poweron default */
	int output_initial;
	/* For input pins with a corresponding irq, the irqs bit offset. */
	int input_irq;
};

/* Platform data description provided by the arch */
struct htc_egpio_platform_data {
	/* This is 'gpiodev' capable */
	struct gpiodev_ops ops;
	/* Beginning of available irqs (eg, IRQ_BOARD_START) */
	int irq_base;
	/* Beginning of available gpios (eg, GPIO_BASE_INCREMENT) */
	int gpio_base;
	/* Shift register number by this value (bus_shift=1 for 32bit register alignment) */
	int bus_shift;
	/* Set if chip requires writing '0' to ack an irq */
	int invertAcks;
	/* Number of registers (optional if all output pins specified
	 * below) */
	int nrRegs;
	/* The location of the irq ack register */
	int ackRegister;

	/* List of pin descriptions.  One must specify all input pins
	 * that have a corresponding irq pin and all output pins with
	 * a non-zero start value.  Specifying other pins is
	 * optional. */
	struct htc_egpio_pinInfo *pins;
	int nr_pins;
};

/* Determine the wakeup irq, to be called during early resume */
extern int htc_egpio_get_wakeup_irq(struct device *dev);

#endif
