#ifndef __SOC_SAMCOP_ADC_H__
#define __SOC_SAMCOP_ADC_H__

#include <linux/adc.h>

#define SAMCOP_ADC_PIN_X    1
#define SAMCOP_ADC_PIN_Y    2
#define SAMCOP_ADC_PIN_Z1   3
#define SAMCOP_ADC_PIN_Z2   4
#define SAMCOP_ADC_PIN_AIN0 5 /* Light sensor on H5000 */
#define SAMCOP_ADC_PIN_AIN1 6
#define SAMCOP_ADC_PIN_AIN2 7
/* AIN3 used by touchscreen, thus use PIN_{X,Y,Z{1,2}} */

struct samcop_adc_platform_data {
	u_int16_t delay;
	struct adc_classdev *acdevs;
	size_t num_acdevs;

	void (*set_power)(int pin_id, int on);
};

struct samcop_adc {
	struct clk *clk;
	struct resource res;
	void *map;
	unsigned int irq;
	struct completion comp;
	struct samcop_adc_platform_data *pdata;
};

#endif /* __SOC_SAMCOP_ADC_H__ */
