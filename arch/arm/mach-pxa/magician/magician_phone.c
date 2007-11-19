/*
 * GSM driver for HTC Magician
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

#include <asm/gpio.h>
#include <asm/mach-types.h>

#include <asm/arch/magician.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>

#include <linux/irq.h>
#include <linux/interrupt.h>

static int bootloader = 0;

#if 0 /* doesn't work as expected. */
module_param(bootloader, int, 0644);
MODULE_PARM_DESC(bootloader, "Start in radio bootloader mode");
#endif

int suspending;
void *magician_gsm_ready;

static irqreturn_t phone_isr(int irq, void *irq_desc)
{
	if (irq == gpio_to_irq(GPIO10_MAGICIAN_GSM_IRQ))
		printk("magician_phone: got gpio 10 irq %d\n",
			!!gpio_get_value(GPIO10_MAGICIAN_GSM_IRQ));
	if (irq == gpio_to_irq(GPIO108_MAGICIAN_GSM_READY)) {
		printk("magician_phone: got gpio 108 irq %d\n",
			!!gpio_get_value(GPIO10_MAGICIAN_GSM_IRQ));
		if (magician_gsm_ready)
			complete(magician_gsm_ready);
	}
	return IRQ_HANDLED;
}

static void magician_gsm_reset(void)
{
	unsigned long timeleft;
	DECLARE_COMPLETION_ONSTACK(gsm_ready);
	magician_gsm_ready = &gsm_ready;

	printk ("GSM Reset\n");

	pxa_gpio_mode (10 | GPIO_IN);
	pxa_gpio_mode (108 | GPIO_IN);

	/* FIXME: why does wince do this? */
	gpio_set_value(GPIO40_MAGICIAN_GSM_OUT2, 0);

	gpio_set_value(GPIO11_MAGICIAN_GSM_OUT1, 0);

	pxa_gpio_mode (GPIO87_MAGICIAN_GSM_SELECT_MD);
	if (bootloader)
		gpio_set_value(GPIO87_MAGICIAN_GSM_SELECT, 1);
	else
		gpio_set_value(GPIO87_MAGICIAN_GSM_SELECT, 0);

	/* stop display to avoid interference? */
	gpio_set_value(GPIO74_LCD_FCLK, 0);

	gpio_set_value(EGPIO_MAGICIAN_GSM_POWER, 1);

	msleep (150);
	gpio_set_value(GPIO74_LCD_FCLK, 1);
	msleep (150);
	gpio_set_value(GPIO74_LCD_FCLK, 0);

	if (GPLR0 & 0x800) printk ("GSM: GPLR bit 11 already set\n");
	gpio_set_value(GPIO11_MAGICIAN_GSM_OUT1, 1);

	gpio_set_value(GPIO86_MAGICIAN_GSM_RESET, 0);
	msleep(15);
	gpio_set_value(GPIO86_MAGICIAN_GSM_RESET, 1);
	gpio_set_value(26, 1);			/* FIXME: GSM_POWER? */
	msleep(150);
	gpio_set_value(GPIO86_MAGICIAN_GSM_RESET, 0);
	msleep(10);

	/* FIXME: correct place for those four? */
	pxa_gpio_mode (GPIO34_FFRXD_MD); // alt 1 in
	pxa_gpio_mode (GPIO35_FFCTS_MD); // alt 1 in
	pxa_gpio_mode (GPIO42_BTRXD_MD); // alt 1 in
	pxa_gpio_mode (GPIO44_BTCTS_MD); // alt 1 in

	pxa_gpio_mode (GPIO36_FFDCD_MD); // alt 1 in
	pxa_gpio_mode (GPIO39_FFTXD_MD); // alt 2 out
	pxa_gpio_mode (GPIO43_BTTXD_MD); // alt 2 out
	pxa_gpio_mode (GPIO41_FFRTS_MD); // alt 2 out
	pxa_gpio_mode (GPIO45_BTRTS_MD); // alt 2 out

	if (PGSR0 & 0x800) printk ("GSM: PGSR bit 11 set\n");
	PGSR0 |= 0x800;

	enable_irq_wake(gpio_to_irq(GPIO10_MAGICIAN_GSM_IRQ));

	timeleft = wait_for_completion_timeout(&gsm_ready, 5*HZ);
	magician_gsm_ready = NULL;
	if (!timeleft)
                printk("no irq 108 during reset\n");
}

