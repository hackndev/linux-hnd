/*
 * battchargemon.c
 *
 * Battery and charger monitor class, mostly inspired by battery.c
 *
 * (c) 2006 Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 * You may use this code as per GPL version 2
 *
 * All volatges, currents, charges, and temperatures in mV, mA, mJ and
 * tenths of a degree unless otherwise stated.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/battchargemon.h>

/**** handlers ****/
static ssize_t battery_show_id(struct class_device *cdev, char *buf) {
	struct battery *bat = container_of(cdev, struct battery, class_dev);
	return sprintf(buf, "%s\n", bat->id);
}

static ssize_t battery_show_charge(struct class_device *cdev, char *buf) {
	int v_normalized, v_max_normalized, voltage, charge;
	struct battery *bat = container_of(cdev, struct battery, class_dev);
	voltage = bat->get_voltage(bat);
	v_normalized = voltage - bat->min_voltage;
	v_normalized = (v_normalized>0) ? v_normalized : 0;
	v_max_normalized = bat->max_voltage - bat->min_voltage;
	charge = v_normalized * 100 / v_max_normalized;
	charge = (charge>0) ? (charge<100?charge:100) : 0;
	return sprintf(buf, "%d%%\n", charge);
}

static ssize_t charger_show_id(struct class_device *cdev, char *buf) {
	struct charger *bat = container_of(cdev, struct charger, class_dev);
	return sprintf(buf, "%s\n", bat->id);
}

static ssize_t battery_show_voltage(struct class_device *cdev, char *buf) {
	struct battery *cl = container_of(cdev, struct battery, class_dev);
	return sprintf(buf, "%d\n", cl->get_voltage(cl));
}

static ssize_t charger_show_status(struct class_device *cdev, char *buf) {
	struct charger *cl = container_of(cdev, struct charger, class_dev);
	return sprintf(buf, "%d\n", cl->get_status(cl));
}

#define show_int_val(_class, _name)   \
static ssize_t _class##_show_##_name(struct class_device *cdev, char *buf) {  \
	struct _class *cl = container_of(cdev, struct _class, class_dev);   \
	return sprintf(buf, "%d\n", cl->_name);          \
}

show_int_val(battery, min_voltage);
show_int_val(battery, max_voltage);
show_int_val(battery, v_current);
show_int_val(battery, temp);

static struct class battery_class = {
	.name = "battery",
//      .hotplug = battery_class_hotplug,
//      .release = battery_class_release,
};

static struct class charger_class = {
	.name = "charger",
};

#define CLS_DEV_ATTR(_class, _name, _mode, _show, _store) \
	struct class_device_attribute _class##_class_device_attr_##_name = { \
		.attr = { .name = #_name, .mode = _mode, .owner = THIS_MODULE }, \
		.show = _show, \
		.store = _store, \
	}

/**** device attributes ****/
static CLS_DEV_ATTR (battery, id, 0444, battery_show_id, NULL);
static CLS_DEV_ATTR (battery, min_voltage, 0444, battery_show_min_voltage, NULL);
static CLS_DEV_ATTR (battery, max_voltage, 0444, battery_show_max_voltage, NULL);
static CLS_DEV_ATTR (battery, voltage, 0444, battery_show_voltage, NULL);
static CLS_DEV_ATTR (battery, v_current, 0444, battery_show_v_current, NULL);
static CLS_DEV_ATTR (battery, temp, 0444, battery_show_temp, NULL);
static CLS_DEV_ATTR (battery, charge, 0444, battery_show_charge, NULL);

static CLS_DEV_ATTR (charger, id, 0444, charger_show_id, NULL);
static CLS_DEV_ATTR (charger, status, 0444, charger_show_status, NULL);

/**** class registration ****/

