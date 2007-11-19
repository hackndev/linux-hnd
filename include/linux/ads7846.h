#include <linux/types.h>
#include <linux/adc.h>

/* Sensor pins available */
enum {
	AD7846_PIN_XPOS  = 0,
	AD7846_PIN_YPOS  = 1,
	AD7846_PIN_Z1POS = 2,
	AD7846_PIN_Z2POS = 3,
	AD7846_PIN_TEMP0 = 4,
	AD7846_PIN_TEMP1 = 5,
	AD7846_PIN_VBAT  = 6,
	AD7846_PIN_VAUX  = 7,

	AD7846_PIN_CUSTOM_START  = 8,
};

struct ads7846_ssp_platform_data {
	/* SSP port to use */
	int port;
	/* PD0 and PD1 bits to use during sampling */
	int pd_bits;
	/* SSP clock frequency in Hz */
	int freq;
	/* Callback to multiplex custom (>=AD7846_PIN_CUSTOM_START)
	   pins. Should return real pin to sample, or <0 if error. */
	int (*mux_custom)(int custom_pin);

	struct adc_classdev *custom_adcs;
	size_t num_custom_adcs;
};

// Deprecated
//int ads7846_sense(struct device *dev, struct adc_request *req);
u32 ads7846_ssp_putget(u32 data);
