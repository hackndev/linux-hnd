/*
 * linux/arch/arm/mach-pxa/palmt700wx/palmt700wx.c
 *
 *  Support for the Palm Treo 700wx.
 *
 *  Author: you
 *
 *  Based on palmt650.c by Alex Osborne <bobofdoom@gmail.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/input.h>
//#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/audio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/palmt700wx-gpio.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/irda.h>
#include <asm/arch/sharpsl.h>
#include <asm/arch/udc.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include "../generic.h"

/*********************************************************
 * SD/MMC card controller
 *********************************************************/

static int palmt700wx_mci_init(struct device *dev,
		irqreturn_t (*palmt700wx_detect_int)(int, void *, struct pt_regs *), void *data)
{
	int err;
	/* setup an interrupt for detecting card insert/remove events */
	set_irq_type(IRQ_GPIO_PALMT700WX_SD_DETECT_N, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMT700WX_SD_DETECT_N, palmt700wx_detect_int,
			SA_INTERRUPT, "SD/MMC card detect", data);
			
	if(err) {
		printk(KERN_ERR "palmt700wx: can't get SD/MMC card detect IRQ\n");
		return err;
	}
	return 0;
}

static void palmt700wx_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMT700WX_SD_DETECT_N, data);
}

static struct pxamci_platform_data palmt700wx_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= palmt700wx_mci_init,
	/* .setpower 	= palmt700wx_mci_setpower,	*/
	.exit		= palmt700wx_mci_exit,

};

/*********************************************************
 * AC97 audio controller
 *********************************************************/

static pxa2xx_audio_ops_t palmt700wx_audio_ops = {
	/*
	.startup	= palmt700wx_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
	*/
};

static struct platform_device palmt700wx_ac97 = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= { .platform_data = &palmt700wx_audio_ops },
};

/*********************************************************
 * IRDA
 *********************************************************/

static void palmt700wx_irda_transceiver_mode(struct device *dev, int mode)
{
#if 0
	SET_PALMT700WX_GPIO(IRDA_SD, mode & IR_OFF);
#endif
}

static struct pxaficp_platform_data palmt700wx_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = palmt700wx_irda_transceiver_mode,
};

/*********************************************************
 * LEDs
 *********************************************************/

static struct platform_device palmt700wx_led = {
	.name	= "palmt700wx-led",
	.id	= -1,
};

/*********************************************************
 * Backlight
 *********************************************************/

static void palmt700wx_set_bl_intensity(int intensity)
{
	PWM_CTRL0 = 0;			/* pre-scalar */
	PWM_PWDUTY0 = intensity;	/* duty cycle */
	PWM_PERVAL0 = 0x1b1;		/* period */
	
	if (intensity > 0) {
		pxa_set_cken(CKEN0_PWM0, 1);
	} else {
		pxa_set_cken(CKEN0_PWM0, 0);
	}
}

static struct corgibl_machinfo palmt700wx_bl_machinfo = {
	.max_intensity		= 0x1ad,
	.default_intensity	= 0xe5,
	.limit_mask		= 0x7f,
	.set_bl_intensity	= palmt700wx_set_bl_intensity,
};

static struct platform_device palmt700wx_bl = {
	.name = "corgi-bl",
	.dev = {
		.platform_data = &palmt700wx_bl_machinfo,
	},
};

/*********************************************************
 * USB Device Controller
 *********************************************************/

static int udc_is_connected(void)
{
	return 1111;//GPLR(GPIO_PALMLD_USB_DETECT) & GPIO_bit(GPIO_PALMLD_USB_DETECT);
}

static void udc_enable(int cmd) 
{
	/**
	  * TODO: find the GPIO line which powers up the USB.
	  */
	switch (cmd)
	{
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk (KERN_NOTICE "USB cmd disconnect\n");
			/* SET_X30_GPIO(USB_PUEN, 0); */
			break;

		case PXA2XX_UDC_CMD_CONNECT:
			printk (KERN_NOTICE "USB cmd connect\n");
			/* SET_X30_GPIO(USB_PUEN, 1); */
			break;
	}
}
static struct pxa2xx_udc_mach_info palmt700wx_udc_mach_info = {
	.udc_is_connected = udc_is_connected,
	.udc_command      = udc_enable,
};

/*********************************************************
 * Keypad
 *********************************************************/

