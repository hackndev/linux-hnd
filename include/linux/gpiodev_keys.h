#ifndef __GPIODEV_KEYS_H
#define __GPIODEV_KEYS_H

#include <linux/input.h>
#include <linux/gpiodev.h>

struct gpiodev_keys_button {
	/* Configuration parameters */
	int keycode;
	struct gpio gpio;
	int active_low;
	char *desc;
	int type;
};

struct gpiodev_keys_platform_data {
	struct gpiodev_keys_button *buttons;
	int nbuttons;
	struct input_dev *input;
};

#endif

