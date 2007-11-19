#include <linux/gpiodev.h>
#include <linux/dpm.h>

struct rs232_serial_pdata {
        struct delayed_work debounce_task;
	struct gpio detect_gpio;
	struct dpm_hardware_ops power_ctrl;
};
