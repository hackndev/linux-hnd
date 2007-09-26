/************************************************************************
 * drivers/power/palm_battery.c						*
 * Battery driver for Palm devices					*
 *									*
 * Author: Michal Hrusecky <Michal.Hrusecky@seznam.cz>			*
 *									*	
 * Based on code for Palm TX by						*
 *									*
 *  Authors: Jan Herman <2hp@seznam.cz>					*
 *  									*
 ************************************************************************/

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/apm-emulation.h>
#include <linux/wm97xx.h>

#include <asm/delay.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <asm/arch/palm-battery.h>

extern struct palm_battery_data palm_battery_info;

struct palm_battery_dev
{
  struct wm97xx * wm;
  int battery_registered;
  int current_voltage;
  int previous_voltage;
  u32 last_battery_update;
};

struct palm_battery_dev bat;

#if defined(CONFIG_APM_EMULATION) || defined(CONFIG_APM_MODULE)
/* original APM hook */
static void (*apm_get_power_status_orig)(struct apm_power_info *info);
#endif

/*
 This formula is based on battery life of my battery 1100mAh. Original battery in Zire72 is Li-On 920mAh
 V_batt = ADCSEL_BMON * 1,889 + 767,8 [mV]
*/

int palm_battery_get_voltage(struct power_supply *b)
{
    if (bat.battery_registered) {
    	bat.previous_voltage = bat.current_voltage;
    	bat.current_voltage = wm97xx_read_aux_adc(bat.wm,  WM97XX_AUX_ID3);
	bat.last_battery_update = jiffies;
    	return bat.current_voltage * 1889/1000 + 7678/10;
    } else {
    	printk("palm_battery: cannot get voltage -> battery driver unregistered\n");
    	return 0;
    }
}

int palm_battery_get_capacity(struct power_supply *b)
{
    if (bat.battery_registered)
        return (((palm_battery_get_voltage(b)-palm_battery_info.bat_min_voltage)
        /(palm_battery_info.bat_max_voltage-palm_battery_info.bat_min_voltage))*100);
    else{
        printk("palm_battery: cannot get capacity -> battery driver unregistered\n");
        return 0;
    }
}

int palm_battery_get_status(struct power_supply *b)
{
        if ( palm_battery_info.ac_connected() &&
        ( ( bat.current_voltage > bat.previous_voltage ) ||
        (bat.current_voltage <= palm_battery_info.bat_max_voltage) ) )
                return POWER_SUPPLY_STATUS_CHARGING;
        else
                return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int palm_battery_get_property(struct power_supply *b,
                                enum power_supply_property psp,
                                union power_supply_propval *val)
{
        switch (psp) {
        case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
                val->intval = palm_battery_info.bat_max_voltage;
                break;
        case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
                val->intval = palm_battery_info.bat_min_voltage;
                break;
        case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
                val->intval = 100;
                break;
        case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
                val->intval = 0;
                break;
        case POWER_SUPPLY_PROP_CHARGE_NOW:
                val->intval = palm_battery_get_capacity(b);
                break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
                val->intval = palm_battery_get_voltage(b);
                break;
        case POWER_SUPPLY_PROP_STATUS:
                val->intval = palm_battery_get_status(b);
                break;
        default:
                break;
        };

        return 0;
}

static enum power_supply_property palm_battery_props[] = {
        POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
        POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
        POWER_SUPPLY_PROP_CHARGE_NOW,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_STATUS,
};

struct power_supply palm_battery = {
	.name		= "palm_battery",
        .get_property   = palm_battery_get_property,
        .properties     = palm_battery_props,
        .num_properties = ARRAY_SIZE(palm_battery_props),
};

static int palm_wm97xx_probe(struct device *dev)
{
    struct wm97xx *wm = dev->driver_data;
    bat.wm = wm;
    return 0;
}

static int palm_wm97xx_remove(struct device *dev)
{
    return 0;
}

static void
palm_wm97xx_shutdown(struct device *dev)
{
#if defined(CONFIG_APM_EMULATION) || defined(CONFIG_APM_MODULE)
    apm_get_power_status = apm_get_power_status_orig;
#endif
}

static int
palm_wm97xx_suspend(struct device *dev, pm_message_t state)
{
	return 0;
}

static int
palm_wm97xx_resume(struct device *dev)
{
	return 0;
}


static struct device_driver  palm_wm97xx_driver = {
    .name	= "wm97xx-touchscreen",
    .bus	= &wm97xx_bus_type,
    .owner	= THIS_MODULE,
    .probe	= palm_wm97xx_probe,
    .remove	= palm_wm97xx_remove,
    .suspend	= palm_wm97xx_suspend,
    .resume	= palm_wm97xx_resume,
    .shutdown	= palm_wm97xx_shutdown
};


#if defined(CONFIG_APM_EMULATION) || defined(CONFIG_APM_MODULE)

/* APM status query callback implementation */
static void palm_apm_get_power_status(struct apm_power_info *info)
{
	int min, max, curr, percent;

	curr = palm_battery_get_voltage(&palm_battery);
	min  = palm_battery_info.bat_min_voltage;
	max  = palm_battery_info.bat_max_voltage;

	curr = curr - min;
	if (curr < 0) curr = 0;
	max = max - min;
	
	percent = curr*100/max;

	info->battery_life = percent;

	info->ac_line_status = palm_battery_info.ac_connected() ? APM_AC_ONLINE : APM_AC_OFFLINE;

	if (info->ac_line_status)
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	else {
		if (percent > 50)
			info->battery_status = APM_BATTERY_STATUS_HIGH;
		else if (percent < 5)
			info->battery_status = APM_BATTERY_STATUS_CRITICAL;
		else
			info->battery_status = APM_BATTERY_STATUS_LOW;
	}

	info->time = percent * palm_battery_info.bat_max_life_mins/100;
	info->units = APM_UNITS_MINS;
}
#endif
static int __init palm_wm97xx_init(void)
{
#ifndef MODULE
    int ret;
#endif

    /* register battery to APM layer */
    bat.battery_registered = 0;

    if(power_supply_register(NULL, &palm_battery))
	printk(KERN_ERR "palm_ac97_probe: could not register battery class\n");
    else {
	bat.battery_registered = 1;
	printk("Battery registered\n");
    }
#if defined(CONFIG_APM_EMULATION) || defined(CONFIG_APM_MODULE)
    apm_get_power_status_orig = apm_get_power_status;
    apm_get_power_status = palm_apm_get_power_status;
#endif
#ifndef MODULE
    /* If we're in kernel, we could accidentally be run before wm97xx
    and thus have panic */
    if((ret = bus_register(&wm97xx_bus_type)) < 0)
        return ret;
#endif
    return driver_register(&palm_wm97xx_driver);
}

static void __exit palm_wm97xx_exit(void)
{
    /* TODO - recover APM callback to original state */
    power_supply_unregister(&palm_battery);
    driver_unregister(&palm_wm97xx_driver);
}

module_init(palm_wm97xx_init);
module_exit(palm_wm97xx_exit);

/* Module information */
MODULE_AUTHOR("Michal Hrusecky <Michal.Hrusecky@seznam.cz>");
MODULE_DESCRIPTION("Universal battery driver for Palm devices");
MODULE_LICENSE("GPL");
