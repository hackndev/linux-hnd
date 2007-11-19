#ifndef __GPIODEV2_H
#define __GPIODEV2_H

#include <linux/device.h>

struct gpio_ops {
	struct device *this;
        int (*get_value)(struct device *this, unsigned gpio_no);
        void (*set_value)(struct device *this, unsigned gpio_no, int val);
        int (*to_irq)(struct device *this, unsigned gpio_no);
};

#define GPIO_BASE_INCREMENT 0x100
#define GPIO_BASE_MASK 0xff

extern struct gpio_ops gpio_desc[16];
	
/* API functions */

static inline void gpiodev_register(unsigned gpio_base, struct device *this, struct gpio_ops *ops)
{
	int i = gpio_base >> 8;
	gpio_desc[i].this = this;
	gpio_desc[i].get_value = ops->get_value;
	gpio_desc[i].set_value = ops->set_value;
	gpio_desc[i].to_irq = ops->to_irq;
}

static inline int gpiodev2_get_value(unsigned gpio)
{
	int (*get_value)(struct device *this, unsigned gpio_no);
	int i = gpio >> 8;
	get_value = gpio_desc[i].get_value;
	if (!get_value) {
		printk(KERN_ERR "gpio_get_value() call for uninitialized GPIO device\n");
		BUG();
		return 0;
	}

	return get_value(gpio_desc[i].this, gpio);
}

static inline void gpiodev2_set_value(unsigned gpio, int val)
{
	void (*set_value)(struct device *this, unsigned gpio_no, int val);
	int i = gpio >> 8;
	set_value = gpio_desc[gpio >> 8].set_value;
	if (!set_value) {
		printk(KERN_ERR "gpio_set_value() call for uninitialized GPIO device\n");
		BUG();
		return;
	}

	return set_value(gpio_desc[i].this, gpio, val);
}

static inline int gpiodev2_to_irq(unsigned gpio)
{
	int (*to_irq)(struct device *this, unsigned gpio_no);
	int i = gpio >> 8;
	to_irq = gpio_desc[gpio >> 8].to_irq;
	if (!to_irq) {
		printk(KERN_ERR "gpio_to_irq() call for uninitialized GPIO device\n");
		BUG();
		return 0;
	}

	return to_irq(gpio_desc[i].this, gpio);
}


#endif /* __GPIODEV2_H */
