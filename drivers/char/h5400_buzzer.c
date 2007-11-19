/*
 * H5400 buzzer driver
 * Copyright 2003 Phil Blundell <pb@nexus.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/hardware/h5400-buzzer.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h5400-gpio.h>
#include <asm/mach-types.h>

static struct timer_list    buzz_timer;
static int on_time, off_time;

#define SET_BUZZER(x)	SET_H5400_GPIO_N(MOTOR_ON_N, x)

static void 
buzz_timer_callback (unsigned long nr)
{
	if (nr) {
		SET_BUZZER (0);
		buzz_timer.data = 0;
		mod_timer (&buzz_timer, jiffies + off_time);
	} else {
		SET_BUZZER (1);
		buzz_timer.data = 1;
		mod_timer (&buzz_timer, jiffies + on_time);
	}
}

static int 
buzzer_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct buzzer_time bt;

	switch(cmd)
	{
	case IOC_SETBUZZER:
		del_timer (&buzz_timer);
		if (copy_from_user (&bt, (void *)arg, sizeof (bt)))
			return -EFAULT;
		printk ("set_buzzer %d %d\n", bt.on_time, bt.off_time);
		on_time = (bt.on_time * HZ) / 1000;
		off_time = (bt.off_time * HZ) / 1000;
		if (bt.on_time) {
			SET_BUZZER (1);
			if (bt.off_time) {
				buzz_timer.data = 1;
				mod_timer (&buzz_timer, jiffies + on_time);
			}
		} else {
			SET_BUZZER (0);
		}
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations buzzer_fops=
{
	owner:		THIS_MODULE,
	ioctl:		buzzer_ioctl,
};

static struct miscdevice buzzer_miscdev=
{
	MISC_DYNAMIC_MINOR,
	"buzzer",
	&buzzer_fops
};

static int __init 
h5400_buzzer_init (void)
{
	if (!machine_is_h5400 ())
		return -ENODEV;

	init_timer (&buzz_timer);
	buzz_timer.function = buzz_timer_callback;

	misc_register (&buzzer_miscdev);

	return 0;
}

static void __exit
h5400_buzzer_exit(void)
{
	SET_BUZZER (0);

	del_timer (&buzz_timer);

	misc_deregister (&buzzer_miscdev);
}

MODULE_AUTHOR("Phil Blundell <pb@nexus.co.uk>");
MODULE_DESCRIPTION("H5400 buzzer driver");
MODULE_LICENSE("GPL");

module_init(h5400_buzzer_init);
module_exit(h5400_buzzer_exit);
