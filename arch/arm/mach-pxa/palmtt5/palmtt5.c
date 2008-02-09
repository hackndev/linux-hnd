/*
 * $Id: palmtt5.c 1074 2007-06-26 07:01:26Z marex_z71 $
 * 
 * Hardware definitions for Palm T5
 *
 * Authors: Marek Vasut <marek.vasut@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (find more info at www.hackndev.com) 
 * 
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/palm_keys.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/corgi_bl.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/palmtt5-init.h>
#include <asm/arch/palmtt5-gpio.h>
#include <asm/arch/audio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/irda.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/serial.h>
#include <asm/arch/palm-battery.h>

#include "../generic.h"

/**************************
 * SD/MMC card controller *
 **************************/

static int palmtt5_mci_init(struct device *dev, irqreturn_t (*palmtt5_detect_int)(int, void *), void *data)
{
	int err;
	
	/**
	 * Setup GPIOs for MMC/SD card controller.
	 */
	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);
		
	/**
	 * Setup an interrupt for detecting card insert/remove events
	 */
	set_irq_type(IRQ_GPIO_PALMTT5_SD_DETECT_N, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMTT5_SD_DETECT_N, palmtt5_detect_int,
			SA_INTERRUPT, "SD/MMC card detect", data);
			
	if(err) {
		printk(KERN_ERR "palmtt5_mci_init: cannot request SD/MMC card detect IRQ\n");
		return -1;
	}
	
	printk("palmtt5_mci_init: irq registered\n");
	
	return 0;
}


static void palmtt5_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMTT5_SD_DETECT_N, data);
}


static struct pxamci_platform_data palmtt5_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= palmtt5_mci_init,
	/* .setpower 	= palmtt5_mci_setpower,	*/
	.exit		= palmtt5_mci_exit,

};


/**********
 * Keypad *
 **********/
 
static struct pxa27x_keyboard_platform_data palmtt5_kbd_data = {
	.nr_rows = 4,
	.nr_cols = 3,
	.keycodes = {
		{	/* row 0 */
			PALM_KEY_POWER,
			PALM_KEY_HOME,
			PALM_KEY_DPAD_CENTER,
		},
		{	/* row 1 */
			PALM_KEY_CALENDAR,
			PALM_KEY_CONTACTS,
			PALM_KEY_FOLDER,
		},
		{	/* row 2 */
			PALM_KEY_DPAD_UP,
			-1,
			PALM_KEY_DPAD_DOWN,
		},
		{
			/* row 3 */
			PALM_KEY_DPAD_RIGHT,
			-1,
			PALM_KEY_DPAD_LEFT,
		},

	},
	.gpio_modes = {
		 GPIO_NR_PALMTT5_KP_MKIN0_MD,
		 GPIO_NR_PALMTT5_KP_MKIN1_MD,
		 GPIO_NR_PALMTT5_KP_MKIN2_MD,
		 GPIO_NR_PALMTT5_KP_MKIN3_MD,
		 GPIO_NR_PALMTT5_KP_MKOUT0_MD,
		 GPIO_NR_PALMTT5_KP_MKOUT1_MD,
		 GPIO_NR_PALMTT5_KP_MKOUT2_MD,
	 },
};

static struct platform_device palmtt5_keypad = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &palmtt5_kbd_data,
	},
};

/*************
 * Battery   *
 *************/

int palmtt5_ac_is_connected (void){
	/* when charger is plugged in, then status is ONLINE */
	return GET_PALMTT5_GPIO(POWER_DETECT)||(!GET_PALMTT5_GPIO(USB_DETECT));
}

static struct palm_battery_data palm_battery_info = {
	.bat_min_voltage	= PALMTT5_BAT_MIN_VOLTAGE,
	.bat_max_voltage	= PALMTT5_BAT_MAX_VOLTAGE,
	.bat_max_life_mins	= PALMTT5_MAX_LIFE_MINS,
	.ac_connected		= &palmtt5_ac_is_connected,
};

EXPORT_SYMBOL_GPL(palm_battery_info);



/*************
 * Backlight *
 *************/

static void palmtt5_bl_power(int on)
{
	if(GET_PALMTT5_GPIO(BL_POWER)!=on) {
		SET_PALMTT5_GPIO(LCD_POWER, on);
		SET_PALMTT5_GPIO(BL_POWER, on);
		pxa_set_cken(CKEN0_PWM0, on);
		pxa_set_cken(CKEN1_PWM1, on);
    	}
}

