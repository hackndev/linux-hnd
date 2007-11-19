/*
 *  This is header for Dynamin Power Management support
 */

#ifndef __DPM_H
#define __DPM_H

#include <linux/gpiodev.h>
#include <linux/workqueue.h>

#ifdef CONFIG_DPM_DEBUG
#define DPM_DEBUG(fmt,arg...) \
    printk(KERN_DEBUG "DPM: " fmt,##arg)
#else
#define DPM_DEBUG(fmt,arg...) ;
#endif

struct dpm_hardware_ops {
	/* If it's possible to control power with a single GPIO, use this */
	struct gpio power_gpio;
	/* If power_gpio uses inverted signal */
	int power_gpio_neg;
	/* Otherwise, method will be called if it's specified */
	int (*power)(int on_off);
};

static inline void dpm_power(struct dpm_hardware_ops *ops, int on_off)
{
	if (ops->power)
		ops->power(on_off);
	else
		gpiodev_set_value(&ops->power_gpio, !!on_off ^ !!ops->power_gpio_neg);
}

#endif /* __DPM_H */
