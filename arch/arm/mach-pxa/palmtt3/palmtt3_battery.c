/*
 * Battery driver lowlevel interface for Palm T|T3
 *
 * Author:  Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 * Original code by:
 *          Matt Reimer <mreimer@vpop.net>
 *          April 2004, 2005
 *
 *          Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>
 *          September 2004
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/battchargemon.h>
#include <linux/apm-emulation.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

#include <linux/mfd/tsc2101.h>
#include <asm/arch/tps65010.h>

#include <asm/arch/palmtt3-gpio.h>
#include <asm/arch/palmtt3-init.h>

#define BATTERY_CHECK_INTERVAL	(HZ * 60)	/* every 60 seconds */


int palmtt3_battery_get_max_voltage(struct battery *bat)
{
	return PALMTT3_BAT_MAX_VOLTAGE;
}

int palmtt3_battery_get_min_voltage(struct battery *bat)
{
	return PALMTT3_BAT_MIN_VOLTAGE;
}

static int match_tsc(struct device * dev, void * data)
{
	return 1;
}

int palmtt3_battery_get_voltage(struct battery *bat)
{
	struct device_driver *tscdrv;
	struct device *tscdev;
	struct tsc2101 *tscdata;
	int batv = 0;
	
	tscdrv = driver_find("tsc2101", &platform_bus_type);
	if(!tscdrv) {
		printk("PalmTT3_battery: driver tsc2101 not found\n");
		return 0;
	}
	tscdev = driver_find_device(tscdrv, NULL, NULL, match_tsc);
	if(!tscdev) {
		printk("PalmTT3_battery: device of driver tsc2101 not found\n");
		return 0;
	}
	tscdata = dev_get_drvdata(tscdev);
	if(!tscdata) {
		printk("PalmTT3_battery: could not get data\n");
		return 0;
	}
	if(!tscdata->meas) {
		printk("PalmTT3_battery: could not get tsc2101_meas data\n");
		return 0;
	}
	batv = ((struct tsc2101_meas *)tscdata->meas->priv)->bat;
	printk("PalmTT3_battery: battery register value: %d\n", batv);
	put_device(tscdev);
	put_driver(tscdrv);
	
	// TODO: does this screw up sysfs so bad?
	// battery_update_charge_link(bat);

	/*
	 Vbat = B/{2^N} * 5 * VREF
	 where
	 	B    - tscdata->miscdata.bat
	 	N    - programmed resolution of A/D (12)
	 	VREF - programmed value of internal reference (seems to be 1.25)
	*/
	return (batv*6250)/4096;
}

int palmtt3_charger_get_usb_status(struct charger *cha)
{
	u8 tps65010_get_chgstatus(void);
	return ((tps65010_get_chgstatus() & TPS_CHG_USB)?1:0);
}

int palmtt3_charger_get_ac_status(struct charger *cha)
{
	u8 tps65010_get_chgstatus(void);
	return ((tps65010_get_chgstatus() & TPS_CHG_AC)?1:0);
}

#ifdef CONFIG_BATTCHARGE_MONITOR
static struct battery palmtt3_battery = {
	.name		 = "palmtt3_batt",
	.id		 = "Li-Ion battery",
	.min_voltage	 = PALMTT3_BAT_MIN_VOLTAGE,
	.max_voltage	 = PALMTT3_BAT_MAX_VOLTAGE,
	.v_current	 = -1,
	.temp		 = -1,
	.get_voltage     = palmtt3_battery_get_voltage,
};

static struct charger palmtt3_usb_charger = {
	.name		 = "palmtt3_usb",
	.id		 = "USB",
	.get_status	 = palmtt3_charger_get_usb_status,
};

static struct charger palmtt3_ac_charger = {
	.name		 = "palmtt3_ac",
	.id		 = "AC",
	.get_status	 = palmtt3_charger_get_ac_status,
};
#endif

/* -------------------------- APM ------------------------------------- */
static void palmtt3_apm_get_power_status(struct apm_power_info *info)
{
	int tps65010_get_charging(void);
	int min, max, curr, percent;

	curr = palmtt3_battery_get_voltage(NULL);
	min  = palmtt3_battery_get_min_voltage(NULL);
	max  = palmtt3_battery_get_max_voltage(NULL);

	curr = curr - min;
	if (curr < 0) curr = 0;
	max = max - min;
	
	percent = curr*100/max;

	info->battery_life = percent;

	info->ac_line_status = tps65010_get_charging() ? APM_AC_ONLINE : APM_AC_OFFLINE;

	if (info->ac_line_status) {
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	} else {
		if (percent > 50)
			info->battery_status = APM_BATTERY_STATUS_HIGH;
		else if (percent < 5)
			info->battery_status = APM_BATTERY_STATUS_CRITICAL;
		else
			info->battery_status = APM_BATTERY_STATUS_LOW;
	}

	/* Consider one "percent" per minute, which is shot in the sky. */
	info->time = percent;
	info->units = APM_UNITS_MINS;
}

typedef void (*apm_get_power_status_t)(struct apm_power_info*);

int set_apm_get_power_status(apm_get_power_status_t t)
{
	apm_get_power_status = t;

	return 0;
}


/* -------------------------------------------------------------------- */

static int palmtt3_battery_probe(struct device *dev)
{
	printk("Battery interface for palmtt3 series\n");
	return 0;
}

static struct device_driver palmtt3_battery_driver = {
	.name	  = "palmtt3_battchargemon",
	.bus	  = &platform_bus_type,
	.probe	  = palmtt3_battery_probe,
};

static int __init palmtt3_battery_init(void)
{
	int retval;
	retval = driver_register(&palmtt3_battery_driver);
	if (retval)
	    return retval;

#ifdef CONFIG_BATTCHARGE_MONITOR
	retval = battery_class_register(&palmtt3_battery);
	retval = charger_class_register(&palmtt3_usb_charger);
	retval = charger_class_register(&palmtt3_ac_charger);

	battery_attach_charger(&palmtt3_battery, &palmtt3_usb_charger);
	battery_attach_charger(&palmtt3_battery, &palmtt3_ac_charger);

	battery_update_charge_link(&palmtt3_battery);
#endif
	if (!retval) {
#ifdef CONFIG_PM
	set_apm_get_power_status(palmtt3_apm_get_power_status);
#endif
	}
	return retval;
}

static void __exit palmtt3_battery_exit(void)
{
#ifdef CONFIG_BATTCHARGE_MONITOR
	battery_remove_charger(0, &palmtt3_ac_charger);
	battery_remove_charger(0, &palmtt3_usb_charger);

	charger_class_unregister(&palmtt3_usb_charger);
	charger_class_unregister(&palmtt3_ac_charger);
	battery_class_unregister(&palmtt3_battery);
	driver_unregister(&palmtt3_battery_driver);
#endif
}

module_init(palmtt3_battery_init);
module_exit(palmtt3_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Pouzanov");
MODULE_DESCRIPTION("Palm T|T3 battery driver");
