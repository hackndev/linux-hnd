#include <linux/input.h>

struct asic3_keys_button {
	/* Configuration parameters */
	int keycode;
	int gpio;
	int active_low;
	char *desc;
	int type;
	/* Internal state vars - add below */
};

struct asic3_keys_platform_data {
	struct asic3_keys_button *buttons;
	int nbuttons;
        struct input_dev *input;
        struct device *asic3_dev;
};
