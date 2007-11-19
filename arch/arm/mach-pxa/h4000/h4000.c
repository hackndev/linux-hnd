/*
 * Hardware definition for HP iPAQ H4000 Handheld Computer
 *
 * Copyright (c) 2006  Anton Vorontsov <cbou@mail.ru>
 * Copyright (c) 2006-7  Paul Sokolovsky <pmiscml@gmail.com>
 * Copyright 2000-2003 Hewlett-Packard Company.
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
 * History:
 * 2004-??-??  Shawn Anderson    Derived the from aximx3.c aximx5.c e7xx.c 
 *                               h1900.c h2200.c h3900.c h5400.c and friends.
 * 2004-04-01  Eddi De Pieri     Move lcd stuff so it can be used as a module.
 * 2004-04-19  Eddi De Pieri     Moving to new 2.6 standard 
 *                               (platform_device / device_driver structure)
 * 2007-03-24  Paul Sokolovsky   Major cleanup.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpiodev.h>
#include <linux/dpm.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/pxa-regs.h>
#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include "../generic.h"
// Specific devices
#include <asm/arch/irda.h>
#include <linux/ads7846.h>
#include <linux/touchscreen-adc.h>
#include <linux/adc_battery.h>
#include <linux/pda_power.h>
#include <linux/rs232_serial.h>
#include <asm/arch/pxa2xx_udc_gpio.h>
#include <asm/arch/pxafb.h>
#include <linux/mfd/asic3_base.h>
#include <asm/hardware/asic3_leds.h>
#include <asm/arch/h4000.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-init.h>
#include <asm/arch/h4000-asic.h>
#include <linux/corgi_bl.h>
#include <linux/gpio_keys.h>


void h4000_ll_pm_init(void);

DEFINE_LED_TRIGGER_SHARED_GLOBAL(h4000_radio_trig);
EXPORT_LED_TRIGGER_SHARED(h4000_radio_trig);

/* Misc devices */
static struct platform_device h4000_lcd       = { .name = "h4000-lcd", };
static struct platform_device h4300_kbd       = { .name = "h4300-kbd", };
static struct platform_device h4000_pcmcia    = { .name = "h4000-pcmcia", };
static struct platform_device h4000_batt      = { .name = "h4000-batt", };
static struct platform_device h4000_bt        = { .name = "h4000-bt", };
static struct platform_device h4000_irda      = { .name = "h4000-irda", };

/* Backlight */
static void h4000_set_bl_intensity(int intensity)
{
	/* LCD brightness is driven by PWM0.
	 * We'll set the pre-scaler to 8, and the period to 1024, this
	 * means the backlight refresh rate will be 3686400/(8*1024) =
	 * 450 Hz which is quite enough.
	 */
	PWM_CTRL0 = 7;            /* pre-scaler */
	PWM_PWDUTY0 = intensity; /* duty cycle */
	PWM_PERVAL0 = H4000_MAX_INTENSITY;      /* period */

	if (intensity > 0) {
		pxa_set_cken(CKEN0_PWM0, 1);
		asic3_set_gpio_out_b(&h4000_asic3.dev,
			GPIOB_BACKLIGHT_POWER_ON, GPIOB_BACKLIGHT_POWER_ON);
	} else {
		pxa_set_cken(CKEN0_PWM0, 0);
		asic3_set_gpio_out_b(&h4000_asic3.dev,
			GPIOB_BACKLIGHT_POWER_ON, 0);
	}
}

static struct corgibl_machinfo h4000_bl_machinfo = {
        .default_intensity = H4000_MAX_INTENSITY / 4,
        .limit_mask = 0xffff,
        .max_intensity = H4000_MAX_INTENSITY,
        .set_bl_intensity = h4000_set_bl_intensity,
};

static struct platform_device h4000_bl = {
        .name = "corgi-bl",
        .dev = {
    		.platform_data = &h4000_bl_machinfo,
	},
};

