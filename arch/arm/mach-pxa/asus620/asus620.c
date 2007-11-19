/*
 *  linux/arch/arm/mach-pxa/asus620.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2006 Vincent Benony
 *  Copyright (c) 2004 Vitaliy Sardyko
 *  Copyright (c) 2003,2004 Adam Turowski
 *  Copyright (c) 2001 Cliff Brake, Accelent Systems Inc.
 *
 *
 *  2001-09-13: Cliff Brake <cbrake@accelent.com>
 *              Initial code for IDP
 *  2003-12-03: Adam Turowski
 *		code adaptation for Asus 620
 *  2004-07-23: Vitaliy Sardyko
 *		updated to 2.6.7 format,
 *		split functions between modules.
 *  2006-01-16: Vincent Benony
 *              gpo handling
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/corgi_bl.h>
//#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/slab.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irda.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/irda.h>

#include <asm/arch/asus620-init.h>
#include <asm/arch/asus620-gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa-pm_ll.h>
#include <linux/backlight.h>

#include "../generic.h"
#include <linux/platform_device.h>

#define DEBUG 1

#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

// Protos
int	asus620_gpo_init(void);
void	asus620_ts_init_chip(void);
int	asus620_ts_init(void);
void	asus620_ts_exit(void);
void	asus620_gpo_resume(void);
void	asus620_gpo_suspend(void);
void	asus620_gpo_clear(unsigned long bits);

/*****************************************************
 *
 * UDC
 *
 *****************************************************/

static int asus620_udc_is_connected(void)
{
	int ret = !!GET_A620_GPIO(USB_DETECT);
	DPRINTK("%d\n", ret);
	return ret;
}

static void asus620_udc_command(int cmd)
{
	DPRINTK("%d\n", cmd);

	switch(cmd){
		case PXA2XX_UDC_CMD_DISCONNECT:
			pxa_gpio_mode (GPIO_NR_A620_USBP_PULLUP | GPIO_IN);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			SET_A620_GPIO(USBP_PULLUP, 1);
			pxa_gpio_mode (GPIO_NR_A620_USBP_PULLUP | GPIO_OUT);
			break;
		default:
			DPRINTK("asus620_udc_control: unknown command!\n");
			break;
	}
}

/*****************************************************
 *
 * Backlight
 *
 *****************************************************/

/*
 * Definyinng minimum because below some value, backlight power just drops
 */
#define MIN_BRIGHT_PHYS 213
#define MAX_BRIGHT_PHYS 0x3ff
#define MAX_BRIGHTNESS (MAX_BRIGHT_PHYS - MIN_BRIGHT_PHYS)
void asus620_backlight_set_intensity(int intensity)
{
	DPRINTK("bright: %d\n", intensity);

	PWM_CTRL0   = 7;
	PWM_PWDUTY0 = intensity + MAX_BRIGHTNESS;
	PWM_PERVAL0 = 0x3ff;

	if (intensity)
		CKEN |= CKEN0_PWM0;
	else
		CKEN &= ~CKEN0_PWM0;
}

static struct corgibl_machinfo asus620_bl_machinfo = {
        .default_intensity = MAX_BRIGHTNESS / 4,
        .limit_mask = 0xffff,
        .max_intensity = MAX_BRIGHTNESS,
        .set_bl_intensity = asus620_backlight_set_intensity,
};


/*****************************************************
 *
 * Devices
 *
 *****************************************************/

// DoC
static struct resource mtd_resources[]=
{
	[0] = {
		.start = 0x00000000,
		.end   = 0x00008000,
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device mtd_device =
{
	.name          = "mtd",
	.id            = -1,
	.resource      = mtd_resources,
	.num_resources = ARRAY_SIZE(mtd_resources),
};

// IR

static void asus620_irda_transceiver_mode(struct device *dev, int mode) 
{
	unsigned long flags;

	local_irq_save(flags);

	if (mode & IR_SIRMODE)
		asus620_gpo_clear(GPO_A620_IRDA_FIR_MODE);
	else if (mode & IR_FIRMODE)
		asus620_gpo_set(GPO_A620_IRDA_FIR_MODE);

	if (mode & IR_OFF)
		asus620_gpo_set(GPO_A620_IRDA_POWER_N);
	else
		asus620_gpo_clear(GPO_A620_IRDA_POWER_N);

	local_irq_restore(flags);
}

static struct pxaficp_platform_data asus620_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_FIRMODE,
	.transceiver_mode = asus620_irda_transceiver_mode,
};

static struct pxa2xx_udc_mach_info asus620_udc_mach_info __initdata = {
	.udc_is_connected = asus620_udc_is_connected,
	.udc_command      = asus620_udc_command,
};

struct platform_device asus620_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &asus620_bl_machinfo,
	},
};

static struct platform_device *devices[] __initdata = {
	&mtd_device,
	&asus620_bl,
};

/*****************************************************
 *
 * Power management
 *
 *****************************************************/

#ifdef CONFIG_PM
static int asus620_probe(struct platform_device *dev)
{
	return 0;
}

static int asus620_suspend(struct platform_device *pdev, pm_message_t state)
{
	asus620_gpo_suspend();
	return 0;
}

static int asus620_resume(struct platform_device *pdev)
{
	asus620_gpo_resume();
#ifdef CONFIG_A620_TS
	asus620_ts_init_chip();
#endif
	return 0;
}

static u32 *addr_a2000400;
static u32 *addr_a2000404;
static u32 *addr_a2000408;
static u32 *addr_a200040c;
static u32 save_a2000400;
static u32 save_a2000404;
static u32 save_a2000408;
static u32 save_a200040c;
static u32 save_bright1;
static u32 save_bright2;

