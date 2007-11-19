/*
 * Copyright (c) 2007 Paul Sokolovsky
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 */

//#define DEBUG

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/adc.h>
#include <linux/adc_battery.h>

#include <asm/irq.h>

#define PIN_NO_VOLT 0
#define PIN_NO_CURR 1
#define PIN_NO_TEMP 2

struct battery_adc_priv {
	struct power_supply batt_cdev;

	struct battery_adc_platform_data *pdata;

	struct adc_request req;
	struct adc_sense pins[3];
	struct adc_sense last_good_pins[3];

	struct workqueue_struct *wq;
	struct delayed_work work;
};

/*
 *  Battery properties
 */

static int adc_battery_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val)
{
	struct battery_adc_priv* drvdata = (struct battery_adc_priv*)psy;
	int voltage;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = drvdata->pdata->charge_status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = drvdata->pdata->battery_info.voltage_max_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = drvdata->pdata->battery_info.voltage_min_design;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = drvdata->pdata->battery_info.charge_full_design;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
		val->intval = drvdata->pdata->battery_info.charge_empty_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = drvdata->last_good_pins[PIN_NO_VOLT].value * drvdata->pdata->voltage_mult;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = drvdata->last_good_pins[PIN_NO_CURR].value * drvdata->pdata->current_mult;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		/* We do calculations in mX, not uX, because todo it in uX we should use "long long"s,
		 * which is a mess (need to use do_div) when you need divide operation). */
		voltage = drvdata->last_good_pins[PIN_NO_VOLT].value * drvdata->pdata->voltage_mult;
		val->intval = ((voltage/1000 - drvdata->pdata->battery_info.voltage_min_design/1000) *
		     (drvdata->pdata->battery_info.charge_full_design/1000 -
		      drvdata->pdata->battery_info.charge_empty_design/1000)) /
		     (drvdata->pdata->battery_info.voltage_max_design/1000 -
		      drvdata->pdata->battery_info.voltage_min_design/1000);
		val->intval *= 1000; /* convert final result to uX */ 
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = drvdata->last_good_pins[PIN_NO_TEMP].value * drvdata->pdata->temperature_mult / 1000;
		break;
	default:
		return -EINVAL;
	};
	return 0;
}

/*
 *  Driver body
 */

static void adc_battery_query(struct battery_adc_priv *drvdata)
{
	struct battery_adc_platform_data *pdata = drvdata->pdata;
	int powered, charging;

	adc_request_sample(&drvdata->req);

	powered = power_supply_am_i_supplied(&drvdata->batt_cdev);
	charging = pdata->is_charging ? pdata->is_charging() : -1;

	if (powered && charging)
		pdata->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	else if (powered && !charging && charging != -1)
		pdata->charge_status = POWER_SUPPLY_STATUS_FULL;
	else
		pdata->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/* Throw away invalid samples, this may happen soon after resume for example. */
	if (drvdata->pins[PIN_NO_VOLT].value > 0) {
	        memcpy(drvdata->last_good_pins, drvdata->pins, sizeof(drvdata->pins));
#ifdef DEBUG
		printk("%d %d %d\n", drvdata->pins[PIN_NO_VOLT].value, 
				     drvdata->pins[PIN_NO_CURR].value, 
				     drvdata->pins[PIN_NO_TEMP].value);
#endif
	}
}

static void adc_battery_charge_power_changed(struct power_supply *bat)
{
	struct battery_adc_priv *drvdata = (struct battery_adc_priv*)bat;
	cancel_delayed_work(&drvdata->work);
	queue_delayed_work(drvdata->wq, &drvdata->work, 0);
}

static void adc_battery_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = container_of(work, struct delayed_work, work);
	struct battery_adc_priv *drvdata = container_of(delayed_work, struct battery_adc_priv, work);

	adc_battery_query(drvdata);
	power_supply_changed(&drvdata->batt_cdev);

	queue_delayed_work(drvdata->wq, &drvdata->work, (5000 * HZ) / 1000);
}

