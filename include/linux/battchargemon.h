/*
 *  battchargemon.h
 *
 *  Battery and charger monitor class
 *
 *  (c) 2006 Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>
 *
 *  You may use this code as per GPL version 2
 *
 */

#ifndef _LINUX_BATTCHARGEMON_H
#define _LINUX_BATTCHARGEMON_H

#include <linux/device.h>

struct battery {
	struct class_device class_dev;
	const char *name;
	char *id;

	int min_voltage;
	int max_voltage;
	
	int v_current;
	int temp;

	int (*get_voltage)(struct battery *);

	struct list_head attached_chargers;
};

struct charger {
	struct class_device class_dev;
	const char *name;
	char *id;

#define CHARGER_DISCONNECTED	0
#define CHARGER_CONNECTED	1
	int (*get_status)(struct charger *);

	struct list_head list;
};

extern int battery_class_register(struct battery *);
extern void battery_class_unregister(struct battery *);

extern int charger_class_register(struct charger *);
extern void charger_class_unregister(struct charger *);

extern int battery_attach_charger(struct battery *batt, struct charger *cha);
extern int battery_remove_charger(struct battery *batt, struct charger *cha);
extern void battery_update_charge_link(struct battery *batt);

#endif

