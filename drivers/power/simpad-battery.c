/*
 *  linux/drivers/misc/simpad-battery.c
 *
 *  Copyright (C) 2005 Holger Hans Peter Freyther
 *  Copyright (C) 2001 Juergen Messerer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Read the Battery Level through the UCB1x00 chip. T-Sinuspad is
 * unsupported for now.
 *
 */

#include <linux/battery.h>
#include <asm/dma.h>
#include "ucb1x00.h"


/*
 * Conversion from AD -> mV
 * 7.5V = 1023   7.3313mV/Digit
 *
 * 400 Units == 9.7V
 * a     = ADC value
 * 21    = ADC error
 * 12600 = Divident to get 2*7.3242
 * 860   = Divider to get 2*7.3242
 * 170   = Voltagedrop over
 */
#define CALIBRATE_BATTERY(a)   ((((a + 21)*12600)/860) + 170)

/*
 * We have two types of batteries a small and a large one
 * To get the right value we to distinguish between those two
 * 450 Units == 15 V
 */
#define CALIBRATE_SUPPLY(a)   (((a) * 1500) / 45)
#define MIN_SUPPLY            12000 /* Less then 12V means no powersupply */

/*
 * Charging Current
 * if value is >= 50 then charging is on
 */
#define CALIBRATE_CHARGING(a)    (((a)* 1000)/(152/4)))

struct simpad_battery_t {
	struct battery  battery;
	struct ucb1x00* ucb;

        /*
	 * Variables for the values to one time support
	 * T-Sinuspad as well
	 */
	int min_voltage;
	int min_current;
	int min_charge;

	int max_voltage;
	int max_current;
	int max_charge;

	int min_supply;
	int charging_led_label;
	int charging_max_label;
	int batt_full;
	int batt_low;
	int batt_critical;
	int batt_empty;
};

static int simpad_get_min_voltage(struct battery* _battery )
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return  battery->min_voltage;
}

static int simpad_get_min_current(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return battery->min_current;
}

static int simpad_get_min_charge(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return battery->min_charge;
}

static int simpad_get_max_voltage(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return battery->max_voltage;
}

static int simpad_get_max_current(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return battery->max_current;
}

static int simpad_get_max_charge(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;
	return battery->max_charge;
}

static int simpad_get_temp(struct battery* _battery)
{
	return 0;
}

static int simpad_get_voltage(struct battery* _battery)
{
	int val;
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;


	ucb1x00_adc_enable(battery->ucb);
	val = ucb1x00_adc_read(battery->ucb, UCB_ADC_INP_AD1, UCB_NOSYNC);
	ucb1x00_adc_disable(battery->ucb);

	return CALIBRATE_BATTERY(val);
}

static int simpad_get_current(struct battery* _battery)
{
	int val;
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;

	ucb1x00_adc_enable(battery->ucb);
	val = ucb1x00_adc_read(battery->ucb, UCB_ADC_INP_AD3, UCB_NOSYNC);
	ucb1x00_adc_disable(battery->ucb);

	return val;
}

static int simpad_get_charge(struct battery* _battery)
{
	int val;
	struct simpad_battery_t* battery = (struct simpad_battery_t*)_battery;

	ucb1x00_adc_enable(battery->ucb);
	val = ucb1x00_adc_read(battery->ucb, UCB_ADC_INP_AD2, UCB_NOSYNC);
	ucb1x00_adc_disable(battery->ucb);

	return CALIBRATE_SUPPLY(val);

}

static int simpad_get_status(struct battery* _battery)
{
	struct simpad_battery_t* battery = (struct simpad_battery_t*)(_battery);
	int vcharger = simpad_get_voltage(_battery);
	int icharger = simpad_get_current(_battery);

	int status = BATTERY_STATUS_UNKNOWN;
	if(icharger > battery->charging_led_label)
		status = BATTERY_STATUS_CHARGING;
	else if(vcharger > battery->min_supply)
		status = BATTERY_STATUS_NOT_CHARGING;
	else
		status = BATTERY_STATUS_DISCHARGING;

	return status;
}

static struct simpad_battery_t simpad_battery  = {
	.battery = {
		.get_min_voltage = simpad_get_min_voltage,
		.get_min_current = simpad_get_min_current,
		.get_min_charge  = simpad_get_min_charge,
		.get_max_voltage = simpad_get_max_voltage,
		.get_max_current = simpad_get_max_current,
		.get_max_charge  = simpad_get_max_charge,
		.get_temp        = simpad_get_temp,
		.get_voltage     = simpad_get_voltage,
		.get_current     = simpad_get_current,
		.get_charge      = simpad_get_charge,
		.get_status      = simpad_get_status,
	},
	.min_voltage = 0,
	.min_current = 0,
	.min_charge  = 0,
	.max_voltage = 0,
	.max_current = 0,
	.max_charge  = 0,

	.min_supply         = 1200,
	.charging_led_label = 18,
	.charging_max_label = 265,
	.batt_full          = 8300,
	.batt_low           = 7300,
	.batt_critical      = 6800,
	.batt_empty         = 6500,
};



/*
 * UCB glue code
 */
static int ucb1x00_battery_add(struct class_device *dev)
{
	struct ucb1x00 *ucb = classdev_to_ucb1x00(dev);
	simpad_battery.ucb  = ucb;

	battery_class_register(&simpad_battery.battery);

	return 0;
}

static void ucb1x00_battery_remove(struct class_device *dev)
{
	return battery_class_unregister(&simpad_battery.battery);
}


static struct ucb1x00_class_interface ucb1x00_battery_interface = {
	.interface   = {
		.add    = ucb1x00_battery_add,
		.remove = ucb1x00_battery_remove,
	},
};


static int __init battery_register(void)
{
	return ucb1x00_register_interface(&ucb1x00_battery_interface);
}

static void __exit battery_unregister(void)
{
	ucb1x00_unregister_interface(&ucb1x00_battery_interface);
}

module_init(battery_register);
module_exit(battery_unregister);

MODULE_AUTHOR("Holger Hans Peter Freyther");
MODULE_LICENSE("GPL");
