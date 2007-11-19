/*
 * GSM driver for HTC Alpine
 *
 * Copyright (C) 2006, Philipp Zabel
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/mach-types.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>

int suspending;

/* FIXME: asic5 needs its own driver */

extern void htcalpine_cpld_egpio_set_value(int gpio, int val);

void *asic5_map;

void asic5_egpio_set_value(int gpio, int val) {
	int bits = 1<<gpio;
	static int egpio;

	printk ("asic5_egpio_set_value(%d, %d)\n", gpio, val);

	if (val)
		egpio |= bits;
	else
		egpio &= ~bits;

	__raw_writew(egpio & 0xffff, asic5_map+0x0);
	__raw_writew(egpio>>16, asic5_map+0x2);
}

void asic5_phone_on(void)
{
	printk ("asic5_phone_on\n");

	__raw_writew((__raw_readw(asic5_map+0x28) | 0x3), asic5_map+0x28);
	__raw_writew((__raw_readw(asic5_map+0x04) | 0x1), asic5_map+0x04);

	__raw_writew((__raw_readw(asic5_map+0x28) | 0xc), asic5_map+0x28);
	__raw_writew((__raw_readw(asic5_map+0x04) & 0xfffd), asic5_map+0x04);

	__raw_writew((__raw_readw(asic5_map+0x28) | 0x30), asic5_map+0x28);
	__raw_writew((__raw_readw(asic5_map+0x04) | 0x4), asic5_map+0x04);

	__raw_writew((__raw_readw(asic5_map+0x28) | 0xc0), asic5_map+0x28);
	__raw_writew((__raw_readw(asic5_map+0x04) & 0xfff7), asic5_map+0x04);
}


static void alpine_gsm_reset(void)
{
	printk ("GSM Reset\n");

	GPSR(36) = GPIO_bit(36);
	pxa_gpio_mode(36 | GPIO_OUT);

	pxa_gpio_mode (10 | GPIO_IN);
	pxa_gpio_mode (108 | GPIO_IN);

	asic5_egpio_set_value(0, 0); /* DTR? */

	asic5_egpio_set_value(11, 0); /* magician: pxa gpio 11 */
	GPCR(87) = GPIO_bit(87);

	/* stop display to avoid interference? */
	GPCR(GPIO74_LCD_FCLK) = GPIO_bit(GPIO74_LCD_FCLK);

        htcalpine_cpld_egpio_set_value(15, 1); /* GSM_POWER */
	asic5_egpio_set_value(22, 1); /* ?? */

	msleep (150);
	GPSR(GPIO74_LCD_FCLK) = GPIO_bit(GPIO74_LCD_FCLK);
	msleep (150);
	GPCR(GPIO74_LCD_FCLK) = GPIO_bit(GPIO74_LCD_FCLK);

	asic5_egpio_set_value(11, 1);

	/* FIXME: disable+reenable 86, is this OK? */
	GPCR(86) = GPIO_bit(86); // ??
	GPSR(86) = GPIO_bit(86); // GSM_RESET
	asic5_egpio_set_value(9, 1); /* magician: pxa gpio 26 */
	msleep(150);
	GPCR(86) = GPIO_bit(86);
	msleep(10);

        pxa_gpio_mode(36 | GPIO36_FFDCD_MD); /* or just GPIO_IN? */

	asic5_phone_on();
}

void alpine_gsm_off(void)
{
	printk ("GSM Off\n");

        htcalpine_cpld_egpio_set_value(1<<15, 0); /* GSM_POWER */
}

static void alpine_phone_configure (int state)
{
	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP:
		printk( KERN_NOTICE "alpine configure phone: PRE_STARTUP\n");
		if (!suspending)
			alpine_gsm_reset();
		asic5_egpio_set_value(1<<11,0); /* ready to talk */
		break;

	case PXA_UART_CFG_POST_STARTUP:
		printk( KERN_NOTICE "alpine configure phone: POST_STARTUP\n");
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		printk( KERN_NOTICE "alpine configure phone: PRE_SHUTDOWN\n");
		if (suspending)
			asic5_egpio_set_value(1<<11, 1); /* shut up, please */
		else
			alpine_gsm_off();
		break;

	default:
		break;
	}
}

static void alpine_phone_suspend (struct platform_device *dev, pm_message_t state)
{
	suspending = 0 /* should be 1, but we need CPLD irq support first, anyway */;
//	return 0;
}

static void alpine_phone_resume (struct platform_device *dev)
{
	suspending = 0;
//	return 0;
}

struct alpine_phone_funcs {
        void (*configure) (int state);
        void (*suspend) (struct platform_device *dev, pm_message_t state);
        void (*resume) (struct platform_device *dev);
};

static int alpine_phone_probe (struct platform_device *dev)
{
	struct alpine_phone_funcs *funcs = (struct alpine_phone_funcs *) dev->dev.platform_data;

	printk ("alpine_phone_probe\n");

	asic5_map = ioremap (PXA_CS1_PHYS + 0x02000000, 0x1000);

	/* configure phone UART */

	funcs->configure = alpine_phone_configure;
	funcs->suspend = alpine_phone_suspend;
	funcs->resume = alpine_phone_resume;

	return 0;
}

static int alpine_phone_remove (struct platform_device *dev)
{
	struct alpine_phone_funcs *funcs = (struct alpine_phone_funcs *) dev->dev.platform_data;

	iounmap(asic5_map);

	funcs->configure = NULL;
	funcs->suspend = NULL;
	funcs->resume = NULL;

	return 0;
}

static struct platform_driver phone_driver = {
	.probe   = alpine_phone_probe,
	.remove  = alpine_phone_remove,
	.driver = {
		.name     = "htcalpine-phone",
	},
};

static __init int alpine_phone_init(void)
{
	if (!machine_is_htcalpine())
		return -ENODEV;
	printk(KERN_NOTICE "alpine Phone Driver\n");

	return platform_driver_register(&phone_driver);
}

static __exit void alpine_phone_exit(void)
{
	platform_driver_unregister(&phone_driver);
}

module_init(alpine_phone_init);
module_exit(alpine_phone_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("HTC Alpine GSM driver");
MODULE_LICENSE("GPL");