static void palmtt5_set_bl_intensity(int intensity)
{
    palmtt5_bl_power(intensity ? 1 : 0);
    if(intensity) {
        PWM_CTRL0   = 0x7;
        PWM_PERVAL0 = PALMTT5_PERIOD;
        PWM_PWDUTY0 = intensity;
    }
}

static struct corgibl_machinfo palmtt5_bl_machinfo = {
    .max_intensity      = PALMTT5_MAX_INTENSITY,
    .default_intensity  = PALMTT5_MAX_INTENSITY,
    .set_bl_intensity   = palmtt5_set_bl_intensity,
    .limit_mask         = PALMTT5_LIMIT_MASK,
};

static struct platform_device palmtt5_backlight = {
    .name       = "corgi-bl",
    .id         = 0,
    .dev        = {
            .platform_data = &palmtt5_bl_machinfo,
    },
};


/********
 * IRDA *
 ********/

static void palmtt5_irda_transceiver_mode(struct device *dev, int mode)
{
        unsigned long flags;

        local_irq_save(flags);

        if (mode & IR_SIRMODE){
		printk (KERN_INFO "IrDA: setting mode to SIR\n");
	} 
	else if (mode & IR_FIRMODE){
		printk (KERN_INFO "IrDA: setting mode to FIR\n");
	}
	if (mode & IR_OFF){
		printk (KERN_INFO "IrDA: turning OFF\n");
		SET_GPIO(GPIO_NR_PALMTT5_IR_DISABLE, 1);
	}
	else {
		printk (KERN_INFO "IrDA: turning ON\n");
		SET_GPIO(GPIO_NR_PALMTT5_IR_DISABLE, 0);
		SET_GPIO(GPIO_NR_PALMTT5_ICP_TXD_MD, 1);
		mdelay(30);
		SET_GPIO(GPIO_NR_PALMTT5_ICP_TXD_MD, 0);	
	}

        local_irq_restore(flags);
}


static struct pxaficp_platform_data palmtt5_ficp_platform_data = {
        .transceiver_cap  = IR_SIRMODE | IR_FIRMODE | IR_OFF,
        .transceiver_mode = palmtt5_irda_transceiver_mode,
};

/*******
 * USB *
 *******/
 
static int palmtt5_udc_is_connected (void){
	int ret = !(GET_GPIO(GPIO_NR_PALMTT5_USB_DETECT));
	if (ret)
		printk (KERN_INFO "palmtt5_udc: device detected [USB_DETECT: %d]\n",ret);
	else
		printk (KERN_INFO "palmtt5_udc: no device detected [USB_DETECT: %d]\n",ret);
		
	return ret;
}


static void palmtt5_udc_command (int cmd){
	
	switch (cmd) {
		case PXA2XX_UDC_CMD_DISCONNECT:
			SET_GPIO(GPIO_NR_PALMTT5_USB_PULLUP, 0);
			SET_GPIO(GPIO_NR_PALMTT5_USB_POWER, 0);
			printk(KERN_INFO "palmtt5_udc: got command PXA2XX_UDC_CMD_DISCONNECT\n");
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			SET_GPIO(GPIO_NR_PALMTT5_USB_POWER, 1);
			SET_GPIO(GPIO_NR_PALMTT5_USB_PULLUP, 1);
			printk(KERN_INFO "palmtt5_udc: got command PXA2XX_UDC_CMD_CONNECT\n");
			break;
	default:
			printk("palmtt5_udc: unknown command '%d'\n", cmd);
	}
}


static struct pxa2xx_udc_mach_info palmtt5_udc_mach_info __initdata = {
	.udc_is_connected = palmtt5_udc_is_connected,
	.udc_command      = palmtt5_udc_command,
};

/* Bluetooth */

void bcm2035_bt_reset(int on)
{
        printk(KERN_NOTICE "Switch BT reset %d\n", on);
        if (on)
                SET_PALMTT5_GPIO( BT_RESET, 1 );
        else
                SET_PALMTT5_GPIO( BT_RESET, 0 );
}
EXPORT_SYMBOL(bcm2035_bt_reset);