#define create_entry_conditional(_name) \
	(bat->_name == -1 ? 0 : \
		class_device_create_file(&bat->class_dev, \
			&battery_class_device_attr_##_name))

int battery_class_register(struct battery *bat)
{
	int rc = 0;

	// init list
	bat->attached_chargers.next = &bat->attached_chargers;
	bat->attached_chargers.prev = &bat->attached_chargers;

	WARN_ON(!bat->name);
	bat->class_dev.class = &battery_class;
	strcpy(bat->class_dev.class_id, bat->name);
	rc = class_device_register(&bat->class_dev);
	if(rc)
		goto out;

	if(bat->id)
		rc |= class_device_create_file(&bat->class_dev,
				&battery_class_device_attr_id);
	rc |= create_entry_conditional(min_voltage);
	rc |= create_entry_conditional(max_voltage);
	rc |= create_entry_conditional(v_current);
	rc |= create_entry_conditional(temp);
	if(bat->get_voltage) {
		rc |= class_device_create_file(&bat->class_dev,
				&battery_class_device_attr_voltage);
		if((bat->min_voltage != -1) && (bat->max_voltage != -1)) {
			rc |= class_device_create_file(&bat->class_dev,
					&battery_class_device_attr_charge);
		}
	}

	if(rc) {
		printk(KERN_ERR "battchargemon: "
				"creation of device entries failed\n");
		class_device_unregister(&bat->class_dev);
	}

out:
	return rc;
}
EXPORT_SYMBOL (battery_class_register);

void battery_class_unregister (struct battery *bat)
{
	class_device_unregister(&bat->class_dev);
}
EXPORT_SYMBOL (battery_class_unregister);

int charger_class_register(struct charger *cha)
{
	int rc = 0;
	WARN_ON(!cha->name);
	cha->class_dev.class = &charger_class;
	strcpy(cha->class_dev.class_id, cha->name);
	rc = class_device_register(&cha->class_dev);
	if(rc)
		goto out;

	if(cha->id)
		rc |= class_device_create_file(&cha->class_dev,
				&charger_class_device_attr_id);
	rc |= class_device_create_file(&cha->class_dev,
			&charger_class_device_attr_status);

	if(rc) {
		printk(KERN_ERR "charger: "
				"creation of device entries failed\n");
		class_device_unregister(&cha->class_dev);
	}

out:
	return rc;
}
EXPORT_SYMBOL (charger_class_register);

void charger_class_unregister (struct charger *cha)
{
	class_device_unregister(&cha->class_dev);
}
EXPORT_SYMBOL (charger_class_unregister);

/**** cherger attaching ****/

// FIXME: there's no way to attach one charger to two batteries using lists.h
//        possible workarounds: implement own linked list?
int battery_attach_charger(struct battery *batt, struct charger *cha)
{
	// TODO: do we need to grab charger device driver here? it would be removed
	//       anyway in correctly written driver...
	list_add(&cha->list, &batt->attached_chargers);
	return 0;
}
EXPORT_SYMBOL(battery_attach_charger);

int battery_remove_charger(struct battery *batt, struct charger *cha)
{
	list_del(&cha->list);
	return 0;
}
EXPORT_SYMBOL(battery_remove_charger);

void battery_update_charge_link(struct battery *batt)
{
	struct charger *cha;
	sysfs_remove_link(&batt->class_dev.kobj, "charger");
	list_for_each_entry(cha, &batt->attached_chargers, list) {
		if(cha->get_status(cha)) {
			if (sysfs_create_link(&batt->class_dev.kobj,
						&cha->class_dev.kobj,
						"charger"))
				printk(KERN_ERR "charger: "
						"sysfs link failed\n");
			return;
		}
	}
}
EXPORT_SYMBOL(battery_update_charge_link);

/**** module init/exit ****/

static int __init battcharge_class_init(void)
{
	int ret = class_register(&battery_class);
	if(!ret) {
		ret = class_register(&charger_class);
		if(!ret) {
			return 0;
		} else {
			class_unregister(&battery_class);
		}
	}
	return ret;
}

static void __exit battcharge_class_exit(void)
{
	class_unregister(&charger_class);
	class_unregister(&battery_class);
}

#ifdef MODULE
module_init(battcharge_class_init);
#else	/* start early */
subsys_initcall(battcharge_class_init);
#endif
module_exit(battcharge_class_exit);
                                                                                
MODULE_DESCRIPTION("Battery and charger driver");
MODULE_AUTHOR("Vladimir Pouzanov");
MODULE_LICENSE("GPL");
