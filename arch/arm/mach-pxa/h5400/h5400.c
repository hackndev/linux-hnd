/*
 * Hardware definitions for HP iPAQ Handheld Computers
 *
 * Copyright 2000-2003 Hewlett-Packard Company.
 * Copyright 2004, 2005 Phil Blundell
 * Copyright 2007 Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2002-08-23   Jamey Hicks        GPIO and IRQ support for iPAQ H5400
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <../drivers/video/pxafb.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/adc.h>
#include <linux/mfd/samcop_adc.h>
#include <linux/gpiodev_keys.h>
#include <linux/gpiodev_diagonal.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/pda_power.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/arch/h5400.h>
#include <asm/arch/h5400-gpio.h>
#include <asm/arch/h5400-asic.h>
#include <asm/arch/h5400-init.h>
#include <asm/arch/udc.h>

#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/irq.h>
#include <asm/types.h>
#include <linux/mfd/samcop_base.h>
#include <asm/hardware/samcop_leds.h>

#include "../generic.h"

/***********************************************************************************/
/*      EGPIO, etc                                                                 */
/***********************************************************************************/

#ifdef DeadCode

static void h5400_control_egpio( enum ipaq_egpio_type x, int setp )
{
	switch (x) {

	case IPAQ_EGPIO_RS232_ON:
		SET_H5400_GPIO (POWER_RS232_N, !setp);
		break;

	default:
		printk("%s: unhandled ipaq gpio=%d\n", __FUNCTION__, x);
	}
}

#endif

/* DMA bouncing */
static int h5400_dma_needs_bounce (struct device *dev, dma_addr_t addr, size_t size)
{
	return (addr >= H5400_SAMCOP_BASE + _SAMCOP_SRAM_Base + SAMCOP_SRAM_SIZE);
};

/****************************************************************************
 * Bluetooth functions and data structures
 ****************************************************************************/

void (*h5400_btuart_configure)(int state);
void (*h5400_hwuart_configure)(int state);
EXPORT_SYMBOL(h5400_btuart_configure);
EXPORT_SYMBOL(h5400_hwuart_configure);

static void h5400_btuart_configure_wrap(int state)
{
	if (h5400_btuart_configure)
		h5400_btuart_configure(state);
	return;
}

static struct platform_pxa_serial_funcs h5400_btuart_funcs = {
	.configure = h5400_btuart_configure_wrap,
};

static void h5400_hwuart_configure_wrap(int state)
{
	if (h5400_hwuart_configure)
		h5400_hwuart_configure(state);
	return;
}

static struct platform_pxa_serial_funcs h5400_hwuart_funcs = {
	.configure = h5400_hwuart_configure_wrap,
};

/****************************************************************************
 * ADC (light sensor)
 ****************************************************************************/

static void h5000_light_sensor_set_power(int pin_id, int on)
{
	if (pin_id != SAMCOP_ADC_PIN_AIN0)
		return;

	SET_H5400_GPIO(POWER_LIGHT_SENSOR_N, !on);
	if (on) msleep(20);
	return;
}

static struct adc_classdev h5000_acdevs[] = {
	{
		.name = "h5000_light_sensor",
		.pin_id = SAMCOP_ADC_PIN_AIN0,
	},
};

/****************************************************************************/

static __inline__ void 
fix_msc (void)
{
	/* fix CS0 for h5400 flash. */
	/* fix CS1 for MediaQ chip.  select 16-bit bus and vlio.  */
	/* fix CS5 for SAMCOP.  */
	MSC0 = 0x129c24f2;
	(void)MSC0;
	MSC1 = 0x7ff424fa;
	(void)MSC1;
	MSC2 = 0x7ff47ff4;
	(void)MSC2;

	MDREFR |= 0x02080000;
}

/****************************************************************************
 * Suspend/resume low level ops
 ****************************************************************************/

static void h5000_resume(void)
{
	pr_debug("%s\n", __FUNCTION__);
	/* bootldr will have screwed up MSCs, so set them right again */
	fix_msc();
	return;
}

static struct pxa_ll_pm_ops h5000_ll_pm_ops = {
	.resume = h5000_resume,
};

/* code below still here, for the reference */

