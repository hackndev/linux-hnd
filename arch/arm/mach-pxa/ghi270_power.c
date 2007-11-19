/*
 * ghi270_power.c  --  Battery monitoring GHI270 H and HG
 *
 * Copyright 2007 SDG Systems, LLC
 * Authors: Aric Blumer <aric     sdgsystems.com>
 *
 *  This program is free software.  You can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/apm-emulation.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

#include <asm/gpio.h>
#include <asm/arch/ghi270.h>
#include <asm/mach-types.h>

static int speriod = 10;  /* Sample at least every 10 seconds (default) */
module_param(speriod, int, 0644);
MODULE_PARM_DESC(speriod, "How often (seconds) to check battery");

static u64 last_update;
static unsigned short normal_i2c[] = { 0x9, 0xb, I2C_CLIENT_END };
static unsigned short probe[I2C_CLIENT_MAX_OPTS] = I2C_CLIENT_DEFAULTS;
static unsigned short ignore[I2C_CLIENT_MAX_OPTS] = I2C_CLIENT_DEFAULTS;
/*
 * We force the 0xb device on bus 1 because it is a device within the battery,
 * and the battery may not be attached.  So we don't want to probe for this
 * device.  We read the LTC4100, and its battery_present bit tells us if it is
 * actually there.
 */
