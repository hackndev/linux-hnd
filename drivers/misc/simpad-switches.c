/*
 * SIMpad Switches/Button implementation
 *
 * Copyright 2004, 2005 Holger Hans Peter Freyther
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/device.h>

#include <asm/dma.h>
#include <asm/semaphore.h>

#include "ucb1x00.h"


static int simpad_switches_add(struct class_device *dev)
{
	return 0;
}

static void simpad_switches_remove(struct class_device *dev)
{

}


static int simpad_switches_resume(struct ucb1x00 *ucb)
{
	return 0;
}


static struct ucb1x00_class_interface simpad_switches_interface = {
	.interface   = {
		.add    = simpad_switches_add,
		.remove = simpad_switches_remove,
	},
	.resume      = simpad_switches_resume,

};


static int __init simpad_switches_init(void)
{
	return ucb1x00_register_interface(&simpad_switches_interface);
}


static void __exit simpad_switches_exit(void)
{
	ucb1x00_unregister_interface(&simpad_switches_interface);
}


module_init(simpad_switches_init);
module_exit(simpad_switches_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Holger Hans Peter Freyther <freyther@handhelds.org>");
MODULE_DESCRIPTION("SIMpad Switches");
