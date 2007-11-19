/*
 * Battery driver lowlevel interface for iPAQ H2200 series
 *
 * Copyright (c) 2004 Matt Reimer
 *               2004 Szabolcs Gyurko
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author:  Matt Reimer <mreimer@vpop.net>
 *          April 2004, 2005
 *
 *          Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>
 *          September 2004
 *
 * To do:
 *
 *   - While suspended, periodically check for battery full and update
 *     the LED status.
 *   - Should we turn off the charger GPIO when the battery is full?
 *   - Is the battery status caching necessary?
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/battery.h>
#include <linux/leds.h>

#include <asm/io.h>
#include <asm/apm.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

#include <asm/arch-pxa/h2200.h>
#include <asm/arch-pxa/h2200-gpio.h>
#include <asm/arch-pxa/h2200-leds.h>

#include "../../w1/w1.h"
#include "../../w1/slaves/w1_ds2760.h"

#undef USE_LEDS_API

#define H2200_BATTERY_MAX_VOLTAGE 4400	/* My measurements */
#define H2200_BATTERY_MIN_VOLTAGE    0	/* Maybe incorrect */
#define H2200_BATTERY_MAX_CURRENT  950	/* MUST BE MEASURED */
#define H2200_BATTERY_MIN_CURRENT -950	/* Maybe incorrect */
#define H2200_BATTERY_MIN_CHARGE     0	/* Maybe incorrect */

#define BATTERY_CHECK_INTERVAL	(HZ * 60)	/* every 60 seconds */

#define POWER_NONE      0
#define POWER_AC        1

/* AC present? */
static int power_status;

/* w1_samcop device */
static struct workqueue_struct *probe_q;
static struct work_struct	probe_work;

/* DS2760 device */
struct device *ds2760_dev = NULL;

/* Cache the battery status for this many seconds. */
#define STATUS_CACHE_TIME (HZ * 1)
static struct ds2760_status battery_status;

static int battery_charge_state;
#define BATTERY_STATE_UNKNOWN	0
#define BATTERY_DISCHARGING	1
#define BATTERY_CHARGING	2
#define BATTERY_FULL		3

/* battery monitor */
static struct timer_list 	monitor_timer;
static struct workqueue_struct *monitor_q;
static struct work_struct	monitor_work;

#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
/* original APM hook */
static void (*apm_get_power_status_orig)(struct apm_power_info *info);
#endif


int
h2200_battery_read_status(void)
{
	if (jiffies < (battery_status.update_time + STATUS_CACHE_TIME))
		return 0;

	if (!ds2760_dev)
		return 0;

	if (!w1_ds2760_status(ds2760_dev, &battery_status)) {
		printk("call to w1_ds2760_status failed (0x%08x)\n",
		       (unsigned int)ds2760_dev);
		return 1;
	}

	return 0;
}

#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
static void 
h2200_apm_get_power_status (struct apm_power_info *info)
{
	h2200_battery_read_status();

	info->ac_line_status = (power_status != POWER_NONE) ?
		APM_AC_ONLINE : APM_AC_OFFLINE;

	if (info->ac_line_status == APM_AC_ONLINE && current > 0) {
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	} else {

		info->battery_status = battery_status.accum_current_mAh >
		    ((battery_status.rated_capacity * 75) / 100) ?
			APM_BATTERY_STATUS_HIGH :
		    battery_status.accum_current_mAh >
			((battery_status.rated_capacity* 25) / 100) ?
			APM_BATTERY_STATUS_LOW : APM_BATTERY_STATUS_CRITICAL;
	}

	info->battery_flag = info->battery_status;

	if (battery_status.rated_capacity)
		info->battery_life = (battery_status.accum_current_mAh * 100) /
				     battery_status.rated_capacity;

	if (info->battery_life > 100)
		info->battery_life = 100;
	if (info->battery_life < 0)
		info->battery_life = 0;

	if (battery_status.current_mA) {
		info->time = - ((battery_status.accum_current_mAh * 6000) /
				battery_status.current_mA) / 100;
		info->units = APM_UNITS_MINS;
	} else
		info->units = APM_UNITS_UNKNOWN;
}
#endif

