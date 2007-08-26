/************************************************************************
 * Palm TX suspend/resume support					*
 * Author: Jan Herman <2hp@seznam.cz>					*
 *									*
 * Based on code from PalmOne Zire 72 PM by:				*
 *          Jan Herman <2hp@seznam.cz>					*
 *	    Sergey Lapin <slapinid@gmail.com>				*
 *									*
 * This program is free software; you can redistribute it and/or modify	*
 * it under the terms of the GNU General Public License version 2 as	*
 * published by the Free Software Foundation.				*
 *									*
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

#include <asm/arch/palmtx-gpio.h>
#include <asm/arch/pxa27x_keyboard.h>

#ifdef CONFIG_PM
static int palmtx_suspend(struct device *dev, pm_message_t state)
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
	SET_PALMTX_GPIO(USB_POWER,0);
        
	/* disable GPIO reset - DO NOT REMOVE !!!!!!!!
	   Palm totally hangs on reset without disabling
	   GPIO reset during sleep */
	PCFR = PCFR_GPROD;

    	return 0;
}

static int palmtx_resume(struct device *dev)
{
	/* Disabled Deep-Sleep mode */
	PCFR &= PCFR_DS;

	/* Re-enable GPIO reset
	   !! DO NOT REMOVE !!
	   !! THIS IS NECCESARY TO ENABLE PALM RESET !! */
	PCFR |= PCFR_GPR_EN;

	/* Below is all the special stuff to resume Palm TX */
	/* Turn on USB power */
	SET_PALMTX_GPIO(USB_POWER,1);
	
	return 0;
}
#else
#define palmtx_suspend NULL
#define palmtx_resume NULL
#endif

static void palmtx_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	/* For future */
        return;
}

static void palmtx_pxa_ll_pm_resume(void)
{
	/* For future */
}

struct pxa_ll_pm_ops palmtx_ll_pm_ops = {
	.suspend	= palmtx_pxa_ll_pm_suspend,
	.resume		= palmtx_pxa_ll_pm_resume,
};

static int palmtx_pm_probe(struct device *dev)
{
	printk(KERN_NOTICE "Palm TX power management driver registered\n");
	return 0;
}

struct device_driver palmtx_pm_driver = {
	.name		= "palmtx-pm",
	.bus		= &platform_bus_type,
	.probe		= palmtx_pm_probe,
	.suspend	= palmtx_suspend,
	.resume		= palmtx_resume,
};

static int __init palmtx_pm_init(void)
{
	return driver_register(&palmtx_pm_driver);
}

static void __exit palmtx_pm_exit(void)
{
	driver_unregister(&palmtx_pm_driver);
}

module_init(palmtx_pm_init);
module_exit(palmtx_pm_exit);

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>");
MODULE_DESCRIPTION("Palm TX power management driver");
MODULE_LICENSE("GPL");
