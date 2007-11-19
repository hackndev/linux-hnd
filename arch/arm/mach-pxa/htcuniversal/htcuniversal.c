/*
 * Hardware definitions for HTC Universal
 *
 * Copyright (c) 2006 Oleg Gusev
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/mfd/asic3_base.h>
#include <linux/ads7846.h>
#include <linux/touchscreen-adc.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>

#include <asm/arch/bitfield.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/irda.h>
#include <asm/arch/ohci.h>
#include <asm/hardware/asic3_leds.h>

#include <asm/arch/htcuniversal.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-init.h>
#include <asm/arch/htcuniversal-asic.h>

#include <asm/hardware/ipaq-asic3.h>

#include "../generic.h"

#include "htcuniversal_bt.h"
#include "htcuniversal_phone.h"
#include "tsc2046_ts.h"

DEFINE_LED_TRIGGER_SHARED_GLOBAL(htcuniversal_radio_trig);
EXPORT_LED_TRIGGER_SHARED(htcuniversal_radio_trig);

/*
 * LEDs
 */
static struct asic3_led htcuniversal_leds[] = {
	{
		.led_cdev  = {
			.name	        = "htcuniversal:red",
			.default_trigger = "htcuniversal-charging",
		},
		.hw_num = 2,

	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:green",
			.default_trigger = "htcuniversal-chargefull",
		},
		.hw_num = 1,
	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:wifi-bt",
			.default_trigger = "htcuniversal-radio",
		},
		.hw_num = 0,
	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:phonebuttons",
			.default_trigger = "htcuniversal-phonebuttons",
		},
		.hw_num = -1,
		.gpio_num = ('D'-'A')*16+GPIOD_BL_KEYP_PWR_ON,
	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:vibra",
			.default_trigger = "htcuniversal-vibra",
		},
		.hw_num = -1,
		.gpio_num = ('D'-'A')*16+GPIOD_VIBRA_PWR_ON,
	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:flashlight1",
			.default_trigger = "htcuniversal-flashlight1",
		},
		.hw_num = -1,
		.gpio_num = ('A'-'A')*16+GPIOA_FLASHLIGHT,
	},
	{
		.led_cdev  = {
			.name	         = "htcuniversal:kbdbacklight",
			.default_trigger = "htcuniversal-kbdbacklight",
		},
		.hw_num = -1,
		.gpio_num = ('D'-'A')*16+GPIOD_BL_KEYB_PWR_ON,
	},
};

static struct asic3_leds_machinfo htcuniversal_leds_machinfo = {
	.num_leds = ARRAY_SIZE(htcuniversal_leds),
	.leds = htcuniversal_leds,
	.asic3_pdev = &htcuniversal_asic3,
};

static struct platform_device htcuniversal_leds_pdev = {
	.name = "asic3-leds",
	.dev = {
		.platform_data = &htcuniversal_leds_machinfo,
	},
};

/*
 * IRDA
 */

static void htcuniversal_irda_transceiver_mode(struct device *dev, int mode)
{
 /* */
}

static struct pxaficp_platform_data htcuniversal_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_FIRMODE,
	.transceiver_mode = htcuniversal_irda_transceiver_mode,
};

/*
 * Bluetooth - Relies on other loadable modules, like ASIC3 and Core,
 * so make the calls indirectly through pointers. Requires that the
 * htcuniversal_bt module be loaded before any attempt to use
 * bluetooth (obviously).
 */

static struct htcuniversal_bt_funcs bt_funcs;

static void
htcuniversal_bt_configure( int state )
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure( state );
}

static struct htcuniversal_phone_funcs phone_funcs;

static void
htcuniversal_phone_configure( int state )
{
	if (phone_funcs.configure != NULL)
		phone_funcs.configure( state );
}

//void htcuniversal_ll_pm_init(void);

extern struct platform_device htcuniversal_bl;
static struct platform_device htcuniversal_lcd       = { .name = "htcuniversal_lcd", };
//static struct platform_device htcuniversal_kbd       = { .name = "htcuniversal_kbd", };
static struct platform_device htcuniversal_buttons   = { .name = "htcuniversal_buttons", };
//static struct platform_device htcuniversal_ts        = { .name = "htcuniversal_ts", };
//static struct platform_device htcuniversal_bt        = { .name = "htcuniversal_bt", };
//static struct platform_device htcuniversal_phone        = { .name = "htcuniversal_phone", };
static struct platform_device htcuniversal_power        = { .name = "htcuniversal_power", };
static struct platform_device htcuniversal_udc       = { .name = "htcuniversal_udc", };

