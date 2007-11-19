/************************************************************************
 * PalmOne LifeDrive suspend/resume support				*
 *									*
 * Author: Jan Herman <2hp@seznam.cz>					*
 *	   Sergey Lapin <slapinid@gmail.com>                            *
 * Modification for Palm LifeDrive: Marek Vasut <marek.vasut@gmail.com> *
 *									*
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

#include <asm/arch/palmld-gpio.h>
#include <asm/arch/pxa27x_keyboard.h>

#define KPASMKP(col)           (col/2==0 ? KPASMKP0 : col/2==1 ? KPASMKP1 : col/2==2 ? KPASMKP2 : KPASMKP3)
#define KPASMKPx_MKC(row, col) (1 << (row + 16*(col%2)))

#ifdef CONFIG_PM
static int palmld_suspend(struct device *dev, pm_message_t state)
{
        /* Wake-Up on RTC event, etc. */
        PWER |= PWER_RTC | PWER_WEP1;
        /* Wake-Up on powerbutton */
        PWER |= PWER_GPIO12;
        PRER |= PWER_GPIO12;

        /* USB, in theory this can even wake us from deep sleep */
//        PWER  |= PWER_GPIO3;
//        PFER  |= PWER_GPIO3;
//        PRER  |= PWER_GPIO3;

        /* Wakeup by keyboard :) */
//        PKWR = 0xe0001;

        /* Enabled Deep-Sleep mode */
        PCFR |= PCFR_DS;

        /* Low power mode */
        PCFR |= PCFR_OPDE;


        /* 3.6.8.1 */
        while(!(OSCC & OSCC_OOK))
            {}

        /* Here are all of special for suspend PalmOne LifeDrive */

        /* Turn off LCD power */
        SET_PALMLD_GPIO(LCD_POWER,0);
        /* Turn screen off */
        SET_PALMLD_GPIO(SCREEN,0);

        /* disable GPIO reset, palm bootloader will hang us */
        PCFR &= PCFR_GPROD;

        return 0;
}

static int palmld_resume(struct device *dev)
{
	/* Disabled Deep-Sleep mode ?? */
	PCFR &= PCFR_DS;

	/* Re-enable GPIO reset */
	PCFR |= PCFR_GPR_EN; /* !! DO NOT REMOVE !! THIS IS NECCESARY FOR ENABLE PALM RESET !! */


	/* Here are all of special to resume PalmOne LifeDrive */

	/* Turn on LCD power */
	SET_PALMLD_GPIO(LCD_POWER,1);
        /* Turn screen on */
        SET_PALMLD_GPIO(SCREEN,1);

	return 0;
}
#else
#define palmld_suspend NULL
#define palmld_resume NULL
#endif

static void palmld_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	/* For future */
        return;
}

static void palmld_pxa_ll_pm_resume(void)
{
	/* For future */
}

struct pxa_ll_pm_ops palmld_ll_pm_ops = {
	.suspend = palmld_pxa_ll_pm_suspend,
	.resume = palmld_pxa_ll_pm_resume,
};

static int (*pxa_pm_enter_orig)(suspend_state_t state);

extern struct pm_ops pxa_pm_ops;

static int palmld_pm_probe(struct device *dev)
{
	printk(KERN_NOTICE "PalmOne LifeDrive power management driver registered\n");
	return 0;
}

struct device_driver palmld_pm_driver = {
	.name = "palmld-pm",
	.bus = &platform_bus_type,
	.probe = palmld_pm_probe,
	.suspend = palmld_suspend,
	.resume = palmld_resume,
};

static int __init palmld_pm_init(void)
{
pxa_pm_enter_orig=pxa_pm_ops.enter;
        pxa_pm_ops.enter = pxa_pm_enter_orig;
	return driver_register(&palmld_pm_driver);
}

static void __exit palmld_pm_exit(void)
{
        pxa_pm_ops.enter = pxa_pm_enter_orig;
	driver_unregister(&palmld_pm_driver);
}

module_init(palmld_pm_init);
module_exit(palmld_pm_exit);

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>, Sergey Lapin <slapinid@gmail.com>");
MODULE_DESCRIPTION("PalmOne LifeDrive power management driver");
MODULE_LICENSE("GPL");