#if 0
static int h5400_pm_callback( int req )
{
	int result = 0;
	static int gpio_0, gpio_1, gpio_2;	/* PXA GPIOs */
	
	printk("%s: %d\n", __FUNCTION__, req);

	switch (req) {
	case PM_RESUME:
		/* bootldr will have screwed up MSCs, so set them right again */
		fix_msc ();

#if 0
		H5400_ASIC_GPIO_GPA_DAT = gpio_a;
		H5400_ASIC_GPIO_GPB_DAT = gpio_b;
#endif

		GPSR0 = gpio_0;
		GPCR0 = ~gpio_0;
		GPSR1 = gpio_1;
		GPCR1 = ~gpio_1;
		GPSR2 = gpio_2;
		GPCR2 = ~gpio_2;

#ifdef DeadCode
		if ( ipaq_model_ops.pm_callback_aux )
			result = ipaq_model_ops.pm_callback_aux(req);
#endif
		break;

	case PM_SUSPEND:
#ifdef DeadCode
		if ( ipaq_model_ops.pm_callback_aux &&
		     ((result = ipaq_model_ops.pm_callback_aux(req)) != 0))
			return result;
#endif

#if 0		
		gpio_a = H5400_ASIC_GPIO_GPA_DAT;
		gpio_b = H5400_ASIC_GPIO_GPB_DAT;
#endif

		gpio_0 = GPLR0;
		gpio_1 = GPLR1;
		gpio_2 = GPLR2;
		break;

	default:
		printk("%s: unrecognized PM callback\n", __FUNCTION__);
		break;
	}
	return result;
}
#endif

/*
 * LEDs
 */

DEFINE_LED_TRIGGER_SHARED_GLOBAL(h5400_radio_trig);
EXPORT_SYMBOL(h5400_radio_trig);

