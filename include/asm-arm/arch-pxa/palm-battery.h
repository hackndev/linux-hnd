/*
 * Palm battery info structure.
 */

struct palm_battery_data {
	int bat_min_voltage;
	int bat_max_voltage;
	int bat_max_life_mins;
	int (*ac_connected)();
};
