#include <linux/power_supply.h>

struct battery_adc_priv;

struct battery_adc_platform_data {
	/* Battery information and characteristics. */
	struct power_supply_info battery_info;

	/* Method to detect battery charge status (adc_battery itself can't detect it)
	   returns 0 or 1. */
	int  (*is_charging)(void);

	char *voltage_pin;
	char *current_pin;
	char *temperature_pin;

	/* Multiplier to convert voltage in raw units to uV (microvolts) */
	int voltage_mult;
	/* Multiplier to convert current in raw units to uA (microamperes) */
	int current_mult;
	/* Multiplier to convert temperature in raw units to milidegrees Celsium */
	int temperature_mult;

	/* Current battery charge status - BATTERY_STATUS_* */
	int charge_status;

	// private
	struct battery_adc_priv *drvdata;
};