static struct samcop_led h5400_leds[] = {
	{
		.led_cdev = {
			.name = "h5400:red-right",
			.default_trigger = "ds2760-battery.0-charging",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,
	},
	{
		.led_cdev = {
			.name = "h5400:green-right",
			.default_trigger = "ds2760-battery.0-full",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
	},
	{
		.led_cdev = {
			.name = "h5400:red-left",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 2,
	},
	{
		.led_cdev = {
			.name = "h5400:green-left",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 3,
	},
	{
		.led_cdev = {
			.name = "h5400:blue",
			.default_trigger = "h5400-radio",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 4,
	},
};

static struct samcop_leds_machinfo h5400_leds_info = {
	.leds = h5400_leds,
	.num_leds = 5,
};

static struct platform_device samcop_leds_pdev = {
	.name = "samcop-leds",
	.id = -1,
	.dev = {
		.platform_data = &h5400_leds_info,
	},
};

/*
 * Power
 */

static int h5000_is_ac_online(void)
{
	return !!(samcop_get_gpio_a(&h5400_samcop.dev) &
	         SAMCOP_GPIO_GPA_ADP_IN_STATUS);
}

static int h5000_is_usb_online(void)
{
	return !!(samcop_get_gpio_a(&h5400_samcop.dev) &
	         SAMCOP_GPIO_GPA_USB_DETECT);
}

static void h5000_set_charge(int flags)
{
	SET_H5400_GPIO(CHG_EN, !!flags);
	SET_H5400_GPIO(EXT_CHG_RATE, !!(flags & PDA_POWER_CHARGE_AC));
	SET_H5400_GPIO(USB_CHG_RATE, !!(flags & PDA_POWER_CHARGE_USB));
}

static char *h5000_supplicants[] = {
	"ds2760-battery.0"
};

static struct pda_power_pdata h5000_power_pdata = {
	.is_ac_online = h5000_is_ac_online,
	.is_usb_online = h5000_is_usb_online,
	.set_charge = h5000_set_charge,
	.supplied_to = h5000_supplicants,
	.num_supplicants = ARRAY_SIZE(h5000_supplicants),
};

static struct resource h5000_power_resourses[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
	},
	[1] = {
		.name = "usb",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
	},
};

static struct platform_device h5000_power_pdev = {
	.name = "pda-power",
	.id = -1,
	.resource = h5000_power_resourses,
	.num_resources = ARRAY_SIZE(h5000_power_resourses),
	.dev = {
		.platform_data = &h5000_power_pdata,
	},
};

static void h5000_fixup_power_irqs(void)
{
	unsigned int ac_irq, usb_irq;

	if (((read_cpuid(CPUID_ID) >> 4) & 0xfff) == 0x2d0) {
		/* h51xx | h55xx */
		ac_irq = H5000_SAMCOP_IRQ_BASE + _IRQ_SAMCOP_WAKEUP1;
		usb_irq = H5000_SAMCOP_IRQ_BASE + _IRQ_SAMCOP_WAKEUP2;
	}
	else {
		/* h54xx */
		ac_irq = H5000_SAMCOP_IRQ_BASE + _IRQ_SAMCOP_WAKEUP2;
		usb_irq = H5000_SAMCOP_IRQ_BASE + _IRQ_SAMCOP_WAKEUP3;
	}

	h5000_power_pdev.resource[0].start = ac_irq;
	h5000_power_pdev.resource[0].end = ac_irq;
	h5000_power_pdev.resource[1].start = usb_irq;
	h5000_power_pdev.resource[1].end = usb_irq;
}


/***********************************************************************************/
/*      Flash                                                                      */
/***********************************************************************************/

static struct mtd_partition h5000_flash0_partitions[] = {
	{
		.name = "bootldr",
		.size = 0x00040000,
		.offset = 0,
		.mask_flags = MTD_WRITEABLE,
	},
	{
		.name = "root",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_APPEND,
	},
};

static struct mtd_partition h5000_flash1_partitions[] = {
	{
		.name = "second root",
		.size = SZ_16M - 0x00040000,
		.offset = 0,
	},
	{
		.name = "asset",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_APPEND,
		.mask_flags = MTD_WRITEABLE,
	},
};

static struct physmap_flash_data h5000_flash_data[] = {
	{
		.width = 4,
		.parts = h5000_flash0_partitions,
		.nr_parts = ARRAY_SIZE(h5000_flash0_partitions),
	},
	{
		.width = 4,
		.parts = h5000_flash1_partitions,
		.nr_parts = ARRAY_SIZE(h5000_flash1_partitions),
	},
};

static struct resource h5000_flash_resources[] = {
	{
		.start = PXA_CS0_PHYS,
		.end = PXA_CS0_PHYS + SZ_32M - 1,
		.flags = IORESOURCE_MEM | IORESOURCE_MEM_32BIT,
	},
	{
		.start = PXA_CS0_PHYS + SZ_32M,
		.end = PXA_CS0_PHYS + SZ_32M + SZ_16M - 1,
		.flags = IORESOURCE_MEM | IORESOURCE_MEM_32BIT,
	},
};

static struct platform_device h5000_flash[] = {
	{
		.name = "physmap-flash",
		.id = 0,
		.resource = &h5000_flash_resources[0],
		.num_resources = 1,
		.dev = {
			.platform_data = &h5000_flash_data[0],
		},
	},
	{
		.name = "physmap-flash",
		.id = 1,
		.resource = &h5000_flash_resources[1],
		.num_resources = 1,
		.dev = {
			.platform_data = &h5000_flash_data[1],
		},
	},
};

/***********************************************************************************/
/*      Initialisation                                                             */
/***********************************************************************************/

static short h5400_gpio_modes[] __initdata = {
	GPIO_NR_H5400_POWER_BUTTON | GPIO_IN,			/* GPIO0 */
	GPIO_NR_H5400_RESET_BUTTON_N | GPIO_IN,			/* GPIO1 */
	GPIO_NR_H5400_OPT_INT | GPIO_IN,			/* GPIO2 */
	GPIO_NR_H5400_BACKUP_POWER | GPIO_IN,			/* GPIO3 */	/* XXX should be an output? */
	GPIO_NR_H5400_ACTION_BUTTON | GPIO_IN,			/* GPIO4 */
	GPIO_NR_H5400_COM_DCD_SOMETHING | GPIO_IN,		/* GPIO5 */
	6 | GPIO_IN,						/* GPIO6 NC */
	GPIO_NR_H5400_RESET_BUTTON_AGAIN_N | GPIO_IN,		/* GPIO7 */
	8 | GPIO_OUT,						/* GPIO8 NC */
	GPIO_NR_H5400_RSO_N | GPIO_IN,				/* GPIO9 BATT_FAULT */
	GPIO_NR_H5400_ASIC_INT_N | GPIO_IN,			/* GPIO10 */
	GPIO_NR_H5400_BT_ENV_0 | GPIO_OUT,			/* GPIO11 */
	GPIO12_32KHz_MD | GPIO_OUT,				/* GPIO12 NC */
	GPIO_NR_H5400_BT_ENV_1 | GPIO_OUT,			/* GPIO13 */
	GPIO_NR_H5400_BT_WU | GPIO_IN,				/* GPIO14 */
	GPIO15_nCS_1_MD,					/* GPIO15 */
	16 | GPIO_OUT,						/* GPIO16 NC */
	17 | GPIO_OUT,						/* GPIO17 NC */

	22 | GPIO_OUT,						/* GPIO22 NC */
	GPIO23_SCLK_MD,
	GPIO_NR_H5400_OPT_SPI_CS_N | GPIO_OUT,
	GPIO25_STXD_MD,
	GPIO26_SRXD_MD,
	27 | GPIO_OUT,						/* GPIO27 NC */

	GPIO34_FFRXD_MD,
	GPIO35_FFCTS_MD,
	GPIO36_FFDCD_MD,
	GPIO37_FFDSR_MD,
	GPIO38_FFRI_MD,
	GPIO39_FFTXD_MD,
	GPIO40_FFDTR_MD,
	GPIO41_FFRTS_MD,
	GPIO42_BTRXD_MD,
	GPIO43_BTTXD_MD,
	GPIO44_BTCTS_MD,
	GPIO45_BTRTS_MD,

	GPIO_NR_H5400_IRDA_SD | GPIO_OUT,			/* GPIO58 */
	59 | GPIO_OUT,						/* GPIO59 XXX docs say "usb charge on" input */
	GPIO_NR_H5400_POWER_SD_N | GPIO_OUT,			/* GPIO60 XXX not really active low? */
	GPIO_NR_H5400_POWER_RS232_N | GPIO_OUT | GPIO_DFLT_HIGH,
	GPIO_NR_H5400_POWER_ACCEL_N | GPIO_OUT | GPIO_DFLT_HIGH,
	63 | GPIO_OUT,						/* GPIO63 NC */
	GPIO_NR_H5400_OPT_NVRAM | GPIO_OUT ,
	GPIO_NR_H5400_CHG_EN | GPIO_OUT ,
	GPIO_NR_H5400_USB_PULLUP | GPIO_OUT ,
	GPIO_NR_H5400_BT_2V8_N | GPIO_OUT | GPIO_DFLT_HIGH,
	GPIO_NR_H5400_EXT_CHG_RATE | GPIO_OUT ,
	69 | GPIO_OUT,						/* GPIO69 NC */
	GPIO_NR_H5400_CIR_RESET | GPIO_OUT ,
	GPIO_NR_H5400_POWER_LIGHT_SENSOR_N | GPIO_OUT | GPIO_DFLT_HIGH,
	GPIO_NR_H5400_BT_M_RESET | GPIO_OUT ,
	GPIO_NR_H5400_STD_CHG_RATE | GPIO_OUT ,
	GPIO_NR_H5400_SD_WP_N | GPIO_IN,			/* GPIO74 XXX docs say output */
	GPIO_NR_H5400_MOTOR_ON_N | GPIO_OUT | GPIO_DFLT_HIGH,
	GPIO_NR_H5400_HEADPHONE_DETECT | GPIO_IN ,
	GPIO_NR_H5400_USB_CHG_RATE | GPIO_OUT ,
	GPIO78_nCS_2_MD,
	GPIO79_nCS_3_MD,
	GPIO80_nCS_4_MD
};

static void __init h5400_map_io(void)
{
	int i;
	int cpuid;

	pxa_map_io ();

	/* Configure power management stuff. */
	PWER = PWER_GPIO0 | PWER_RTC;
	PFER = PWER_GPIO0 | PWER_RTC;
	PRER = 0;
	PCFR = PCFR_OPDE;
	CKEN = CKEN6_FFUART;

#if 0
	h5400_asic_write_register (H5400_ASIC_CPM_ClockControl, H5400_ASIC_CPM_CLKCON_LED_CLKEN);
	h5400_asic_write_register (H5400_ASIC_LED_LEDPS, 0xf42400);		/* 4Hz */

	H5400_ASIC_SET_BIT (H5400_ASIC_CPM_ClockControl, H5400_ASIC_CPM_CLKCON_GPIO_CLKEN);
	H5400_ASIC_SET_BIT (H5400_ASIC_GPIO_GPA_CON2, 0x16);
#endif

	for (i = 0; i < ARRAY_SIZE(h5400_gpio_modes); i++) {
		int mode = h5400_gpio_modes[i];
		pxa_gpio_mode(mode);
	}

#if 0
	/* Does not currently work */
	/* Add wakeup on AC plug/unplug */
	PWER  |= PWER_GPIO12;
#endif

	fix_msc ();

	/* Set sleep states for PXA GPIOs */
	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

	/* hook up btuart & hwuart and power bluetooth */
	/* The h54xx has pxa-250, so it gets btuart */
	/* and h51xx and h55xx have pxa-255, so they get hwuart */
	cpuid = read_cpuid(CPUID_ID);
	if (((cpuid >> 4) & 0xfff) == 0x2d0) {
		pxa_set_hwuart_info(&h5400_hwuart_funcs);
	} else {
		pxa_set_btuart_info(&h5400_btuart_funcs);
	}

	pxa_pm_set_ll_ops(&h5000_ll_pm_ops);
}

/* ------------------- */

static int 
h5400_udc_is_connected (void)
{
	return 1;
}

static void 
h5400_udc_command (int cmd) 
{
	switch (cmd)
	{
	case PXA2XX_UDC_CMD_DISCONNECT:
		GPCR(GPIO_NR_H5400_USB_PULLUP) |= GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		GPDR(GPIO_NR_H5400_USB_PULLUP) &= ~GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		break;
	case PXA2XX_UDC_CMD_CONNECT:
		GPDR(GPIO_NR_H5400_USB_PULLUP) |= GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		GPSR(GPIO_NR_H5400_USB_PULLUP) |= GPIO_bit(GPIO_NR_H5400_USB_PULLUP);
		break;
	default:
		printk("_udc_control: unknown command!\n");
		break;
	}
}

static struct pxa2xx_udc_mach_info h5400_udc_mach_info = {
	.udc_is_connected = h5400_udc_is_connected,
	.udc_command      = h5400_udc_command,
};

/* ------------------- */

static struct platform_device *samcop_child_devs[] = {
	&h5000_power_pdev,
	&samcop_leds_pdev,
};

static struct resource samcop_resources[] = {
	[0] = {
		.start	= H5400_SAMCOP_BASE,
		.end	= H5400_SAMCOP_BASE + 0x00ffffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO_H5400_ASIC_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct samcop_platform_data samcop_platform_data = {
	.irq_base = H5000_SAMCOP_IRQ_BASE,
	.clocksleep = SAMCOP_CPM_CLKSLEEP_UCLK_ON,
	.pllcontrol = 0x60002,		/* value from wince via bootblaster */
	.samcop_adc_pdata = {
		.delay = 500,
		.acdevs = h5000_acdevs,
		.num_acdevs = ARRAY_SIZE(h5000_acdevs),
		.set_power = h5000_light_sensor_set_power,
	},
	.gpio_pdata = {
		.gpacon1 = 0xa2aaaaaa,
		.gpacon2 = 0x5a,
		.gpadat = 0x0,
		.gpaup = 0x0,
		.gpioint = { 0x66666666, 0x60006, 0x0, },
		.gpioflt = { 0x3fff0000, 0x3fff3fff, 0x3fff, 0, 0, 0x3fff3fff, 0, },
		.gpioenint = { 0x3f, 0x7f, },
	},
	.child_devs = samcop_child_devs,
	.num_child_devs = ARRAY_SIZE(samcop_child_devs),

};

struct platform_device h5400_samcop = {
	.name		= "samcop",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(samcop_resources),
	.resource	= samcop_resources,
	.dev		= {
		.platform_data = &samcop_platform_data,
	},
};
EXPORT_SYMBOL(h5400_samcop);

static struct gpiodev_keys_button h5400_button_table[] = {
	{ KEY_POWER,      { &pxagpio_device.dev, GPIO_NR_H5400_POWER_BUTTON }, 1, "Power button" },
	{ _KEY_RECORD,    { &h5400_samcop.dev, SAMCOP_GPA_RECORD },	1, "Record button" },
	{ _KEY_CALENDAR,  { &h5400_samcop.dev, SAMCOP_GPA_APPBTN1 },	1, "Calendar button" },
	{ _KEY_CONTACTS,  { &h5400_samcop.dev, SAMCOP_GPA_APPBTN2 },	1, "Contacts button" },
	{ _KEY_MAIL,      { &h5400_samcop.dev, SAMCOP_GPA_APPBTN3 },	1, "Mail button" },
	{ _KEY_HOMEPAGE,  { &h5400_samcop.dev, SAMCOP_GPA_APPBTN4 },	1, "Homepage button" },
	{ KEY_VOLUMEUP,	  { &h5400_samcop.dev, SAMCOP_GPA_TOGGLEUP },		1, "Volume up" },
	{ KEY_VOLUMEDOWN, { &h5400_samcop.dev, SAMCOP_GPA_TOGGLEDOWN },		1, "Volume down" },
};


static struct gpiodev_keys_platform_data h5400_pxa_keys_data = {
	.buttons = h5400_button_table,
	.nbuttons = ARRAY_SIZE(h5400_button_table),
};

static struct platform_device h5400_pxa_keys = {
	.name = "gpiodev-keys",
	.dev = {
		.platform_data = &h5400_pxa_keys_data,
	},
};

static struct gpiodev_diagonal_button h5400_joypad_table[] = {
	{ { &h5400_samcop.dev, SAMCOP_GPA_JOYSTICK1 }, 1, GPIODEV_DIAG_LEFT | GPIODEV_DIAG_UP, "Joypad left-up" },
	{ { &h5400_samcop.dev, SAMCOP_GPA_JOYSTICK2 }, 1, GPIODEV_DIAG_RIGHT | GPIODEV_DIAG_UP, "Joypad right-up" },
	{ { &h5400_samcop.dev, SAMCOP_GPA_JOYSTICK3 }, 1, GPIODEV_DIAG_RIGHT | GPIODEV_DIAG_DOWN, "Joypad right-down" },
	{ { &h5400_samcop.dev, SAMCOP_GPA_JOYSTICK4 }, 1, GPIODEV_DIAG_LEFT | GPIODEV_DIAG_DOWN, "Joypad left-down" },
	{ { &h5400_samcop.dev, SAMCOP_GPA_JOYSTICK5 }, 1, GPIODEV_DIAG_ACTION, "Joypad action" },
};

static struct gpiodev_diagonal_platform_data h5400_pxa_joypad_data = {
	.buttons = h5400_joypad_table,
	.nbuttons = ARRAY_SIZE(h5400_joypad_table),
	.dir_disables_enter = 1,
};

static struct platform_device h5400_pxa_joypad = {
	.name = "gpiodev-diagonal",
	.dev = {
		.platform_data = &h5400_pxa_joypad_data,
	},
};


static void __init
h5400_init (void)
{
	led_trigger_register_shared("h5400-radio", &h5400_radio_trig);
	h5000_fixup_power_irqs ();
	platform_device_register (&h5000_flash[0]);
	platform_device_register (&h5000_flash[1]);
	platform_device_register (&h5400_samcop);
	platform_device_register (&h5400_pxa_keys);
	platform_device_register (&h5400_pxa_joypad);
	pxa_set_udc_info (&h5400_udc_mach_info);
	pxa_set_dma_needs_bounce (h5400_dma_needs_bounce);
}

MACHINE_START(H5400, "HP iPAQ H5400")
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
        .map_io		= h5400_map_io,
        .init_irq	= pxa_init_irq,
        .timer = &pxa_timer,
        .init_machine = h5400_init,
MACHINE_END