static void asus620_pxa_ll_pm_suspend(unsigned long resume_addr)
{
	asus620_gpo_suspend();

	save_a2000400 = *addr_a2000400;
	save_a2000404 = *addr_a2000404;
	save_a2000408 = *addr_a2000408;
	save_a200040c = *addr_a200040c;

	save_bright1  = PWM_PWDUTY0;
	save_bright2  = PWM_PERVAL0;

	*addr_a2000400 = 0xe3a00101; // mov r0, #0x40000000
	*addr_a2000404 = 0xe380060f; // orr r0, r0, #0x0f000000
	*addr_a2000408 = 0xe3800008; // orr r0, r0, #8
	*addr_a200040c = 0xe590f000; // ldr pc, [r0]

	//*addr_a2000400 = 0xe3a0044f; // mov r0, #0x4f000000
	//*addr_a2000404 = 0xe3800008; // orr r0, r0, #8
	//*addr_a2000408 = 0xe590f000; // ldr pc, [r0]

	return;
}

static void asus620_pxa_ll_pm_resume(void)
{
	*addr_a2000400 = save_a2000400;
	*addr_a2000404 = save_a2000404;
	*addr_a2000408 = save_a2000408;
	*addr_a200040c = save_a200040c;

	asus620_gpo_set(GPO_A620_BACKLIGHT);
	PWM_PWDUTY0 = save_bright1;
	PWM_PERVAL0 = save_bright2;

	asus620_gpo_resume();

	SSCR0  = 0;
	SSCR0 |= 0xB; /* 12 bits */
	SSCR0 |= SSCR0_National;
	SSCR0 |= 0x1100; /* 100 mhz */

	SSCR1  = 0;

	SSCR0 |= SSCR0_SSE;
}

static struct pxa_ll_pm_ops asus620_ll_pm_ops = {
	.suspend = asus620_pxa_ll_pm_suspend,
	.resume  = asus620_pxa_ll_pm_resume,
};

void asus620_ll_pm_init(void) {
	addr_a2000400 = phys_to_virt(0xa2000400);
	addr_a2000404 = phys_to_virt(0xa2000404);
	addr_a2000408 = phys_to_virt(0xa2000408);
	addr_a200040c = phys_to_virt(0xa200040c);

	pxa_pm_set_ll_ops(&asus620_ll_pm_ops);
}
#else
#define asus620_suspend NULL
#define asus620_resume NULL
#endif

static struct platform_driver asus620_driver = {
	.driver		= {
		.name	= "a620",
	},
	.probe          = asus620_probe,
#ifdef CONFIG_PM
	.suspend        = asus620_suspend,
	.resume         = asus620_resume,
#endif
};

/*****************************************************
 *
 * Additional IO mapping
 *
 *****************************************************/

static struct map_desc doc_io_desc[] __initdata = {
	{ 0xef800000, 0x00000000, 0x00002000, MT_DEVICE }, /* DiskOnChip Millenium + */
};

static void __init asus620_map_io(void)
{
	pxa_map_io();
	iotable_init(doc_io_desc,ARRAY_SIZE(doc_io_desc));
}

/*****************************************************
 *
 * Init
 *
 *****************************************************/

static void __init asus620_init(void)
{
	DPRINTK("asus620_init()\n");

	GPDR0 = 0xD38B6000;			// Directions
	GPDR1 = 0xFCFFA881;
	GPDR2 = 0x0001FBFF;

	GAFR0_L = 0x00000004;			// Alternate functions
	GAFR0_U = 0x001A8002;
	GAFR1_L = 0x69908010;
	GAFR1_U = 0xAAA58AAA;

	if (GPLR2 & 0x400)
		GAFR2_L = 0x0A8AAAAA;
	else
		GAFR2_L = 0x0A0AAAAA;
	GAFR2_U = 0x00000002;

	PGSR0 = 0x00020000;			// Sleep GPIO 17 : GPIO_NR_A620_CF_ENABLE
	PGSR1 = 0x00700000;			// 52, 53, 54 : CF
	PGSR2 = 0x00010000;			// 80 : nCS[4] SRAM

//	PGSR0 = 0x00080000;			// GPIO(19)    : CF related to sleep mode
//	PGSR1 = 0x03ff0000;			// GPIO(48-57) : CF by PXA memory controller
//	PGSR2 = 0x00010400;			// GPIO(74,80) : LCD enable control, nCS[4] bank 4 SRAM (GPO)

	PWER = PWER_RTC | PWER_GPIO0;		// Alarm or power button
	PFER = PWER_GPIO0;
	PRER = 0;
	PEDR = 0;
	PCFR = PCFR_OPDE;

	asus620_gpo_init();

	/* Just remove USB pullup, in order to reset connection. */
	pxa_gpio_mode (GPIO_NR_A620_USBP_PULLUP | GPIO_IN);
	
	pxa_set_udc_info(&asus620_udc_mach_info);
	pxa_set_ficp_info(&asus620_ficp_platform_data);

#ifdef CONFIG_PM
	asus620_ll_pm_init();
#endif

	if (platform_driver_register(&asus620_driver)) {
		printk(KERN_ERR "A620 driver registration failed.\n");
	}
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(A620, "Asus MyPal A620")
	MODULE_AUTHOR("Adam Turowski, Vitaliy Sardyko, Vincent Benony")
	//.phys_ram     = 0xa0000000,
	.boot_params  = 0xa0000100,
	.phys_io      = 0x40000000,
	.io_pg_offst  = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io       = asus620_map_io,
	.init_irq     = pxa_init_irq,
	.timer        = &pxa_timer,
	.init_machine = asus620_init
MACHINE_END