void bcm2035_bt_power(int on)
{
        printk(KERN_NOTICE "Switch BT power %d\n", on);
        if (on)
                SET_PALMTT5_GPIO( BT_POWER, 1 );
        else
                SET_PALMTT5_GPIO( BT_POWER, 0 );
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

static pxa2xx_audio_ops_t palmtt5_audio_ops = {
	/*
	.startup  = palmtt5_audio_startup,
	.shutdown = mst_audio_shutdown,
	.suspend  = mst_audio_suspend,
	.resume   = mst_audio_resume,
	*/
};

static struct platform_device palmtt5_ac97 = {
	.name = "pxa2xx-ac97",
	.id   = -1,
	.dev  = { .platform_data = &palmtt5_audio_ops },
};

/********************
 * Power Management *
 ********************/

struct platform_device palmtt5_pm = {
	.name = "palmtt5-pm",
	.id = -1,
	.dev = {
		.platform_data = NULL,
		},
};

static struct platform_device *devices[] __initdata = {
         &palmtt5_keypad, 
	 &palmtt5_ac97,
	 &palmtt5_pm,
	 &palmtt5_backlight,
	 &bcm2035_bt,
};

/***************
 * framebuffer *
 ***************/
 
/* 
 * framebuffer settings as from palmos: 
 *
 * LCCR1: 0x1f030d3f	[ BLW=0x1f, ELW=0x3, HSW=0x3, PPL=0x13f ]
 * LCCR2: 0x070801df	[ BFW=0x7,  EFW=0x8, VSW=0x0, LPP=0x1df ]
 *
 * the fields marked with * are custom values that seem to fix a
 * persistent buzz of the TS present with palmos too
 *
 */

static struct pxafb_mode_info palmtt5_lcd_modes[] = {
    {
	.pixclock		= 0,	
	.bpp			= 16,	// BPP

#ifndef CONFIG_PALMTT5_DISABLE_BORDER
	.xres			= 320,
	.yres			= 480,
	.left_margin		= 32,
	.right_margin		= 1,
	.upper_margin		= 7,
	.lower_margin		= 1,
	.hsync_len		= 4,
	.vsync_len		= 1,
#else
	.xres			= 324,
	.yres			= 484,
	.left_margin		= 28,
	.right_margin		= 8,
	.upper_margin		= 3,
	.lower_margin		= 6,
	.hsync_len		= 6,
	.vsync_len		= 3,
	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
#endif
    },
};

static struct pxafb_mach_info palmtt5_lcd_screen = {
	.modes                  = palmtt5_lcd_modes,
	.num_modes              = ARRAY_SIZE(palmtt5_lcd_modes),
        .lccr0                  = LCCR0_Color | LCCR0_Sngl | LCCR0_LDM   | LCCR0_SFM   |
                                  LCCR0_IUM   | LCCR0_EFM  | LCCR0_Act   | LCCR0_QDM   |
                                  LCCR0_BM    | LCCR0_OUM  | LCCR0_RDSTM | LCCR0_CMDIM |
                                  LCCR0_OUC   | LCCR0_LDDALT,
        /* Set divisor to 2 to get rid of screen whining */
        .lccr3                  = LCCR3_PixClkDiv(2) | LCCR3_HorSnchL | LCCR3_VrtSnchL |
                                  LCCR3_PixFlEdg     | LCCR3_Bpp(4),
	.pxafb_backlight_power	= NULL,
};

static struct map_desc palmtt5_io_desc[] __initdata = {
	// PCMCIA socket 0
	{
		.virtual	= 0xf1000000,
		.pfn		= __phys_to_pfn(0x20000000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
};
	

static void __init palmtt5_map_io(void)
{
	pxa_map_io();
			
	iotable_init(palmtt5_io_desc, ARRAY_SIZE(palmtt5_io_desc));
}

/****************
 * Init Machine *
 ****************/

static void __init palmtt5_init(void)
{
	// disable primary codec interrupt to prevent WM9712 constantly interrupting the CPU
	// and preventing the boot process to complete (Thanx Alex & Shadowmite!)
	GCR &= ~GCR_PRIRDY_IEN;

	// configure AC97's GPIOs
	pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
	pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
	pxa_gpio_mode(GPIO31_SYNC_AC97_MD);                                 
	
	set_pxa_fb_info   ( &palmtt5_lcd_screen );
	pxa_set_btuart_info(&bcm2035_pxa_bt_funcs);
	pxa_set_mci_info  ( &palmtt5_mci_platform_data );
	pxa_set_udc_info  ( &palmtt5_udc_mach_info );
	pxa_set_ficp_info ( &palmtt5_ficp_platform_data );	

	platform_add_devices( devices, ARRAY_SIZE(devices) );
}



MACHINE_START(XSCALE_PALMTT5, "Palm Tungsten T5")
	.phys_io	= PALMTT5_PHYS_IO_START,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io         = palmtt5_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmtt5_init
MACHINE_END
