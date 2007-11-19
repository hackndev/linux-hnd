/*
 * Hardware definitions for HTC Himalaya
 *
 * Copyright 2004 Xanadux.org
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Authors: w4xy@xanadux.org
 *
 * History:
 *
 * 2004-02-07	W4XY		   Initial port heavily based on h1900.c
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/types.h>
#include <asm/delay.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irq.h>
#include <asm/arch/udc.h>
#include <asm-arm/arch-pxa/serial.h>

#include <asm/hardware/ipaq-asic3.h>
#include <asm/arch-pxa/htchimalaya.h>
#include <asm/arch-pxa/htchimalaya-gpio.h>
#include <asm/arch-pxa/htchimalaya-asic.h>

#include <linux/serial_core.h>

#include "../generic.h"

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/mfd/asic3_base.h>

#include <linux/mfd/tmio_mmc.h>    /* TODO: replace with asic3 */

static void __init himalaya_init_irq( void )
{
	/* Initialize standard IRQs */
	pxa_init_irq();
}

/*
 * Common map_io initialization
 */
static void __init himalaya_map_io(void)
{

	pxa_map_io();

#if 0
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;
#endif

#if 1
	GAFR0_L = 0x98000000;
	GAFR0_U = 0x494A8110;
	GAFR1_L = 0x699A8159;
	GAFR1_U = 0x0005AAAA;
	GAFR2_L = 0xA0000000;
	GAFR2_U = 0x00000002;

	/* don't do these for now because one of them turns the screen to mush */
	/* reason: the ATI chip gets reset / LCD gets disconnected:
	 * a fade-to-white means that the ati 3200 registers are set incorrectly */
	GPCR0   = 0xFF00FFFF;
	GPCR1   = 0xFFFFFFFF;
	GPCR2   = 0xFFFFFFFF;

	GPSR0   = 0x444F88EF;
	GPSR1   = 0x57BF7306; // 0xD7BF7306;
	GPSR2   = 0x03FFE008;

	PGSR0   = 0x40DF88EF;
	PGSR1   = 0x53BF7206;
	PGSR2   = 0x03FFE000;

	GPDR0   = 0xD7E9A042;
	GPDR1   = 0xFCFFABA3;
	GPDR2   = 0x000FEFFE;

	GRER0   = 0x00000000;
	GRER1   = 0x00000000;
	GRER2   = 0x00000000;

	GFER0   = 0x00000000;
	GFER1   = 0x00000000;
	GFER2   = 0x00000000;
#endif

#if 0
#if 1
	/* power up the UARTs which likely got switched off, above */
	GPDR(GPIO_NR_HIMALAYA_UART_POWER) |= GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
	GPSR(GPIO_NR_HIMALAYA_UART_POWER) = GPIO_bit(GPIO_NR_HIMALAYA_UART_POWER);
#endif

	pxa_serial_funcs[2].configure = (void *) ser_stuart_gpio_config;
	pxa_serial_funcs[0].configure = (void *) ser_ffuart_gpio_config;
	pxa_set_stuart_info(&pxa_serial_funcs[2]);
	pxa_set_ffuart_info(&pxa_serial_funcs[0]);
#endif

#if 0
	/* Add wakeup on AC plug/unplug (and resume button) */
	PWER = PWER_RTC | PWER_GPIO4 | PWER_GPIO0;
	PFER = PWER_RTC | PWER_GPIO4 | PWER_GPIO0;
	PRER =            PWER_GPIO4 | PWER_GPIO0;
	PCFR = PCFR_OPDE;
#endif
}

/* 
 * All the asic3 dependant devices 
 */

extern struct platform_device himalaya_bl;
static struct platform_device himalaya_udc = { .name = "himalaya-udc", };
static struct platform_device himalaya_lcd = { .name = "himalaya-lcd", };
//static struct platform_device himalaya_leds = { .name = "himalaya-leds", };
#ifdef CONFIG_HIMALAYA_PCMCIA
static struct platform_device himalaya_pcmcia    = { .name =
                                            "htchimalaya_pcmcia", };
