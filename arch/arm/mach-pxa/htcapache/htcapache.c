/* Support for HTC Apache phones.
 *
 * (c) Copyright 2007 Kevin O'Connor <kevin@koconnor.net>
 *
 * Based on code from: Alex Osborne <bobofdoom@gmail.com>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/delay.h> // mdelay
#include <linux/input.h>
#include <linux/ad7877.h>
#include <linux/touchscreen-adc.h>
#include <linux/gpio_keys.h>
#include <linux/adc_battery.h>
#include <linux/pda_power.h>
#include <linux/mfd/htc-egpio.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/htcapache-gpio.h>
#include <asm/arch/mmc.h>
#include <asm/arch/serial.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/irda.h> // struct pxaficp_platform_data

#include "../generic.h"
#include "../../../../drivers/net/wireless/acx/acx_hw.h"


/****************************************************************
 * EGPIO
 ****************************************************************/

// Location of the egpio chip in physical ram.
enum { EGPIO_BASE = PXA_CS2_PHYS+0x02000000 };

static struct resource egpio_resources[] = {
	[0] = {
		.start  = EGPIO_BASE,
		.end    = EGPIO_BASE + 0x20,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_GPIO(GPIO_NR_HTCAPACHE_EGPIO_IRQ),
		.end    = IRQ_GPIO(GPIO_NR_HTCAPACHE_EGPIO_IRQ),
		.flags  = IORESOURCE_IRQ,
	},
};

struct htc_egpio_pinInfo pins[] = {
	// Input pins with IRQs
	{.pin_nr = EGPIO_NR_HTCAPACHE_PWR_IN_PWR
	 , .type = HTC_EGPIO_TYPE_INPUT
	 , .input_irq = EGPIO_NR_HTCAPACHE_PWR_IN_PWR+8},
	{.pin_nr = EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR
	 , .type = HTC_EGPIO_TYPE_INPUT
	 , .input_irq = EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR+8},
	{.pin_nr = EGPIO_NR_HTCAPACHE_SND_IN_JACK
	 , .type = HTC_EGPIO_TYPE_INPUT
	 , .input_irq = EGPIO_NR_HTCAPACHE_SND_IN_JACK+8},
	{.pin_nr = EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ
	 , .type = HTC_EGPIO_TYPE_INPUT
	 , .input_irq = EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ+8},
	// Output pins that default on
	{.pin_nr = EGPIO_NR_HTCAPACHE_PWR_CHARGE
	 , .type = HTC_EGPIO_TYPE_OUTPUT
	 , .output_initial = 1},  // Disable charger
};

struct htc_egpio_platform_data egpio_data = {
	.invertAcks = 1,
	.irq_base = IRQ_BOARD_START,
	.gpio_base = GPIO_BASE_INCREMENT,
	.nrRegs = 3,
	.pins = pins,
	.nr_pins = ARRAY_SIZE(pins),
};

static struct platform_device egpio = {
	.name = "htc-egpio",
	.id = -1,
	.resource = egpio_resources,
	.num_resources = ARRAY_SIZE(egpio_resources),
	.dev    = {
		.platform_data = &egpio_data,
	},
};

// Compatibility wrappers
#define IRQ_EGPIO(x)   (IRQ_BOARD_START + (x) + 8)
int htcapache_egpio_get(int bit) {
	return egpio_data.ops.get(&egpio.dev, bit);
}
EXPORT_SYMBOL(htcapache_egpio_get);
void htcapache_egpio_set(int bit, int value) {
	egpio_data.ops.set(&egpio.dev, bit, value);
}
EXPORT_SYMBOL(htcapache_egpio_set);
int htcapache_egpio_to_irq(int bit) {
	return egpio_data.ops.to_irq(&egpio.dev, bit);
}
EXPORT_SYMBOL(htcapache_egpio_to_irq);


/****************************************************************
 * LCD power
 ****************************************************************/