/* IrDA */
static void h4000_irda_transceiver_mode(struct device *dev, int mode)
{
	unsigned long flags;

	local_irq_save(flags);

	if (mode & IR_OFF) {
		DPM_DEBUG("h4000_irda: Turning off port\n");
		gpio_set_value(H4000_ASIC3_GPIO_BASE + GPIOD_NR_IR_ON_N, 1);
	} else {
		DPM_DEBUG("h4000_irda: Turning on port\n");
		gpio_set_value(H4000_ASIC3_GPIO_BASE + GPIOD_NR_IR_ON_N, 0);
	}

	local_irq_restore(flags);
}

static struct pxaficp_platform_data h4000_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = h4000_irda_transceiver_mode,
};

/* LEDs */
static struct asic3_led h4000_leds[] = {
	{
		.led_cdev  = {
			.name	         = "h4000:red",
			.default_trigger = "main-battery-charging",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,
	},
	{
		.led_cdev  = {
			.name	         = "h4000:right-green",
			.default_trigger = "main-battery-full",
			.flags = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
	},
	{
		.led_cdev  = {
			.name	         = "h4000:left-green",
			.default_trigger = "h4000-wifi",
		},
		.hw_num = -1,
		.gpio_num = 4,
	},
	{
		.led_cdev  = {
			.name	         = "h4000:blue",
			.default_trigger = "bluetooth",
		},
		.hw_num = -1,
		.gpio_num = 5,
	},
};

static struct asic3_leds_machinfo h4000_leds_machinfo = {
	.num_leds = ARRAY_SIZE(h4000_leds),
	.leds = h4000_leds,
	.asic3_pdev = &h4000_asic3,
};

static struct platform_device h4000_leds_pdev = {
	.name = "asic3-leds",
	.dev = {
		.platform_data = &h4000_leds_machinfo,
	},
};

/* Keys/buttons */
static struct gpio_keys_button gpio_buttons[] = {
	{ KEY_POWER,	GPIO_NR_H4000_POWER_BUTTON_N,	1, "Power button"	},
	{ KEY_ENTER,	GPIO_NR_H4000_ACTION_BUTTON_N,	1, "Action button"	},
	{ KEY_UP,	GPIO_NR_H4000_UP_BUTTON_N,	1, "Up button"		},
	{ KEY_DOWN,	GPIO_NR_H4000_DOWN_BUTTON_N,	1, "Down button"	},
	{ KEY_LEFT,	GPIO_NR_H4000_LEFT_BUTTON_N,	1, "Left button"	},
	{ KEY_RIGHT,	GPIO_NR_H4000_RIGHT_BUTTON_N,	1, "Right button"	},
	{ _KEY_RECORD,    H4000_ASIC3_GPIO_BASE+H4000_RECORD_BTN_IRQ,		1, "Record button" },
	{ _KEY_HOMEPAGE,  H4000_ASIC3_GPIO_BASE+H4000_TASK_BTN_IRQ,		1, "Home button" },
	{ _KEY_MAIL,      H4000_ASIC3_GPIO_BASE+H4000_MAIL_BTN_IRQ,		1, "Mail button" },
	{ _KEY_CONTACTS,  H4000_ASIC3_GPIO_BASE+H4000_CONTACTS_BTN_IRQ, 	1, "Contacts button" },
	{ _KEY_CALENDAR,  H4000_ASIC3_GPIO_BASE+H4000_CALENDAR_BTN_IRQ,	1, "Calendar button" },
};

static struct gpio_keys_platform_data gpio_keys_data = {
        .buttons = gpio_buttons,
        .nbuttons = ARRAY_SIZE(gpio_buttons),
};

static struct platform_device h4000_buttons = {
        .name = "gpio-keys",
        .dev = { .platform_data = &gpio_keys_data, }
};

/* Power */
static int h4000_is_ac_online(void)
{
	return !gpio_get_value(GPIO_NR_H4000_AC_IN_N);
}

static struct pda_power_pdata h4000_power_pdata = {
	.is_ac_online = h4000_is_ac_online,
};

static struct resource h4000_power_resourses[] = {
	[0] = {
		.name = "ac",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
		         IORESOURCE_IRQ_LOWEDGE,
		.start = gpio_to_irq(GPIO_NR_H4000_AC_IN_N),
		.end = gpio_to_irq(GPIO_NR_H4000_AC_IN_N),
	},
};

static struct platform_device h4000_power = {
	.name = "pda-power",
	.id = -1,
	.resource = h4000_power_resourses,
	.num_resources = ARRAY_SIZE(h4000_power_resourses),
	.dev = {
		.platform_data = &h4000_power_pdata,
	},
};

/* Batteries */
static int h4000_is_charging(void)
{
	return GET_H4000_GPIO(CHARGING);
}

static struct battery_adc_platform_data h4000_main_batt_params = {
	.battery_info = {
		.name = "main-battery",
		.voltage_max_design = 4200000,
		.voltage_min_design = 3500000,
		.charge_full_design  = 1000000,
		.use_for_apm = 1,
	},
	.voltage_pin = "ads7846-ssp:vmbat",
	.current_pin = "ads7846-ssp:imbat",
	.temperature_pin = "ads7846-ssp:tmbat",
	/* Coefficient is 1.01471 */
	.voltage_mult = 1015,

