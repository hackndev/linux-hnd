
/*
 *
 * htcuniversal_udc.c:
 * htcuniversal specific code for the pxa27x usb device controller.
 * 
 * Use consistent with the GNU GPL is permitted.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <linux/mfd/asic3_base.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

static void htcuniversal_udc_command(int cmd)
{
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev,
					   1<<GPIOB_USB_PUEN, 0);
//		SET_HTCUNIVERSAL_GPIO(USB_PUEN,0);
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		asic3_set_gpio_out_b(&htcuniversal_asic3.dev,
					   1<<GPIOB_USB_PUEN,  1<<GPIOB_USB_PUEN);
//		SET_HTCUNIVERSAL_GPIO(USB_PUEN,1);
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static int htcuniversal_udc_is_connected(void)
{
	return (GET_HTCUNIVERSAL_GPIO(USB_DET) != 0);
}

static struct pxa2xx_udc_mach_info htcuniversal_udc_info __initdata = {
	.udc_is_connected = htcuniversal_udc_is_connected,
	.udc_command      = htcuniversal_udc_command,
};

static int htcuniversal_udc_probe(struct platform_device * dev)
{
	asic3_set_gpio_dir_b(&htcuniversal_asic3.dev, 1<<GPIOB_USB_PUEN, 1<<GPIOB_USB_PUEN);

	pxa_set_udc_info(&htcuniversal_udc_info);
	return 0;
}

static struct platform_driver htcuniversal_udc_driver = {
	.driver	  = {
		.name     = "htcuniversal_udc",
	},
	.probe    = htcuniversal_udc_probe,
};

static int __init htcuniversal_udc_init(void)
{
	return platform_driver_register(&htcuniversal_udc_driver);
}

module_init(htcuniversal_udc_init);
MODULE_LICENSE("GPL");
