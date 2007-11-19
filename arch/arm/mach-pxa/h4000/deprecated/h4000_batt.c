/*
 * Copyright (C) 2003 Joshua Wise
 * Copyright (C) 2005 Pawel Kolodziejski
 * Copyright (C) 2006-7 Paul Sokolovsky
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/mfd/asic3_base.h>
#include <linux/platform_device.h>
#include <linux/battery.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch-pxa/h4000-gpio.h>
#include <asm/arch-pxa/h4000-asic.h>
#include <asm/apm.h>

//#define DEBUG

/* Actually had battery fault at 3358 -pfalcon
   Also, wince appears to shutdown at much higer
   value, and using this one gives battery percentages
   completely not matching wince. Anyway, way to solve 
   this is to figure out how to get battery *capacity*,
   not voltage.
*/
#define BATTERY_MIN 3400
#define BATTERY_MAX 3950

/* from ads7846.c */
/* The ADS7846 has touchscreen and other sensors.
 * Earlier ads784x chips are somewhat compatible.
 */
#define ADS_START               (1 << 7)
#define ADS_A2A1A0_d_y          (1 << 4)        /* differential */
#define ADS_A2A1A0_d_z1         (3 << 4)        /* differential */
#define ADS_A2A1A0_d_z2         (4 << 4)        /* differential */
#define ADS_A2A1A0_d_x          (5 << 4)        /* differential */
#define ADS_A2A1A0_temp0        (0 << 4)        /* non-differential */
#define ADS_A2A1A0_vbatt        (2 << 4)        /* non-differential */
#define ADS_A2A1A0_vaux         (6 << 4)        /* non-differential */
#define ADS_A2A1A0_temp1        (7 << 4)        /* non-differential */
#define ADS_8_BIT               (1 << 3)
#define ADS_12_BIT              (0 << 3)
#define ADS_SER                 (1 << 2)        /* non-differential */
#define ADS_DFR                 (0 << 2)        /* differential */
#define ADS_PD10_PDOWN          (0 << 0)        /* lowpower mode + penirq */
#define ADS_PD10_ADC_ON         (1 << 0)        /* ADC on */
#define ADS_PD10_REF_ON         (2 << 0)        /* vREF on + penirq */
#define ADS_PD10_ALL_ON         (3 << 0)        /* ADC + vREF on */


extern struct platform_device h4000_asic3;
#define asic3 &h4000_asic3.dev
extern void h4000_set_led(int color, int duty_time, int cycle_time);
extern u32 ads7846_ssp_putget(u32 data);
static int h4000_get_sample(int source);

static struct timer_list timer_bat;

static struct {
	int voltage;
	int icurrent;
	int temperature;
	int backup_voltage;
	int unk;
	int vbat;
} h4000_batt_info;

/*
 *  Battery classdev
 */

int get_min_voltage(struct battery *b)
{
    return 0;
}
int get_min_current(struct battery *b)
{
    return 0;
}
int get_min_charge(struct battery *b)
{
    return 0;
}
int get_max_voltage(struct battery *b)
{
    return 4200; /* mV */
}
int get_max_current(struct battery *b)
{
    return 500; /* mA */
}
int get_max_charge(struct battery *b)
{
    return 1000; /* mAh */
}
int get_status(struct battery *bat)
{
	int status = GET_H4000_GPIO(CHARGING) ?
		BATTERY_STATUS_CHARGING : BATTERY_STATUS_NOT_CHARGING;
	return status;
}
int get_voltage(struct battery *b)
{
    return h4000_batt_info.voltage;
}
int bat_get_current(struct battery *b)
{
    return h4000_batt_info.icurrent;
}
int get_temp(struct battery *b)
{
    return h4000_batt_info.temperature;
}

int backup_get_voltage(struct battery *b)
{
    return h4000_batt_info.backup_voltage;
}

static struct battery h4000_battery_main = {
    .name               = "h4000_main",
    .id                 = "main",
    .get_min_voltage    = get_min_voltage,
    .get_min_current    = get_min_current,
    .get_min_charge     = get_min_charge,
    .get_max_voltage    = get_max_voltage,
    .get_max_current    = get_max_current,
    .get_max_charge     = get_max_charge,
    .get_temp           = get_temp,
    .get_voltage        = get_voltage,
    .get_current        = bat_get_current,
//    .get_charge         = get_charge,
    .get_status         = get_status,
};
static struct battery h4000_battery_backup = {
    .name               = "h4000_backup",
    .id                 = "backup",
    .get_voltage        = backup_get_voltage,
};

