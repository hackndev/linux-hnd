#ifndef __GPIODEV_H
#define __GPIODEV_H

#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/gpio.h>

/* Interface */

/* This structure must be first member of device platform_data structure 
   of a device which provides gpiodev interface. All method pointers
   must be non-NULL, so stubs must be used for non-implemented ones. */
struct gpiodev_ops {
        int (*get)(struct device *this, unsigned gpio_no);
        void (*set)(struct device *this, unsigned gpio_no, int val);
        int (*to_irq)(struct device *this, unsigned gpio_no);
};
	
/* Generalized GPIO structure */

struct gpio {
	struct device *gpio_dev;
	unsigned gpio_no;
};

/* API functions */

static inline int gpiodev_get_value(struct gpio *gpio)
{
	struct gpiodev_ops *ops = gpio->gpio_dev->platform_data;
	return ops->get(gpio->gpio_dev, gpio->gpio_no);
}
static inline void gpiodev_set_value(struct gpio *gpio, int val)
{
	struct gpiodev_ops *ops = gpio->gpio_dev->platform_data;
	ops->set(gpio->gpio_dev, gpio->gpio_no, val);
}
static inline int gpiodev_to_irq(struct gpio *gpio)
{
	struct gpiodev_ops *ops = gpio->gpio_dev->platform_data;
	return ops->to_irq(gpio->gpio_dev, gpio->gpio_no);
}

#endif /* __GPIODEV_H */
