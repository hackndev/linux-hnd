/*
 * Battery class device for the Toshiba e-series pdas
 *
 * Copyright (c) Ian Molton 2003
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/battery.h>
#include <asm/irq.h>
#include <asm/arch/hardware.h>
#include <asm/arch/eseries-gpio.h>

/* ---------------------------------------------------------------- */
/* Callbacks */

extern int wm97xx_read_adc(void);

static int eseries_battery_get_voltage(struct battery *bat) {
	return (wm97xx_read_adc()*3700)/1756; //FIXME - not accurate
}

static int eseries_battery_get_status(struct battery *bat) {
	if(GPLR0 & (1<<17))
		return BATTERY_STATUS_CHARGING;
	return BATTERY_STATUS_NOT_CHARGING;
}

struct battery eseries_battery = {
	.name           = "main battery",
	.get_voltage	= eseries_battery_get_voltage,
	.get_status	= eseries_battery_get_status,
};

/* ---------------------------------------------------------------- */
/* Probing */

static void eseries_battery_probe(void *dummy){
	static int registered;

	if(GPLR0 & (1<<10)){
		if(registered){
			printk("battery: duplicate reg. attempt!\n");
			return;
		}
		if(GPLR0 & (1<<26))
			eseries_battery.id = "standard battery";
		else
			eseries_battery.id = "extended battery";
		battery_class_register(&eseries_battery);
		registered = 1;
	}
	else{
		if(registered){
			battery_class_unregister(&eseries_battery);
			registered = 0;
		}
	}
}

struct work_struct task;

static irqreturn_t eseries_battery_irq(int irq, void *dev_id, struct pt_regs *regs) {
	INIT_WORK(&task, eseries_battery_probe, NULL);
	schedule_work(&task);
	return IRQ_HANDLED;
}

static int eseries_battery_init(void) {
	int ret;

	if(!(machine_is_e740() || machine_is_e750() || machine_is_e800()))
		return -ENODEV;

	ret = request_irq(IRQ_GPIO(10), eseries_battery_irq, 0, "battery", NULL);
	if(ret)
		return ret;
	set_irq_type(IRQ_GPIO(10), IRQT_BOTHEDGE);

	eseries_battery_probe(NULL);

	return 0;
}

module_init(eseries_battery_init);