static void lcd_power(int on, struct fb_var_screeninfo *si)
{
	if (on) {
		printk(KERN_DEBUG "lcd power on\n");
		htcapache_write_mc_reg(0, 0x40);
		udelay(2000);
		htcapache_write_mc_reg(0, 0xc0);
		udelay(2000);
		htcapache_write_mc_reg(0, 0xd0);
		udelay(2000);
		htcapache_write_mc_reg(0, 0xf0);
	} else {
		printk(KERN_DEBUG "lcd power off\n");
		htcapache_write_mc_reg(0, 0xd0);
		udelay(2000);
		htcapache_write_mc_reg(0, 0xc0);
		udelay(2000);
		htcapache_write_mc_reg(0, 0x40);
		udelay(2000);
		htcapache_write_mc_reg(0, 0x00);
	}
}


/****************************************************************
 * Frame buffer
 ****************************************************************/

static struct pxafb_mode_info lcd_mode = {
	.pixclock		= 192307,
	.xres			= 240,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 11,
	.vsync_len		= 3,
	// These margins are from WinCE
	.left_margin		= 19,
	.right_margin		= 10,
	.upper_margin		= 2,
	.lower_margin		= 2,
};
static struct pxafb_mach_info lcd = {
        .modes			= &lcd_mode,
        .num_modes		= 1,

	.lccr0			= LCCR0_LDDALT | LCCR0_Act,
	.pxafb_lcd_power	= lcd_power,
};


/****************************************************************
 * Touchscreen
 ****************************************************************/

static struct ad7877_platform_data ad7877_data = {
	.dav_irq = IRQ_GPIO(GPIO_NR_HTCAPACHE_TS_DAV),
};

static struct platform_device ad7877 = {
	.name = "ad7877",
	.id = -1,
	.dev    = {
		.platform_data = &ad7877_data,
	},
};

static struct tsadc_platform_data tsadc = {
	.pressure_factor = 100000, // XXX - adjust value
	.max_sense = 4096,
	.min_pressure = 1,   // XXX - don't know real value.
	.pen_gpio = GPIO_NR_HTCAPACHE_TS_PENDOWN,

	.x_pin = "ad7877:x",
	.y_pin = "ad7877:y",
	.z1_pin = "ad7877:z1",
	.z2_pin = "ad7877:z2",
	.num_xy_samples = 1,
	.num_z_samples = 1,
};

static struct resource htcapache_pen_irq = {
	.start = IRQ_GPIO(GPIO_NR_HTCAPACHE_TS_PENDOWN),
	.end = IRQ_GPIO(GPIO_NR_HTCAPACHE_TS_PENDOWN),
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
};

static struct platform_device htcapache_ts = {
	.name = "ts-adc",
	.id = -1,
	.resource = &htcapache_pen_irq,
	.num_resources = 1,
	.dev    = {
		.platform_data = &tsadc,
	},
};


/****************************************************************
 * Power management
 ****************************************************************/

static void set_charge(int flags)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_USB_PWR
			    , flags == PDA_POWER_CHARGE_USB);

	// XXX - enable/disable battery charger once charge complete
	// detection available.
}

static int ac_on(void)
{
	int haspower = !htcapache_egpio_get(EGPIO_NR_HTCAPACHE_PWR_IN_PWR);
	int hashigh = !htcapache_egpio_get(EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR);
	return haspower && hashigh;
}

static int usb_on(void)
{
	int haspower = !htcapache_egpio_get(EGPIO_NR_HTCAPACHE_PWR_IN_PWR);
	int hashigh = !htcapache_egpio_get(EGPIO_NR_HTCAPACHE_PWR_IN_HIGHPWR);
	return haspower && !hashigh;
}

static struct pda_power_pdata power_pdata = {
	.is_ac_online	= ac_on,
	.is_usb_online	= usb_on,
	.set_charge	= set_charge,
};