int
h2200_battery_get_max_voltage(struct battery *bat)
{
	return H2200_BATTERY_MAX_VOLTAGE;
}

int
h2200_battery_get_min_voltage(struct battery *bat)
{
	return H2200_BATTERY_MIN_VOLTAGE;
}

int
h2200_battery_get_voltage(struct battery *bat)
{
	h2200_battery_read_status();
	return battery_status.voltage_mV;
}

int
h2200_battery_get_max_current(struct battery *bat)
{
	return H2200_BATTERY_MAX_CURRENT;
}

int
h2200_battery_get_min_current(struct battery *bat)
{
	return H2200_BATTERY_MIN_CURRENT;
}

int
h2200_battery_get_current(struct battery *bat)
{
	h2200_battery_read_status();
	return battery_status.current_mA;
}

int
h2200_battery_get_max_charge(struct battery *bat)
{
	h2200_battery_read_status();
	return battery_status.rated_capacity;
}

int
h2200_battery_get_min_charge(struct battery *bat)
{
	return H2200_BATTERY_MIN_CHARGE;
}

int
h2200_battery_get_charge(struct battery *bat)
{
	h2200_battery_read_status();
	return battery_status.accum_current_mAh;
}

int
h2200_battery_get_temp(struct battery *bat)
{
	h2200_battery_read_status();
	return battery_status.temp_C;
}

int
h2200_battery_get_status(struct battery *bat)
{
	int status;

	status = (GET_H2200_GPIO(AC_IN_N)) ?
		BATTERY_STATUS_NOT_CHARGING : BATTERY_STATUS_CHARGING;

	return status;
}

void
h2200_battery_update_status(void *data)
{
	static int full_counter;

	h2200_battery_read_status();

	if (battery_charge_state == BATTERY_STATE_UNKNOWN)
		full_counter = 0;

	/* Set the proper charging rate. */
	power_status = GET_H2200_GPIO(AC_IN_N) ? 0 : 1;
	if (power_status) {

		SET_H2200_GPIO(CHG_EN, 1);

		if (battery_status.current_mA > 10) {

			battery_charge_state = BATTERY_CHARGING;
			full_counter = 0;

		} else if (battery_status.current_mA < 10 &&
			   battery_charge_state != BATTERY_FULL) {

			/* Don't consider the battery to be full unless
			 * we've seen the current < 10 mA at least two
			 * consecutive times. */

			full_counter++;

			if (full_counter < 2)
				battery_charge_state = BATTERY_CHARGING;
			else  {

				unsigned char acr[2];

				acr[0] = (battery_status.rated_capacity * 4) >> 8;
				acr[1] = (battery_status.rated_capacity * 4) & 0xff;

				if (w1_ds2760_write(ds2760_dev, acr,
				    DS2760_CURRENT_ACCUM_MSB, 2) < 2)
					printk(KERN_ERR "ACR reset failed\n");
				battery_charge_state = BATTERY_FULL;
			}
		}

		if (battery_charge_state == BATTERY_CHARGING) {

			/* Blink the LED while plugged in and charging. */
#ifdef USE_LEDS_API
			//leds_set_brightness(h2200_power_led, 100);
			leds_set_frequency(h2200_power_led, 500);
#else
			h2200_set_led(0, 4, 16);
#endif
		} else {

			/* Set the LED solid while on AC and not charging. */
#ifdef USE_LEDS_API
			//leds_set_brightness(h2200_power_led, 100);
			leds_set_frequency(h2200_power_led, 0);
#else
			h2200_set_led(0, 8, 8);
#endif
		}

	} else {

		battery_charge_state = BATTERY_DISCHARGING;
		full_counter = 0;

		SET_H2200_GPIO(CHG_EN, 0);

		/* Turn off the LED while unplugged from AC. */
#ifdef USE_LEDS_API
		//leds_set_brightness(h2200_power_led, 0);
		leds_set_frequency(h2200_power_led, 0);
#else
		h2200_set_led(0, 0, 0);
#endif
	}

}

