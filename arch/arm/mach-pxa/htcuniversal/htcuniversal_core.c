/* Core Hardware driver for Hx4700 (Serial, ASIC3, EGPIOs)
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer             Converted  basic structure to support hx4700
 * 2005-04-30	Todd Blumer		Add IRDA code from H2200
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

#include <linux/mfd/asic3_base.h>
#include <asm/hardware/ipaq-asic3.h>

volatile u_int16_t *egpios;
u_int16_t egpio_reg;

static int htc_bootloader = 0;	/* Is the stock HTC bootloader installed? */

/*
 * may make sense to put egpios elsewhere, but they're here now
 * since they share some of the same address space with the TI WLAN
 *
 * EGPIO register is write-only
 */

void
htcuniversal_egpio_enable( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg |= bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(htcuniversal_egpio_enable);

void
htcuniversal_egpio_disable( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg &= ~bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(htcuniversal_egpio_disable);

#ifdef CONFIG_PM

void htcuniversal_ll_pm_init(void);

static int htcuniversal_suspend(struct platform_device *dev, pm_message_t state)
{
	/* Turn off external clocks here, because htcuniversal_power and asic3_mmc
	 * scared to do so to not hurt each other. (-5 mA) */


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
	asic3_set_clock_cdex(&htcuniversal_asic3.dev, 0xffff, CLOCK_CDEX_SOURCE1
	                                                     |CLOCK_CDEX_LED0
	                                                     |CLOCK_CDEX_LED1
	                                                     |CLOCK_CDEX_LED2
	                                                     |CLOCK_CDEX_EX0
	                                                     |CLOCK_CDEX_EX1);

	*egpios = 0;	/* turn off all egpio power */

	/* Wake up enable. */
	PWER =    PWER_GPIO0 
		| PWER_GPIO1 /* reset */
		| PWER_GPIO9 /* USB */
		| PWER_GPIO10 /* AC on USB */
		| PWER_GPIO14 /* ASIC3 mux */
		| PWER_RTC;
	/* Wake up on falling edge. */
	PFER =    PWER_GPIO0 
		| PWER_GPIO1
		| PWER_GPIO9
		| PWER_GPIO10
		| PWER_GPIO14;

	/* Wake up on rising edge. */
	PRER =    PWER_GPIO0 
		| PWER_GPIO1
		| PWER_GPIO9
		| PWER_GPIO10;
	/* 3.6864 MHz oscillator power-down enable */
	PCFR = PCFR_OPDE | PCFR_PI2CEN | PCFR_GPROD | PCFR_GPR_EN;

	PGSR0 = 0x09088004;
	PGSR1 = 0x00020002;
	PGSR2 = 0x8001c000;
	PGSR3 = 0x00106284;

	PSLR  = 0xcc000000;

#if 0
	/*
	 * If we're using bootldr and not the stock HTC bootloader,
	 * we want to wake up periodically to see if the charge is full while
	 * it is suspended.  We do this with the OS timer 4 in the pxa270.
	 */
	if (!htc_bootloader) {
		OMCR4 = 0x4b;   /* Periodic, self-resetting, 1-second timer */
		OSMR4 = 5;      /* Wake up bootldr after x seconds so it can
				   figure out what to do with the LEDs. */
		OIER |= 0x10;   /* Enable interrupt source for Timer 4 */
		OSCR4 = 0;      /* This starts the timer */
	}
#endif

	asic3_set_extcf_select(&htcuniversal_asic3.dev, ASIC3_EXTCF_OWM_EN, 0);

	return 0;
}

static int htcuniversal_resume(struct platform_device *dev)
{
	htcuniversal_egpio_enable(0);

	return 0;
}
#else
#   define htcuniversal_suspend NULL
#   define htcuniversal_resume NULL
#endif

static int
htcuniversal_core_probe( struct platform_device *dev )
{

	printk( KERN_NOTICE "HTC Universal Core Hardware Driver\n" );

	egpios = (volatile u_int16_t *)ioremap_nocache(HTCUNIVERSAL_EGPIO_BASE, sizeof *egpios );
	if (!egpios)
		return -ENODEV;
	else
	 printk( KERN_NOTICE "HTC Universal Core: egpio at phy=0x%8.8x is at virt=0x%p\n",
	  HTCUNIVERSAL_EGPIO_BASE, egpios );

	printk("Using stock HTC first stage bootloader\n");
	htc_bootloader = 1;

	htcuniversal_ll_pm_init();

	return 0;
}

static int
htcuniversal_core_remove( struct platform_device *dev )
{

	if (egpios != NULL)
		iounmap( (void *)egpios );

	return 0;
}

static struct platform_driver htcuniversal_core_driver = {
	.driver = {
		.name     = "htcuniversal_core",
	},
	.probe    = htcuniversal_core_probe,
	.remove   = htcuniversal_core_remove,
	.suspend  = htcuniversal_suspend,
	.resume   = htcuniversal_resume,
};

static int __init
htcuniversal_core_init( void )
{
	return platform_driver_register( &htcuniversal_core_driver );
}


static void __exit
htcuniversal_core_exit( void )
{
	platform_driver_unregister( &htcuniversal_core_driver );
}

module_init( htcuniversal_core_init );
module_exit( htcuniversal_core_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Universal Core Hardware Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/
