/*
 *
 */
#include <linux/gpiodev.h>
#include <linux/dpm.h>

struct pxa2xx_udc_gpio_info {
	struct gpio detect_gpio;
	int detect_gpio_negative;
	struct dpm_hardware_ops power_ctrl;	
};

