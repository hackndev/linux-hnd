/*
 *
 * Hardware definitions for the HTC Magician
 *
 * Copyright 2006 Philipp Zabel
 *
 * Based on hx4700.c
 * Copyright 2005 SDG Systems, LLC
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/corgi_bl.h>
#include <linux/ads7846.h>
#include <linux/spi/ads7846.h>
#include <linux/touchscreen-adc.h>
#include <linux/gpio_keys.h>
#include <linux/pda_power.h>
#include <linux/mfd/htc-egpio.h>
#include <linux/mfd/pasic3.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>

#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/magician.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa2xx_spi.h>
#include <asm/arch/mmc.h>
#include <asm/arch/udc.h>
#include <asm/arch/audio.h>
#include <asm/arch/irda.h>
#include <asm/arch/ohci.h>

#include "../generic.h"

/*
 * IRDA
 */

static void magician_irda_transceiver_mode(struct device *dev, int mode)
{
	unsigned long flags;

	local_irq_save(flags);
	gpio_set_value(GPIO83_MAGICIAN_nIR_EN, mode & IR_OFF);
	local_irq_restore(flags);
}

static struct pxaficp_platform_data magician_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = magician_irda_transceiver_mode,
};

/*
 * Phone
 */

struct magician_phone_funcs {
        void (*configure) (int state);
        void (*suspend) (struct platform_device *dev, pm_message_t state);
        void (*resume) (struct platform_device *dev);
};

static struct magician_phone_funcs phone_funcs;

static void magician_phone_configure (int state)
{
	if (phone_funcs.configure)
		phone_funcs.configure (state);
}

static int magician_phone_suspend (struct platform_device *dev, pm_message_t state)
{
	if (phone_funcs.suspend)
		phone_funcs.suspend (dev, state);
	return 0;
}

static int magician_phone_resume (struct platform_device *dev)
{
	if (phone_funcs.resume)
		phone_funcs.resume (dev);
	return 0;
}

static struct platform_pxa_serial_funcs magician_pxa_phone_funcs = {
	.configure = magician_phone_configure,
	.suspend   = magician_phone_suspend,
	.resume    = magician_phone_resume,
};