/* ADC and touchscreen */

// Old driver
static struct tsc2046_mach_info htcuniversal_ts_platform_data = {
       .port     = 1,
       .clock    = CKEN23_SSP1,
       .pwrbit_X = 1,
       .pwrbit_Y = 1,
       .irq	 = 0  /* asic3 irq */
};

static struct platform_device htcuniversal_ts        = {
       .name = "htcuniversal_ts",
       .dev  = {
              .platform_data = &htcuniversal_ts_platform_data,
       },
};

// New driver
struct ads7846_ssp_platform_data htcuniversal_ssp_params = {
	.port = 1,
	.pd_bits = 1,
	.freq = 720000,
};
static struct platform_device tsc2046_ssp = { 
	.name = "ads7846-ssp", 
	.id = -1,
	.dev = {
		.platform_data = &htcuniversal_ssp_params,
	}
};

struct tsadc_platform_data htcuniversal_ts_params = {
//	.pen_gpio = GPIO_NR_HX4700_TOUCHPANEL_IRQ_N,
	.x_pin = "ads7846-ssp:x",
	.y_pin = "ads7846-ssp:y",
	.z1_pin = "ads7846-ssp:z1",
	.z2_pin = "ads7846-ssp:z2",
	.pressure_factor = 100000,
	.min_pressure = 2,
	.max_jitter = 10,
};
static struct resource htcuniversal_pen_irq = {
	.start = HTCUNIVERSAL_ASIC3_IRQ_BASE + ASIC3_GPIOA_IRQ_BASE + GPIOA_TOUCHSCREEN_N,
	.end = HTCUNIVERSAL_ASIC3_IRQ_BASE + ASIC3_GPIOA_IRQ_BASE + GPIOA_TOUCHSCREEN_N,
	.flags = IORESOURCE_IRQ,
};
static struct platform_device htcuniversal_ts2 = {
	.name = "ts-adc",
	.id = -1,
	.resource = &htcuniversal_pen_irq,
	.num_resources = 1,
	.dev = {
		.platform_data = &htcuniversal_ts_params,
	}
};

/* Bluetooth */

static struct platform_device htcuniversal_bt = {
	.name = "htcuniversal_bt",
	.id = -1,
	.dev = {
		.platform_data = &bt_funcs,
	},
};

static struct platform_device htcuniversal_phone = {
	.name = "htcuniversal_phone",
	.id = -1,
	.dev = {
		.platform_data = &phone_funcs,
	},
};

/* PXA2xx Keys */

static struct gpio_keys_button htcuniversal_button_table[] = {
	{ KEY_POWER, GPIO_NR_HTCUNIVERSAL_KEY_ON_N, 1 },
};

static struct gpio_keys_platform_data htcuniversal_pxa_keys_data = {
	.buttons = htcuniversal_button_table,
	.nbuttons = ARRAY_SIZE(htcuniversal_button_table),
};

static struct platform_device htcuniversal_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &htcuniversal_pxa_keys_data,
	},
	.id = -1,
};

/****************************************************************
 * Keyboard
 ****************************************************************/