#endif

static struct platform_device *himalaya_asic3_devices[] __initdata = {
  &himalaya_lcd,
  &himalaya_udc,
//  &himalaya_leds,
};

/*
 * the ASIC3 should really only be referenced via the asic3_base
 * module.  it contains functions something like asic3_gpio_b_out()
 * which should really be used rather than macros.
 *
 */

static int himalaya_get_mmc_ro(struct platform_device *dev)
{
// return (((asic3_get_gpio_status_d(asic3_dev)) & (1<<GPIOD_SD_WRITE_PROTECT)) != 0);
 return asic3_gpio_get_value(asic3_dev,GPIO_SD_WRITE_PROTECT);
}

static struct tmio_mmc_hwconfig himalaya_mmc_hwconfig = {
        .mmc_get_ro = himalaya_get_mmc_ro,
};

static struct asic3_platform_data asic3_platform_data = {
	.gpio_a = {
		.dir		= 0xbfff,
		.init		= 0x4061, /* or 406b */
		.sleep_out	= 0x4001,
		.batt_fault_out	= 0x4001,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x9800, 
	},
	.gpio_b = {
		.dir		= 0xffff,
		.init		= 0x0f98, /* or 0fb8 */
		.sleep_out	= 0x8220,
		.batt_fault_out	= 0x0220,
		.sleep_conf	= 0x000c,
		.alt_function	= 0x0000, 
	},
	.gpio_c = {
		.dir		= 0x0187,
		.init		= 0xfe04,             
		.sleep_out	= 0xfe00,            
		.batt_fault_out	= 0xfe00,            
		.sleep_conf	= 0x0008, 
		.alt_function	= 0x0003,
	},
	.gpio_d = {
		.dir		= 0x10e0,
		.init		= 0x6907,            
		.sleep_mask	= 0x0000,
		.sleep_out	= 0x6927,            
		.batt_fault_out = 0x6927,  
		.sleep_conf	= 0x0008, 
		.alt_function	= 0x0000,
	},
	.bus_shift		 = 2,
	.irq_base = HTCHIMALAYA_ASIC3_IRQ_BASE,

	.child_devs     = himalaya_asic3_devices,
	.num_child_devs = ARRAY_SIZE(himalaya_asic3_devices),
	.tmio_mmc_hwconfig	 = &himalaya_mmc_hwconfig,
};

static struct resource asic3_resources[] = {
	[0] = {
		.start  = HIMALAYA_ASIC3_GPIO_PHYS,
		.end    = HIMALAYA_ASIC3_GPIO_PHYS + 0xfffff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_NR_HIMALAYA_ASIC3,
		.end    = IRQ_NR_HIMALAYA_ASIC3,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		.start  = HIMALAYA_ASIC3_MMC_PHYS,
		.end    = HIMALAYA_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_HIMALAYA_SD_IRQ_N),
		.flags  = IORESOURCE_IRQ,
	},
};


struct platform_device himalaya_asic3 = {
	.name       = "asic3",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(asic3_resources),
	.resource   = asic3_resources,
	.dev = {
		.platform_data = &asic3_platform_data
	},
};
struct device *asic3_dev=&himalaya_asic3.dev;

EXPORT_SYMBOL_GPL(asic3_dev);

static struct platform_device *devices[] __initdata = {
	&himalaya_asic3,
};

static void __init himalaya_init(void)
{
	platform_add_devices (devices, ARRAY_SIZE (devices));
}

MACHINE_START(HIMALAYA, "HTC Himalaya")
	/* MAINTAINER("Xanadux.org")*/
	.phys_io = 0x40000000, 
	.io_pg_offst = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params = 0xa0000100,
	.map_io = himalaya_map_io,
	.init_irq = himalaya_init_irq,
	.timer = &pxa_timer,
	.init_machine = himalaya_init,
MACHINE_END