static unsigned short force[] = { 1, 0xb, I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short *forces[] = { force, 0 };
static struct i2c_client_address_data addr_data = {
	.normal_i2c	= normal_i2c,
	.probe		= probe,
	.ignore		= ignore,
	.forces		= forces,
};
static struct i2c_driver ghi270_driver;
static struct i2c_client client_ltc4100;
static int ltc4100_attached;
static struct i2c_client client_bq20z80;
static int bq20z80_attached;
static int onAC;
static int batteryPresent;
static int batteryCharging;
static int rsoc;
static int voltage;
static int temp;
static int time;
static int current_now;

static int update_params(void);

#ifdef CONFIG_POWER_SUPPLY

static int
get_property(struct power_supply *b, enum power_supply_property psp,
             union power_supply_propval *val)
{
	if (time_after64(jiffies_64, last_update + speriod * HZ)) {
		int retry = 10;
		while (!update_params()) {
			msleep(200);
			if (!--retry) {
				printk(KERN_ERR "ghi270_power: "
					"Battery comm fail\n");
				break;
			}
		}
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = 100;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = current_now;
			break;
		case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CHARGE_NOW:
			val->intval = rsoc;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = voltage;
			break;
		case POWER_SUPPLY_PROP_STATUS:
			if (batteryCharging) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			} else {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
			val->intval = time;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = temp;
			break;
		default:
			return -EINVAL;
	};

	return 0;
}

static enum power_supply_property props[] = {
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static struct power_supply battery_info = {
	.name = "ghi270",
	.get_property = get_property,
	.properties = props,
	.num_properties = ARRAY_SIZE(props),
};
#endif

static struct delayed_work battery_work;

#ifdef CONFIG_APM_POWER
#    error CONFIG_APM_POWER must not be defined for GHI270.
#endif

#ifdef CONFIG_PM
static void
my_apm_get_power_status(struct apm_power_info *info)
{
	if (!info) return;

	if (time_after64(jiffies_64, last_update + speriod * HZ)) {
		if (!update_params()) {
			schedule_delayed_work(&battery_work, HZ);
		}
	}

	info->ac_line_status = onAC ? APM_AC_ONLINE : APM_AC_OFFLINE;

	if (batteryCharging) {
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	} else {
		if (batteryPresent) {
			info->battery_status = APM_BATTERY_STATUS_HIGH;
		} else {
			info->battery_status = APM_BATTERY_STATUS_NOT_PRESENT;
		}
	}

	info->battery_life = rsoc;
	info->time         = time;
	info->units        = APM_UNITS_MINS;

	info->battery_flag = (1 << info->battery_status);
}
#endif


DECLARE_MUTEX(update_mutex);

static int
update_params(void)
{
	int status;
	int retval = 1;
	static u64 first_failure = 0;

	if (!ltc4100_attached) return 0;

	down(&update_mutex);

	status = i2c_smbus_read_word_data(&client_ltc4100, 0x13);

	if (status != -1) {
		if (first_failure) {
			printk(KERN_DEBUG "Time between failures: %llu\n",
				jiffies_64 - first_failure);
			first_failure = 0;
		}

		last_update    = jiffies_64;
		onAC           = (status & (1 << 15)) ? 1 : 0;
		batteryPresent = (status & (1 << 14)) ? 1 : 0;

		if (machine_is_ghi270()) {
			batteryCharging =
				pxa_gpio_get_value(GHI270_H_GPIO26_CHGEN)?1:0;
		} else {
			batteryCharging =
				pxa_gpio_get_value(GHI270_HG_GPIO53_CHGEN)?1:0;
		}

		rsoc        = 0;
		voltage     = 0;
		temp        = 0;
		time        = 0;
		current_now = 0;

		if (batteryPresent && bq20z80_attached) {
			/* Only read the battery itself when it is there. */
			int tmp;
			tmp = i2c_smbus_read_word_data(&client_bq20z80, 0xd);
			if (tmp != -1) {
				/* Relative state of charge */
				pr_debug("RSOC: %d\n", tmp);
				rsoc = tmp;
			} else printk(KERN_ERR "rsoc fail\n");
			tmp = i2c_smbus_read_word_data(&client_bq20z80, 0x9);
			if (tmp != -1) {
				pr_debug("Voltage: %d\n", tmp);
				voltage = tmp;
			} else printk(KERN_ERR "voltage fail\n");
			tmp = i2c_smbus_read_word_data(&client_bq20z80, 0x8);
			if (tmp != -1) {
				pr_debug("Temperature: %d\n", tmp);
				temp = tmp;
			} else printk(KERN_ERR "temp fail\n");
			tmp = i2c_smbus_read_word_data(&client_bq20z80, 0x11);
			if (tmp != -1) {
				pr_debug("Time: %d\n", tmp);
				time = tmp;
			} else printk(KERN_ERR "time fail\n");
			tmp = i2c_smbus_read_word_data(&client_bq20z80, 0xa);
			if (tmp != -1) {
				pr_debug("Current: %d\n", tmp);
				current_now = -((short)tmp);
			} else printk(KERN_ERR "current fail\n");
		}
	} else {
		retval = 0;
		if (!first_failure) {
			/* There has been a problem with reading the LTC4100
			 * on the proto HG that I'm using.  Hopefully others
			 * will not have this problem.  The H does not, and
			 * the battery circuitry is the same.
			 */
			printk(KERN_DEBUG "First failure\n");
			first_failure = jiffies_64;
		}
	}

	up(&update_mutex);

	return retval;
}


static void
battery_work_callback(struct work_struct *data)
{
	static int failcount = 0;
	static int failflag = 0;
	if (!update_params()) {
		if (!failflag) failcount++;
		if (failcount == 10) {
			printk(KERN_ERR "Cannot communicate with "
				"battery controller.\n");
			failflag = 1;
			failcount = 0;
		}
		schedule_delayed_work(&battery_work, HZ);
	} else {
		if (failflag) {
			failflag = 0;
			printk(KERN_ERR "Battery OK now\n");
		}
		failcount = 0;
	}
}

static irqreturn_t
smbus_isr(int this_irq, void *dev_id)
{
	schedule_delayed_work(&battery_work, 0);
	return IRQ_HANDLED;
}

static int
detect(struct i2c_adapter *adapter, int address, int kind)
{
	int err = 0;

	if (address == 9) {
		/* LTC4100 */

		if (!i2c_check_functionality(adapter,
				I2C_FUNC_SMBUS_WORD_DATA)) {
			goto exit;
		}

		memset(&client_ltc4100, 0, sizeof(client_ltc4100));

		i2c_set_clientdata(&client_ltc4100, &client_ltc4100);
		client_ltc4100.addr = address;
		client_ltc4100.adapter = adapter;
		client_ltc4100.driver = &ghi270_driver;
		strlcpy(client_ltc4100.name, "ghi270_ltc4100", I2C_NAME_SIZE);

		if (0x2 !=
		    (err = i2c_smbus_read_word_data(&client_ltc4100, 0x11))) {
			if (err != -1) {
				printk(KERN_ERR "Did not read expected\n");
			}
		}

		if ((err = i2c_attach_client(&client_ltc4100))) {
			goto exit;
		}

		INIT_DELAYED_WORK(&battery_work, battery_work_callback);
		if (!update_params()) {
			mdelay(100);
			if (!update_params()) {
				mdelay(100);
				if (!update_params()) {
				    printk(KERN_ERR "Battery update fail\n");
				}
			}
		}
#		ifdef CONFIG_POWER_SUPPLY
			power_supply_register(0, &battery_info);
#		endif
		ltc4100_attached = 1;
	} else {
		/* bq20z80 */
		if (bq20z80_attached) {
			printk(KERN_ERR "Already Attached!!\n");
			return 0;
		}


		memset(&client_bq20z80, 0, sizeof(client_bq20z80));

		i2c_set_clientdata(&client_bq20z80, &client_bq20z80);
		client_bq20z80.addr = address;
		client_bq20z80.adapter = adapter;
		client_bq20z80.driver = &ghi270_driver;
		strlcpy(client_bq20z80.name, "ghi270_bq20z80", I2C_NAME_SIZE);

		/* This device may not be on the i2c bus at any given time
		 * because it is in the battery itself. */

		if ((err = i2c_attach_client(&client_bq20z80))) {
			goto exit;
		}
		bq20z80_attached = 1;
	}
	return 0;

exit:
	return err;
}

static int
attach_adapter(struct i2c_adapter *adapter)
{
	int retval = 0;
	if (i2c_adapter_id(adapter) == 1) {
		retval = i2c_probe(adapter, &addr_data, detect);
	}
	return retval;
}

static int
detach_client(struct i2c_client *client)
{
	int err;

	if (client->addr == 9) {
		cancel_delayed_work(&battery_work);
		flush_scheduled_work();

#       	ifdef CONFIG_POWER_SUPPLY
			power_supply_unregister(&battery_info);
#       	endif
	}

	if ((err = i2c_detach_client(client))) {
		return err;
	}

	return 0;
}

static int
suspend(struct i2c_client *i2cc, pm_message_t mesg)
{
	/* Make sure we get a fresh read on resume. */
	last_update = 0;
	return 0;
}

static struct i2c_driver ghi270_driver = {
	.driver = {
		.name = "ghi270_battery",
	},
	.id 		= 0xfefe,	/* This ID is apparently unused. */
	.attach_adapter = attach_adapter,
	.detach_client 	= detach_client,
	.suspend 	= suspend,
};

static int __init mod_init(void)
{
	printk(KERN_INFO "GHI270 Power Driver\n");
	if (machine_is_ghi270()) {
		gpio_direction_input(GHI270_H_GPIO26_CHGEN);
	} else {
		gpio_direction_input(GHI270_HG_GPIO53_CHGEN);
	}
	gpio_direction_input(GHI270_GPIO13_PWR_INT);

#	ifdef CONFIG_PM
		apm_get_power_status = my_apm_get_power_status;
#	endif

	if (request_irq(gpio_to_irq(GHI270_GPIO13_PWR_INT), smbus_isr,
		IRQF_DISABLED | IRQF_TRIGGER_FALLING, "SMBus", 0)) {
		printk(KERN_ERR "ghi270_battery: can't get SMBus IRQ\n");
		/* Not Fatal */
	}

	return i2c_add_driver(&ghi270_driver);
}

static void __exit mod_exit(void)
{
	free_irq(gpio_to_irq(GHI270_GPIO13_PWR_INT), 0);
#	ifdef CONFIG_PM
		apm_get_power_status = 0;
#	endif
	i2c_del_driver(&ghi270_driver);
	printk(KERN_INFO "GHI270 Power Driver Exiting\n");
}

MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("GHI270 Battery Driver");
MODULE_LICENSE("GPL");

module_init(mod_init);
module_exit(mod_exit);
/* vim600: set noexpandtab sw=8 ts=8 :*/
