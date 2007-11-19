/*
 * LCD backlight support for AXIM5
 *
 * Copyright (c) 2005 Ian Molton
 * Copyright (c) 2005 SDG Systems, LLC
 * Copyright (c) 2006 Todd Blumer <todd@sdgsystems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 2006-11-04    Anton Vorontsov <cbou@mail.ru>
 * Convert backlight control code to use corgi-bl driver.
 * 2006-11-04    Paul Sokolovsky <pmiscml@gamil.com>
 * Make a standalone driver, using mq11xx_base API.
 */

#include <linux/platform_device.h>
#include <linux/corgi_bl.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/aximx5-gpio.h>
#include <linux/mfd/mq11xx.h>

static struct mediaq11xx_base *mq_base;

static
void axim5_set_bl_intensity(int intensity)
{
	if (intensity < 7) intensity = 0;
	/* LCD brightness on Dell Axim is driven by PWM0.
	 * We'll set the pre-scaler to 8, and the period to 1024, this
	 * means the backlight refresh rate will be 3686400/(8*1024) =
	 * 450 Hz which is quite enough.
	 */
	PWM_CTRL0 = 7;
	PWM_PERVAL0 = 0x3FF;
	PWM_PWDUTY0 = intensity;

	if (intensity) {
		mq_base->set_GPIO (mq_base, GPIO_NR_AXIMX5_MQ1132_BACKLIGHT,
				   MQ_GPIO_IN | MQ_GPIO_OUT1);
		CKEN |= CKEN0_PWM0;
	}
	else {
		CKEN &= ~CKEN0_PWM0;
		mq_base->set_GPIO (mq_base, GPIO_NR_AXIMX5_MQ1132_BACKLIGHT,
				   MQ_GPIO_IN | MQ_GPIO_OUT0);
	}

	return;
}

static
struct corgibl_machinfo axim5_bl_machinfo = {
	.max_intensity = 0x3FF,
	.default_intensity = 0x3FF/4,
	.limit_mask = 0xFFF,
	.set_bl_intensity = axim5_set_bl_intensity
};

struct platform_device aximx5_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &axim5_bl_machinfo
	}
};

static int __init aximx5_bl_init(void)
{
        if (mq_driver_get()) {
                printk(KERN_ERR "MediaQ base driver not found\n");
                return -ENODEV;
        }

        if (mq_device_enum(&mq_base, 1) <= 0) {
                mq_driver_put();
                printk(KERN_ERR "MediaQ 1132 chip not found\n");
                return -ENODEV;
        }

        return platform_device_register(&aximx5_bl);
}

static void __exit aximx5_bl_exit(void)
{
        platform_device_unregister(&aximx5_bl);
        mq_driver_put();
}

module_init(aximx5_bl_init);
module_exit(aximx5_bl_exit);

MODULE_AUTHOR("Andrew Zabolotny, Anton Vorontsov");
MODULE_DESCRIPTION("Backlight proxy driver for Axim X5");
MODULE_LICENSE("GPL");
