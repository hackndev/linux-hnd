/****************************************************************
 * Hardware definitions for PalmOne Zire 72			*
 *								*
 * Authors:							*
 * 	Vladimir "Farcaller" Pouzanov <farcaller@gmail.com>	*
 * 	Sergey Lapin <slapinid@gmail.com> 			*
 *      Alex Osborne <bobofdoom@gmail.com>			*
 *	Jan Herman   <2hp@seznam.cz>				*
 ****************************************************************/

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
#include <linux/irq.h>
#include <linux/delay.h>

#include <media/ov96xx.h>
#include <asm/arch/audio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/pxa-pm_ll.h>
#include <linux/corgi_bl.h>
#include <asm/arch/serial.h>
#include <linux/gpio_keys.h>

#include <asm/arch/udc.h>
#include <asm/arch/irda.h>
#include <asm/arch/mmc.h>
#include <asm/arch/palmz72-gpio.h>
#include <asm/arch/palmz72-init.h>
#include <asm/arch/pxa_camera.h>
#include <asm/arch/pxa2xx_udc_gpio.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include "../generic.h"

#define DEBUG

/**************************
 * SD/MMC card controller *
 **************************/
 
static int palmz72_mci_init(struct device *dev, irqreturn_t (*palmz72_detect_int)(int, void *), void *data)
{
	int err;
	
	/*
	 * Setup an interrupt for detecting card insert/remove events
	 */
	set_irq_type(IRQ_GPIO_PALMZ72_SD_DETECT_N, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMZ72_SD_DETECT_N, palmz72_detect_int,
			SA_INTERRUPT, "SD/MMC card detected", data);
			
	if(err) {
		printk(KERN_ERR "palmz72_mci_init: cannot request SD/MMC card detect IRQ\n");
		return -1;
	}
	
	printk("palmz72_mci_init: irq registered\n");
	
	return 0;
}

static void palmz72_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMZ72_SD_DETECT_N, data);
}

static struct pxamci_platform_data palmz72_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= palmz72_mci_init,
	/* .setpower 	= palmz72_mci_setpower,	*/
	.exit		= palmz72_mci_exit,

};

/*******
 * LED *
 *******/

static struct platform_device palmz72_led_device = {
        .name           = "palmz72-led",
	.id             = -1,
};

/*******
 * USB *
 *******/

static int palmz72_udc_power( int power )
{
	if (power) {
		SET_GPIO(GPIO_NR_PALMZ72_USB_POWER, 1);
		SET_GPIO(GPIO_NR_PALMZ72_USB_PULLUP, 1);
	} else {
		SET_GPIO(GPIO_NR_PALMZ72_USB_PULLUP, 0);
		SET_GPIO(GPIO_NR_PALMZ72_USB_POWER, 0);
	}
	
	return power;
}

struct pxa2xx_udc_gpio_info palmz72_udc_info = {
	.detect_gpio		= {&pxagpio_device.dev, GPIO_NR_PALMZ72_USB_DETECT},
	.detect_gpio_negative	= 1,
	.power_ctrl		= {
		.power		= &palmz72_udc_power,
	},
};

static struct platform_device palmz72_udc = {
	.name	= "pxa2xx-udc-gpio",
	.dev	= {
		.platform_data	= &palmz72_udc_info
	}
};

/**********
 * Keypad *
 **********/

static struct pxa27x_keyboard_platform_data palmz72_kbd_data = {
	.nr_rows = 4,
	.nr_cols = 3,
	.keycodes = {
		{	/* row 0 */
			KEY_F8,			/* Power key 	*/
			KEY_F11,		/* Photos  	*/
			KEY_ENTER,		/* DPAD Center	*/
		},
		{	/* row 1 */
			KEY_F9,			/* Calendar	*/
			KEY_F10,		/* Contacts 	*/
			KEY_F12,		/* Media 	*/
		},
		{	/* row 2 */
			KEY_UP,			/* D-PAD UP	*/
			0,			/* unused 	*/
			KEY_DOWN,		/* D-PAD DOWN 	*/
		},
		{
			/* row 3 */
			KEY_RIGHT,		/* D-PAD RIGHT 	*/
			0,			/* unused 	*/
			KEY_LEFT,		/* D-PAD LEFT 	*/
		},
	},
	.gpio_modes = {
		 GPIO_NR_PALMZ72_KP_MKIN0_MD,
		 GPIO_NR_PALMZ72_KP_MKIN1_MD,
		 GPIO_NR_PALMZ72_KP_MKIN2_MD,
		 GPIO_NR_PALMZ72_KP_MKIN3_MD,	 
		 GPIO_NR_PALMZ72_KP_MKOUT0_MD,
		 GPIO_NR_PALMZ72_KP_MKOUT1_MD,
		 GPIO_NR_PALMZ72_KP_MKOUT2_MD,

	 },
};

static struct platform_device palmz72_keypad = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &palmz72_kbd_data,
	},
};

/********************************
 * GPIO Key - Voice Memo Button *
 ********************************/