static struct resource power_resources[] = {
	[0] = {
		.name	= "ac",
		.start	= IRQ_EGPIO(EGPIO_NR_HTCAPACHE_PWR_IN_PWR),
		.end	= IRQ_EGPIO(EGPIO_NR_HTCAPACHE_PWR_IN_PWR),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device power_dev = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= power_resources,
	.num_resources	= ARRAY_SIZE(power_resources),
	.dev = {
		.platform_data	= &power_pdata,
	},
};


/****************************************************************
 * Battery monitoring
 ****************************************************************/

static struct battery_adc_platform_data apache_main_batt_params = {
	.battery_info = {
		.name = "main-battery",
		.voltage_max_design = 4200000,
		.voltage_min_design = 3500000,
		.charge_full_design = 1000000,
		.use_for_apm = 1,
	},
	.voltage_pin = "ad7877:aux1",
	.current_pin = "ad7877:aux2",
	.temperature_pin = "ad7877:bat1",
	/* Coefficient is 1.08 */
	.voltage_mult = 1080,
	/* Coefficient is 1.13 */
	.current_mult = 1130,
};

static struct platform_device apache_main_batt = {
	.name = "adc-battery",
	.id = -1,
	.dev = {
		.platform_data = &apache_main_batt_params,
	}
};


/****************************************************************
 * LEDS
 ****************************************************************/

static struct platform_device leds = {
	.name = "htcapache-leds",
};


/****************************************************************
 * Bluetooth
 ****************************************************************/

static void btuart_configure(int state)
{
	int tries;
	switch (state) {
	case PXA_UART_CFG_POST_STARTUP:
		pxa_gpio_mode(GPIO42_BTRXD_MD);
		pxa_gpio_mode(GPIO43_BTTXD_MD);
		pxa_gpio_mode(GPIO44_BTCTS_MD);
		pxa_gpio_mode(GPIO45_BTRTS_MD);

		htcapache_egpio_set(EGPIO_NR_HTCAPACHE_BT_POWER, 1);
		mdelay(5);
		htcapache_egpio_set(EGPIO_NR_HTCAPACHE_BT_RESET, 1);
		/*
		 * BRF6150's RTS goes low when firmware is ready
		 * so check for CTS=1 (nCTS=0 -> CTS=1). Typical 150ms
		 */
		tries = 0;
		do {
			mdelay(10);
		} while ((BTMSR & MSR_CTS) == 0 && tries++ < 50);
		printk("btuart: post_startup (%d)\n", tries);
		break;
	case PXA_UART_CFG_PRE_SHUTDOWN:
		htcapache_egpio_set(EGPIO_NR_HTCAPACHE_BT_POWER, 0);
		htcapache_egpio_set(EGPIO_NR_HTCAPACHE_BT_RESET, 0);
		printk("btuart: pre_shutdown\n");
		break;
	}
}

static struct platform_pxa_serial_funcs btuart_funcs = {
        .configure = btuart_configure,
};


/****************************************************************
 * Irda
 ****************************************************************/

static void irda_transceiver_mode(struct device *dev, int mode)
{
	printk("irda: transceiver_mode=%d\n", mode);
	// XXX - don't know transceiver enable/disable pins.
	// XXX - don't know if FIR supported or how to enable.
}

static struct pxaficp_platform_data apache_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = irda_transceiver_mode,
};


/****************************************************************
 * Wifi
 ****************************************************************/

static int
wlan_start(void)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER1, 1);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER2, 1);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER3, 1);
	mdelay(250);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_RESET, 1);
        mdelay(100);
        return 0;
}

static int
wlan_stop(void)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER1, 0);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER2, 0);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_POWER3, 0);
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_WIFI_RESET, 0);
        return 0;
}

enum {
	WLAN_BASE = PXA_CS2_PHYS,
};

