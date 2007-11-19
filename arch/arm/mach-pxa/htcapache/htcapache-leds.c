/*
 * Support for LEDS on the HTC Apache phone.
 *
 * (c) Copyright 2006 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/leds.h> // led_classdev
#include <linux/platform_device.h> // platform_device

#include <asm/hardware.h> // __REG
#include <asm/arch/pxa-regs.h> // GPSR
#include <asm/arch/htcapache-gpio.h>
#include <asm/gpio.h> // gpio_set_value


struct apache_leds {
	struct led_classdev led;
	int num, mask, val;
};

#define ledtoal(Led) container_of((Led), struct apache_leds, led)


/****************************************************************
 * Brightness functions
 ****************************************************************/

// Set a LED attached to a PXA GPIO line.
static void
pxa_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct apache_leds *al = ledtoal(led_cdev);
	gpio_set_value(al->num, brightness);
}

// Set a LED attached to the apache egpio chip.
static void
egpio_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct apache_leds *al = ledtoal(led_cdev);
	htcapache_egpio_set(al->num, brightness);
}

static u32 oldLED;

// Set a LED attached to the apache "micro-controller"
static void
mc_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct apache_leds *al = ledtoal(led_cdev);
	u32 val = 0;
	if (al->num == 2)
		val = oldLED;
	val &= ~al->mask;
	if (brightness)
		val |= al->val;
	if (al->num == 2)
		oldLED = val;
	htcapache_write_mc_reg(al->num, val);
}


/****************************************************************
 * LED definitions
 ****************************************************************/

static struct apache_leds LEDS[] = {
	{
		.led = {
			.name = "apache:vibra",
			.brightness_set = egpio_set,
		},
		.num = EGPIO_NR_HTCAPACHE_LED_VIBRA,
	}, {
		.led = {
			.name = "apache:kbd_bl",
			.brightness_set = egpio_set,
		},
		.num = EGPIO_NR_HTCAPACHE_LED_KBD_BACKLIGHT,
	}, {
		.led = {
			.name = "apache:flashlight",
			.brightness_set = pxa_set,
		},
		.num = GPIO_NR_HTCAPACHE_LED_FLASHLIGHT,
	}, {
		.led = {
			.name = "apache:phone_bl",
			.brightness_set = mc_set,
		},
		.num = 1,
		.val = 0x20,
		.mask = 0xff,
	}, {
		.led = {
			.name = "apacheRight:green",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0xa0,
		.mask = 0xe0,
	}, {
		.led = {
			.name = "apacheRight:red",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0xc0,
		.mask = 0xe0,
	}, {
		.led = {
			.name = "apacheRight:amber",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0xe0,
		.mask = 0xe0,
	}, {
		.led = {
			.name = "apacheLeft:green",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0x14,
		.mask = 0x1c,
	}, {
		.led = {
			.name = "apacheLeft:blue",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0x18,
		.mask = 0x1c,
	}, {
		.led = {
			.name = "apacheLeft:cyan",
			.brightness_set = mc_set,
		},
		.num = 2,
		.val = 0x1c,
		.mask = 0x1c,
	}
};


/****************************************************************
 * Setup
 ****************************************************************/

static int leds_probe(struct platform_device *pdev)
{
	int i, ret=0;
	for (i=0; i<ARRAY_SIZE(LEDS); i++) {
		ret = led_classdev_register(&pdev->dev, &LEDS[i].led);
		if (ret) {
			printk(KERN_ERR "Error: can't register %s led\n",
			       LEDS[i].led.name);
			goto err;
		}
	}
	// Turn off all LEDs
	for (i=0; i<ARRAY_SIZE(LEDS); i++)
		LEDS[i].led.brightness_set(&LEDS[i].led, 0);
	return 0;
err:
	while (--i >= 0)
		led_classdev_unregister(&LEDS[i].led);
	return ret;
}

static int leds_remove(struct platform_device *pdev)
{
	int i;
	for (i=0; i<ARRAY_SIZE(LEDS); i++)
                led_classdev_unregister(&LEDS[i].led);
	return 0;
}

static struct platform_driver leds_driver = {
        .probe = leds_probe,
        .remove = leds_remove,
        .driver = {
                .name = "htcapache-leds",
        },
};

static int __init leds_init(void)
{
	return platform_driver_register(&leds_driver);
}

static void __exit leds_exit(void)
{
	platform_driver_unregister(&leds_driver);
}

module_init(leds_init)
module_exit(leds_exit)

MODULE_LICENSE("GPL");