static struct pxa27x_keyboard_platform_data palmt700wx_kbd_data = {
	.nr_rows = 8,
	.nr_cols = 7,
	.keycodes = {
	{	/* row 0 */
		KEY_O,		// "O" 
		KEY_LEFT,		// "5-Way Left" },
		KEY_RIGHTSHIFT,	// "Alternate" },
		KEY_L,		// "L" },
		KEY_A,		// "A" },
		KEY_Q,		// "Q" },
		KEY_RIGHTCTRL, 	// "Right Shift" },
	}, {	/* row 1 */
		KEY_P,		// "P" },
		KEY_RIGHT,		// "5-Way Right" },
		KEY_LEFTSHIFT,	//"Left Shift" },
		KEY_Z,		// "Z" },
		KEY_S,		// "S" },
		KEY_W,		// "W" },
		-1,	// "Unused" },
	}, {	/* row 2 */
		KEY_F1,		// "Phone" },
		KEY_UP,		// "5-Way Up" },
		KEY_0,		// "0" },
		KEY_X,		// "X" },
		KEY_D,		// "D" },
		KEY_E,		// "E" },
		-1, // "Unused" },
	}, {	/* row 3 */
		KEY_F2,		// "Calendar" },
		KEY_DOWN,		// "5-Way Down" },
		KEY_SPACE,		// "Space" },
		KEY_C,		// "C" },
		KEY_F,		// "F" },
		KEY_R,		// "R" },
		-1, // "Unused" },
	}, {	/* row 4 */
		KEY_F3,		// "Mail" },
		KEY_SELECT,	// "5-Way Center" },
		KEY_HOME,		// "Unused" },
		KEY_V,		// "V" },
		KEY_G,		// "G" },
		KEY_T,		// "T" },
		-1, // "Unused" },
	}, {	/* row 5 */
		KEY_F4,		// "Off" },
		KEY_VOLUMEUP,	// "Volume Up" },
		KEY_DOT,		// "." },
		KEY_B,		// "B" },
		KEY_H,		// "H" },
		KEY_Y,		// "Y" },
		-1, // "Unused" },
	}, {	/* row 6 */
		KEY_F5,		// "Mute" },
		KEY_VOLUMEDOWN,	// "Volume Down" },
		KEY_KPENTER,	// "Return" },
		KEY_N,		// "N" },
		KEY_J,		// "J" },
		KEY_U,		// "U" },
		-1, // "Unused" },
	}, {	/* row 7 */
		KEY_RIGHTALT,	// "Alt" },
		KEY_MENU,		// "Unused" },
		KEY_BACKSPACE,		// "P" },
		KEY_M,		// "M"
		KEY_K,		// "K"
		KEY_I,		// "I"
		-1, // "Unused" },
	},

	},
	.gpio_modes = {
		 GPIO_NR_PALMT700WX_KP_MKIN0_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN1_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN2_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN3_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN4_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN5_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN6_MD,
		 GPIO_NR_PALMT700WX_KP_MKIN7_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT0_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT1_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT2_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT3_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT4_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT5_MD,
		 GPIO_NR_PALMT700WX_KP_MKOUT6_MD,
	 },
};

static struct platform_device palmt700wx_kbd = {
	.name	= "pxa27x-keyboard",
	.dev	= {
		.platform_data	= &palmt700wx_kbd_data,
	},
};



static struct platform_device *devices[] __initdata = {
	&palmt700wx_kbd, &palmt700wx_ac97, &palmt700wx_bl,
	&palmt700wx_led,
};

/*********************************************************
 * LCD
 *********************************************************/

/* when we upgrade to 2.6.19+ remote OLDLCD */
#define OLDLCD
#ifndef OLDLCD
static struct pxafb_mode_info palmt700wx_lcd_mode __initdata = {
	/* pixclock is set by lccr3 below */
	.pixclock		= 50000,	
	.xres			= 320,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	/* fixme: these are the margins PalmOS has set,
	  * 	   they seem to work but could be better.
	  */
	.left_margin		= 20,
	.right_margin		= 8,
	.upper_margin		= 8,
	.lower_margin		= 5,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
};
#endif

static struct pxafb_mach_info palmt700wx_lcd __initdata = {
	.lccr0			= 0x4000080,
	.lccr3			= 0x4700003,
	//.pxafb_backlight_power	= palm_backlight_power,
	//
	//
#ifdef OLDLCD
	/* pixclock is set by lccr3 below */
	.pixclock		= 50000,	
	.xres			= 320,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	/* fixme: these are the margins PalmOS has set,
	  * 	   they seem to work but could be better.
	  */
	.left_margin		= 20,
	.right_margin		= 8,
	.upper_margin		= 8,
	.lower_margin		= 5,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
#else
	.num_modes		= 1,
	.modes			= &palmt700wx_lcd_mode,
#endif
};

static struct map_desc palmt700wx_io_desc[] __initdata = {
  	{	/* Devs */
		.virtual	= PALMT700WX_ASIC6_VIRT,
		.pfn		= __phys_to_pfn(PALMT700WX_ASIC6_PHYS),
		.length		= PALMT700WX_ASIC6_SIZE,
		.type		= MT_DEVICE
	},
};

static void __init palmt700wx_map_io(void)
{
	pxa_map_io();
	iotable_init(palmt700wx_io_desc, ARRAY_SIZE(palmt700wx_io_desc));
}

static void __init palmt700wx_init(void)
{
	/* Disable PRIRDY interrupt to avoid hanging when loading AC97 */
	GCR &= ~GCR_PRIRDY_IEN;
	set_pxa_fb_info(&palmt700wx_lcd);
	pxa_set_mci_info(&palmt700wx_mci_platform_data);
	pxa_set_ficp_info(&palmt700wx_ficp_platform_data);
	pxa_set_udc_info( &palmt700wx_udc_mach_info );
	platform_add_devices(devices, ARRAY_SIZE(devices));

#if 0
	/* configure power switch to resume from standby */
	PWER |= PWER_GPIO12;
	PRER |= PWER_GPIO12;
#endif
}

MACHINE_START(T700WX, "Palm Treo 700w")
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= palmt700wx_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= palmt700wx_init,
MACHINE_END

