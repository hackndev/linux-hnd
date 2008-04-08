/* arch/arm/mach-pxa/palmtt3/palmtt3_bt.c
 *
 * Palm Tungsten|T3 bluetooth driver
 *
 * copied from palmtt5_bt.c and changed bit
 *
 * Tomas Cech <Tomas.Cech@matfyz.cz>
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/mutex.h>

#include <asm/hardware.h>
#include <asm/arch/serial.h>
#include <asm/arch/palmtt3-gpio.h>
#include <asm/arch/tps65010.h>
#include <asm/arch/gpio.h>

#include "palmtt3_bt.h"

static void palmtt3_bt_configure(int state)
{
	switch (state) {
	case PXA_UART_CFG_PRE_STARTUP:
		printk(KERN_NOTICE "Switch BT on\n");
		tps65010_set_gpio_out_value(GPIO_NR_PALMTT3_TPS65010_BT_POWER,0);
		tps65010_lock();
		gpio_set_value(GPIO45_BTRTS, 1);
		tps65010_unlock();
		printk(KERN_DEBUG "GPIO44,45 statuses: %d, %d\n", !!GET_GPIO(GPIO44_BTCTS), !!GET_GPIO(GPIO45_BTRTS));
		tps65010_set_gpio_out_value(GPIO_NR_PALMTT3_TPS65010_BT_POWER,1);
		tps65010_lock();
		msleep(300);
		gpio_set_value(GPIO45_BTRTS, 0);
		tps65010_unlock();
		printk(KERN_DEBUG "GPIO44,45 statuses: %d, %d\n", !!GET_GPIO(GPIO44_BTCTS), !!GET_GPIO(GPIO45_BTRTS));
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		printk(KERN_NOTICE "Switch BT off\n");
		tps65010_set_gpio_out_value(GPIO_NR_PALMTT3_TPS65010_BT_POWER,0);
		break;

	default:
		break;
	}
}

static int palmtt3_bt_probe(struct platform_device *dev)
{
	struct palmtt3_bt_funcs *funcs = dev->dev.platform_data;

	/* configure UART */

/*	FIXME: write proper modes
	pxa_gpio_mode(GPIO42_BTRXD_MD);
	pxa_gpio_mode(GPIO43_BTTXD_MD);
	pxa_gpio_mode(GPIO44_BTCTS_MD);
	pxa_gpio_mode(GPIO45_BTRTS_MD);
*/
	funcs->configure = palmtt3_bt_configure;

	return 0;
}

static int palmtt3_bt_remove(struct platform_device *dev)
{
	struct palmtt3_bt_funcs *funcs = dev->dev.platform_data;

	funcs->configure = NULL;

	return 0;
}

static int palmtt3_bt_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int palmtt3_bt_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver palmtt3_bt_driver = {
	.driver		= {
		.name	= "palmtt3_bt",
	},
	.probe		= palmtt3_bt_probe,
	.remove		= palmtt3_bt_remove,
	.suspend	= palmtt3_bt_suspend,
	.resume		= palmtt3_bt_resume,
};

static int __init palmtt3_bt_init(void)
{
	printk(KERN_NOTICE "PalmT|T3: Bluetooth driver registered\n");
	return platform_driver_register(&palmtt3_bt_driver);
}

static void __exit palmtt3_bt_exit(void)
{
	platform_driver_unregister(&palmtt3_bt_driver);
}

module_init(palmtt3_bt_init);
module_exit(palmtt3_bt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tomas Cech <Tomas.Cech@matfyz.cz>");
MODULE_DESCRIPTION("PalmT|T3 bluetooth driver");