#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button palmz72_pxa_buttons[] = {
	{KEY_F7, GPIO_NR_PALMZ72_KP_DKIN7, 0, "Voice Memo Button" },
};

static struct gpio_keys_platform_data palmz72_pxa_keys_data = {
	.buttons = palmz72_pxa_buttons,
	.nbuttons = ARRAY_SIZE(palmz72_pxa_buttons),
};

static struct platform_device palmz72_pxa_keys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &palmz72_pxa_keys_data,
	},
};
#endif

/********
 * IRDA *
 ********/

static void palmz72_irda_transceiver_mode(struct device *dev, int mode)
{
        unsigned long flags;
	
        local_irq_save(flags);

        if (mode & IR_SIRMODE){
                printk (KERN_INFO "IRDA: setting mode to SIR\n");
        }
        else if (mode & IR_FIRMODE){
                printk (KERN_INFO "IRDA: setting mode to FIR\n");
        }
        if (mode & IR_OFF){
                printk (KERN_INFO "IRDA: turning OFF\n");
                SET_PALMZ72_GPIO(IR_SD, 1);
        }
        else {
                printk (KERN_INFO "IRDA: turning ON\n");
                SET_PALMZ72_GPIO(IR_SD, 0);
                SET_PALMZ72_GPIO(ICP_TXD_MD, 1);
                mdelay(30);
                SET_PALMZ72_GPIO(ICP_TXD_MD, 0);
        }

        local_irq_restore(flags);
}


static struct pxaficp_platform_data palmz72_ficp_platform_data = {
        .transceiver_cap  = IR_SIRMODE | IR_FIRMODE | IR_OFF,
        .transceiver_mode = palmz72_irda_transceiver_mode,
};

/*************
 * Bluetooth * 
 *************/

void bcm2035_bt_reset(int on)
{
	printk(KERN_NOTICE "Switch BT reset %d\n", on);
	if (on)
		SET_PALMZ72_GPIO( BT_RESET, 1 );
	else
		SET_PALMZ72_GPIO( BT_RESET, 0 );
}
EXPORT_SYMBOL(bcm2035_bt_reset);

void bcm2035_bt_power(int on)
{
	printk(KERN_NOTICE "Switch BT power %d\n", on);
	if (on)
		SET_PALMZ72_GPIO( BT_POWER, 1 );
	else
		SET_PALMZ72_GPIO( BT_POWER, 0 );
}
EXPORT_SYMBOL(bcm2035_bt_power);


struct bcm2035_bt_funcs {
	void (*configure) ( int state );
};

static struct bcm2035_bt_funcs bt_funcs;

static void
bcm2035_bt_configure( int state )
{
	if (bt_funcs.configure != NULL)
		bt_funcs.configure( state );
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

/*************************
 * AC97 audio controller *
 *************************/


static pxa2xx_audio_ops_t palmz72_audio_ops = {
/*	
	.startup	= palmz72_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
*/	
};



static struct platform_device palmz72_ac97 = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= { .platform_data = &palmz72_audio_ops },
};

/**************
 * LCD Border *
 **************/
struct platform_device palmz72_border = {
	.name = "palmz72-border",
	.id = -1,
};

/*************
 * Backlight *
 *************/

static void palmz72_bl_power(int on)
{
	SET_PALMZ72_GPIO(BL_POWER, 1);
	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_set_cken(CKEN1_PWM1, 1);
}

static void palmz72_set_bl_intensity(int intensity)
{
	palmz72_bl_power(intensity ? 1 : 0);
	if(intensity) {
	    PWM_CTRL0   = 0x3f;
	    PWM_PERVAL0 = PALMZ72_PERIOD;
	    PWM_PWDUTY0 = intensity;
	}
}

static struct corgibl_machinfo palmz72_bl_machinfo = {
	.max_intensity		= PALMZ72_MAX_INTENSITY,
	.default_intensity	= PALMZ72_MAX_INTENSITY,
	.set_bl_intensity	= palmz72_set_bl_intensity,
	.limit_mask		= PALMZ72_LIMIT_MASK,
};

static struct platform_device palmz72_backlight = {
	.name		= "corgi-bl",
	.id		= 0,
	.dev		= {
		    .platform_data = &palmz72_bl_machinfo,
	},
};

/***************
 * framebuffer *
 ***************/

static struct pxafb_mode_info palmz72_lcd_modes[] = {
    {
	/* Dump at 94000000 to 9400000c
	   LCCR1: 1b070d3f:	1b = 27 = left_margin
				07 = 7  = right_margin
				0d = 13 = hsync_lenght
	   LCCR2: 0708013f:	07 = 7  = upper_margin
				08 = 8  = lower_margin
				01 = 1  = vsync_lenght
	*/
			    
	.pixclock		= 0,
	.xres			= 320,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 6,  /* This value is optimized for older problematic LCD panels */
	.left_margin		= 27,
	.right_margin		= 7,
	.vsync_len		= 1,
	.upper_margin		= 7,  /* This value is optimized for older problematic LCD panels */
	.lower_margin		= 8,  /* This value is optimized for older problematic LCD panels */
    }
};

