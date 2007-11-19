/*
 * LED MMC-Card Activity Trigger
 *
 * Author: Jan Herman <2hp@seznam.cz>
 *
 * Based on code IDE-disk Trigger from Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/leds.h>

static void ledtrig_mmc_timerfunc(unsigned long data);

DEFINE_LED_TRIGGER(ledtrig_mmc);
static DEFINE_TIMER(ledtrig_mmc_timer, ledtrig_mmc_timerfunc, 0, 0);
static int mmc_activity;
static int mmc_lastactivity;

void ledtrig_mmc_activity(void)
{
	mmc_activity++;
	if (!timer_pending(&ledtrig_mmc_timer))
		mod_timer(&ledtrig_mmc_timer, jiffies + msecs_to_jiffies(10));
}
EXPORT_SYMBOL(ledtrig_mmc_activity);

static void ledtrig_mmc_timerfunc(unsigned long data)
{
	if (mmc_lastactivity != mmc_activity) {
		mmc_lastactivity = mmc_activity;
		led_trigger_event(ledtrig_mmc, LED_FULL);
	    	mod_timer(&ledtrig_mmc_timer, jiffies + msecs_to_jiffies(10));
	} else {
		led_trigger_event(ledtrig_mmc, LED_OFF);
	}
}

static int __init ledtrig_mmc_init(void)
{
	led_trigger_register_simple("mmc-card", &ledtrig_mmc);
	return 0;
}

static void __exit ledtrig_mmc_exit(void)
{
	led_trigger_unregister_simple(ledtrig_mmc);
}

module_init(ledtrig_mmc_init);
module_exit(ledtrig_mmc_exit);

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>");
MODULE_DESCRIPTION("LED MMC Card Activity Trigger");
MODULE_LICENSE("GPL");
