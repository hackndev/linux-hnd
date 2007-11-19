/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * 2006-10-30: Moved udc code from blueangel.c
 *             (Michael Horne <asylumed@gmail.com>)
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#include <asm/arch/htchimalaya-gpio.h>
#include <asm/arch/htchimalaya-asic.h>

static int himalaya_udc_is_connected(void) {
	int ret = !(GPLR(GPIO_NR_HIMALAYA_USB_DETECT_N) & GPIO_bit(GPIO_NR_HIMALAYA_USB_DETECT_N));
	printk("udc_is_connected returns %d\n",ret);

	return ret;
}

static void himalaya_udc_command(int cmd) {
	switch(cmd){
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk("_udc_control: disconnect\n");
//			asic3_set_gpio_out_a(asic3_dev, 1<<GPIOA_USB_PUEN, 1<<GPIOA_USB_PUEN);
			asic3_gpio_set_value(asic3_dev,GPIO_USB_PUEN,1);
                break;
                case PXA2XX_UDC_CMD_CONNECT:
			printk("_udc_control: connect\n");
//			asic3_set_gpio_out_a(asic3_dev, 1<<GPIOA_USB_PUEN, 0);
			asic3_gpio_set_value(asic3_dev,GPIO_USB_PUEN,0);
		break;
		default:
			printk("_udc_control: unknown command!\n");
		break;
	}
}

static struct pxa2xx_udc_mach_info himalaya_udc_mach_info = {
	.udc_is_connected = himalaya_udc_is_connected,
	.udc_command      = himalaya_udc_command,
};

static int himalaya_udc_probe(struct device *dev)
{
	printk("himalaya udc register");

	asic3_set_gpio_dir_a(asic3_dev, 1<<GPIOA_USB_PUEN, 1<<GPIOA_USB_PUEN);

    	pxa_set_udc_info(&himalaya_udc_mach_info);
	return 0;
}

static struct platform_driver himalaya_udc_driver = {
	.driver   = {
	        .probe   = himalaya_udc_probe,
		.name    = "himalaya-udc",
	}
};

static int himalaya_udc_init(void) {
	return platform_driver_register(&himalaya_udc_driver);
}

module_init(himalaya_udc_init);

MODULE_AUTHOR("Michael Horne <asylumed@gmail.com>");
MODULE_DESCRIPTION("UDC init for the HTC Himalaya");
MODULE_LICENSE("GPL");
