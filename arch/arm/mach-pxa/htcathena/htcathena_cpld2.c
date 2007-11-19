
/* CPLD2 driver for HTC Athena
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-03-29   Todd Blumer             Converted  basic structure to support hx4700
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
#include <asm/arch/htcathena-gpio.h>
#include <asm/arch/htcathena-asic.h>

volatile u_int16_t *egpios;
static u_int16_t egpio_reg;

static int htc_bootloader = 0;	/* Is the stock HTC bootloader installed? */

void htcathena_cpld2_gpio_set( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg |= bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(htcathena_cpld2_gpio_set);

void htcathena_cpld2_gpio_clr( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg &= ~bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(htcathena_cpld2_gpio_clr);

u_int16_t htcathena_cpld2_gpio_get(void)
{
 return *egpios;
}
EXPORT_SYMBOL_GPL(htcathena_cpld2_gpio_get);

#ifdef CONFIG_PM

//void htcathena_ll_pm_init(void);

static int htcathena_suspend(struct platform_device *dev, pm_message_t state)
{

	*egpios = 0;	/* turn off all CPLD2 gpio power */

	/* Wake up enable. */
	PWER =    PWER_GPIO0 /* power */
		| PWER_GPIO11 /* GSM */
		| PWER_GPIO14 /* CPLD1 mux */
		| PWER_RTC;
	/* Wake up on falling edge. */
	PFER =    PWER_GPIO0;

	/* Wake up on rising edge. */
	PRER =    PWER_GPIO0 
		| PWER_GPIO11
		| PWER_GPIO14;
	/* 3.6864 MHz oscillator power-down enable */
	PCFR = PCFR_OPDE | PCFR_PI2CEN | PCFR_GPROD | PCFR_GPR_EN;

#if 0
	PGSR0 = 0x09088004;
	PGSR1 = 0x00020002;
	PGSR2 = 0x8001c000;
	PGSR3 = 0x00106284;

	PSLR  = 0xcc000000;
#endif

	return 0;
}

static int htcathena_resume(struct platform_device *dev)
{
	htcathena_cpld2_gpio_set(0);

	return 0;
}
#else
#   define htcathena_suspend NULL
#   define htcathena_resume NULL
#endif

static int
htcathena_core_probe( struct platform_device *dev )
{
 u_int16_t	cpld2reg;
 int		boardid;

	printk( KERN_NOTICE "HTC Athena CPLD2 GPIO Driver\n" );

	egpios = (volatile u_int16_t *)ioremap_nocache(HTCATHENA_CPLD2_BASE, sizeof *egpios );
	if (!egpios)
		return -ENODEV;
	else
	 printk( KERN_NOTICE "HTC Athena CPLD2: gpio at phy=0x%8.8x is at virt=0x%p\n",
	  HTCATHENA_CPLD2_BASE, egpios );

	printk("Using stock HTC first stage bootloader\n");
	htc_bootloader = 1;
	
	boardid=0;
	cpld2reg=htcathena_cpld2_gpio_get();

	if( cpld2reg & (1<<HTCATHENA_BOARDID0) ) {
		boardid |= 1;
	}
	if( cpld2reg & (1<<HTCATHENA_BOARDID1) ) {
		boardid |= 2;
	}
	if( cpld2reg & (1<<HTCATHENA_BOARDID2) ) {
		boardid |= 4;
	}
	printk("HTC Athena boardid=%d\n",boardid);

//	htcathena_ll_pm_init();

	return 0;
}

static int
htcathena_core_remove( struct platform_device *dev )
{

	if (egpios != NULL)
		iounmap( (void *)egpios );

	return 0;
}

static struct platform_driver htcathena_core_driver = {
	.driver = {
		.name     = "htcathena_core",
	},
	.probe    = htcathena_core_probe,
	.remove   = htcathena_core_remove,
	.suspend  = htcathena_suspend,
	.resume   = htcathena_resume,
};

static int __init
htcathena_core_init( void )
{
	return platform_driver_register( &htcathena_core_driver );
}


static void __exit
htcathena_core_exit( void )
{
	platform_driver_unregister( &htcathena_core_driver );
}

module_init( htcathena_core_init );
module_exit( htcathena_core_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("HTC Athena CPLD2 Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/