static int adc_battery_probe(struct platform_device *pdev)
{
        int retval;
	struct battery_adc_platform_data *pdata = pdev->dev.platform_data;
	struct battery_adc_priv *drvdata;
	int i, j;
	enum power_supply_property props[] = {
		POWER_SUPPLY_PROP_STATUS,
		POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
		POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
		POWER_SUPPLY_PROP_VOLTAGE_NOW,
		POWER_SUPPLY_PROP_CURRENT_NOW,
		POWER_SUPPLY_PROP_CHARGE_NOW,
		POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
		POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
		POWER_SUPPLY_PROP_TEMP,
	};

	// Initialize ts data structure.
	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->batt_cdev.name           = pdata->battery_info.name;
	drvdata->batt_cdev.use_for_apm    = pdata->battery_info.use_for_apm;
	drvdata->batt_cdev.num_properties = ARRAY_SIZE(props);
	drvdata->batt_cdev.get_property   = adc_battery_get_property;
	drvdata->batt_cdev.external_power_changed =
	                          adc_battery_charge_power_changed;

	if (!pdata->voltage_pin) {
		drvdata->batt_cdev.num_properties--;
		props[3] = -1;
	}
	if (!pdata->current_pin) {
		drvdata->batt_cdev.num_properties--;
		props[4] = -1;
	}
	if (!pdata->temperature_pin) {
		drvdata->batt_cdev.num_properties--;
		props[8] = -1;
	}

	drvdata->batt_cdev.properties = kmalloc(
	                sizeof(*drvdata->batt_cdev.properties) *
	                drvdata->batt_cdev.num_properties, GFP_KERNEL);
	if (!drvdata->batt_cdev.properties)
		return -ENOMEM;

	j = 0;
	for (i = 0; i < ARRAY_SIZE(props); i++) {
		if (props[i] == -1)
			continue;
		drvdata->batt_cdev.properties[j++] = props[i];
	}

	retval = power_supply_register(&pdev->dev, &drvdata->batt_cdev);
	if (retval) {
		printk("adc-battery: Error registering battery classdev");
		return retval;
	}

	drvdata->req.senses = drvdata->pins;
	drvdata->req.num_senses = ARRAY_SIZE(drvdata->pins);
	drvdata->pins[PIN_NO_VOLT].name = pdata->voltage_pin;
	drvdata->pins[PIN_NO_CURR].name = pdata->current_pin;
	drvdata->pins[PIN_NO_TEMP].name = pdata->temperature_pin;

	adc_request_register(&drvdata->req);

	/* Here we assume raw values in mV */
	if (!pdata->voltage_mult)
		pdata->voltage_mult = 1000;
	/* Here we assume raw values in mA */
	if (!pdata->current_mult)
		pdata->current_mult = 1000;
	/* Here we assume raw values in 1/10 C */
	if (!pdata->temperature_mult)
		pdata->temperature_mult = 1000;

	drvdata->pdata = pdata;
	pdata->drvdata = drvdata; /* Seems ugly, we need better solution */

	platform_set_drvdata(pdev, drvdata);

        // Load initial values ASAP
	adc_battery_query(drvdata);

	// Still schedule next sampling soon
	INIT_DELAYED_WORK(&drvdata->work, adc_battery_work_func);
	drvdata->wq = create_workqueue(pdev->dev.bus_id);
	if (!drvdata->wq)
		return -ESRCH;

	queue_delayed_work(drvdata->wq, &drvdata->work, (5000 * HZ) / 1000);

	return retval;
}

static int adc_battery_remove(struct platform_device *pdev)
{
	struct battery_adc_priv *drvdata = platform_get_drvdata(pdev);
	cancel_delayed_work(&drvdata->work);
	destroy_workqueue(drvdata->wq);
	power_supply_unregister(&drvdata->batt_cdev);
	adc_request_unregister(&drvdata->req);
	kfree(drvdata->batt_cdev.properties);
	return 0;
}

static struct platform_driver adc_battery_driver = {
	.driver		= {
		.name	= "adc-battery",
	},
	.probe		= adc_battery_probe,
	.remove		= adc_battery_remove,
};

static int __init adc_battery_init(void)
{
	return platform_driver_register(&adc_battery_driver);
}

static void __exit adc_battery_exit(void)
{
	platform_driver_unregister(&adc_battery_driver);
}

module_init(adc_battery_init)
module_exit(adc_battery_exit)

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("Battery driver for ADC device");
MODULE_LICENSE("GPL");
