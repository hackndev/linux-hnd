/* Core Hardware driver for Hx4700 (ASIC3, EGPIOs)
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
#include <linux/dpm.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>
#include <asm/arch/hx4700-core.h>

#include <linux/mfd/asic3_base.h>
#include <asm/hardware/ipaq-asic3.h>

#define EGPIO_OFFSET	0
#define EGPIO_BASE	(PXA_CS5_PHYS+EGPIO_OFFSET)

volatile u_int16_t *egpios;
u_int16_t egpio_reg;

static int htc_bootloader = 0;	/* Is the stock HTC bootloader installed? */
static u32 save[4];
static u32 save2[13];

/*
 * may make sense to put egpios elsewhere, but they're here now
 * since they share some of the same address space with the TI WLAN
 *
 * EGPIO register is write-only
 */

void
hx4700_egpio_enable( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg |= bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL(hx4700_egpio_enable);

void
hx4700_egpio_disable( u_int16_t bits )
{
	unsigned long flags;

	local_irq_save(flags);

	egpio_reg &= ~bits;
	*egpios = egpio_reg;

	local_irq_restore(flags);
}
EXPORT_SYMBOL(hx4700_egpio_disable);

#ifdef CONFIG_PM
static int hx4700_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* Turn off external clocks here, because hx4700_power and asic3_mmc
	 * scared to do so to not hurt each other. (-5 mA) */
#if 0
	asic3_set_clock_cdex(&hx4700_asic3.dev,
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
	asic3_set_clock_cdex(&hx4700_asic3.dev, 0xffff, 0x21c2);

	*egpios = 0;	/* turn off all egpio power */
	/*
	 * Note that WEP1 wake up event is used by bootldr to set the
	 * LEDS when power is applied/removed for charging.
	 */
	PWER = PWER_RTC | PWER_GPIO0 | PWER_GPIO1 | PWER_GPIO12 | PWER_WEP1;	// rtc + power + reset + asic3 + wep1
	PFER = PWER_GPIO1;				// Falling Edge Detect
	PRER = PWER_GPIO0 | PWER_GPIO12;		// Rising Edge Detect

	PGSR0 = 0x080DC01C;
	PGSR1 = 0x34CF0002;
	PGSR2 = 0x0123C18C;
	/* PGSR3 = 0x00104202; */
	PGSR3 = 0x00100202;

	/* These next checks are specifically for charging.  We want to enable
	* it if it is already enabled */
	/* Check for charge enable, GPIO 72 */
	if(GPLR2 & (1 << 8)) {
		/* Set it */
		PGSR2 |= (1U << 8);
	} else {
		/* Clear it */
		PGSR2 &= ~(1U << 8);
	}
	/* Check for USB_CHARGE_RATE, GPIO 96 */
	if(GPLR3 & (1 << 0)) {
		/* Set it */
		PGSR3 |= (1U << 0);
	} else {
		/* Clear it */
		PGSR3 &= ~(1U << 0);
	}

	PCFR = PCFR_GPROD|PCFR_DC_EN|PCFR_GPR_EN|PCFR_OPDE
		|PCFR_FP|PCFR_PI2CEN; /* was 0x1091; */
	/* The 2<<2 below turns on the Power Island state preservation
	 * and counters.  This allows us to wake up bootldr after a
	 * period of time, and it can set the LEDs correctly based on
	 * the power state.  The bootldr turns it off when it's
	 * charged.
	 */
	PSLR=0xc8000000 | (2 << 2);

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

	asic3_set_extcf_select(&hx4700_asic3.dev, ASIC3_EXTCF_OWM_EN, 0);

	return 0;
}

static int hx4700_resume(struct platform_device *pdev)
{
	hx4700_egpio_enable(0);
	return 0;
}
#else
#   define hx4700_suspend NULL
#   define hx4700_resume NULL
#endif

static void
hx4700_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	int i;
	u32 csum, tmp, *p;

	/* Save the 13 words at 0xa0038000. */
	for (p = phys_to_virt(0xa0038000), i = 0; i < 13; i++)
		save2[i] = p[i];

	/* Save the first four words at 0xa0000000. */
	for (p = phys_to_virt(0xa0000000), i = 0; i < 4; i++)
		save[i] = p[i];

	/* Set the first four words at 0xa0000000 to:
	 * resume address; MMU control; TLB base addr; domain id */
	p[0] = resume_addr;

	asm( "mrc\tp15, 0, %0, c1, c0, 0" : "=r" (tmp) );
	p[1] = tmp & ~(0x3987);	    /* mmu off */

	asm( "mrc\tp15, 0, %0, c2, c0, 0" : "=r" (tmp) );
	p[2] = tmp;	/* Shouldn't matter, since MMU will be off. */

	asm( "mrc\tp15, 0, %0, c3, c0, 0" : "=r" (tmp) );
	p[3] = tmp;	/* Shouldn't matter, since MMU will be off. */

	/* Set PSPR to the checksum the HTC bootloader wants to see. */
	for (csum = 0, i = 0; i < 52; i++) {
		tmp = p[i] & 0x1;
		tmp = tmp << 31;
		tmp |= tmp >> 1;
		csum += tmp;
	}

	PSPR = csum;
}

