/*
 *  Palm Treo 650 GSM Management
 *
 *  Copyright (C) 2007 Alex Osborne
 *
 *  This code is available under the GNU GPL version 2 or later.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/arch/hardware.h>
#include <asm/arch/palmt650-gpio.h>
#include <asm/arch/serial.h>

static DECLARE_WAIT_QUEUE_HEAD (host_wake_queue);
static int suspending;
static struct timer_list wake_timer;
static int wake_timer_running;

static ssize_t gsm_power_on_write(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);
	SET_ASIC6_GPIO(GSM_POWER, on);
	printk(KERN_INFO "palmt650_gsm: Setting GSM power to %ld\n", on);
	return count;
}

static ssize_t gsm_power_on_read(struct device *dev, 
				struct device_attribute *attr,
				char *buf)
{
	return strlcpy(buf, GET_ASIC6_GPIO(GSM_POWER) ? "1\n" : "0\n", 3);
}

static ssize_t gsm_wake_write(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);
	SET_PALMT650_GPIO(GSM_WAKE, on);
	printk(KERN_INFO "palmt650_gsm: Setting GSM wake to %ld\n", on);
	return count;
}

static ssize_t gsm_wake_read(struct device *dev, 
				struct device_attribute *attr,
				char *buf)
{
	return strlcpy(buf, GET_PALMT650_GPIO(GSM_WAKE) ? "1\n" : "0\n", 3);
}


static DEVICE_ATTR(power_on, 0644, gsm_power_on_read, gsm_power_on_write);
static DEVICE_ATTR(wake, 0644, gsm_wake_read, gsm_wake_write);

static struct attribute *palmt650_gsm_attrs[] = {
	&dev_attr_power_on.attr,
	&dev_attr_wake.attr,
	NULL
};

static struct attribute_group palmt650_gsm_attr_group = {
	.name	= NULL,
	.attrs	= palmt650_gsm_attrs,
};

static void palmt650_gsm_configure(int state)
{
	/* we want to keep the GSM module powered during sleep so ignore
	 * configure requests if the system is going into suspend.
	 */
	if (suspending) return;

	switch (state) {
        case PXA_UART_CFG_PRE_STARTUP: 
		printk(KERN_NOTICE "palmt650_gsm_configure: power on\n");
		SET_ASIC6_GPIO(GSM_POWER, 1);
		if (!sleep_on_timeout(&host_wake_queue, HZ)) {
			printk(KERN_ERR "%s: timeout waiting for host wake\n",
					__FUNCTION__);
		}
		break;
	case PXA_UART_CFG_POST_SHUTDOWN:
		printk(KERN_NOTICE "palmt650_gsm_configure: power off\n");
		SET_ASIC6_GPIO(GSM_POWER, 0);
		break;
	}
}

static void palmt650_gsm_set_txrx(int txrx) 
{
	int old_wake = GET_PALMT650_GPIO(GSM_WAKE) ? 1 : 0;
	int new_wake = (txrx & PXA_SERIAL_TX) ? 1 : 0;
	int tmout;

	/* the timer will deassert wake, so we only need to worry about 
	 * TX on events.
	 */
	if (!new_wake)
		return;

	if (new_wake != old_wake) {
		SET_PALMT650_GPIO(GSM_WAKE, new_wake);

		/* give the modem time to wakeup. Ideally we'd wait on a
		 * host_wake interrupt but serial_core has interrupts disabled
		 * so we try watching GEDR manually
		 */
		tmout = 500000;
		while (--tmout && !(GEDR(GPIO_NR_PALMT650_GSM_HOST_WAKE)
				& GPIO_bit(GPIO_NR_PALMT650_GSM_HOST_WAKE))) {
			udelay(1);
		}
		if (!tmout) {
			printk(KERN_ERR "%s: timed out waiting for host wake\n",
				__FUNCTION__);
		}
	}

	/* if there's no transmits for 1 second put the modem back to sleep */
	mod_timer(&wake_timer, jiffies + HZ);
}

static int palmt650_gsm_get_txrx(void)
{
	return PXA_SERIAL_RX | (GET_PALMT650_GPIO(GSM_WAKE) ? PXA_SERIAL_TX : 0);
}

static int 
palmt650_gsm_uart_suspend(struct platform_device *dev, pm_message_t state)
{
	suspending = 1;
	/* TODO: ensure RTS is deasserted during sleep to queue messages
	 *       and setup waking upon GSM event.
	 */
	return 0;
}

static int palmt650_gsm_uart_resume(struct platform_device *dev)
{
	suspending = 0;
	return 0;
}

static struct platform_pxa_serial_funcs palmt650_gsm_ffuart_funcs = {
	.configure = palmt650_gsm_configure,
	.set_txrx = palmt650_gsm_set_txrx,
	.get_txrx = palmt650_gsm_get_txrx,
	.suspend = palmt650_gsm_uart_suspend,
	.resume = palmt650_gsm_uart_resume,
};

static irqreturn_t palmt650_gsm_host_wake_int(int irq, void *data)
{
	wake_up(&host_wake_queue);
	return IRQ_HANDLED;
}

void palmt650_gsm_wake_timeout(unsigned long data)
{
	SET_PALMT650_GPIO(GSM_WAKE, 0);
}

static int __init palmt650_gsm_probe(struct platform_device *pdev)
{
	int err;

	/* configure FFUART gpios */
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO35_FFCTS_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	pxa_gpio_mode(GPIO41_FFRTS_MD);

	/* extra signals */
	pxa_gpio_mode(GPIO_NR_PALMT650_GSM_HOST_WAKE_MD);
	pxa_gpio_mode(GPIO_NR_PALMT650_GSM_WAKE_MD);
	SET_PALMT650_GPIO(GSM_WAKE, 0);
	SET_ASIC6_GPIO(GSM_POWER, 0);

	/* setup an interrupt for detecting host wake events */
	set_irq_type(IRQ_GPIO_PALMT650_GSM_HOST_WAKE, IRQT_RISING);
	err = request_irq(IRQ_GPIO_PALMT650_GSM_HOST_WAKE, palmt650_gsm_host_wake_int,
			SA_INTERRUPT, "GSM host wake", pdev);
	if(err) {
		printk(KERN_ERR "palmt650_gsm: can't get GSM host wake IRQ\n");
		return err;
	}

	/* register our callbacks with the PXA serial driver */
	pxa_set_ffuart_info(&palmt650_gsm_ffuart_funcs);

	suspending = 0;
	init_timer(&wake_timer);
	wake_timer.function = palmt650_gsm_wake_timeout;
	wake_timer.data = 0;
	wake_timer_running = 0;

	return sysfs_create_group(&pdev->dev.kobj, &palmt650_gsm_attr_group);
}

static int palmt650_gsm_remove(struct platform_device *pdev)
{
	pxa_set_ffuart_info(NULL);
	sysfs_remove_group(&pdev->dev.kobj, &palmt650_gsm_attr_group);
	return 0;
}

static struct platform_driver palmt650_gsm_driver = {
	.probe 		= palmt650_gsm_probe,
	.remove		= palmt650_gsm_remove,
	.driver = {
		.name 	= "palmt650-gsm",
	}
};

static int __devinit palmt650_gsm_init(void)
{
	return platform_driver_register(&palmt650_gsm_driver);
}

static void palmt650_gsm_exit(void)
{
	platform_driver_unregister(&palmt650_gsm_driver);
}

module_init(palmt650_gsm_init);
module_exit(palmt650_gsm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION("Palm Treo 650 GSM management");
