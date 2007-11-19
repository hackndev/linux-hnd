#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/asus730-gpio.h>

static int a730_udc_is_connected(void)
{
    return (GET_A730_GPIO(USB_CABLE_DETECT_N) != 0);
}

static void a730_udc_command(int command)
{
    switch (command)
    {
	case PXA2XX_UDC_CMD_DISCONNECT:
	    SET_A730_GPIO(USB_PULL_UP, 0);
	    break;
	case PXA2XX_UDC_CMD_CONNECT:
	    SET_A730_GPIO(USB_PULL_UP, 1);
	    break;
	default:
	    printk("%s: unknown command '%d'.", __FUNCTION__, command);
    }
}

static struct pxa2xx_udc_mach_info a730_udc_info = {
    .udc_is_connected = a730_udc_is_connected,
    .udc_command = a730_udc_command,
};

static int a730_udc_probe(struct platform_device *pdev)
{
    printk("Probing Asus MyPal A730(W) UDC\n");
    pxa_set_udc_info(&a730_udc_info);
    
    return 0;
}

static int a730_udc_remove(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver a730_udc_driver = {
    .driver = {
	.name = "a730-udc",
    },
    .probe = a730_udc_probe,
    .remove = a730_udc_remove,
    //.suspend = a730_udc_suspend,
    //.resume = a730_udc_resume,
};

static int __init a730_udc_init(void)
{
    if (!machine_is_a730()) return -ENODEV;
    printk("Probing Asus MyPal A730(W) UDC\n");
    pxa_set_udc_info(&a730_udc_info);
    return 0;
}

static void __exit a730_udc_exit(void)
{
    return;
}

module_init(a730_udc_init);
module_exit(a730_udc_exit);

MODULE_AUTHOR("Server Nikolaenko <mypal_hh@utl.ru");
MODULE_DESCRIPTION("Asus MyPal A730(W) UDC support");
MODULE_LICENSE("GPL");
