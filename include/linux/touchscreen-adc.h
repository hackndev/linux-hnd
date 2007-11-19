#include "adc.h"

struct tsadc_platform_data {
	// Information on pen down gpio.
	int pen_gpio;
	int (*ispendown)(void);

	// Details of touchscreen
	/* If you know resistance of TS panel, and really want to know 
	   "resistance of touch" (whatever that is), set this. In use by 
	   ts-adc. */
	int x_plate_ohms;
	/* Otherwise, of more interest is relative pressure of touch.
	   Use this to normalize standard stylus touch pressure value to
	   be in range TBD */
	int pressure_factor;
	/* If relative pressure goes below this value, consider stylus is
	   being lifted, and stop TS sampling. */
	int min_pressure;
	/* Some touchscreen controllers inherently cannot provide 
	   correct pressure value when pen up is in progress. For example,
	   x & y may be already invalid, but pressure still appear higher
	   than min_pressure. For these, set this to true, and current 
	   measurement validity will be checked by the *next* pressure 
	   value. */
	int delayed_pressure;

	// Debouncing parameters
	/* Number of X/Y samplings done for one measurement */
	int num_xy_samples;
	/* Number of Z1/Z2 samplings done for one measurement */
	int num_z_samples;
	/* Samples during measurement must converge to be within bounds
	   of this value, or the whole measurement will be discarded. */
	int max_jitter;

	// ADC chip info.
	int max_sense;
	char *x_pin, *y_pin, *z1_pin, *z2_pin;
};