static int h4000_get_sample(int source)
{
        int sample, mux_b = 0, mux_d = 0;
        int addr = ADS_A2A1A0_vaux;

        if (source == -1)  addr = ADS_A2A1A0_vbatt;
        if (source & 1)  mux_d |= GPIOD_MUX_SEL0;
        if (source & 2)  mux_d |= GPIOD_MUX_SEL1;
        if (source & 4)  mux_b |= GPIOB_MUX_SEL2;
        /* Select sampling source */
	asic3_set_gpio_out_b(asic3, GPIOB_MUX_SEL2, mux_b);
	asic3_set_gpio_out_d(asic3, GPIOD_MUX_SEL1|GPIOD_MUX_SEL0, mux_d);
	/* Likely not yet fully settled, by seems stable */
	udelay(50);
	sample = ads7846_ssp_putget(ADS_START | addr | ADS_12_BIT | ADS_SER | ADS_PD10_ADC_ON);
	return sample;
}

static void h4000_battery_query(void)
{
	int sample;

	sample = h4000_get_sample(0);
	/* Throw away invalid samples, this does happen soon after resume for example. */
	if (sample > 0) {
		h4000_batt_info.voltage = sample;
		h4000_batt_info.icurrent = h4000_get_sample(1);
		h4000_batt_info.backup_voltage = h4000_get_sample(2);
		h4000_batt_info.temperature = h4000_get_sample(3);
		h4000_batt_info.unk = h4000_get_sample(7);
		h4000_batt_info.vbat = h4000_get_sample(-1);
#ifdef DEBUG
		printk("%d %d %d %d %d %d\n", h4000_batt_info.voltage, 
					      h4000_batt_info.icurrent, 
					      h4000_batt_info.backup_voltage, 
					      h4000_batt_info.temperature, 
					      h4000_batt_info.unk, 
					      h4000_batt_info.vbat);
#endif
	}
}

static void h4000_battery_timer_func(unsigned long nr)
{
	h4000_battery_query();
	mod_timer(&timer_bat, jiffies + (5000 * HZ) / 1000);
}

static void h4000_apm_get_power_status(struct apm_power_info *info)
{
	int battery_power = ((h4000_batt_info.voltage - BATTERY_MIN) * 100) / (BATTERY_MAX - BATTERY_MIN);
	/* There's nothing impossible with battery being overcharged or overdischarged
	if (battery_power < 0)
		battery_power = 0;
	else if (battery_power > 100)
		battery_power = 100;
	*/

	info->battery_life = battery_power;

	if (!GET_H4000_GPIO(AC_IN_N))
		info->ac_line_status = APM_AC_ONLINE;
	else
		info->ac_line_status = APM_AC_OFFLINE;

	if (GET_H4000_GPIO(CHARGING))
		info->battery_status = APM_BATTERY_STATUS_CHARGING;
	/*else if (!(GPLR(GPIO_NR_H4000_MBAT_IN) & GPIO_bit(GPIO_NR_H4000_MBAT_IN)))
		info->battery_status = APM_BATTERY_STATUS_NOT_PRESENT;*/
	else {
		if (info->battery_life > 50)
			info->battery_status = APM_BATTERY_STATUS_HIGH;
		else if (info->battery_life > 5)
			info->battery_status = APM_BATTERY_STATUS_LOW;
		else
			info->battery_status = APM_BATTERY_STATUS_CRITICAL;
	}

	/* Consider one "percent" per minute, which is shot in the sky. */
	info->time = battery_power;
	info->units = APM_UNITS_MINS;
}

static int h4000_batt_probe(struct platform_device *dev)
{
        int retval;

        // Load initial values ASAP
	h4000_battery_query();

	init_timer(&timer_bat);
	timer_bat.function = h4000_battery_timer_func;
	timer_bat.data = (unsigned long)NULL;
	// Still schedule next sampling soon
	mod_timer(&timer_bat, jiffies + (500 * HZ) / 1000);

#ifdef CONFIG_PM
	apm_get_power_status = h4000_apm_get_power_status;
#endif
	retval = battery_class_register(&h4000_battery_main);
	if (retval)
		printk("h4000_batt: Error registering main battery classdev");
	retval = battery_class_register(&h4000_battery_backup);
	if (retval)
		printk("h4000_batt: Error registering backup battery classdev");

	return retval;
}

static struct platform_driver h4000_batt_driver = {
	.driver		= {
		.name	= "h4000-batt",
	},
	.probe		= h4000_batt_probe,
};

static int __init h4000_batt_init(void)
{
	if (!machine_is_h4000())
		return -ENODEV;

	return platform_driver_register(&h4000_batt_driver);
}

static void __exit h4000_batt_exit(void)
{
	del_timer_sync(&timer_bat);
	platform_driver_unregister(&h4000_batt_driver);
}

module_init(h4000_batt_init)
module_exit(h4000_batt_exit)

MODULE_AUTHOR("Paul Sokolovsky");
MODULE_DESCRIPTION("Battery driver for the iPAQ h4000");
MODULE_LICENSE("GPL");
