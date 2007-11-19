/************************************************************************
 * PalmOne Zire72 suspend/resume support				*
 *									*
 * Authors: Jan Herman <2hp@seznam.cz>					*
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

#include <asm/arch/palmz72-gpio.h>
#include <asm/arch/pxa27x_keyboard.h>

#ifdef CONFIG_PM
static int palmz72_suspend(struct device *dev, pm_message_t state)
{
	/* Wake-Up on RTC event, etc. */
	PWER |= PWER_RTC | PWER_WEP1;

	/* Wakeup by keyboard :) */
	PKWR = 0xe0001;
	
	/* Enabled Deep-Sleep mode */
	PCFR |= PCFR_DS;

	/* Low power mode */
	PCFR |= PCFR_OPDE;
	
	/* 3.6.8.1 */
	while(!(OSCC & OSCC_OOK)) 
	    {}

	/* Turn off LCD power */
	SET_PALMZ72_GPIO(LCD_POWER,0);
	/* Turn screen off */
	SET_PALMZ72_GPIO(SCREEN,0);
	/* Turn off USB power */
	SET_PALMZ72_GPIO(USB_POWER,0);

        /* hold current GPIO levels for now
         * TODO: find out what we can disable.
         */
        PGSR0 = GPLR0;
        PGSR1 = GPLR1;
        PGSR2 = GPLR2;
        PGSR3 = GPLR3;
	
	/* disable GPIO reset - DO NOT REMOVE! */
	/* not it, but we can't sleep otherwise */
	PCFR &= PCFR_GPR_EN;

    	return 0;
}

static int palmz72_resume(struct device *dev)
{
		
	/* Disabled Deep-Sleep mode ?? */
	PCFR &= PCFR_DS;

	/* Re-enable GPIO reset */
	PCFR |= PCFR_GPR_EN; /* !! DO NOT REMOVE !! THIS IS NECCESARY FOR ENABLE PALM RESET !! */


	/* Here are all of special to resume PalmOne Zire 72 */

	/* Turn on LCD power */
	SET_PALMZ72_GPIO(LCD_POWER,1);
	/* Turn screen on */
	SET_PALMZ72_GPIO(SCREEN,1);
	/* Turn on USB power */
	SET_PALMZ72_GPIO(USB_POWER,1);
	
	return 0;
}
#else
#define palmz72_suspend NULL
#define palmz72_resume NULL
#endif

/* We have some black magic here
 * PalmOS ROM on recover expects special struct phys address
 * to be transferred via PSPR. Using this struct PalmOS restores
 * its state after sleep. As for Linux, we need to setup it the
 * same way. More than that, PalmOS ROM changes some values in memory.
 * For now only one location is found, which needs special treatment.
 * Thanks to Alex Osborne, Andrzej Zaborowski, and lots of other people
 * for reading backtraces for me :)
 */

#define PALMZ72_SAVE_DWORD ((unsigned long *)0xc0000050)

static struct {
	u32 magic0;		/* 0x0 */  
	u32 magic1;		/* 0x4 */ 
	u32 resume_addr;	/* 0x8 */ 
	u32 pad[11];		/* 0xc..0x37 */ 
	                                                                                                                                                                              
	u32 arm_control;	/* 0x38 */ 
	u32 aux_control;	/* 0x3c */
	u32 ttb;		/* 0x40 */ 
	u32 domain_access;	/* 0x44 */
	u32 process_id;		/* 0x48 */
} palmz72_resume_info = {
	.magic0 = 0xb4e6,
	.magic1 = 1,

	/* reset state, MMU off etc */
	.arm_control = 0,
	.aux_control = 0,
	.ttb = 0,
	.domain_access = 0,
	.process_id = 0,
};
static unsigned long store_ptr;
static void palmz72_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	/* hold current GPIO levels on outputs */
	PGSR0 = GPLR0;
	PGSR1 = GPLR1;
	PGSR2 = GPLR2;
	PGSR3 = GPLR3;

	/* setup the resume_info struct for the original bootloader */
	palmz72_resume_info.resume_addr = resume_addr;
	printk(KERN_INFO "PSPR=%lu\n", virt_to_phys(&palmz72_resume_info));
	/* Storing memory touched by ROM */
	store_ptr = *PALMZ72_SAVE_DWORD;
	PSPR = virt_to_phys(&palmz72_resume_info);
}

static void palmz72_pxa_ll_pm_resume(void)
{
	/* Restoring memory touched by ROM */
	*PALMZ72_SAVE_DWORD = store_ptr;
}

struct pxa_ll_pm_ops palmz72_ll_pm_ops = {
	.suspend = palmz72_pxa_ll_pm_suspend,
	.resume = palmz72_pxa_ll_pm_resume,
};

static int palmz72_pm_probe(struct device *dev)
{
	printk(KERN_NOTICE "PalmOne Zire72 power management driver registered\n");
#ifdef CONFIG_PM
	pxa_pm_set_ll_ops(&palmz72_ll_pm_ops);
#endif
	return 0;
}

struct device_driver palmz72_pm_driver = {
	.name = "palmz72-pm",
	.bus = &platform_bus_type,
	.probe = palmz72_pm_probe,
	.suspend = palmz72_suspend,
	.resume = palmz72_resume,
};

static int __init palmz72_pm_init(void)
{
	return driver_register(&palmz72_pm_driver);
}

static void __exit palmz72_pm_exit(void)
{
	driver_unregister(&palmz72_pm_driver);
}

module_init(palmz72_pm_init);
module_exit(palmz72_pm_exit);

MODULE_AUTHOR("Jan Herman <2hp@seznam.cz>, Sergey Lapin <slapinid@gmail.com>");
MODULE_DESCRIPTION("PalmOne Zire 72 power management driver");
MODULE_LICENSE("GPL");