	.is_charging = h4000_is_charging,
};

static struct platform_device h4000_main_batt = { 
	.name = "adc-battery",
	.id = 1,
	.dev = {
		.platform_data = &h4000_main_batt_params,
	}
};

static struct battery_adc_platform_data h4000_backup_batt_params = {
	.battery_info = {
		.name = "backup-battery",
 		.voltage_max_design = 4200000,
 		.voltage_min_design = 0,
 		.charge_full_design = 200000,
	},
	.voltage_pin = "ads7846-ssp:vbckbat",
};

static struct platform_device h4000_backup_batt = { 
	.name = "adc-battery",
	.id = 2,
	.dev = {
		.platform_data = &h4000_backup_batt_params,
	}
};

/* UDC device */
struct pxa2xx_udc_gpio_info h4000_udc_info = {
	.detect_gpio = {&pxagpio_device.dev, GPIO_NR_H4000_USB_DETECT_N},
	.detect_gpio_negative = 1,
	.power_ctrl = {
		.power_gpio = {&h4000_asic3.dev, ASIC3_GPIOD_IRQ_BASE + GPIOD_USB_PULLUP},
	},

};

static struct platform_device h4000_udc = { 
	.name = "pxa2xx-udc-gpio",
	.dev = {
		.platform_data = &h4000_udc_info
	}
};

/* ADC device */
static int h4000_ads7846_mux(int custom_pin)
{
        int mux_b = 0, mux_d = 0;

        custom_pin -= AD7846_PIN_CUSTOM_START;
        if (custom_pin & 1)  mux_d |= GPIOD_MUX_SEL0;
        if (custom_pin & 2)  mux_d |= GPIOD_MUX_SEL1;
        if (custom_pin & 4)  mux_b |= GPIOB_MUX_SEL2;
        /* Select sampling source */
	asic3_set_gpio_out_b(&h4000_asic3.dev, GPIOB_MUX_SEL2, mux_b);
	asic3_set_gpio_out_d(&h4000_asic3.dev, GPIOD_MUX_SEL1|GPIOD_MUX_SEL0, mux_d);
	/* Likely not yet fully settled, by seems stable */
	udelay(50);
	return AD7846_PIN_VAUX;
}

static struct adc_classdev h4000_custom_pins[] = {
	{
		.name = "vbckbat",
		.pin_id = AD7846_PIN_CUSTOM_VBACKUP,
	},
	{
		.name = "vmbat",
		.pin_id = AD7846_PIN_CUSTOM_VBAT,
	},
	{
		.name = "imbat",
		.pin_id = AD7846_PIN_CUSTOM_IBAT,
	},
	{
		.name = "tmbat",
		.pin_id = AD7846_PIN_CUSTOM_TBAT,
	},
};
static struct ads7846_ssp_platform_data h4000_ts_ssp_params = {
	.port = 1,
	.pd_bits = 1,
	.freq = 100000,
	.mux_custom = h4000_ads7846_mux,
	.custom_adcs = h4000_custom_pins,
	.num_custom_adcs = ARRAY_SIZE(h4000_custom_pins),
};
static struct platform_device ads7846_ssp     = { 
	.name = "ads7846-ssp", 
	.id = -1,
	.dev = {
		.platform_data = &h4000_ts_ssp_params,
	}
};

/* TS device (virtual on top of ADC) */
static struct tsadc_platform_data h4000_ts_params = {
	.pen_gpio = GPIO_NR_H4000_PEN_IRQ_N,
	.x_pin = "ads7846-ssp:x",
	.y_pin = "ads7846-ssp:y",
	.z1_pin = "ads7846-ssp:z1",
	.z2_pin = "ads7846-ssp:z2",
	.pressure_factor = 100000,
	.min_pressure = 4,
	.max_jitter = 8,
};
static struct resource h4000_pen_irq = {
	.start = IRQ_GPIO(GPIO_NR_H4000_PEN_IRQ_N),
	.end = IRQ_GPIO(GPIO_NR_H4000_PEN_IRQ_N),
	.flags = IORESOURCE_IRQ,
};
static struct platform_device h4000_ts        = { 
	.name = "ts-adc",
	.id = -1,
	.resource = &h4000_pen_irq,
	.num_resources = 1,
	.dev = {
		.platform_data = &h4000_ts_params,
	}
};

/* RS232 device */
static struct rs232_serial_pdata h4000_rs232_data = {
	.detect_gpio = {&pxagpio_device.dev, GPIO_NR_H4000_SERIAL_DETECT},
	.power_ctrl = {	
		.power_gpio = {&h4000_asic3.dev, ASIC3_GPIOA_IRQ_BASE + GPIOA_RS232_ON},
	},
};
static struct platform_device h4000_serial = { 
	.name	= "rs232-serial",
	.dev	= {
		.platform_data = &h4000_rs232_data
	},
};

/* Framebuffer device */
/*
 * LCCR0: 0x003008f9 -- ENB=0x1,  CMS=0x0, SDS=0x0, LDM=0x1,
 *                      SFM=0x1,  IUM=0x1, EFM=0x1, PAS=0x1,
 *                      res=0x0,  DPD=0x0, DIS=0x0, QDM=0x1,
 *                      PDD=0x0,  BM=0x1,  OUM=0x1, res=0x0
 * LCCR1: 0x13070cef -- BLW=0x13, ELW=0x7, HSW=0x3, PPL=0xef
 * LCCR2: 0x0708013f -- BFW=0x7,  EFW=0x8, VSW=0x0, LPP=0x13f
 * LCCR3: 0x04700008 -- res=0x0,  DPC=0x0, BPP=0x4, OEP=0x0, PCP=0x1
 *                      HSP=0x1,  VSP=0x1, API=0x0, ACD=0x0, PCD=0x8
 */

static struct pxafb_mode_info h4000_lcd_modes[] = {
{
        .pixclock     = 171521,     // (160756 > 180849)
        .xres         = 240,        // PPL + 1
        .yres         = 320,        // LPP + 1
        .bpp          = 16,         // BPP (0x4 == 16bits)
        .hsync_len    = 4,          // HSW + 1
        .vsync_len    = 1,          // VSW + 1
        .left_margin  = 20,         // BLW + 1
        .upper_margin = 8,          // BFW + 1
        .right_margin = 8,          // ELW + 1
        .lower_margin = 8,          // EFW + 1
        .sync         = 0,
},
};

static struct pxafb_mach_info sony_acx502bmu = {
        .modes		= h4000_lcd_modes,
        .num_modes	= ARRAY_SIZE(h4000_lcd_modes), 
        