static void
hx4700_pxa_ll_pm_resume(void)
{
	int i;
	u32 *p;

	/* Restore the first four words at 0xa0000000. */
	for (p = phys_to_virt(0xa0000000), i = 0; i < 4; i++)
		p[i] = save[i];

	/* Restore the 13 words at 0xa0038000. */
	for (p = phys_to_virt(0xa0038000), i = 0; i < 13; i++)
		p[i] = save2[i];

	/* XXX Do we need to flush the cache? */
}

struct pxa_ll_pm_ops hx4700_ll_pm_ops = {
	.suspend = hx4700_pxa_ll_pm_suspend,
	.resume  = hx4700_pxa_ll_pm_resume,
};

/* automatic backlight brightness control */
static ssize_t auto_brightness_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", GET_HX4700_GPIO(AUTO_SENSE) ? 1 : 0);
}

static ssize_t auto_brightness_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
        int auto_brightness = simple_strtoul(buf, NULL, 10) ? 1 : 0;

	SET_HX4700_GPIO(AUTO_SENSE, auto_brightness);

        return count;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show,
		   auto_brightness_store);


static int
hx4700_core_probe( struct platform_device *pdev )
{
	u32 *bootldr;
	int i;
	int ret = 0;

	printk( KERN_NOTICE "hx4700 Core Hardware Driver\n" );

	egpios = (volatile u_int16_t *)ioremap_nocache( EGPIO_BASE, sizeof *egpios );
	if (!egpios)
		return -ENODEV;

	/* Is the stock HTC bootloader installed? */

	bootldr = (u32 *) ioremap(PXA_CS0_PHYS, 1024 * 1024);

	/* Windows Mobile 2003 Second Edition v. 4.21.1088 Build 15045.2.6.0
	 * ROM date 4/13/05, rev 1.10.08 ENG, bootloader 1.01,
	 * XIP v4.21.15045.0 */
	i = 0x000414dc / 4;
	if (bootldr[i]   == 0xe59f1360 && /* ldr r1, [pc, #864] ; power base */
	    bootldr[i+1] == 0xe5914008 && /* ldr r4, [r1, #8]   ; PSPR */
	    bootldr[i+2] == 0xe1320004) { /* teq r2, r4 */

		printk("Stock HTC WM2003 bootloader detected\n");
		htc_bootloader = 1;
		pxa_pm_set_ll_ops(&hx4700_ll_pm_ops);
	}

	/* XXX Which version of WM2005 is this? */
	i = 0x00041d68 / 4;
	if (bootldr[i]   == 0xe59f1354 && /* ldr r1, [pc, #852] ; power base */
	    bootldr[i+1] == 0xe5914008 && /* ldr r4, [r1, #8]   ; PSPR */
	    bootldr[i+2] == 0xe1320004) { /* teq r2, r4 */

		printk("Stock HTC WM2005 bootloader detected\n");
		htc_bootloader = 1;
		pxa_pm_set_ll_ops(&hx4700_ll_pm_ops);
	}

	/* WM 5.0 OS 5.1.70 Build 14406.1.1.1 */
	i = 0x00041340 / 4;
	if (bootldr[i]   == 0xe59f1354 && /* ldr r1, [pc, #852] ; power base */
	    bootldr[i+1] == 0xe5914008 && /* ldr r4, [r1, #8]   ; PSPR */
	    bootldr[i+2] == 0xe1320004) { /* teq r2, r4 */

		printk("Stock HTC WM2005 bootloader detected\n");
		htc_bootloader = 1;
		pxa_pm_set_ll_ops(&hx4700_ll_pm_ops);
	}

	iounmap(bootldr);

	ret = device_create_file(&pdev->dev, &dev_attr_auto_brightness);
	if (ret)
		iounmap(egpios);

	return ret;
}

static int
hx4700_core_remove( struct platform_device *pdev )
{
	struct hx4700_core_funcs *funcs = pdev->dev.platform_data;

	device_remove_file(&pdev->dev, &dev_attr_auto_brightness);

	if (egpios != NULL)
		iounmap( (void *)egpios );
	funcs->udc_detect = NULL;
	return 0;
}

static struct platform_driver hx4700_core_driver = {
	.driver = {
		.name     = "hx4700-core",
	},
	.probe    = hx4700_core_probe,
	.remove   = hx4700_core_remove,
	.suspend  = hx4700_suspend,
	.resume   = hx4700_resume,
};

static int __init
hx4700_core_init( void )
{
	return platform_driver_register( &hx4700_core_driver );
}


static void __exit
hx4700_core_exit( void )
{
	platform_driver_unregister( &hx4700_core_driver );
}

module_init( hx4700_core_init );
module_exit( hx4700_core_exit );

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("hx4700 Core Hardware Driver");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/
