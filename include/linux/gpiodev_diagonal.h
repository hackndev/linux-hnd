#ifndef __GPIODEV_DIAGONAL_H
#define __GPIODEV_DIAGONAL_H

#include <linux/input.h>
#include <linux/timer.h>
#include <linux/gpiodev.h>

#define GPIODEV_DIAG_UP   	1
#define GPIODEV_DIAG_RIGHT	2
#define GPIODEV_DIAG_LEFT	4
#define GPIODEV_DIAG_DOWN	8
#define GPIODEV_DIAG_ACTION	16

struct gpiodev_diagonal_button {
	/* Configuration parameters */
	struct gpio gpio;
	int active_low;
	int directions;
	char *desc;
};

struct gpiodev_diagonal_platform_data {
	struct gpiodev_diagonal_button *buttons;
	int nbuttons;
	int dir_disables_enter;
	struct input_dev *input;
	struct timer_list debounce_timer;
};

#endif