        .lccr0        = LCCR0_Act | LCCR0_Sngl | LCCR0_Color,
        .lccr3        = LCCR3_OutEnH | LCCR3_PixFlEdg | LCCR3_Acb(0),

        //.pxafb_backlight_power = ,
        //.pxafb_lcd_power =       ,
};

/* SoC device */
static struct platform_device *h4000_asic3_devices[] __initdata = {
	&h4000_serial,
	&h4000_lcd,
#ifdef CONFIG_IPAQ_H4000_BACKLIGHT
	&h4000_bl,
#endif
	&h4300_kbd,
	&h4000_buttons,
	&h4000_ts,
	&h4000_udc,
	&h4000_pcmcia,
	&h4000_power,
	&h4000_main_batt,
	&h4000_backup_batt,
	&h4000_batt,
	&h4000_bt,
	&h4000_irda,
	&h4000_leds_pdev,
};

static struct asic3_platform_data h4000_asic3_platform_data = {
	.gpio_a = {
		.dir            = 0xfc7f,
		/* Turn (keep) serial on, in case we have initial console there.
		   h4000_serial.c will verify if anything is actually connected there,
		   and turn it off otherwise. */
		.init           = 1<<GPIOA_RS232_ON,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000,
		.sleep_conf     = 0x000c,
	},
	.gpio_b = {
		.dir            = 0xddbf, /* 0xdfbd for h4300 kbd irq */
		/* 0x1c00, 0x1c05 for h4300 kbd wakeup/power */
		/* LCD and backlight active */
		.init           = GPIOB_LCD_ON | GPIOB_LCD_PCI | GPIOB_BACKLIGHT_POWER_ON,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000,
		.sleep_conf     = 0x000c,
	},
	.gpio_c = {
		.dir            = 0xffff, /* 0xfff7 for h4300 key rxd spi */
		/* 0x4700 */
		/* Flash and LCD active */
		.init           = GPIOC_FLASH_RESET_N | GPIOC_LCD_3V3_ON | GPIOC_LCD_5V_EN | GPIOC_LCD_N3V_EN,
		/* Keep flash active even during sleep/battery fault */
		.sleep_out      = GPIOC_FLASH_RESET_N,
		.batt_fault_out = GPIOC_FLASH_RESET_N,
		.alt_function   = 0x0003, /* 0x003b for h4300 kbd spi */ 
		.sleep_conf     = 0x000c,
	},
	.gpio_d = {
		.dir            = 0xef03, /* 0xef7b for h4300, no buttons here*/
		/* 0x0f02 */
		/* LCD active, IR inactive, MUX_SEL likely random */
		.init           = GPIOD_PCI | GPIOD_MUX_SEL0 | GPIOD_MUX_SEL1 | GPIOD_IR_ON_N 
				  | GPIOD_LCD_RESET_N 
				  /* Be ready to boot via NFS */
				  | GPIOD_USB_ON | 1<<GPIOD_USB_PULLUP, 
		.sleep_out      = GPIOD_IR_ON_N,
		.batt_fault_out = GPIOD_IR_ON_N,
		.alt_function   = 0x0000,
		.sleep_conf     = 0x000c,
	},
	.irq_base = H4000_ASIC3_IRQ_BASE,
	.gpio_base = H4000_ASIC3_GPIO_BASE,

