/************************************************************************
 * linux/arch/arm/mach-pxa/palmtt5/palmtt5_battery.c                    *
 * Battery driver for Palm Tungsten|T5                                  *
 *                                                                      *
 * Author: Marek Vasut <marek.vasut@gmail.com>                          *
 *                                                                      *
 ************************************************************************/
      
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>

#include <asm/arch/pm.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>

#include <asm/arch/palmtt5-gpio.h>
#include <asm/arch/pxa27x_keyboard.h>

#ifdef CONFIG_PM
static int palmtt5_suspend(struct device *dev, pm_message_t state)
{
	/* Wake-Up on RTC event, etc. */
	PWER |= PWER_RTC | PWER_WEP1 ;

	/* Wakeup by keyboard */	
	PKWR = 0xe0000;
		
	/* Enabled Deep-Sleep mode */
	PCFR |= PCFR_DS;

	/* Low power mode */
	PCFR |= PCFR_OPDE;
	
	/* 3.6.8.1 */
	while(!(OSCC & OSCC_OOK)) 
	    {}

	/* Turn off USB power */
	SET_PALMTT5_GPIO(USB_POWER,0);

        
	/* disable GPIO reset - DO NOT REMOVE!!!!!!!! Palm totally hangs on reset without disabling GPIO reset during sleep */
	PCFR = PCFR_GPROD;

    	return 0;
}

static int palmtt5_resume(struct device *dev)
{
		
	/* Disabled Deep-Sleep mode */
	PCFR &= PCFR_DS;

	/* Re-enable GPIO reset */
	PCFR |= PCFR_GPR_EN; /* !! DO NOT REMOVE !! THIS IS NECCESARY FOR ENABLE PALM RESET !! */


	/* Here are all of special to resume Palm T5  */

	/* Turn on USB power */
	SET_PALMTT5_GPIO(USB_POWER,1);
	
	return 0;
}
#else
#define palmtt5_suspend NULL
#define palmtt5_resume NULL
#endif

static void palmtt5_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	/* For future */
        return;
}

static void palmtt5_pxa_ll_pm_resume(void)
{
	/* For future */
}

struct pxa_ll_pm_ops palmtt5_ll_pm_ops = {
	.suspend = palmtt5_pxa_ll_pm_suspend,
	.resume = palmtt5_pxa_ll_pm_resume,
};

static int palmtt5_pm_probe(struct device *dev)
{
	printk(KERN_NOTICE "Palm T5 power management driver registered\n");
	return 0;
}

struct device_driver palmtt5_pm_driver = {
	.name = "palmtt5-pm",
	.bus = &platform_bus_type,
	.probe = palmtt5_pm_probe,
	.suspend = palmtt5_suspend,
	.resume = palmtt5_resume,
};

static int __init palmtt5_pm_init(void)
{
	return driver_register(&palmtt5_pm_driver);
}

static void __exit palmtt5_pm_exit(void)
{
	driver_unregister(&palmtt5_pm_driver);
}

module_init(palmtt5_pm_init);
module_exit(palmtt5_pm_exit);

MODULE_AUTHOR("Marek Vasut <marek.vasut@gmail.com>");
MODULE_DESCRIPTION("Palm Tungsten|T5 power management driver");
MODULE_LICENSE("GPL");
