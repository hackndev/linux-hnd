/*
 * linux/arch/arm/mach-pxa/palmt650/palmt650.c
 *
 *  Support for the Palm Treo 650 PDA phone.
 *
 *  Author:	Alex Osborne <bobofdoom@gmail.com> 2005-2007
 *  		P3T3, Petr Blaha <pb@p3t3.org> 2007
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/audio.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/irda.h>
#include <asm/arch/sharpsl.h>
#include <asm/arch/udc.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include <asm/arch/palmt650-gpio.h>
#include <asm/arch/palmt650-init.h>

#include "../generic.h"

/*********************************************************
 * SD/MMC card controller
 *********************************************************/

static int palmt650_mci_init(struct device *dev,
		irqreturn_t (*palmt650_detect_int)(int, void *), void *data)
{
	int err;
	/* setup an interrupt for detecting card insert/remove events */
	set_irq_type(IRQ_GPIO_PALMT650_SD_DETECT_N, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMT650_SD_DETECT_N, palmt650_detect_int,
			SA_INTERRUPT, "SD/MMC card detect", data);
			
	if(err) {
		printk(KERN_ERR "palmt650: can't get SD/MMC card detect IRQ\n");
		return err;
	}
	return 0;
}

static void palmt650_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMT650_SD_DETECT_N, data);
}

static struct pxamci_platform_data palmt650_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= palmt650_mci_init,
	/* .setpower 	= palmt650_mci_setpower,	*/
	.exit		= palmt650_mci_exit,

};

/*********************************************************
 * AC97 audio controller
 *********************************************************/

static pxa2xx_audio_ops_t palmt650_audio_ops = {
	/*
	.startup	= palmt650_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
	*/
};

static struct platform_device palmt650_ac97 = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= { .platform_data = &palmt650_audio_ops },
};

/*********************************************************
 * IRDA
 *********************************************************/

static void palmt650_irda_transceiver_mode(struct device *dev, int mode)
{
/*	SET_PALMT650_GPIO(IRDA_SD, mode & IR_OFF);	*/
}

static struct pxaficp_platform_data palmt650_ficp_platform_data = {
	.transceiver_cap  = IR_SIRMODE | IR_OFF,
	.transceiver_mode = palmt650_irda_transceiver_mode,
};

/*********************************************************
 * LEDs
 *********************************************************/
static struct platform_device palmt650_led = {
	.name	= "palmt650-led",
	.id	= -1,
};

/*********************************************************
 * Backlight
 *********************************************************/

static void palmt650_set_bl_power(int on)
{
	/* TODO: set GPIOs */
	pxa_set_cken(CKEN0_PWM0, on);
	pxa_set_cken(CKEN1_PWM1, on);
}

static void palmt650_set_bl_intensity(int intensity)
{
	palmt650_set_bl_power(intensity ? 1 : 0);
	if (intensity) {
		PWM_CTRL0   = 0x1;
		PWM_PERVAL0 = PALMT650_BL_PERIOD;
		PWM_PWDUTY0 = intensity;
	}
}

static struct corgibl_machinfo palmt650_bl_machinfo = {
	.max_intensity		= PALMT650_MAX_INTENSITY,
	.default_intensity	= PALMT650_DEFAULT_INTENSITY,
	.limit_mask		= PALMT650_LIMIT_MASK,
	.set_bl_intensity	= palmt650_set_bl_intensity,
};


static struct platform_device palmt650_backlight = {
	.name	= "corgi-bl",
	.id	= 0,
	.dev = {
		    .platform_data = &palmt650_bl_machinfo,
	},
};
/*********************************************************
 * Power management
 *********************************************************/