static struct pxa27x_keyboard_platform_data htcuniversal_kbd = {
	.nr_rows = 8,
	.nr_cols = 8,
	.keycodes = {
		{
			/* row 0 */
			KEY_ENTER,
			KEY_MINUS,
			KEY_ESC,
			KEY_1,	
			KEY_TAB,
			KEY_CAPSLOCK,
			KEY_LEFTSHIFT,
			KEY_RIGHTALT,	/* Fn */
		}, {	/* row 1 */
			KEY_COMMA,
			KEY_EQUAL,
			KEY_F1,	
			KEY_2,	
			KEY_Q,	
			KEY_A,	
			KEY_Z,	
			KEY_LEFTCTRL,
		}, {	/* row 2 */
			KEY_UP,	
			KEY_I,	
			KEY_F2,	
			KEY_3,	
			KEY_W,	
			KEY_S,	
			KEY_X,	
			KEY_F6,	 
		}, {	/* row 3 */
			KEY_DOT,
			KEY_O,	
			KEY_F3,	
			KEY_4,	
			KEY_E,	
			KEY_D,	
			KEY_C,	
			KEY_LEFTALT,
		}, {	/* row 4 */
			KEY_F9,	
			KEY_P,	
			KEY_F4,	
			KEY_5,	
			KEY_R,	
			KEY_F,	
			KEY_V,	
			KEY_SPACE,
		}, {	/* row 5 */
			KEY_RIGHT,
			KEY_BACKSPACE,
			KEY_F5,	
			KEY_6,
			KEY_T,
			KEY_G,
			KEY_B,
			KEY_F7,
		}, {	/* row 6 */
			KEY_F9,	
			KEY_K,	
			KEY_9,	
			KEY_7,	
			KEY_Y,	
			KEY_H,	
			KEY_N, 	
			KEY_LEFT,
		}, {	/* row 7 */
			KEY_F10,
			KEY_L,
			KEY_0,
			KEY_8,
			KEY_U,
			KEY_J,
			KEY_M,
			KEY_DOWN,
		},
	},
	.gpio_modes = {
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN0_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN1_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN2_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN3_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN4_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN5_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN6_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKIN7_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT0_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT1_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT2_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT3_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT4_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT5_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT6_MD,
		 GPIO_NR_HTCUNIVERSAL_KP_MKOUT7_MD,
	 },
};

static struct platform_device htcuniversal_pxa_keyboard = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &htcuniversal_kbd,
	},
};
/* Core Hardware Functions */

struct platform_device htcuniversal_core = {
	.name		= "htcuniversal_core",
	.id		= 0,
	.dev = {
		.platform_data = NULL,
	},
};

static struct platform_device *devices[] __initdata = {
	&tsc2046_ssp,
	&htcuniversal_core,
//	&htcuniversal_flash,
	&htcuniversal_pxa_keyboard,
	&htcuniversal_pxa_keys,
};

static struct platform_device *htcuniversal_asic3_devices[] __initdata = {
	&htcuniversal_lcd,
#ifdef CONFIG_HTCUNIVERSAL_BACKLIGHT
	&htcuniversal_bl,
#endif
	&htcuniversal_buttons,
	&htcuniversal_ts,
	&htcuniversal_ts2,
	&htcuniversal_bt,
	&htcuniversal_phone,
	&htcuniversal_power,
	&htcuniversal_udc,
	&htcuniversal_leds_pdev,
};

static struct asic3_platform_data htcuniversal_asic3_platform_data = {

   /* Setting ASIC3 GPIO registers to the below initialization states
    * HTC Universal asic3 information: 
    * http://wiki.xda-developers.com/index.php?pagename=UniversalASIC3
    * http://wiki.xda-developers.com/index.php?pagename=ASIC3
    *
    * dir:	Direction of the GPIO pin. 0: input, 1: output.
    *      	If unknown, set as output to avoid power consuming floating input nodes
    * init:	Initial state of the GPIO bits
    *
    * These registers are configured as they are on Wince.
    */
        .gpio_a = {
		.dir            = (1<<GPIOA_LCD_PWR5_ON)    |
				  (1<<GPIOA_FLASHLIGHT)     |
				  (1<<GPIOA_UNKNOWN9)       |
				  (1<<GPIOA_SPK_PWR2_ON)    |
				  (1<<GPIOA_UNKNOWN4)       |
				  (1<<GPIOA_EARPHONE_PWR_ON)|
				  (1<<GPIOA_AUDIO_PWR_ON)   |
				  (1<<GPIOA_SPK_PWR1_ON)    |
				  (1<<GPIOA_I2C_EN),
		.init           = (1<<GPIOA_LCD_PWR5_ON)    |
				  (1<<GPIOA_I2C_EN),
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000,
		.sleep_conf     = 0x000c,
        },
        .gpio_b = {
		.dir            = 0xc142,
		.init           = 0x8842, // TODO: 0x0900
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000,
                .sleep_conf     = 0x000c,
        },
        .gpio_c = {
                .dir            = 0xc7e7,
                .init           = 0xc6e0, // TODO: 0x8000
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
		.alt_function   = 0x0007, // GPIOC_LED_RED | GPIOC_LED_GREEN | GPIOC_LED_BLUE
                .sleep_conf     = 0x000c,
        },
        .gpio_d = {
		.dir            = 0xffc0,
		.init           = 0x7840, // TODO: 0x0000
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000,
		.sleep_conf     = 0x0008,
        },
	.bus_shift = 1,
	.irq_base = HTCUNIVERSAL_ASIC3_IRQ_BASE,

