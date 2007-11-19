/*
 * htcsable_udc.c:
 * htcsable specific code for the pxa27x usb device controller.
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
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>

static void htcsable_udc_command(int cmd)
{
	switch (cmd) {
	case PXA2XX_UDC_CMD_DISCONNECT:
    printk("htcsable udc disconnect\n");
		asic3_set_gpio_out_a(&htcsable_asic3.dev,
					   1<<GPIOA_USB_PUEN,  1<<GPIOA_USB_PUEN);
		break;
	case PXA2XX_UDC_CMD_CONNECT:
    printk("htcsable udc connect\n");
		asic3_set_gpio_out_a(&htcsable_asic3.dev,
					   1<<GPIOA_USB_PUEN, 0);
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static int htcsable_udc_is_connected(void)
{
 int ret = ((asic3_get_gpio_status_b(&htcsable_asic3.dev) & (1<<GPIOB_USB_DETECT)) != 0);
 printk("htcsable_udc_is_connected returns %d\n",ret);
 return ret;
}

static struct pxa2xx_udc_mach_info htcsable_udc_info __initdata = {
	.udc_is_connected = htcsable_udc_is_connected,
	.udc_command      = htcsable_udc_command,
};

static int htcsable_udc_probe(struct platform_device * dev)
{
	asic3_set_gpio_dir_a(&htcsable_asic3.dev, 1<<GPIOA_USB_PUEN, 1<<GPIOA_USB_PUEN);

	pxa_set_udc_info(&htcsable_udc_info);
	return 0;
}

static struct platform_driver htcsable_udc_driver = {
  .driver = {
    .name     = "htcsable_udc",
  },
	.probe    = htcsable_udc_probe,
};

static int __init htcsable_udc_init(void)
{
 printk("htcsable_udc_init\n");
	return platform_driver_register(&htcsable_udc_driver);
}

module_init(htcsable_udc_init);
MODULE_LICENSE("GPL");
