/*
 * pxa2xx_udc_gpio.c: Generic driver for GPIO-controlled PXA2xx UDC.
 * 
 * */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dpm.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxa2xx_udc_gpio.h>

static struct pxa2xx_udc_gpio_info *pdata;

static void pda_udc_command(int cmd)
{
	switch (cmd) {
		case PXA2XX_UDC_CMD_DISCONNECT:
			DPM_DEBUG("pda_udc: Turning off port\n");
			dpm_power(&pdata->power_ctrl, 0);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			DPM_DEBUG("pda_udc: Turning on port\n");
			dpm_power(&pdata->power_ctrl, 1);
			break;
		default:
			printk("pda_udc: unknown command!\n");
			break;
	}
}

static int pda_udc_is_connected(void)
{
	int status = !!gpiodev_get_value(&pdata->detect_gpio) ^ pdata->detect_gpio_negative;
	return status;
}

static struct pxa2xx_udc_mach_info pda_udc_info __initdata = {
	.udc_is_connected = pda_udc_is_connected,
	.udc_command      = pda_udc_command,
};

static int pda_udc_probe(struct platform_device * pdev)
{
        printk("pxa2xx-udc-gpio: Generic driver for GPIO-controlled PXA2xx UDC\n");
	pdata = pdev->dev.platform_data;

	pxa_set_udc_info(&pda_udc_info);
	return 0;
}

static struct platform_driver pda_udc_driver = {
	.driver	  = {
		.name     = "pxa2xx-udc-gpio",
	},
	.probe    = pda_udc_probe,
};

static int __init pda_udc_init(void)
{
	return platform_driver_register(&pda_udc_driver);
}

#ifdef MODULE
module_init(pda_udc_init);
#else	/* start early for dependencies */
fs_initcall(pda_udc_init);
#endif

MODULE_AUTHOR("Paul Sokolovsky <pmiscml@gmail.com>");
MODULE_DESCRIPTION("Generic driver for GPIO-controlled PXA2xx UDC");
MODULE_LICENSE("GPL");