	.child_devs     = h4000_asic3_devices,
	.num_child_devs = ARRAY_SIZE(h4000_asic3_devices),
};

static struct resource h4000_asic3_resources[] = {
        /* GPIO part */
	[0] = {
		.start  = H4000_ASIC3_PHYS,
		.end    = H4000_ASIC3_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO(GPIO_NR_H4000_ASIC3_IRQ),
		.flags  = IORESOURCE_IRQ,
	},
        /* SD part */
	[2] = {
		.start  = H4000_ASIC3_SD_PHYS,
		.end    = H4000_ASIC3_SD_PHYS + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = IRQ_GPIO(GPIO_NR_H4000_SD_IRQ_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device h4000_asic3 = {
	.name           = "asic3",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(h4000_asic3_resources),
	.resource       = h4000_asic3_resources,
	.dev = { .platform_data = &h4000_asic3_platform_data, },
};
EXPORT_SYMBOL(h4000_asic3);



#if 0
// This function is useful for low-level debugging only
void h4000_set_led(int color, int duty_time, int cycle_time)
{
        if (color == H4000_RED_LED) {
                asic3_set_led(asic3, 0, duty_time, cycle_time, 6);
                asic3_set_led(asic3, 1, 0, cycle_time, 6);
        }

        if (color == H4000_GREEN_LED) {
                asic3_set_led(asic3, 1, duty_time, cycle_time, 6);
                asic3_set_led(asic3, 0, 0, cycle_time, 6);
        }

        if (color == H4000_YELLOW_LED) {
                asic3_set_led(asic3, 1, duty_time, cycle_time, 6);
                asic3_set_led(asic3, 0, duty_time, cycle_time, 6);
        }
}
EXPORT_SYMBOL(h4000_set_led);
#endif

static void __init h4000_map_io(void)
{
	pxa_map_io();

	/* Wake up enable. */
	PWER = PWER_GPIO0 | PWER_RTC;
	/* Wake up on falling edge. */
	PFER = PWER_GPIO0 | PWER_RTC;
	/* Wake up on rising edge. */
	PRER = 0;
	/* 3.6864 MHz oscillator power-down enable */
	PCFR = PCFR_OPDE;
}

static void __init h4000_init(void)
{
#if 0
        // Reset ASIC3
	// So far, this is for debugging only
	// 100ms is *too* much for real use
	// (but visible by naked eye).
	SET_H4000_GPIO(ASIC3_RESET_N, 0);
	mdelay(100);
	SET_H4000_GPIO(ASIC3_RESET_N, 1);
	mdelay(10);
#endif

	/* Configure FFUART, for RS232 and serial console */
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO35_FFCTS_MD);
	pxa_gpio_mode(GPIO36_FFDCD_MD);
	pxa_gpio_mode(GPIO37_FFDSR_MD);
	pxa_gpio_mode(GPIO38_FFRI_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	pxa_gpio_mode(GPIO40_FFDTR_MD);
	pxa_gpio_mode(GPIO41_FFRTS_MD);

#ifdef CONFIG_PM
	h4000_ll_pm_init();
#endif
	set_pxa_fb_info(&sony_acx502bmu);
	pxa_set_ficp_info(&h4000_ficp_platform_data);
	platform_device_register(&ads7846_ssp);
	platform_device_register(&h4000_asic3);
	printk("machine_arch_type=%d\n", machine_arch_type);

	led_trigger_register_shared("h4000-radio", &h4000_radio_trig);
}

MACHINE_START(H4000, "HP iPAQ H4100")
	/* Maintainer h4000 port team h4100-port@handhelds.org */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= h4000_map_io,
	.init_irq	= pxa_init_irq,
	.init_machine	= h4000_init,
	.timer		= &pxa_timer,
MACHINE_END

MACHINE_START(H4300, "HP iPAQ H4300")
	/* Maintainer h4000 port team h4100-port@handhelds.org */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= h4000_map_io,
	.init_irq	= pxa_init_irq,
	.init_machine	= h4000_init,
	.timer		= &pxa_timer,
MACHINE_END