static struct resource magician_phone_resources[] = {
	[0] = {
		.start	= gpio_to_irq(GPIO10_MAGICIAN_GSM_IRQ),
		.end	= gpio_to_irq(GPIO10_MAGICIAN_GSM_IRQ),
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= gpio_to_irq(GPIO108_MAGICIAN_GSM_READY),
		.end	= gpio_to_irq(GPIO108_MAGICIAN_GSM_READY),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device magician_phone = {
	.name = "magician-phone",
	.dev  = {
		.platform_data = &phone_funcs,
		},
	.id   = -1,
	.num_resources	= ARRAY_SIZE(magician_phone_resources),
	.resource	= magician_phone_resources,
};

/*
 * Initialization code
 */

static void __init magician_map_io(void)
{
	pxa_map_io();
}

static void __init magician_init_irq(void)
{
	pxa_init_irq();

	/* we can't have ds1wm set the irq type because magician needs IAS=0 but rising edge */
	set_irq_type(gpio_to_irq(GPIO107_MAGICIAN_DS1WM_IRQ), IRQ_TYPE_EDGE_RISING);
}

/*
 * Power Management
 */

struct platform_device magician_pm = {
	.name = "magician-pm",
	.id   = -1,
	.dev  = {
		.platform_data = NULL,
	},
};

/*
 * HTC EGPIO on the magician (Xilinx CPLD)
 *
 * 7 32-bit aligned 8-bit registers
 * 3x 8-bit output, 1x 4-bit irq, 3x 8-bit input
 *
 */

static struct resource egpio_resources[] = {
	[0] = {
		.start	= PXA_CS3_PHYS,
		.end	= PXA_CS3_PHYS + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gpio_to_irq(GPIO13_MAGICIAN_CPLD_IRQ),
		.end	= gpio_to_irq(GPIO13_MAGICIAN_CPLD_IRQ),
		.flags	= IORESOURCE_IRQ,
	},
};

struct htc_egpio_pinInfo egpio_pins[] = {
	/* Output pins that default on */
	{
		.pin_nr         = EGPIO_MAGICIAN_GSM_RESET - MAGICIAN_EGPIO_BASE,
		.type           = HTC_EGPIO_TYPE_OUTPUT,
		.output_initial = 1,
	},
	{
		.pin_nr         = EGPIO_MAGICIAN_IN_SEL1 - MAGICIAN_EGPIO_BASE,
		.type           = HTC_EGPIO_TYPE_OUTPUT,
		.output_initial = 1,
	},
	/* Input pins with associated IRQ */
	{
		.pin_nr         = EGPIO_MAGICIAN_EP_INSERT - MAGICIAN_EGPIO_BASE,
		.type           = HTC_EGPIO_TYPE_INPUT,
		.input_irq      = 1 /* IRQ_MAGICIAN_EP_IRQ - IRQ_BOARD_START */,
	},
};

struct htc_egpio_platform_data egpio_data = {
	.bus_shift   = 1,			/* 32bit alignment instead of 16bit */
	.irq_base    = IRQ_BOARD_START,		/* 16 IRQs, we only have 4 though */
	.gpio_base   = GPIO_BASE_INCREMENT,
	.nrRegs      = 7,
	.ackRegister = 3,
	.pins        = egpio_pins,
	.nr_pins     = ARRAY_SIZE(egpio_pins),
};

struct platform_device egpio = {
	.name = "htc-egpio",
	.id   = -1,
	.dev  = {
		.platform_data = &egpio_data,
	},
	.resource      = egpio_resources,
	.num_resources = ARRAY_SIZE(egpio_resources),
};
EXPORT_SYMBOL(egpio); /* needed for magician_pm only */

/*
 * USB Device Controller
 */

static void magician_udc_command(int cmd)
{
	printk ("magician_udc_command(%d)\n", cmd);
	switch(cmd)	{
	case PXA2XX_UDC_CMD_CONNECT:
		UP2OCR = UP2OCR_HXOE | UP2OCR_DMPUE | UP2OCR_DMPUBE;
		gpio_set_value(GPIO27_MAGICIAN_USBC_PUEN, 1);
		break;
	case PXA2XX_UDC_CMD_DISCONNECT:
		UP2OCR = UP2OCR_HXOE | UP2OCR_DMPUE | UP2OCR_DMPUBE;
		gpio_set_value(GPIO27_MAGICIAN_USBC_PUEN, 0);
		break;
	}
}

static struct pxa2xx_udc_mach_info magician_udc_mach_info __initdata = {
/*	.udc_is_connected = is not used by pxa27x_udc.c at all */
	.udc_command      = magician_udc_command,
};

/*
 * GPIO Keys
 */

static struct gpio_keys_button magician_button_table[] = {
	{KEY_POWER,      GPIO0_MAGICIAN_KEY_POWER,         0, "Power button"},
	{KEY_ESC,        GPIO37_MAGICIAN_KEY_PHONE_HANGUP, 0, "Phone hangup button"},
	{_KEY_CONTACTS,  GPIO38_MAGICIAN_KEY_CONTACTS,     0, "Contacts button"},
	{_KEY_CALENDAR,  GPIO90_MAGICIAN_KEY_CALENDAR,     0, "Calendar button"},
	{KEY_CAMERA,     GPIO91_MAGICIAN_KEY_CAMERA,       0, "Camera button"},
	{KEY_UP,         GPIO93_MAGICIAN_KEY_UP,           0, "Up button"},
	{KEY_DOWN,       GPIO94_MAGICIAN_KEY_DOWN,         0, "Down button"},
	{KEY_LEFT,       GPIO95_MAGICIAN_KEY_LEFT,         0, "Left button"},
	{KEY_RIGHT,      GPIO96_MAGICIAN_KEY_RIGHT,        0, "Right button"},
	{KEY_KPENTER,    GPIO97_MAGICIAN_KEY_ENTER,        0, "Action button"},
	{KEY_RECORD,     GPIO98_MAGICIAN_KEY_RECORD,       0, "Record button"},
	{KEY_VOLUMEUP,   GPIO100_MAGICIAN_KEY_VOL_UP,      0, "Volume slider (up)"},
	{KEY_VOLUMEDOWN, GPIO101_MAGICIAN_KEY_VOL_DOWN,    0, "Volume slider (down)"},
	{KEY_PHONE,      GPIO102_MAGICIAN_KEY_PHONE_LIFT,  0, "Phone lift button"},
	{KEY_PLAY,       GPIO99_MAGICIAN_HEADPHONE_IN,     0, "Headset button"},

	{SW_HEADPHONE_INSERT, EGPIO_MAGICIAN_EP_INSERT,    0, "Headphone switch", EV_SW},
};

static struct gpio_keys_platform_data magician_gpio_keys_data = {
	.buttons  = magician_button_table,
	.nbuttons = ARRAY_SIZE(magician_button_table),
};

static struct platform_device magician_gpio_keys = {
	.name = "gpio-keys",
	.dev  = {
		.platform_data = &magician_gpio_keys_data,
		},
	.id   = -1,
};

/*
 * LCD
 */

static struct platform_device magician_lcd = {
	.name = "magician-lcd",
	.dev  = {
		.platform_data = NULL,
	},
	.id   = -1,
};

/*
 * Backlight
 */

static void magician_set_bl_intensity(int intensity)
{
	if (intensity) {
		PWM_CTRL0 = 1;
		PWM_PERVAL0 = 0xc8;
		if (intensity > 0xc7) {
			PWM_PWDUTY0 = intensity - 0x48;
			gpio_set_value(EGPIO_MAGICIAN_BL_POWER2, 1);
		} else {
			PWM_PWDUTY0 = intensity;
			gpio_set_value(EGPIO_MAGICIAN_BL_POWER2, 0);
		}
		gpio_set_value(EGPIO_MAGICIAN_BL_POWER, 1);
		pxa_set_cken(CKEN0_PWM0, 1);
	} else {
		gpio_set_value(EGPIO_MAGICIAN_BL_POWER, 0);
		pxa_set_cken(CKEN0_PWM0, 0);
	}
}

static struct corgibl_machinfo magician_bl_machinfo = {
	.default_intensity = 0x64,
	.limit_mask        = 0x0b,
	.max_intensity     = 0xc7+0x48,
	.set_bl_intensity  = magician_set_bl_intensity,
};

static struct platform_device magician_bl = {
	.name = "corgi-bl",
	.dev  = {
		.platform_data = &magician_bl_machinfo,
	},
	.id   = -1,
};

/*
 * External power
 */

static int magician_is_ac_online(void)
{
	return gpio_get_value(EGPIO_MAGICIAN_CABLE_STATE_AC);
}

static int magician_is_usb_online(void)
{
	return gpio_get_value(EGPIO_MAGICIAN_CABLE_STATE_USB);
}

static void magician_set_charge(int flags)
{
	gpio_set_value(GPIO30_MAGICIAN_nCHARGE_EN, !flags);
	gpio_set_value(EGPIO_MAGICIAN_CHARGE_EN, flags);
}

static char *magician_supplicants[] = {
	"ds2760-battery.0", "backup-battery"
};

static struct pda_power_pdata magician_power_pdata = {
	.is_ac_online = magician_is_ac_online,
	.is_usb_online = magician_is_usb_online,
	.set_charge = magician_set_charge,
	.supplied_to = magician_supplicants,
	.num_supplicants = ARRAY_SIZE(magician_supplicants),
};

static struct resource magician_power_resources[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ,
		.start = IRQ_MAGICIAN_AC,
		.end = IRQ_MAGICIAN_AC,
	},
	[1] = {
		.name = "usb",
		.flags = IORESOURCE_IRQ,
		.start = IRQ_MAGICIAN_AC,
		.end = IRQ_MAGICIAN_AC,
	},
};

static struct platform_device magician_power = {
	.name = "pda-power",
	.id = -1,
	.resource = magician_power_resources,
	.num_resources = ARRAY_SIZE(magician_power_resources),
	.dev = {
		.platform_data = &magician_power_pdata,
	},
};

/*
 * Touchscreen, the TSC2046 chipset is pin-compatible to ADS7846
 */

/*
 * Option 1: common ts-adc driver, readout problems
 */
static struct ads7846_ssp_platform_data ads7846_ssp_params = {
	.port = 2, /* SSP2 */
	.pd_bits = 1,
};

static struct platform_device ads7846_ssp = {
	.name = "ads7846-ssp",
	.id = -1,
	.dev = {
		.platform_data = &ads7846_ssp_params,
	},
};

static struct tsadc_platform_data ads7846_ts_params = {
	.pen_gpio = GPIO115_MAGICIAN_nPEN_IRQ,
	.x_pin = "ads7846-ssp:x",
	.y_pin = "ads7846-ssp:y",
	.z1_pin = "ads7846-ssp:z1",
	.z2_pin = "ads7846-ssp:z2",
	.pressure_factor = 100000,
	.min_pressure = 2,
	.max_jitter = 8,
};

static struct resource ads7846_pen_irq = {
	.start = gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ),
	.end = gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ),
	.flags = IORESOURCE_IRQ,
};

