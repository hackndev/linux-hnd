#include "adc.h"

// Driver for the AD7877 chip.

// Sensors pins available.
enum {
	AD7877_SEQ_YPOS  = 0,
	AD7877_SEQ_XPOS  = 1,
	AD7877_SEQ_Z2    = 2,
	AD7877_SEQ_AUX1  = 3,
	AD7877_SEQ_AUX2  = 4,
	AD7877_SEQ_AUX3  = 5,
	AD7877_SEQ_BAT1  = 6,
	AD7877_SEQ_BAT2  = 7,
	AD7877_SEQ_TEMP1 = 8,
	AD7877_SEQ_TEMP2 = 9,
	AD7877_SEQ_Z1    = 10,

	AD7877_NR_SENSE  = 11,
};

struct ad7877_platform_data {
	int dav_irq;
};

// Sense function - deprecated
//int ad7877_sense(struct device *dev, struct adc_request *req);
