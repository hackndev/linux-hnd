/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * h3600 atmel micro companion support, key subdevice
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
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/platform_device.h>

#include <asm/arch/hardware.h>

#include <asm/arch/h3600.h>
#include <asm/arch/SA-1100.h>

#include <asm/hardware/micro.h>

/*--- methods ---*/
static micro_private_t *p_micro;

/*--- keys ---*/
static struct input_dev *micro_key_input;

#define NUM_KEYS 10 
int keycodes[NUM_KEYS] = {
	_KEY_RECORD,  // 1:  Record button
	_KEY_CALENDAR,// 2:  Calendar
	_KEY_CONTACTS,// 3:  Contacts (looks like Outlook)
	_KEY_MAIL,    // 4:  Envelope (Q on older iPAQs)
	_KEY_HOMEPAGE,// 5:  Start (looks like swoopy arrow)
	KEY_UP,       // 6:  Up
	KEY_RIGHT,    // 7:  Right
	KEY_LEFT,     // 8:  Left
	KEY_DOWN      // 9:  Down
};

static void micro_key_receive (int len, unsigned char *data)
{
	int key, down;

	if (0) printk(KERN_ERR "micro key receive\n");

	down = (0x80 & data[0])?1:0;
	key  = 0x7f & data[0]; 
	
	if (key < NUM_KEYS ) {
		input_report_key(micro_key_input, keycodes[key], down);
		input_sync(micro_key_input);
	}
}

static int micro_key_probe (struct platform_device *pdev)
{
	int i;
	
	if (0) printk(KERN_ERR "micro key probe : begin\n");

	micro_key_input = input_allocate_device();

	micro_key_input->evbit[0] = BIT(EV_KEY);
	set_bit(EV_KEY, micro_key_input->evbit);
	for (i = 0; i < NUM_KEYS; i++) {
		set_bit (keycodes[i], micro_key_input->keybit);
	}

	micro_key_input->name = "h3600 micro keys";

        input_register_device (micro_key_input);

	/*--- callback ---*/
	p_micro = dev_get_drvdata(&(pdev->dev));
	spin_lock(p_micro->lock);
	p_micro->h_key = micro_key_receive;
	spin_unlock(p_micro->lock);
	
	if (0) printk(KERN_ERR "micro key probe : end\n");

	return 0;
}

static int micro_key_remove (struct platform_device *pdev)
{
        input_unregister_device (micro_key_input);
	{
		spin_lock(p_micro->lock);
		p_micro->h_key = NULL;
		spin_unlock(p_micro->lock);
	}

        return 0;
}

//static void micro_key_release (struct device *dev) { }

static int micro_key_suspend ( struct platform_device *pdev, pm_message_t statel) 
{
	{
		spin_lock(p_micro->lock);
		p_micro->h_key = NULL;
		spin_unlock(p_micro->lock);
	}
	return 0;
}

static int micro_key_resume ( struct platform_device *pdev) 
{
	{
		spin_lock(p_micro->lock);
		p_micro->h_key = NULL;
		spin_unlock(p_micro->lock);
	}
	return 0;
}

struct platform_driver micro_key_device_driver = {
	.driver = {
		.name    = "h3600-micro-keys",
	},
	.probe   = micro_key_probe,
	.remove  = micro_key_remove,
	.suspend = micro_key_suspend,
	.resume  = micro_key_resume,
};

static int micro_key_init (void) 
{
	return platform_driver_register(&micro_key_device_driver);
}

static void micro_key_cleanup (void) 
{
	platform_driver_unregister (&micro_key_device_driver);
}

module_init (micro_key_init);
module_exit (micro_key_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gremlin.it");
MODULE_DESCRIPTION("driver for iPAQ Atmel micro keys");


