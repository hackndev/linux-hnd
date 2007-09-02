/*
 * Hardware definitions for Palm Tungsten E2.
 *
 * Based on: palmld.c from Alex Osborne.
 *
 * Author: Carlos Eduardo Medaglia Dyonisio <cadu@nerdfeliz.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/corgi_bl.h>

#include <asm/arch/audio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/mmc.h>
#include <asm/arch/serial.h>
#include <asm/arch/palmte2-gpio.h>
#include <asm/arch/palmte2-init.h>

#include <linux/gpio_keys.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include "../generic.h"

#define DEBUG

/*
 * AC97 audio controller
 */

static pxa2xx_audio_ops_t palmte2_audio_ops = {
/*
	.startup	= palmte2_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
*/
};

static struct platform_device palmte2_ac97 = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= {
	    .platform_data	= &palmte2_audio_ops
	},
};

/*
 * Backlight
 */

static void palmte2_set_bl_intensity(int intensity)
{
        if(intensity) {
                PWM_CTRL0   = 0x7;
                PWM_PERVAL0 = PALMTE2_PERIOD;
                PWM_PWDUTY0 = intensity;
        }
}

static struct corgibl_machinfo palmte2_bl_machinfo = {
    .max_intensity      = PALMTE2_MAX_INTENSITY,
    .default_intensity  = PALMTE2_MAX_INTENSITY,
    .set_bl_intensity   = palmte2_set_bl_intensity,
    .limit_mask         = PALMTE2_LIMIT_MASK,
};

static struct platform_device palmte2_backlight = {
    .name       = "corgi-bl",
    .id         = 0,
    .dev        = {
            .platform_data = &palmte2_bl_machinfo,
    },
};

/*
 * Bluetooth
 */

void bcm2035_bt_reset(int on)
{
	printk(KERN_NOTICE "Switch BT reset %d\n", on);
	SET_PALMTE2_GPIO(BT_RESET, on ? 1 : 0);
}
EXPORT_SYMBOL(bcm2035_bt_reset);

void bcm2035_bt_power(int on)
{
	printk(KERN_NOTICE "Switch BT power %d\n", on);
	SET_PALMTE2_GPIO(BT_POWER, on ? 1 : 0);
}
EXPORT_SYMBOL(bcm2035_bt_power);

struct bcm2035_bt_funcs {
	void (*configure) (int state);
};

static struct bcm2035_bt_funcs bt_funcs;

static void bcm2035_bt_configure(int state)
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure(state);
}

static struct platform_pxa_serial_funcs bcm2035_pxa_bt_funcs = {
	.configure = bcm2035_bt_configure,
};

static struct platform_device bcm2035_bt = {
	.name = "bcm2035-bt",
	.id = -1,
	.dev = {
		.platform_data = &bt_funcs,
	},
};

/*
 * Keypad - gpio_keys
 */

#ifdef CONFIG_KEYBOARD_GPIO

static struct gpio_keys_button palmte2_pxa_buttons[] = {
	{KEY_F4,	GPIO_NR_PALMTE2_KP_DKIN0, 1, "Notes"},
	{KEY_F3,	GPIO_NR_PALMTE2_KP_DKIN1, 1, "Tasks"},
	{KEY_F1,	GPIO_NR_PALMTE2_KP_DKIN2, 1, "Calendar"},
	{KEY_F2,	GPIO_NR_PALMTE2_KP_DKIN3, 1, "Contacts"},
	{KEY_ENTER,	GPIO_NR_PALMTE2_KP_DKIN4, 1, "Center"},
	{KEY_DOWN,	GPIO_NR_PALMTE2_KP_DKIN5, 1, "Left"},
	{KEY_UP,	GPIO_NR_PALMTE2_KP_DKIN6, 1, "Right"},
	{KEY_RIGHT,	GPIO_NR_PALMTE2_KP_DKIN7, 1, "Down"},
	{KEY_LEFT,	GPIO_NR_PALMTE2_KP_DKIN8, 1, "Up"},
};

static struct gpio_keys_platform_data palmte2_pxa_keys_data = {
	.buttons = palmte2_pxa_buttons,
	.nbuttons = ARRAY_SIZE(palmte2_pxa_buttons),
};

static struct platform_device palmte2_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &palmte2_pxa_keys_data,
	},
};

#endif

/*
 * Framebuffer
 */

static struct pxafb_mode_info palmte2_lcd_modes[] = {
    {
	.pixclock		= 0,
	.xres			= 320,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	.left_margin		= 28,
	.right_margin		= 7,
	.upper_margin		= 7,
	.lower_margin		= 5,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
    },
};

static struct pxafb_mach_info palmte2_lcd_screen = {
	.modes			= palmte2_lcd_modes,
	.num_modes		= ARRAY_SIZE(palmte2_lcd_modes),
	.lccr0 = 0x000000F9,
	.lccr3 = 0x04700006,
	.pxafb_backlight_power	= NULL,
};

/*
 * Init Machine
 */

static struct platform_device *devices[] __initdata = {
	&palmte2_ac97,
#ifdef CONFIG_KEYBOARD_GPIO
	&palmte2_pxa_keys,
#endif
	&palmte2_backlight,
	&bcm2035_bt,
};

static void __init palmte2_init(void)
{
	/* int reg; */

	GCR &= ~GCR_PRIRDY_IEN;

	set_pxa_fb_info(&palmte2_lcd_screen);

	pxa_set_btuart_info(&bcm2035_pxa_bt_funcs);

        platform_add_devices(devices, ARRAY_SIZE(devices));

/*
	SET_GPIO(32, 0);
	SET_GPIO(80, 0);

	reg = __REG(0x40E0005C);
	__REG(0x40E0005C) = (reg | 0x0FF00000);
*/
}

MACHINE_START(TUNGE2, "Palm Tungsten E2")

	/* Maintainer: Carlos E. Medaglia Dyonisio <cadu@nerdfeliz.com> */

	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params    = 0xa0000100,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmte2_init
MACHINE_END