static irqreturn_t
h2200_ac_plug_isr (int isr, void *data)
{

	/* Update the LED status right away, for timely feedback. */
	power_status = GET_H2200_GPIO(AC_IN_N) ? 0 : 1;
	if (power_status) {
		/* Start off with a solid LED; if we're charging,
		   the LED state will be updated when the queued work runs. */
#ifdef USE_LEDS_API
		//leds_set_brightness(h2200_power_led, 100);
		leds_set_frequency(h2200_power_led, 0);
#else
		h2200_set_led(0, 8, 8);
#endif
	} else {
#ifdef USE_LEDS_API
		//leds_set_brightness(h2200_power_led, 0);
		leds_set_frequency(h2200_power_led, 0);
#else
		h2200_set_led(0, 0, 0);
#endif
	}

	/* Force the data to be re-read. */
	battery_status.update_time = 0;

	/* Wait a few seconds to let the DS2760 update its current reading. */
	queue_delayed_work(monitor_q, &monitor_work, HZ * 7);

	return IRQ_HANDLED;
}

static irqreturn_t
h2200_battery_door_isr (int isr, void *data)
{
	int door_open = GET_H2200_GPIO(BATT_DOOR_N) ? 0 : 1;

	if (door_open)
		printk(KERN_ERR "battery door opened!\n");
	else
		printk(KERN_ERR "battery door closed\n");

	return IRQ_HANDLED;
}

static struct battery h2200_battery = {
	.name		 = "h2200 battery",
	.id		 = "battery0",
	.get_voltage     = h2200_battery_get_voltage,
	.get_min_voltage = h2200_battery_get_min_voltage,
	.get_max_voltage = h2200_battery_get_max_voltage,
	.get_current     = h2200_battery_get_current,
	.get_min_current = h2200_battery_get_min_current,
	.get_max_current = h2200_battery_get_max_current,
	.get_charge      = h2200_battery_get_charge,
	.get_min_charge  = h2200_battery_get_min_charge,
	.get_max_charge  = h2200_battery_get_max_charge,
	.get_temp        = h2200_battery_get_temp,
	.get_status      = h2200_battery_get_status,
};

/* -------------------------------------------------------------------- */

static int
h2200_battery_match_callback(struct device *dev, void *data)
{
	struct w1_slave *sl;

	if (!(dev->driver && dev->driver->name &&
	     (strcmp(dev->driver->name, "w1_slave_driver") == 0)))
		return 0;

	sl = container_of(dev, struct w1_slave, dev);

	/* DS2760 w1 slave device names begin with the family number 0x30. */
	if (strncmp(sl->name, "30-", 3) != 0)
		return 0;

	return 1;
}

void
h2200_battery_probe_work(void *data)
{
	struct bus_type *bus;

	/* Get the battery w1 slave device. */
	bus = find_bus("w1");
	if (bus)
		ds2760_dev = bus_find_device(bus, NULL, NULL,
					     h2200_battery_match_callback);

	if (!ds2760_dev) {
		/* No DS2760 device found; try again later. */
		queue_delayed_work(probe_q, &probe_work, HZ * 5);
		return;
	}
}


static void
h2200_battery_timer(unsigned long data)
{
	queue_work(monitor_q, &monitor_work);
	mod_timer(&monitor_timer, jiffies + BATTERY_CHECK_INTERVAL);
}