static struct resource acx_resources[] = {
	[0] = {
		.start  = WLAN_BASE,
		.end    = WLAN_BASE + 0x20,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_EGPIO(EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ),
		.end    = IRQ_EGPIO(EGPIO_NR_HTCAPACHE_WIFI_IN_IRQ),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct acx_hardware_data acx_data = {
	.start_hw       = wlan_start,
	.stop_hw        = wlan_stop,
};

static struct platform_device acx_device = {
	.name   = "acx-mem",
	.dev    = {
		.platform_data = &acx_data,
	},
	.num_resources  = ARRAY_SIZE(acx_resources),
	.resource       = acx_resources,
};


/****************************************************************
 * Pull out keyboard
 ****************************************************************/

static struct pxa27x_keyboard_platform_data htcapache_kbd = {
	.nr_rows = 7,
	.nr_cols = 7,
	.keycodes = {
		{
			/* row 0 */
			-1,		// Unused
			KEY_LEFTSHIFT,	// Left Shift
			-1,		// Unused
			KEY_Q,		// Q
			KEY_W,		// W
			KEY_E,		// E
			KEY_R,	 	// R
		}, {	/* row 1 */
			-1,		// Unused
			-1,		// Unused
			KEY_LEFTALT,	// Red Dot
			KEY_T,		// T
			KEY_Y,		// Y
			KEY_U,		// U
			KEY_I,		// I
		}, {	/* row 2 */
			-1,		// Unused
			KEY_LEFTMETA,	// Windows Key
			-1,		// Unused
			KEY_ENTER,	// Return
			KEY_SPACE,	// Space
			KEY_BACKSPACE,	// Backspace
			KEY_A,		// A
		}, {	/* row 3 */
			-1,		// Unused
			KEY_S,		// S
			KEY_D,		// D
			KEY_F,		// F
			KEY_G,		// G
			KEY_H,		// H
			KEY_J,		// J
		}, {	/* row 4 */
			KEY_LEFTCTRL,	// Left Menu
			KEY_K,		// K
			KEY_Z,		// Z
			KEY_X,		// X
			KEY_C,		// C
			KEY_V,		// V
			KEY_B,		// B
		}, {	/* row 5 */
			KEY_RIGHTCTRL,	// Right Menu
			KEY_N,		// N
			KEY_M,		// M
			KEY_O,		// O
			KEY_L,		// L
			KEY_P,		// P
			KEY_DOT, 	// .
		}, {	/* row 6 */
			-1,		// Unused
			KEY_LEFT,	// Left Arrow
			KEY_DOWN,	// Down Arrow
			KEY_UP,		// Up Arrow
			KEY_ESC,	// OK button
			KEY_TAB,	// Tab
			KEY_RIGHT,	// Right Arrow
		},
	},
	.gpio_modes = {
		 GPIO_NR_HTCAPACHE_KP_MKIN0_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN1_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN2_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN3_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN4_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN5_MD,
		 GPIO_NR_HTCAPACHE_KP_MKIN6_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT0_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT1_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT2_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT3_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT4_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT5_MD,
		 GPIO_NR_HTCAPACHE_KP_MKOUT6_MD,
	 },
};

static struct platform_device htcapache_keyboard = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &htcapache_kbd,
	},
};


/****************************************************************
 * Buttons on side
 ****************************************************************/

static struct gpio_keys_button htcapache_button_list[] = {
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_POWER, .keycode = KEY_POWER
	  , .active_low = 1},
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_RECORD, .keycode = KEY_RECORD},
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_VOLUP, .keycode = KEY_VOLUMEUP},
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_VOLDOWN, .keycode = KEY_VOLUMEDOWN},
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_BROWSER, .keycode = KEY_WWW},
	{ .gpio = GPIO_NR_HTCAPACHE_BUTTON_CAMERA, .keycode = KEY_CAMERA},
	{ .gpio = GPIO_NR_HTCAPACHE_KP_PULLOUT, .keycode = KEY_KEYBOARD
	  , .active_low = 1},
};

static struct gpio_keys_platform_data htcapache_buttons_data = {
	.buttons = htcapache_button_list,
	.nbuttons = ARRAY_SIZE(htcapache_button_list),
};

static struct platform_device htcapache_buttons = {
        .name   = "gpio-keys",
        .id     = -1,
	.dev	=  {
		.platform_data	= &htcapache_buttons_data,
	},
};


/****************************************************************
 * Buttons on front
 ****************************************************************/

