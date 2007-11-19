/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * h3600 atmel micro companion support, battery subdevice
 * based on previous kernel 2.4 version 
 * Author : Alessandro Gardich <gremlin@gremlin.it>
 *
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
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

#include <asm/arch/hardware.h>

#include <asm/arch/h3600.h>
#include <asm/arch/SA-1100.h>

#include <asm/hardware/micro.h>

#define BATT_PERIOD 10*HZ

#define H3600_BATT_STATUS_HIGH         0x01
#define H3600_BATT_STATUS_LOW          0x02
#define H3600_BATT_STATUS_CRITICAL     0x04
#define H3600_BATT_STATUS_CHARGING     0x08
#define H3600_BATT_STATUS_CHARGEMAIN   0x10
#define H3600_BATT_STATUS_DEAD         0x20 /* Battery will not charge */
#define H3600_BATT_STATUS_NOTINSTALLED 0x20 /* For expansion pack batteries */
#define H3600_BATT_STATUS_FULL         0x40 /* Battery fully charged (and connected to AC) */
#define H3600_BATT_STATUS_NOBATTERY    0x80
#define H3600_BATT_STATUS_UNKNOWN      0xff


//static struct power_supply_dev *micro_battery;

static micro_private_t *p_micro;

struct timer_list batt_timer;

struct { 
	int ac;
	int update_time;
	int chemistry;
	int voltage;
	int temperature;
	int flag;
} micro_battery;

static void micro_battery_receive (int len, unsigned char *data) {
	if (0) { 
		printk(KERN_ERR "h3600_battery - AC = %02x\n", data[0]);
		printk(KERN_ERR "h3600_battery - BAT1 chemistry = %02x\n", data[1]);
		printk(KERN_ERR "h3600_battery - BAT1 voltage = %d %02x%02x\n", (data[3]<<8)+data[2], data[2], data[3]);
		printk(KERN_ERR "h3600_battery - BAT1 status = %02x\n", data[4]);
	}

	micro_battery.ac = data[0];
	micro_battery.chemistry = data[1];
	micro_battery.voltage = ((((unsigned short)data[3]<<8)+data[2]) * 5000L ) * 1000 / 1024;
	micro_battery.flag = data[4];

	if (len == 9) { 
		if (0) { 
			printk(KERN_ERR "h3600_battery - BAT2 chemistry = %02x\n", data[5]);
			printk(KERN_ERR "h3600_battery - BAT2 voltage = %d %02x%02x\n", (data[7]<<8)+data[6], data[6], data[7]);
			printk(KERN_ERR "h3600_battery - BAT2 status = %02x\n", data[8]);
		}
	}
}

static void micro_temperature_receive (int len, unsigned char *data) {
	micro_battery.temperature = ((unsigned short)data[1]<<8)+data[0];
}

void h3600_battery_read_status(unsigned long data) {

	if (++data % 2) 
		h3600_micro_tx_msg(0x09,0,NULL);
	else 
		h3600_micro_tx_msg(0x06,0,NULL);

	batt_timer.expires += BATT_PERIOD;
	batt_timer.data = data;

	add_timer(&batt_timer);
}

int get_capacity(struct power_supply *b) {
    switch (micro_battery.flag) { 
    case H3600_BATT_STATUS_HIGH :     return 100; break;
    case H3600_BATT_STATUS_LOW :      return 50; break;
    case H3600_BATT_STATUS_CRITICAL : return 5; break;
    default: break;
    }
    return 0;
}

int get_status(struct power_supply *b) {

   if (micro_battery.flag == H3600_BATT_STATUS_UNKNOWN)
      return POWER_SUPPLY_STATUS_UNKNOWN;

   if (micro_battery.flag & H3600_BATT_STATUS_FULL) 
      return POWER_SUPPLY_STATUS_FULL;

   if ((micro_battery.flag & H3600_BATT_STATUS_CHARGING) ||
       (micro_battery.flag & H3600_BATT_STATUS_CHARGEMAIN))
      return POWER_SUPPLY_STATUS_CHARGING;
 
   return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int micro_batt_get_property(struct power_supply *b,
                                   enum power_supply_property psp,
                                   union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_status(b);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 4700000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_capacity(b);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = micro_battery.temperature;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = micro_battery.voltage;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static enum power_supply_property micro_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static struct power_supply h3600_battery = {
	.name               = "main-battery",
	.properties         = micro_batt_props,
	.num_properties     = ARRAY_SIZE(micro_batt_props),
	.get_property       = micro_batt_get_property,
	.use_for_apm        = 1,
};

static int micro_batt_probe (struct platform_device *pdev)
{
	if (1) printk(KERN_ERR "micro battery probe : begin\n");

	power_supply_register(&pdev->dev, &h3600_battery);

	{ /*--- callback ---*/
		p_micro = platform_get_drvdata(pdev);
		spin_lock(p_micro->lock);
		p_micro->h_batt = micro_battery_receive;
		p_micro->h_temp = micro_temperature_receive;
		spin_unlock(p_micro->lock);
	}

	{ /*--- timer ---*/
		init_timer(&batt_timer);
		batt_timer.expires = jiffies + BATT_PERIOD;
		batt_timer.data = 0;
		batt_timer.function = h3600_battery_read_status;

		add_timer(&batt_timer);
	}

	if (1) printk(KERN_ERR "micro battery probe : end\n");
	return 0;
}

static int micro_batt_remove (struct platform_device *pdev)
{
	power_supply_unregister(&h3600_battery);
	{ /*--- callback ---*/
		init_timer(&batt_timer);
		p_micro->h_batt = NULL; 
		p_micro->h_temp = NULL; 
		spin_unlock(p_micro->lock);
	}
	{ /*--- timer ---*/
		del_timer_sync(&batt_timer);
	}
        return 0;
}

static int micro_batt_suspend ( struct platform_device *pdev, pm_message_t state) 
{
	{ /*--- timer ---*/
		del_timer(&batt_timer);
	}
	return 0;
}

static int micro_batt_resume ( struct platform_device *pdev) 
{
	{ /*--- timer ---*/
		add_timer(&batt_timer);
	}
	return 0;
}

struct platform_driver micro_batt_device_driver = {
	.driver  = {
		.name    = "h3600-micro-battery",
	},
	.probe   = micro_batt_probe,
	.remove  = micro_batt_remove,
	.suspend = micro_batt_suspend,
	.resume  = micro_batt_resume,
};

static int micro_batt_init (void) 
{
	return platform_driver_register(&micro_batt_device_driver);
}

static void micro_batt_cleanup (void) 
{
	platform_driver_unregister (&micro_batt_device_driver);
}

module_init (micro_batt_init);
module_exit (micro_batt_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gremlin.it");
MODULE_DESCRIPTION("driver for iPAQ Atmel micro battery");


