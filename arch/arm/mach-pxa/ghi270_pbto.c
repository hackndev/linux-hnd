/*
 * Power button time out (PBTO) handler for the GHI270 devices.
 *
 * Copyright 2007, SDG Systems, LLC
 * Authors: Aric Blumer <aric     sdgsystems.com>
 *
 *  This program is free software.  You can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/ghi270.h>

static struct input_dev *keydev;
static int pbto = 0;
static int do_esc = 0;

/*
 * These parameters can be changed at run time in
 * /sys/module/ghi270_pbto/parameters/...
 */
module_param(pbto, int, 0644);
MODULE_PARM_DESC(pbto, "How long (tenth seconds) power button must be pressed before it registers.");
/* Do escape makes things work like the Zaurus:  If you press the button in
 * less time than pbto, it sends an escape. */
module_param(do_esc, int, 0644);
MODULE_PARM_DESC(do_esc, "1 = Send escape if button held less than pbto.");

/*
 * Power button time-out:  It requires three states:  Idle, Debounce, and
 * Down.  The for the debounce state, we ignore everything, and then we start
 * looking at the GPIO to see if they change it.  This is implemented by the
 * ISR moving from the Idle state to the Debounce state.  It does so by
 * kicking off a timer which then re-enables the interrupt and changes its
 * polarity if the button is still held.  It also kicks off a timer for the
 * timeout period.  If an interrupt occurs before the timer expires, then we
 * do not suspend.  If the timer expires, we do suspend.
 */

static void power_debounce_timer_func(unsigned long junk);
DEFINE_TIMER(power_debounce_timer, power_debounce_timer_func, 0, 0);
static u64 off_jiffies64;

static volatile enum { PB_IDLE, PB_DEBOUNCE, PB_DOWN, PB_DOWN_POWER_OFF } isr_state = PB_IDLE;

static void
power_debounce_timer_func(unsigned long junk)
{
    unsigned long flags;

    local_irq_save(flags);
    switch(isr_state) {
	case PB_IDLE:
	    printk("Unexpected PB_IDLE\n");
	    break;
	case PB_DEBOUNCE:
	    if(!pxa_gpio_get_value(GHI270_GPIO0_KEY_nON)) {
		/* Power is still pressed */
		set_irq_type(gpio_to_irq(GHI270_GPIO0_KEY_nON), IRQT_RISING); /* Look for release */
		isr_state = PB_DOWN;
		mod_timer(&power_debounce_timer, jiffies + (pbto * HZ) / 10 + 2);
	    } else {
		isr_state = PB_IDLE;
		set_irq_type(gpio_to_irq(GHI270_GPIO0_KEY_nON), IRQT_FALLING);
	    }
	    enable_irq(gpio_to_irq(GHI270_GPIO0_KEY_nON));
	    break;
	case PB_DOWN:
	    if(!pxa_gpio_get_value(GHI270_GPIO0_KEY_nON)) {
		/* Still down, so let's suspend. */
		if(time_after64(jiffies_64, off_jiffies64 + msecs_to_jiffies(1500))) {
		    /* Prohibit a bunch of power key events to be queued. */
		    off_jiffies64 = jiffies_64;
		    input_report_key(keydev, KEY_POWER, 1);
		    input_report_key(keydev, KEY_POWER, 0);
		    input_sync(keydev);
		}
		PWM_PWDUTY0 = 1023;        /* Turn off the LCD light for feedback */
		isr_state = PB_DOWN_POWER_OFF;
	    } else {
		/* It's no longer down, so be safe and don't do power off. */
		isr_state = PB_IDLE;
	    }
	    break;
	case PB_DOWN_POWER_OFF:
	    printk("State error in pbto.\n");
	    isr_state = PB_IDLE;
	    break;
    }
    local_irq_restore(flags);
}

static irqreturn_t
power_isr(int irq, void *irq_desc)
{
    switch(isr_state) {
	case PB_IDLE:
	    disable_irq(gpio_to_irq(GHI270_GPIO0_KEY_nON));
	    mod_timer(&power_debounce_timer, jiffies + msecs_to_jiffies(20));
	    isr_state = PB_DEBOUNCE;
	    break;
	case PB_DEBOUNCE:
	    break;
	case PB_DOWN:
	    /* irq already enabled to get here */
	    del_timer(&power_debounce_timer);
	    set_irq_type(gpio_to_irq(GHI270_GPIO0_KEY_nON), IRQT_FALLING);
	    isr_state = PB_IDLE;
	    /* Key was released. */
	    if(do_esc) {
		input_report_key(keydev, KEY_ESC, 1);
		input_report_key(keydev, KEY_ESC, 0);
		input_sync(keydev);
	    }
	    break;
	case PB_DOWN_POWER_OFF:
	    /*
	     * This state is used for some platforms that need to halt the
	     * suspend until the user releases the power button.  This is
	     * because for some platforms, holding the power button for about
	     * 15 seconds causes a hardware reset, but only if the device is
	     * on.  So we can't sleep in that case or it won't reset.  To use
	     * it, you need to export the isr_state or something like that,
	     * and then monitor the state and only suspend if it goes to
	     * PB_IDLE.  GHI270 doesn't use this state, but I left it here in
	     * case someone wants to make this a general driver.
	     */
	    isr_state = PB_IDLE;
	    set_irq_type(gpio_to_irq(GHI270_GPIO0_KEY_nON), IRQT_FALLING);
	    break;
    }

    return IRQ_HANDLED;
}

static int __init mod_init(void)
{
    printk(KERN_INFO "GHI270 Power Button Driver\n");

    /* Power Key Stuff */
    keydev = input_allocate_device();
    if(!keydev) {
	printk("Could not allocate Power Key Device\n");
	return -ENODEV;
    } else {
	keydev->name = "power_key";
	keydev->evbit[0] = BIT(EV_KEY);
	keydev->keybit[LONG(KEY_POWER)] |= BIT(KEY_POWER);
	keydev->keybit[LONG(KEY_ESC)]   |= BIT(KEY_ESC);
	input_register_device(keydev);

	/* Power Key ISR */
	set_irq_type(gpio_to_irq(GHI270_GPIO0_KEY_nON), IRQT_FALLING);
	if (request_irq(gpio_to_irq(GHI270_GPIO0_KEY_nON), power_isr,
		IRQF_DISABLED | IRQF_TRIGGER_FALLING,
		"GHI270 Power", 0) != 0) {
	    printk(KERN_NOTICE "Unable to request POWER interrupt.\n");
	}
    }

    return 0;
}

static void __exit mod_exit(void)
{
    free_irq(gpio_to_irq(GHI270_GPIO0_KEY_nON), 0);
    input_unregister_device(keydev);
    del_timer_sync(&power_debounce_timer);
    printk(KERN_INFO "GHI270 Power Button Driver Exiting\n");
}

MODULE_AUTHOR("SDG Systems, LLC");
MODULE_DESCRIPTION("GHI270 Battery Driver");
MODULE_LICENSE("GPL");

module_init(mod_init);
module_exit(mod_exit);
/* vim600: set noexpandtab sw=4 ts=8 :*/
