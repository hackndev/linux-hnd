#ifndef _ASIC3_BASE_H
#define _ASIC3_BASE_H

#include <asm/types.h>
#include <linux/gpiodev.h>

/* Private API - for ASIC3 devices internal use only */
#define HDR_IPAQ_ASIC3_ACTION(ACTION,action,fn,FN)                  \
u32  asic3_get_gpio_ ## action ## _ ## fn (struct device *dev);      \
void asic3_set_gpio_ ## action ## _ ## fn (struct device *dev, u32 bits, u32 val);

#define HDR_IPAQ_ASIC3_FN(fn,FN)					\
	HDR_IPAQ_ASIC3_ACTION ( MASK,mask,fn,FN)			\
	HDR_IPAQ_ASIC3_ACTION ( DIR, dir, fn, FN)			\
	HDR_IPAQ_ASIC3_ACTION ( OUT, out, fn, FN)			\
	HDR_IPAQ_ASIC3_ACTION ( LEVELTRI, trigtype, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( RISING, rising, fn, FN)			\
	HDR_IPAQ_ASIC3_ACTION ( LEVEL, triglevel, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( SLEEP_MASK, sleepmask, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( SLEEP_OUT, sleepout, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( BATT_FAULT_OUT, battfaultout, fn, FN)	\
	HDR_IPAQ_ASIC3_ACTION ( INT_STATUS, intstatus, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( ALT_FUNCTION, alt_fn, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( SLEEP_CONF, sleepconf, fn, FN)		\
	HDR_IPAQ_ASIC3_ACTION ( STATUS, status, fn, FN) 

/* Public API */

#define ASIC3_GPIOA_IRQ_BASE	0
#define ASIC3_GPIOB_IRQ_BASE	16
#define ASIC3_GPIOC_IRQ_BASE	32
#define ASIC3_GPIOD_IRQ_BASE	48
#define ASIC3_LED0_IRQ		64
#define ASIC3_LED1_IRQ		65
#define ASIC3_LED2_IRQ		66
#define ASIC3_SPI_IRQ		67
#define ASIC3_SMBUS_IRQ		68
#define ASIC3_OWM_IRQ		69

#define ASIC3_NR_GPIO_IRQS	64	/* 16 bits each GPIO A...D banks */
#define ASIC3_NR_IRQS		(ASIC3_OWM_IRQ + 1)

extern int asic3_irq_base(struct device *dev);

extern void asic3_write_register(struct device *dev, unsigned int reg, 
                                 u32 value);
extern u32  asic3_read_register(struct device *dev, unsigned int reg);

/* old clock api */
extern void asic3_set_clock_sel(struct device *dev, u32 bits, u32 val);
extern u32  asic3_get_clock_cdex(struct device *dev);
extern void asic3_set_clock_cdex(struct device *dev, u32 bits, u32 val);

extern void asic3_set_extcf_select(struct device *dev, u32 bits, u32 val);
extern void asic3_set_extcf_reset(struct device *dev, u32 bits, u32 val);
extern void asic3_set_sdhwctrl(struct device *dev, u32 bits, u32 val);

extern void asic3_set_led(struct device *dev, int led_num, int duty_time, 
                          int cycle_time, int timebase);

extern int asic3_register_mmc(struct device *dev);
extern int asic3_unregister_mmc(struct device *dev);

/* Accessors for GPIO banks */
HDR_IPAQ_ASIC3_FN(a, A)
HDR_IPAQ_ASIC3_FN(b, B)
HDR_IPAQ_ASIC3_FN(c, C)
HDR_IPAQ_ASIC3_FN(d, D)

#define _IPAQ_ASIC3_GPIO_BANK_A      0
#define _IPAQ_ASIC3_GPIO_BANK_B      1
#define _IPAQ_ASIC3_GPIO_BANK_C      2
#define _IPAQ_ASIC3_GPIO_BANK_D      3

#define ASIC3_GPIO_bit(gpio) (1 << (gpio & 0xf))

extern int asic3_get_gpio_bit(struct device *dev, int gpio);
extern void asic3_set_gpio_bit(struct device *dev, int gpio, int val);
extern int asic3_gpio_get_value(struct device *dev, unsigned gpio);
extern void asic3_gpio_set_value(struct device *dev, unsigned gpio, int val);


struct tmio_mmc_hwconfig;

struct asic3_platform_data
{
	// Must be first member
	struct gpiodev_ops gpiodev_ops;

	/* Standard MFD properties */
	int irq_base;
	int gpio_base;
	struct platform_device **child_devs;
	int num_child_devs;

	struct {
		u32 dir;
		u32 init;
		u32 sleep_mask;
		u32 sleep_out;
		u32 batt_fault_out;
		u32 sleep_conf;
		u32 alt_function;
	} gpio_a, gpio_b, gpio_c, gpio_d;

	unsigned int bus_shift;
	struct tmio_mmc_hwconfig *tmio_mmc_hwconfig;
};

#endif /* _ASIC3_BASE_H */