	.child_devs     = htcuniversal_asic3_devices,
	.num_child_devs = ARRAY_SIZE(htcuniversal_asic3_devices),
};

static struct resource htcuniversal_asic3_resources[] = {
	[0] = {
		.start	= HTCUNIVERSAL_ASIC3_GPIO_PHYS,
		.end	= HTCUNIVERSAL_ASIC3_GPIO_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= HTCUNIVERSAL_IRQ(ASIC3_EXT_INT),
		.end	= HTCUNIVERSAL_IRQ(ASIC3_EXT_INT),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start  = HTCUNIVERSAL_ASIC3_MMC_PHYS,
		.end    = HTCUNIVERSAL_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = HTCUNIVERSAL_IRQ(ASIC3_SDIO_INT_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device htcuniversal_asic3 = {
	.name           = "asic3",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(htcuniversal_asic3_resources),
	.resource       = htcuniversal_asic3_resources,
	.dev = { .platform_data = &htcuniversal_asic3_platform_data, },
};
EXPORT_SYMBOL(htcuniversal_asic3);

static struct pxafb_mode_info htcuniversal_lcd_modes[] = {
{
	.pixclock		= 96153,
	.xres			= 480,
	.yres			= 640,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,
	.left_margin		= 20,
	.right_margin		= 8,
	.upper_margin		= 7,
	.lower_margin		= 8,

//	.sync			= FB_SYNC_HOR_LOW_ACT|FB_SYNC_VERT_LOW_ACT,

},
};

static struct pxafb_mach_info sony_acx526akm = {
        .modes		= htcuniversal_lcd_modes,
        .num_modes	= ARRAY_SIZE(htcuniversal_lcd_modes), 

	/* fixme: use constants defined in pxafb.h */
	.lccr0			= 0x00000080,
	.lccr3			= 0x00400000,
//	.lccr4			= 0x80000000,
};

static struct platform_pxa_serial_funcs htcuniversal_pxa_bt_funcs = {
	.configure = htcuniversal_bt_configure,
};
static struct platform_pxa_serial_funcs htcuniversal_pxa_phone_funcs = {
	.configure = htcuniversal_phone_configure,
};

/* USB OHCI */

static int htcuniversal_ohci_init(struct device *dev)
{
	/* missing GPIO setup here */

	/* got the value from wince */
	UHCHR=UHCHR_CGR;

	return 0;
}

static struct pxaohci_platform_data htcuniversal_ohci_platform_data = {
	.port_mode = PMM_PERPORT_MODE,
	.init = htcuniversal_ohci_init,
};

static void __init htcuniversal_map_io(void)
{
	pxa_map_io();

	pxa_set_btuart_info(&htcuniversal_pxa_bt_funcs);
	pxa_set_ffuart_info(&htcuniversal_pxa_phone_funcs);
}

static void __init htcuniversal_init(void)
{
	set_pxa_fb_info(&sony_acx526akm);

	platform_device_register(&htcuniversal_asic3);
	platform_add_devices(devices, ARRAY_SIZE(devices) );
	pxa_set_ficp_info(&htcuniversal_ficp_platform_data);
	pxa_set_ohci_info(&htcuniversal_ohci_platform_data);

	led_trigger_register_shared("htcuniversal-radio", &htcuniversal_radio_trig);
}

MACHINE_START(HTCUNIVERSAL, "HTC Universal")
	/* Maintainer xanadux.org */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= htcuniversal_map_io,
	.init_irq	= pxa_init_irq,
	.init_machine	= htcuniversal_init,
	.timer		= &pxa_timer,
MACHINE_END