static int
h2200_battery_probe(struct device *dev)
{
	printk("Battery interface for iPAQ h2200 series - (c) 2004 Szabolcs Gyurko\n");

	battery_charge_state = BATTERY_STATE_UNKNOWN;

	/* Install an interrupt handler for AC plug/unplug. */
	set_irq_type(H2200_IRQ(AC_IN_N), IRQT_BOTHEDGE);
	request_irq(H2200_IRQ(AC_IN_N), h2200_ac_plug_isr, SA_SAMPLE_RANDOM,
		     "AC plug", NULL);

	/* Install an interrupt handler for battery door open/close. */
	set_irq_type(H2200_IRQ(BATT_DOOR_N), IRQT_BOTHEDGE);
	request_irq(H2200_IRQ(BATT_DOOR_N), h2200_battery_door_isr,
		    SA_SAMPLE_RANDOM, "battery door", NULL);

	/* Create a workqueue in which the battery monitor will run. */
	monitor_q = create_singlethread_workqueue("battmon");
	INIT_WORK(&monitor_work, h2200_battery_update_status, NULL);

	/* Create a timer to run the battery monitor every
	   BATTERY_CHECK_INTERVAL seconds. */
	init_timer(&monitor_timer);
	monitor_timer.function = h2200_battery_timer;
	monitor_timer.data = 0;

	/* Start the first monitor task a few seconds from now. */
	mod_timer(&monitor_timer, jiffies + HZ * 10);

#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
	apm_get_power_status = h2200_apm_get_power_status;
#endif

#ifdef USE_LEDS_API
	leds_acquire(h2200_power_led); /* XXX check return val */
#endif

	/* Get the DS2760 W1 device. */
	probe_q = create_singlethread_workqueue("battmonprb");
	INIT_WORK(&probe_work, h2200_battery_probe_work, NULL);
	h2200_battery_probe_work(NULL);

	return 0;
}

static void
h2200_battery_shutdown(struct device *dev)
{
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
	apm_get_power_status = apm_get_power_status_orig;
#endif

	del_timer_sync(&monitor_timer);

	cancel_delayed_work(&monitor_work);
	flush_workqueue(monitor_q);
	destroy_workqueue(monitor_q);

	cancel_delayed_work(&probe_work);
	flush_workqueue(probe_q);
	destroy_workqueue(probe_q);

	if (ds2760_dev)
		put_device(ds2760_dev);

	free_irq(H2200_IRQ(AC_IN_N), NULL);
	free_irq(H2200_IRQ(BATT_DOOR_N), NULL);

#ifdef USE_LEDS_API
	//leds_set_brightness(h2200_power_led, 0);
	leds_set_frequency(h2200_power_led, 0);
	leds_release(h2200_power_led);
#else
	h2200_set_led(0, 0, 0);
#endif
}

static int
h2200_battery_remove(struct device *dev)
{
	h2200_battery_shutdown(dev);
	return 0;
}

static int
h2200_battery_suspend(struct device *dev, pm_message_t state)
{
	cancel_delayed_work(&monitor_work);
	flush_workqueue(monitor_q);

	return 0;
}

static int
h2200_battery_resume(struct device *dev)
{
	battery_charge_state = BATTERY_STATUS_UNKNOWN;
	queue_work(monitor_q, &monitor_work);
	mod_timer(&monitor_timer, jiffies + HZ * 5);

	return 0;
}

static struct device_driver h2200_battery_driver = {
	.name	  = "h2200 battery",
	.bus	  = &platform_bus_type,
	.probe	  = h2200_battery_probe,
	.remove   = h2200_battery_remove,
	.shutdown = h2200_battery_shutdown,
	.suspend  = h2200_battery_suspend,
	.resume	  = h2200_battery_resume
};

static int __init
h2200_battery_init(void)
{
	int retval;

	retval = driver_register(&h2200_battery_driver);
	if (retval)
	    return retval;

	retval = battery_class_register(&h2200_battery);

	return retval;
}

static void __exit h2200_battery_exit(void)
{
	battery_class_unregister(&h2200_battery);
	driver_unregister(&h2200_battery_driver);
}

module_init(h2200_battery_init);
module_exit(h2200_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>");
MODULE_DESCRIPTION("HP iPAQ h2200 battery driver");

/*
 * Local Variables:
 *  mode:c
 *  c-style:"K&R"
 *  c-basic-offset:8
 * End:
 *
 */