static struct gpio_keys_button front_button_list[] = {
	{ .gpio = 15, .keycode = KEY_PHONE},     // Call button
	{ .gpio = 14, .keycode = KEY_LEFTCTRL},  // Left menu
	{ .gpio = 13, .keycode = KEY_ESC},       // Hangup
	{ .gpio = 12, .keycode = KEY_RIGHTCTRL}, // Right menu
	{ .gpio = 11, .keycode = KEY_OK},        // Ok
	{ .gpio = 10, .keycode = KEY_LEFTMETA},  // Windows key
	{ .gpio = 7,  .keycode = KEY_RIGHT},     // Joystick right
	{ .gpio = 6,  .keycode = KEY_UP},        // Joystick up
	{ .gpio = 5,  .keycode = KEY_LEFT},      // Joystick left
	{ .gpio = 4,  .keycode = KEY_DOWN},      // Joystick down
	{ .gpio = 3,  .keycode = KEY_ENTER},     // Joystick press
};

static struct gpio_keys_platform_data front_buttons_data = {
	.buttons = front_button_list,
	.nbuttons = ARRAY_SIZE(front_button_list),
};

static struct platform_device front_buttons = {
	.name   = "htcapache-frontkeys",
	.id     = -1,
	.dev	=  {
		.platform_data	= &front_buttons_data,
	},
};


/****************************************************************
 * USB client controller
 ****************************************************************/

static void udc_command(int cmd)
{
	switch (cmd)
	{
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk(KERN_NOTICE "USB cmd disconnect\n");
			gpio_set_value(GPIO_NR_HTCAPACHE_USB_PUEN, 1);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			printk(KERN_NOTICE "USB cmd connect\n");
			gpio_set_value(GPIO_NR_HTCAPACHE_USB_PUEN, 0);
			break;
	}
}

static struct pxa2xx_udc_mach_info htcapache_udc_mach_info = {
	.udc_command      = udc_command,
};


/****************************************************************
 * Mini-SD card
 ****************************************************************/

static int
htcapache_mci_init(struct device *dev
		   , irqreturn_t (*ih)(int, void *)
		   , void *data)
{
	int err = request_irq(IRQ_GPIO(GPIO_NR_HTCAPACHE_SD_CARD_DETECT_N)
			      , ih
			      , IRQF_DISABLED | IRQF_TRIGGER_RISING
			        | IRQF_TRIGGER_FALLING
			      , "MMC/SD card detect", data);
	if (err) {
		printk(KERN_ERR "htcapache_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static void htcapache_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	// XXX - No idea if this is correct for apache..
	if ((1 << vdd) & p_d->ocr_mask) {
		printk(KERN_NOTICE "MMC power up (vdd=%d mask=%08x)\n"
		       , vdd, p_d->ocr_mask);
		gpio_set_value(GPIO_NR_HTCAPACHE_SD_POWER_N, 1);
	} else {
		printk(KERN_NOTICE "MMC power down (vdd=%d mask=%08x)\n"
		       , vdd, p_d->ocr_mask);
		gpio_set_value(GPIO_NR_HTCAPACHE_SD_POWER_N, 0);
	}
}

static void htcapache_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO(GPIO_NR_HTCAPACHE_SD_CARD_DETECT_N), data);
}

static struct pxamci_platform_data htcapache_mci_platform_data = {
	.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init           = htcapache_mci_init,
	.setpower       = htcapache_mci_setpower,
	.exit           = htcapache_mci_exit,
};


/****************************************************************
 * Init
 ****************************************************************/

extern struct platform_device htcapache_bl;

static struct platform_device *devices[] __initdata = {
	&egpio,
	&htcapache_keyboard,
	&htcapache_buttons,
	&ad7877,
	&power_dev,
	&apache_main_batt,
	&htcapache_ts,
	&htcapache_bl,
	&leds,
	&acx_device,
	&front_buttons,
};

void htcapache_ll_pm_init(void);

static void __init htcapache_init(void)
{
	set_pxa_fb_info(&lcd);
	platform_add_devices(devices, ARRAY_SIZE(devices));
	pxa_set_btuart_info(&btuart_funcs);
	pxa_set_udc_info(&htcapache_udc_mach_info);
	pxa_set_mci_info(&htcapache_mci_platform_data);
	pxa_set_ficp_info(&apache_ficp_platform_data);
#ifdef CONFIG_PM
	htcapache_ll_pm_init();
#endif
}

MACHINE_START(HTCAPACHE, "HTC Apache")
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io 	= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcapache_init,
MACHINE_END
