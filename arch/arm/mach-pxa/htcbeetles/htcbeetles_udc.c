/*
 * htcbeetles_udc.c:
 * htcbeetles specific code for the pxa27x usb device controller.
 * 
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/htcbeetles-gpio.h>
#include <asm/arch/htcbeetles-asic.h>

extern struct platform_device htcbeetles_asic3;

static void htcbeetles_udc_command(int cmd)
{
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
		asic3_set_gpio_out_a(&htcbeetles_asic3.dev,
					   1<<GPIOA_USB_PUEN, 0);
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		asic3_set_gpio_out_a(&htcbeetles_asic3.dev,
					   1<<GPIOA_USB_PUEN,  1<<GPIOA_USB_PUEN);
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static int htcbeetles_udc_is_connected(void)
{
 return ((asic3_get_gpio_status_a(&htcbeetles_asic3.dev) & (1<<GPIOB_USB_DETECT)) != 0);
}

static struct pxa2xx_udc_mach_info htcbeetles_udc_info __initdata = {
	.udc_is_connected = htcbeetles_udc_is_connected,
	.udc_command      = htcbeetles_udc_command,
};

static int htcbeetles_udc_probe(struct device * dev)
{
	pxa_set_udc_info(&htcbeetles_udc_info);
	return 0;
}

static struct device_driver htcbeetles_udc_driver = {
	.name     = "htcbeetles_udc",
	.bus      = &platform_bus_type,
	.probe    = htcbeetles_udc_probe,
};

static int __init htcbeetles_udc_init(void)
{
	return driver_register(&htcbeetles_udc_driver);
}

module_init(htcbeetles_udc_init);
MODULE_LICENSE("GPL");