void magician_gsm_off(void)
{
	printk ("GSM Off\n");
	gpio_set_value(GPIO11_MAGICIAN_GSM_OUT1, 0);
	gpio_set_value(GPIO74_LCD_FCLK, 0);
	gpio_set_value(GPIO87_MAGICIAN_GSM_SELECT, 0);
	gpio_set_value(GPIO86_MAGICIAN_GSM_RESET, 0);
	gpio_set_value(26, 0);		/* FIXME: GSM_POWER? */

	pxa_gpio_mode (GPIO10_MAGICIAN_GSM_IRQ | GPIO_OUT);
	gpio_set_value(GPIO10_MAGICIAN_GSM_IRQ, 0);
	pxa_gpio_mode (GPIO108_MAGICIAN_GSM_READY | GPIO_OUT);
	gpio_set_value(GPIO108_MAGICIAN_GSM_READY, 0);

	/* set to output and clear to save power */
	pxa_gpio_mode (GPIO34_FFRXD | GPIO_OUT);
	GPCR(34) = GPIO_bit(34);
	pxa_gpio_mode (GPIO35_FFCTS | GPIO_OUT);
	GPCR(35) = GPIO_bit(35);
	pxa_gpio_mode (GPIO36_FFDCD | GPIO_OUT);
	GPCR(36) = GPIO_bit(36);
	pxa_gpio_mode (GPIO39_FFTXD | GPIO_OUT);
	GPCR(39) = GPIO_bit(39);
	pxa_gpio_mode (GPIO41_FFRTS | GPIO_OUT);
	GPCR(41) = GPIO_bit(41);
	pxa_gpio_mode (GPIO42_BTRXD | GPIO_OUT);
	GPCR(42) = GPIO_bit(42);
	pxa_gpio_mode (GPIO43_BTTXD | GPIO_OUT);
	GPCR(43) = GPIO_bit(43);
	pxa_gpio_mode (GPIO44_BTCTS | GPIO_OUT);
	GPCR(44) = GPIO_bit(44);
	pxa_gpio_mode (GPIO45_BTRTS | GPIO_OUT);
	GPCR(45) = GPIO_bit(45);

	PGSR0 &= ~0x800; // 11 off during suspend

	disable_irq_wake(gpio_to_irq(GPIO10_MAGICIAN_GSM_IRQ));

	gpio_set_value(EGPIO_MAGICIAN_GSM_POWER, 0);
}

static void magician_phone_configure (int state)
{
	switch (state) {

	case PXA_UART_CFG_PRE_STARTUP:
		printk( KERN_NOTICE "magician configure phone: PRE_STARTUP\n");
		if (!suspending)
			magician_gsm_reset();
		gpio_set_value(GPIO11_MAGICIAN_GSM_OUT1, 0); /* ready to talk */
		break;

	case PXA_UART_CFG_POST_STARTUP:
		printk( KERN_NOTICE "magician configure phone: POST_STARTUP\n");
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		printk( KERN_NOTICE "magician configure phone: PRE_SHUTDOWN\n");
		if (suspending)
			gpio_set_value(GPIO11_MAGICIAN_GSM_OUT1, 1); /* shut up, please */
		else
			magician_gsm_off();
		break;

	default:
		break;
	}
}

static int magician_phone_suspend (struct platform_device *dev, pm_message_t state)
{
	suspending = 1;
	return 0;
}

static int magician_phone_resume (struct platform_device *dev)
{
	suspending = 0;
	return 0;
}

struct magician_phone_funcs {
        void (*configure) (int state);
        int (*suspend) (struct platform_device *dev, pm_message_t state);
        int (*resume) (struct platform_device *dev);
};

static int magician_phone_probe (struct platform_device *dev)
{
	struct magician_phone_funcs *funcs = (struct magician_phone_funcs *) dev->dev.platform_data;
	int retval, irq;

	funcs->configure = magician_phone_configure;
	funcs->suspend = magician_phone_suspend;
	funcs->resume = magician_phone_resume;

	irq = platform_get_irq(dev, 0);
	set_irq_type(irq, IRQT_RISING | IRQT_FALLING);
	retval = request_irq(irq, phone_isr, IRQF_DISABLED, "magician_phone 10", NULL);
	if (retval) {
		printk("Unable to get interrupt 10\n");
		free_irq(irq, NULL);
		return -ENODEV;
	}
	irq = platform_get_irq(dev, 1);
	set_irq_type(irq, IRQT_RISING | IRQT_FALLING);
	retval = request_irq(irq, phone_isr, IRQF_DISABLED, "magician_phone 108", NULL);
	if (retval) {
		printk("Unable to get interrupt 108\n");
		free_irq(irq, NULL);
		return -ENODEV;
	}

	return 0;
}

static int magician_phone_remove (struct platform_device *dev)
{
	struct magician_phone_funcs *funcs = (struct magician_phone_funcs *) dev->dev.platform_data;

	funcs->configure = NULL;
	funcs->suspend = NULL;
	funcs->resume = NULL;

	free_irq(platform_get_irq(dev, 0), NULL);
	free_irq(platform_get_irq(dev, 1), NULL);

	return 0;
}

static struct platform_driver phone_driver = {
	.probe   = magician_phone_probe,
	.remove  = magician_phone_remove,
	.driver = {
		.name     = "magician-phone",
	},
};

static __init int magician_phone_init(void)
{
	if (!machine_is_magician())
		return -ENODEV;
	printk(KERN_NOTICE "magician Phone Driver\n");

	return platform_driver_register(&phone_driver);
}

static __exit void magician_phone_exit(void)
{
	platform_driver_unregister(&phone_driver);
}

module_init(magician_phone_init);
module_exit(magician_phone_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("HTC Magician GSM driver");
MODULE_LICENSE("GPL");