struct platform_device palmt650_pm = {
	.name = "palmt650-pm",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

/*********************************************************
 * GSM Baseband Processor
 *********************************************************/
struct platform_device palmt650_gsm = {
	.name = "palmt650-gsm",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

/*********************************************************
 * USB Device Controller
 *********************************************************/

static int udc_is_connected(void)
{
	/* TODO: Implement this. (Check bit 3 of register 0x36 of ASIC6). */
	/* Note: The current version PXA27x UDC does not actually ever call 
	 * udc_is_connected so for the moment this can be left unimplemented.
	 */
	return 1;
}

static void udc_enable(int cmd) 
{
	switch (cmd)
	{
		case PXA2XX_UDC_CMD_DISCONNECT:
			printk (KERN_NOTICE "USB cmd disconnect\n");
			SET_PALMT650_GPIO(USB_PULLUP, 0); 
			break;

		case PXA2XX_UDC_CMD_CONNECT:
			printk (KERN_NOTICE "USB cmd connect\n");
			SET_PALMT650_GPIO(USB_PULLUP, 1);
			break;
	}
}
static struct pxa2xx_udc_mach_info palmt650_udc_mach_info = {
	.udc_is_connected = udc_is_connected,
	.udc_command      = udc_enable,
};

/*********************************************************
 * Keypad
 *********************************************************/

static struct pxa27x_keyboard_platform_data palmt650_kbd_data = {
	.nr_rows = 8,
	.nr_cols = 7,
	.debounce_ms = 32,
	.keycodes = {
	{	/* row 0 */
		KEY_O,			/* "O" */
		KEY_LEFT,		/* "5-Way Left" */
		-1,			/* "Alternate" */
		KEY_L,			/* "L" */
		KEY_A,			/* "A" */
		KEY_Q,			/* "Q" */
		KEY_LEFTCTRL,	 	/* "Right Shift" */
	}, {	/* row 1 */
		KEY_P,			/* "P" */
		KEY_RIGHT,		/* "5-Way Right" */
		KEY_LEFTSHIFT,		/* "Left Shift" */
		KEY_Z,			/* "Z" */
		KEY_S,			/* "S" */
		KEY_W,			/* "W" */
		-1,			/* "Unused" */
	}, {	/* row 2 */
		-1,			/* "Phone" */
		KEY_UP,			/* "5-Way Up" */
		KEY_0,			/* "0" */
		KEY_X,			/* "X" */
		KEY_D,			/* "D" */
		KEY_E,			/* "E" */
		-1,			/* "Unused" */
	}, {	/* row 3 */
		KEY_F10,		/* "Calendar" */
		KEY_DOWN,		/* "5-Way Down" */
		KEY_SPACE,		/* "Space" */
		KEY_C,			/* "C" */
		KEY_F,			/* "F" */
		KEY_R,			/* "R" */
		-1,			/* "Unused" */
	}, {	/* row 4 */
		KEY_F12,		/* "Mail" */
		KEY_ENTER,		/* "5-Way Center" */
		KEY_F9,			/* "HOME" */
		KEY_V,			/* "V" */
		KEY_G,			/* "G" */
		KEY_T,			/* "T" */
		-1,			/* "Unused" */
	}, {	/* row 5 */
		KEY_F8,			/* "Off" */
		KEY_VOLUMEUP,		/* "Volume Up" */
		KEY_DOT,		/* "." */
		KEY_B,			/* "B" */
		KEY_H,			/* "H" */
		KEY_Y,			/* "Y" */
		-1, 			/* "Unused" */
	}, {	/* row 6 */
		KEY_F11,		/* "Mute" */
		KEY_VOLUMEDOWN,		/* "Volume Down" */
		KEY_ENTER,		/* "Return" */
		KEY_N,			/* "N" */
		KEY_J,			/* "J" */
		KEY_U,			/* "U" */
		-1,			/* "Unused" */
	}, {	/* row 7 */
		KEY_RIGHTALT,		/* "Alt" */
		-1,			/* "Unused" */
		KEY_BACKSPACE,		/* "P" */
		KEY_M,			/* "M"*/
		KEY_K,			/* "K"*/
		KEY_I,			/* "I"*/
		-1,			/* "Unused" */
	},

	},
	.gpio_modes = {
		 GPIO_NR_PALMT650_KP_MKIN0_MD,
		 GPIO_NR_PALMT650_KP_MKIN1_MD,
		 GPIO_NR_PALMT650_KP_MKIN2_MD,
		 GPIO_NR_PALMT650_KP_MKIN3_MD,
		 GPIO_NR_PALMT650_KP_MKIN4_MD,
		 GPIO_NR_PALMT650_KP_MKIN5_MD,
		 GPIO_NR_PALMT650_KP_MKIN6_MD,
		 GPIO_NR_PALMT650_KP_MKIN7_MD,
		 GPIO_NR_PALMT650_KP_MKOUT0_MD,
		 GPIO_NR_PALMT650_KP_MKOUT1_MD,
		 GPIO_NR_PALMT650_KP_MKOUT2_MD,
		 GPIO_NR_PALMT650_KP_MKOUT3_MD,
		 GPIO_NR_PALMT650_KP_MKOUT4_MD,
		 GPIO_NR_PALMT650_KP_MKOUT5_MD,
		 GPIO_NR_PALMT650_KP_MKOUT6_MD,
	 },
};

static struct platform_device palmt650_kbd = {
	.name	= "pxa27x-keyboard",
	.dev	= {
		.platform_data	= &palmt650_kbd_data,
	},
};

/*********************************************************
 * Machine initalisation
 *********************************************************/

static struct platform_device *devices[] __initdata = {
	&palmt650_ac97,
	&palmt650_pm,
	&palmt650_kbd,
	&palmt650_backlight,
	&palmt650_led,
	&palmt650_gsm,
};

/*********************************************************
 * LCD
 *********************************************************/

static struct pxafb_mode_info palmt650_lcd_mode = {
	/* pixclock is set by lccr3 below */
	.pixclock		= 0,	
	.xres			= 320,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	/* fixme: these are the margins PalmOS has set,
	 * 	   they seem to work but could be better.
	 */
	.left_margin		= 24,
	.right_margin		= 4,
	.upper_margin		= 8,
	.lower_margin		= 5,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info palmt650_lcd = {
	.lccr0 		= LCCR0_ENB     | LCCR0_Color    | LCCR0_Sngl | 
			  LCCR0_Act     | LCCR0_4PixMono | LCCR0_OUC  | 
			  LCCR0_LDDALT,   
	.lccr3 		= LCCR3_PixClkDiv(3) | LCCR3_HorSnchL | LCCR3_VrtSnchL |
			  LCCR3_PixFlEdg     | LCCR3_OutEnH   | LCCR3_Bpp(4),
	.num_modes	= 1,
	.modes		= &palmt650_lcd_mode,
};

static struct map_desc palmt650_io_desc[] = {
  	{	/* Devs */
		.virtual	= PALMT650_ASIC6_VIRT,
		.pfn		= __phys_to_pfn(PALMT650_ASIC6_PHYS),
		.length		= PALMT650_ASIC6_SIZE,
		.type		= MT_DEVICE
	},
};

static void __init palmt650_map_io(void)
{
	pxa_map_io();
	iotable_init(palmt650_io_desc, ARRAY_SIZE(palmt650_io_desc));
}

void palmt650_pxa_ll_pm_init(void);

static void __init palmt650_init(void)
{
	/* Disable PRIRDY interrupt to avoid hanging when loading AC97 */
	GCR &= ~GCR_PRIRDY_IEN;

	/* set AC97's GPIOs	*/
	pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
	pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
	pxa_gpio_mode(GPIO31_SYNC_AC97_MD);

	/* Other GPIOs */
	pxa_gpio_mode(GPIO_NR_PALMT650_USB_PULLUP_MD);

	set_pxa_fb_info(&palmt650_lcd);
	pxa_set_mci_info(&palmt650_mci_platform_data);
	pxa_set_ficp_info(&palmt650_ficp_platform_data);
	pxa_set_udc_info( &palmt650_udc_mach_info );

#ifdef CONFIG_PM
	palmt650_pxa_ll_pm_init();
#endif

	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(XSCALE_PALMTREO650, "Palm Treo 650")
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= palmt650_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= palmt650_init,
MACHINE_END