static struct platform_device ads7846_ts = {
	.name = "ts-adc",
	.id = -1,
	.resource = &ads7846_pen_irq,
	.num_resources = 1,
	.dev = {
		.platform_data = &ads7846_ts_params,
	},
};

/*
 * Option 2: old driver based on hx4700, mimicking wince behavior, unclean code
 */
static struct platform_device magician_ts = {
	.name = "magician-ts",
	.dev = {
		.platform_data = NULL,
		},
};

#if 0
/*
 * Option 3: Nokia ADS7846 SPI driver, doesn't work
 */
int magician_get_pendown_state(void)
{
	return gpio_get_value(GPIO115_MAGICIAN_nPEN_IRQ) ? 0 : 1;
}

EXPORT_SYMBOL(magician_get_pendown_state);

static struct ads7846_platform_data magician_tsc2046_platform_data __initdata = {
	.x_max = 0xffff,
	.y_max = 0xffff,
	.x_plate_ohms = 180,	/* GUESS */
	.y_plate_ohms = 180,	/* GUESS */
	.vref_delay_usecs = 100,
	.pressure_max = 255,
	.debounce_max = 10,
	.debounce_tol = 3,
	.get_pendown_state = &magician_get_pendown_state,
};

struct pxa2xx_spi_chip tsc2046_chip_info = {
	.tx_threshold = 1,
	.rx_threshold = 2,
	.timeout = 64,
};

