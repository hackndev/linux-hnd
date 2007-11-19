/* arch/arm/mach-pxa/palmtt3/palmtt3_bt.c
 *
 * Palm Tungsten|T3 bluetooth driver
 *
 * copied from palmz72.c and changed bit
 *
 * Tomas Cech <Tomas.Cech@matfyz.cz>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/arch/palmtt3-gpio.h>
#include <asm/arch/tps65010.h>
#include <linux/platform_device.h>
#include <asm/arch/serial.h>


void bcm2035_bt_reset(int on)
{
        printk(KERN_NOTICE "Switch BT reset %d\n", on);
        if (on) {
		/* FIXME: it's just template, GPIOs are not the right ones */
		pxa_gpio_mode(GPIO83_NSSP_TX);
		pxa_gpio_mode(GPIO84_NSSP_TX);
	}
        else {

	}
}
EXPORT_SYMBOL(bcm2035_bt_reset);


void bcm2035_bt_power(int on)
{
        printk(KERN_NOTICE "Switch BT power %d\n", on);
        if (on)
		tps65010_set_gpio_out_value(GPIO_NR_PALMTT3_TPS65010_BT_POWER,1);
        else
		tps65010_set_gpio_out_value(GPIO_NR_PALMTT3_TPS65010_BT_POWER,0);
}
EXPORT_SYMBOL(bcm2035_bt_power);


/*
static struct platform_pxa_serial_funcs bcm2035_pxa_bt_funcs = {
        .configure = bcm2035_bt_configure,
};
EXPORT_SYMBOL(bcm2035_pxa_bt_funcs);
*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tomas Cech <Tomas.Cech@matfyz.cz>");
MODULE_DESCRIPTION("Palm T|T3 bluetooth driver");
