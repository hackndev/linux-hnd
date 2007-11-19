/* Core Hardware driver for HTC Sable (Serial, ASIC3, EGPIOs)
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer             Converted  basic structure to support hx4700
 * 2005-04-30	Todd Blumer		Add IRDA code from H2200
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/pm.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>

#include <linux/mfd/asic3_base.h>
#include <asm/hardware/ipaq-asic3.h>




#ifdef CONFIG_PM

void htcsable_ll_pm_init(void);

static int htcsable_suspend(struct platform_device *dev, pm_message_t state)
{
	/* Turn off external clocks here, because htcsable_power and asic3_mmc
	 * scared to do so to not hurt each other. (-5 mA) */
#if 0
	asic3_set_clock_cdex(&htcsable_asic3.dev,
		CLOCK_CDEX_EX0 | CLOCK_CDEX_EX1, 0 | 0);
#endif
	/* 0x20c2 is HTC clock value
	 * CLOCK_CDEX_SOURCE		2
	 * CLOCK_CDEX_SPI		0
	 * CLOCK_CDEX_OWM		0
	 *
	 * CLOCK_CDEX_PWM0		0
	 * CLOCK_CDEX_PWM1		0
	 * CLOCK_CDEX_LED0		1
	 * CLOCK_CDEX_LED1		1
	 *
	 * CLOCK_CDEX_LED2		0
	 * CLOCK_CDEX_SD_HOST		0
	 * CLOCK_CDEX_SD_BUS		0
	 * CLOCK_CDEX_SMBUS		0
	 *
	 * CLOCK_CDEX_CONTROL_CX	0
	 * CLOCK_CDEX_EX0		1
	 * CLOCK_CDEX_EX1		0
	 * */
	asic3_set_clock_cdex(&htcsable_asic3.dev, 0xffff, 0x73cc);

	/* Wake up enable. */
	PWER =    PWER_GPIO0 
		| PWER_GPIO1 /* reset */
		| PWER_GPIO9 /* */
		| PWER_RTC;
	/* Wake up on falling edge. */
	PFER =    PWER_GPIO0 ;

	/* Wake up on rising edge. */
	PRER =    PWER_GPIO0 
		| PWER_GPIO9;

	/* 3.6864 MHz oscillator power-down enable */
	PCFR = PCFR_OPDE | PCFR_PI2CEN | (1<<5);

	PGSR0 = 0x00004018;
	PGSR1 = 0x00020802;
	PGSR2 = 0x0001c000;
	PGSR3 = 0x00100080;

	PSLR  = 0xcc000000;

	asic3_set_extcf_select(&htcsable_asic3.dev, ASIC3_EXTCF_OWM_EN, 0);

	return 0;
}

static int htcsable_resume(struct platform_device *dev)
{
	return 0;
}
#else
#   define htcsable_suspend NULL
#   define htcsable_resume NULL
#endif

static int
htcsable_core_probe( struct platform_device *dev )
{
	printk( KERN_NOTICE "HTC Sable Core Hardware Driver\n" );

	htcsable_ll_pm_init();

	return 0;
}

static int
htcsable_core_remove( struct platform_device *dev )
{

	return 0;
}

struct platform_driver htcsable_core_driver = {
	.driver = {
		.name     = "htcsable_core",
	},
	.probe    = htcsable_core_probe,
	.remove   = htcsable_core_remove,
	.suspend  = htcsable_suspend,
	.resume   = htcsable_resume,
};

static int __init
htcsable_core_init( void )
{
	return platform_driver_register( &htcsable_core_driver );
}


static void __exit
htcsable_core_exit( void )
{
	platform_driver_unregister( &htcsable_core_driver );
}

module_init( htcsable_core_init );
module_exit( htcsable_core_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Sable Core Hardware Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/