static struct spi_board_info magician_spi_board_info[] __initdata = {
	[0] = {
	       .modalias = "ads7846",
	       .bus_num = 2,
	       .max_speed_hz = 1857143,
	       .irq = gpio_to_irq(GPIO115_MAGICIAN_nPEN_IRQ),
	       .platform_data = &magician_tsc2046_platform_data,
	       .controller_data = &tsc2046_chip_info,
	       },
};

static struct resource pxa_spi_ssp2_resources[] = {
	[0] = {
	       .start = __PREG(SSCR0_P(2)),
	       .end = __PREG(SSCR0_P(2)) + 0x2c,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_SSP2,
	       .end = IRQ_SSP2,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct pxa2xx_spi_master pxa_ssp2_master_info = {
	.ssp_type = PXA27x_SSP,
	.clock_enable = CKEN3_SSP2,
//      .num_chipselect = 1,
};

static struct platform_device pxa_spi_ssp2 = {
	.name = "pxa2xx-spi",
	.id = 2,
	.resource = pxa_spi_ssp2_resources,
	.num_resources = ARRAY_SIZE(pxa_spi_ssp2_resources),
	.dev = {
		.platform_data = &pxa_ssp2_master_info,
		},
};
#endif

/*
 * LEDs
 */

static struct pasic3_led pasic3_leds[] = {
	{
		.led = {
			.name            = "magician:red",
			.default_trigger = "ds2760-battery.0-charging", /* and alarm */
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,
		.bit2   = PASIC3_BIT2_LED0, /* 0x08 */
		.mask   = PASIC3_MASK_LED0,
	},
	{
		.led = {
			.name            = "magician:green",
			.default_trigger = "ds2760-battery.0-charging-or-full", /* and GSM radio */
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
		.bit2   = PASIC3_BIT2_LED1, /* 0x10 */
		.mask   = PASIC3_MASK_LED1,
	},
	{
		.led = {
			.name            = "magician:blue",
			.default_trigger = "magician-bluetooth",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 2,
		.bit2   = PASIC3_BIT2_LED2, /* 0x20 */
		.mask   = PASIC3_MASK_LED2,
	},
};

static struct platform_device pasic3;

static struct pasic3_leds_machinfo __devinit pasic3_leds_machinfo = {
	.num_leds = ARRAY_SIZE(pasic3_leds),
	.power_gpio = EGPIO_MAGICIAN_LED_POWER,
	.leds = pasic3_leds,
};

static struct platform_device pasic3_led = {
	.name = "pasic3-led",
	.id   = -1,
	.dev  = {
		.platform_data = &pasic3_leds_machinfo,
	},
};

static struct platform_device magician_led = {
	.name = "magician-led",
	.id   = -1,
};

/*
 * PASIC3 with DS1WM
 */

static struct platform_device *pasic3_devices[] = {
	&pasic3_led,
};

static struct resource pasic3_resources[] = {
	[0] = {
		.start  = PXA_CS2_PHYS,
		.end	= PXA_CS2_PHYS + 0x18,
		.flags  = IORESOURCE_MEM,
	},
	/* No IRQ handler in the PASIC3, DS1WM needs an external IRQ */
	[1] = {
		.start  = gpio_to_irq(GPIO107_MAGICIAN_DS1WM_IRQ),
		.end    = gpio_to_irq(GPIO107_MAGICIAN_DS1WM_IRQ),
		.flags  = IORESOURCE_IRQ,
	}
};

static struct pasic3_platform_data pasic3_platform_data = {
	.bus_shift	= 2,

	.child_devs	= pasic3_devices,
	.num_child_devs	= ARRAY_SIZE(pasic3_devices),
	.clock_rate	= 4000000,
};

static struct platform_device pasic3 = {
	.name		= "pasic3",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pasic3_resources),
	.resource	= pasic3_resources,
	.dev = {
		.platform_data = &pasic3_platform_data,
	},
};

/*
 * MMC/SD
 */

static struct pxamci_platform_data magician_mci_platform_data;

static int magician_mci_init(struct device *dev, irq_handler_t magician_detect_int, void *data)
{
	int err;

	err = request_irq(IRQ_MAGICIAN_SD, magician_detect_int,
			  IRQF_DISABLED | IRQF_SAMPLE_RANDOM,
			  "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "magician_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static void magician_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

        if ((1 << vdd) & p_d->ocr_mask)
		gpio_set_value(EGPIO_MAGICIAN_SD_POWER, 1);
	else
		gpio_set_value(EGPIO_MAGICIAN_SD_POWER, 0);
}

static int magician_mci_get_ro(struct device *dev)
{
	return (!gpio_get_value(EGPIO_MAGICIAN_nSD_READONLY));
}

static void magician_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_MAGICIAN_SD, data);
}

static struct pxamci_platform_data magician_mci_platform_data = {
	.ocr_mask = MMC_VDD_32_33|MMC_VDD_33_34,
	.init     = magician_mci_init,
	.get_ro   = magician_mci_get_ro,
	.setpower = magician_mci_setpower,
	.exit     = magician_mci_exit,
};


/* USB OHCI */

static int magician_ohci_init(struct device *dev)
{
	/* missing GPIO setup here */

	/* no idea what this does, got the values from haret */
	UHCHR = (UHCHR | UHCHR_SSEP2 | UHCHR_PCPL | UHCHR_CGR) &
	    ~(UHCHR_SSEP1 | UHCHR_SSEP3 | UHCHR_SSE);

	return 0;
}

static struct pxaohci_platform_data magician_ohci_platform_data = {
	.port_mode = PMM_PERPORT_MODE,
	.init = magician_ohci_init,
};


/* StrataFlash */

static void magician_set_vpp(struct map_info *map, int vpp)
{
	static int old_vpp = 0;
	if (vpp == old_vpp)
		return;
	if (vpp)
		gpio_set_value(EGPIO_MAGICIAN_FLASH_VPP, 1);
	else
		gpio_set_value(EGPIO_MAGICIAN_FLASH_VPP, 0);
	old_vpp = vpp;
}

#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition wince_partitions[] = {
	{
		name:		"bootloader",
		offset:		0,
		size:		1*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	},
	{
		name:		"osrom",
		size:		143*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"backup_area",
		size:		32*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"extrom",
		size:		76*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"unused",
		size:		2*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"splash",
		size:		1*0x40000,
		offset:		MTDPART_OFS_NXTBLK,
	},
	{
		name:		"asset",
		size:		1*0x40000,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
		offset:		MTDPART_OFS_NXTBLK,
	},
};
#endif

#define PXA_CS_SIZE		0x04000000

static struct resource magician_flash_resource = {
	.start = PXA_CS0_PHYS,
	.end   = PXA_CS0_PHYS + PXA_CS_SIZE - 1,
	.flags = IORESOURCE_MEM,
};

struct physmap_flash_data magician_flash_data = {
	.width = 4,
	.set_vpp = magician_set_vpp,
#ifdef CONFIG_MTD_PARTITIONS
	.nr_parts = ARRAY_SIZE(wince_partitions),
	.parts = wince_partitions,
#endif
};

static struct platform_device magician_flash = {
	.name          = "physmap-flash",
	.id            = -1,
	.num_resources = 1,
	.resource      = &magician_flash_resource,
	.dev = {
		.platform_data = &magician_flash_data,
	},
};


static struct platform_device *devices[] __initdata = {
	&magician_pm,
	&magician_gpio_keys,
	&egpio,			// irqs and gpios
	&magician_lcd,		// needs the cpld to power on/off the lcd
	&magician_bl,
	&magician_power,
	&magician_ts,		// old touchscreen driver based on hx4700
	&ads7846_ssp,
	&ads7846_ts,		// common adc-debounce touchscreen driver
	&magician_phone,
	&magician_flash,
	&pasic3,
	&magician_led,
#if 0
	&pxa_spi_ssp2,
#endif
};

static void __init magician_init(void)
{

	/* BIG TODO */

#if 0				// GPIO/MSC setup from bootldr
	/* 0x1350       0x41664 (resume) */
	GPSR0 = 0x4034c000;	// 0x0030c800 <-- set 11, clear 18, clear 30
	GPSR1 = 0x0002321a;	// GPSR1 = (PGSR1 & 0x01000100) | 0x0002420a <-- clear 36 44 45, set 46 and get 40 and 66 from PGSR1
	GPSR2 = 0x0009c000;	// 0x0009c000
	GPSR3 = 0x01180000;	// 0x00180000 <-- clear gpio24

	// GPCR only here

	GPDR0 = 0xfdfdc80c;	// 0xfdfdc80c
	GPDR1 = 0xff23990b;	// 0xff23ab83
	GPDR2 = 0x02c9ffff;	// 0x02c9ffff
	GPDR3 = 0x01b60780;	// 0x01b60780
	GAFR0_L = 0xa2000000;	// 0xa2000000
	GAFR0_U = 0x490a845a;	// 0x490a854a <-- 2(1->0) 4(0->1)
	GAFR1_L = 0x6998815a;	// 0x6918005a <-- 20(1->0) 23(2->0) 27(2->0)
	GAFR1_U = 0xaaa07958;	// 0xaaa07958
	GAFR2_L = 0xaa0aaaaa;	// 0xaa0aaaaa
	GAFR2_U = 0x010e0f3a;	// 0x010e0f3a
	GAFR3_L = 0x54000000;	// 0x54000000
	GAFR3_U = 0x00001405;	// 0x00001405
	MSC0 = 0x7ff001a0;	// 0x7ff005a2 <--       ( RBUFF,RRR RDN, RDF, RBW,RT ) ( RBUFF,RRR RDN, RDF, RBW,RT )
	MSC1 = 0x7ff01880;	// 0x18801880 <--
	MSC2 = 0x16607ff0;	// 0x16607ff0
#endif

	/* set USBC_PUEN - in wince it is turned off when the plug is pulled. */
	gpio_set_value(GPIO27_MAGICIAN_USBC_PUEN, 1);
		// should this be controlled by the USBC_DET irq from the cpld? (on gpio 13)

	platform_add_devices(devices, ARRAY_SIZE(devices));
#if 0
	spi_register_board_info(magician_spi_board_info,
				ARRAY_SIZE(magician_spi_board_info));
#endif
        pxa_set_mci_info(&magician_mci_platform_data);
	pxa_set_ohci_info(&magician_ohci_platform_data);
	pxa_set_btuart_info(&magician_pxa_phone_funcs);
	pxa_set_udc_info(&magician_udc_mach_info);
	pxa_set_ficp_info(&magician_ficp_platform_data);
}


MACHINE_START(MAGICIAN, "HTC Magician")
	.phys_io = 0x40000000,.io_pg_offst =
	(io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params = 0xa0000100,
	.map_io = magician_map_io,
	.init_irq = magician_init_irq,
	.timer = &pxa_timer,
	.init_machine = magician_init,
MACHINE_END