static struct pxafb_mach_info palmz72_lcd_screen = {
	.modes			= palmz72_lcd_modes,
	.num_modes		= ARRAY_SIZE(palmz72_lcd_modes),
	.lccr0 = 0x07B008F9,
	.lccr3 = 0x03700007,  
	.pxafb_backlight_power	= NULL,
};

/********************
 * Power Management *
 ********************/

struct platform_device palmz72_pm = {
	.name = "palmz72-pm",
	.id = -1,
	.dev = {
		.platform_data = NULL,
		},
};

/*
 * Camera: OV9640 sensor on PXA quick capture interface
 */

struct ov96xx_platform_data ov9640_data = {
	.reset_gpio = 34,
	.power_gpio = 32,
};
#if 0
static struct platform_device ov9640 = {
	.name	= "ov96xx",
	.id	= -1,
	.dev = {
		.platform_data = &ov9640_data,
	},
};
#endif

static struct platform_device palmz72_ci = {
	.name		= "pxacif",
	.id		= -1,
	.dev		= {
		    .platform_data = NULL,
	},
};

/****************
 * Init Machine *
 ****************/

static struct platform_device *devices[] __initdata = {
	&palmz72_ac97,
	&palmz72_pm,
	&palmz72_backlight,
	&palmz72_keypad,
#ifdef CONFIG_KEYBOARD_GPIO
	&palmz72_pxa_keys,
#endif
	&palmz72_led_device,
	&bcm2035_bt,
	&palmz72_ci,
#if 0
	&ov9640,
#endif
	&palmz72_border,
	&palmz72_udc,
};

/***********************************************************************
 *
 * OV9640 Functions
 *
 ***********************************************************************/
static void ov9640_gpio_init(void)
{
	pxa_gpio_mode(81 | GPIO_ALT_FN_2_IN);	/* CIF_DD[0] */
	pxa_gpio_mode(55 | GPIO_ALT_FN_1_IN);	/* CIF_DD[1] */
	pxa_gpio_mode(51 | GPIO_ALT_FN_1_IN);	/* CIF_DD[2] */
	pxa_gpio_mode(50 | GPIO_ALT_FN_1_IN);	/* CIF_DD[3] */
	pxa_gpio_mode(52 | GPIO_ALT_FN_1_IN);	/* CIF_DD[4] */
	pxa_gpio_mode(48 | GPIO_ALT_FN_1_IN);	/* CIF_DD[5] */
	pxa_gpio_mode(93 | GPIO_ALT_FN_1_IN);	/* CIF_DD[6] */
	pxa_gpio_mode(108 | GPIO_ALT_FN_1_IN);	/* CIF_DD[7] */
	pxa_gpio_mode(53 | GPIO_ALT_FN_2_OUT);	/* CIF_MCLK */
	pxa_gpio_mode(54 | GPIO_ALT_FN_3_IN);	/* CIF_PCLK */
	pxa_gpio_mode(85 | GPIO_ALT_FN_3_IN);	/* CIF_LV */
	pxa_gpio_mode(84 | GPIO_ALT_FN_3_IN);	/* CIF_FV */
	
#if 0
	pxa_gpio_mode(32 | GPIO_OUT);
	pxa_gpio_mode(34 | GPIO_OUT);

	set_GPIO_mode(50 | GPIO_OUT);	/*CIF_PD */
	set_GPIO_mode(19 | GPIO_IN);	/*CIF_RST */
#endif
}
/***********************************************************************
 *
 * end of OV9640 Functions
 *
 ***********************************************************************/

static void __init palmz72_init(void)
{
	// disable interrupt to prevent WM9712 constantly interrupting the CPU
	// and preventing the boot process to complete (Thanx Alex & Shadowmite!)

	GCR &= ~GCR_PRIRDY_IEN;

	// set AC97's GPIOs

	pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
	pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
	pxa_gpio_mode(GPIO31_SYNC_AC97_MD);

	ov9640_gpio_init();
		
	switch(palmz72_lcd_modes[0].bpp)
	{
		case 8:
			palmz72_lcd_screen.lccr3=0x03700007;
			break;
		case 16:
			/* right value for lcc3 is 0x04700007, but little change reducing display whine */
			palmz72_lcd_screen.lccr3=0x04700004;
			break;
	}
	set_pxa_fb_info( &palmz72_lcd_screen );
	pxa_set_btuart_info(&bcm2035_pxa_bt_funcs);
	pxa_set_mci_info( &palmz72_mci_platform_data );
	pxa_set_ficp_info( &palmz72_ficp_platform_data );
        platform_add_devices( devices, ARRAY_SIZE(devices) );
}

MACHINE_START(PALMZ72, "Palm Zire 72")

	/********************************************************/
 	/* Maintainers: Vladimir Pouzanov <farcaller@gmail.com> */
	/* 		Sergey Lapin <slapinid@gmail.com> 	*/
	/* 		Jan Herman <2hp@seznam.cz> 		*/
	/********************************************************/

	.boot_params	= 0xa0000100,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmz72_init
MACHINE_END

