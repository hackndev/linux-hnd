/*
 * linux/arch/arm/mach-pxa/palmld/palmld.c
 *
 *  Support for the Palm LifeDrive.
 *
 *  Author: Alex Osborne <bobofdoom@gmail.com> 2005-2006
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/gpio_keys.h>
#include <linux/corgi_bl.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/audio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/bitfield.h>
#include <asm/arch/irda.h>
#include <asm/arch/palmld-gpio.h>
#include <asm/arch/palmld-init.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/arch/pxa-pm_ll.h>
#include <asm/arch/serial.h>
#include <asm/arch/palmlcd-border.h>
#include <asm/arch/palm-battery.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include "../generic.h"

/*********************************************************
 * SD/MMC card controller
 *********************************************************/
 
static int palmld_mci_init(struct device *dev, irqreturn_t (*palmld_detect_int)(int, void *), void *data)
{
	int err;
	
	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);
	pxa_gpio_mode(GPIO_NR_PALMLD_SD_DETECT_N | GPIO_IN);

	/**
	 * Setup an interrupt for detecting card insert/remove events
	 */
	set_irq_type(IRQ_GPIO_PALMLD_SD_DETECT_N, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMLD_SD_DETECT_N, palmld_detect_int,
			SA_INTERRUPT, "SD/MMC card detect", data);
			
	if(err) {
		printk(KERN_ERR "palmld_mci_init: cannot request SD/MMC card detect IRQ\n");
		return -1;
	}
	
	
	printk("palmld_mci_init: irq registered\n");
	
	return 0;
}

static void palmld_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMLD_SD_DETECT_N, data);
}

static struct pxamci_platform_data palmld_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= palmld_mci_init,
	/* .setpower 	= palmld_mci_setpower,	*/
	.exit		= palmld_mci_exit,

};

/*********************************************************
 * Bluetooth
 *********************************************************/

void palmld_bt_reset(int on)
{
        printk(KERN_NOTICE "Switch BT reset %d\n", on);
        SET_PALMLD_GPIO( BT_RESET, on ? 1 : 0 );
}

void palmld_bt_power(int on)
{
        printk(KERN_NOTICE "Switch BT power %d\n", on);
        SET_PALMLD_GPIO( BT_POWER, on ? 1 : 0 );
}


struct bcm2035_bt_funcs {
        void (*configure) ( int state );
	void (*power) ( int state );
	void (*reset) ( int state );
};

static struct bcm2035_bt_funcs bt_funcs = {
	.reset = palmld_bt_reset,
	.power = palmld_bt_power,
};

static void
palmld_bt_configure( int state )
{
        if (bt_funcs.configure != NULL)
                bt_funcs.configure( state );
}

static struct platform_pxa_serial_funcs bcm2035_pxa_bt_funcs = {
        .configure = palmld_bt_configure,
};

static struct platform_device bcm2035_bt = {
        .name = "bcm2035-bt",
        .id = -1,
        .dev = {
                .platform_data = &bt_funcs,
        },
};

/*********************************************************
 * AC97 audio controller
 *********************************************************/

static pxa2xx_audio_ops_t palmld_audio_ops = {
	/*
	.startup	= palmld_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
	*/
};

static struct platform_device palmld_ac97 = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= { .platform_data = &palmld_audio_ops },
};

/*********************************************************
 * IRDA
 *********************************************************/

static void palmld_irda_transceiver_mode(struct device *dev, int mode)
{
        unsigned long flags;

        local_irq_save(flags);

        if (mode & IR_SIRMODE){
                printk (KERN_INFO "palmld_irda: setting mode to SIR\n");
        }
        else if (mode & IR_FIRMODE){
                printk (KERN_INFO "palmld_irda: setting mode to FIR\n");
        }
        if (mode & IR_OFF){
                printk (KERN_INFO "palmld_irda: turning tranceiver OFF\n");
                SET_PALMLD_GPIO(IR_DISABLE, 1);
        }
        else {
                printk (KERN_INFO "palmld_irda: turning tranceiver ON\n");
                SET_PALMLD_GPIO(IR_DISABLE, 0);
                SET_PALMLD_GPIO(ICP_TXD_MD, 1);
                mdelay(30);
                SET_PALMLD_GPIO(ICP_TXD_MD, 0);
        }

        local_irq_restore(flags);
}


static struct pxaficp_platform_data palmld_ficp_platform_data = {
        .transceiver_cap  = IR_SIRMODE | IR_FIRMODE | IR_OFF,
        .transceiver_mode = palmld_irda_transceiver_mode,
};

/*********************************************************
 * LEDs
 *********************************************************/
static struct platform_device palmldled_device = {
          .name           = "palmld-led",
          .id             = -1,
};
		  
/*********************************************************
 * Cypress EZUSB SX2 USB2.0 Controller
 *********************************************************/
static struct platform_device palmldusb2_device = {
        .name		= "sx2-udc",
        .id		= -1,
};
		  
/*********************************************************
 * LCD Border
 *********************************************************/
static struct palmlcd_border_pdata border_machinfo = {
    .select_gpio	= GPIO_NR_PALMLD_BORDER_SELECT,
    .switch_gpio	= GPIO_NR_PALMLD_BORDER_SWITCH,
};

struct platform_device palmld_border = {
	.name	= "palmlcd-border",
	.id	= -1,
	.dev	= {
		.platform_data = &border_machinfo,
	},
};

/*********************************************************
 * Battery
 *********************************************************/

int palmld_ac_is_connected (void){
	/* when charger is plugged in and USB is not connected,
	   then status is ONLINE */
	return ((GET_PALMLD_GPIO(POWER_DETECT)) &&
		!(GET_PALMLD_GPIO(USB_DETECT_N)));
}

static struct palm_battery_data palm_battery_info = {
	.bat_min_voltage	= PALMLD_BAT_MIN_VOLTAGE,
	.bat_max_voltage	= PALMLD_BAT_MAX_VOLTAGE,
	.bat_max_life_mins	= PALMLD_MAX_LIFE_MINS,
	.ac_connected		= &palmld_ac_is_connected,
};

EXPORT_SYMBOL_GPL(palm_battery_info);

/*********************************************************
 * Backlight
 *********************************************************/

static void palmld_bl_power(int on)
{
/*    SET_PALMLD_GPIO(BL_POWER, on); */ /* to be determined */
    pxa_set_cken(CKEN0_PWM0, on);
    pxa_set_cken(CKEN1_PWM1, on);
    mdelay(50);
}

static void palmld_set_bl_intensity(int intensity)
{
    palmld_bl_power(intensity ? 1 : 0);
    if(intensity) {
        PWM_CTRL0   = 0x7;
        PWM_PERVAL0 = PALMLD_PERIOD;
        PWM_PWDUTY0 = intensity;
    }
}

static struct corgibl_machinfo palmld_bl_machinfo = {
    .max_intensity	= PALMLD_MAX_INTENSITY,
    .default_intensity	= PALMLD_MAX_INTENSITY,
    .set_bl_intensity	= palmld_set_bl_intensity,
    .limit_mask		= PALMLD_LIMIT_MASK,
};

static struct platform_device palmld_backlight = {
    .name	= "corgi-bl",
    .id		= 0,
    .dev	= {
	    .platform_data = &palmld_bl_machinfo,
    },
};

/*********************************************************
 * Keypad
 *********************************************************/

static struct pxa27x_keyboard_platform_data palmld_kbd_data = {
	.nr_rows = 4,
	.nr_cols = 3,
	.keycodes = {
		{	/* row 0 */
			-1,		/* unused */
			KEY_F10,	/* Folder */
			KEY_UP,		/* Nav up */
		},
		{	/* row 1 */
			KEY_F11,	/* Picture */
			KEY_F12,	/* Star */
			KEY_RIGHT,	/* Nav right */
		},
		{	/* row 2 */
			KEY_F9,		/* Home */
			KEY_F7,		/* Voice memo */
			KEY_DOWN,	/* Nav down */
		},
		{
			/* row 3 */
			KEY_F6,		/* Rotate display */
			KEY_ENTER,	/* Nav centre */
			KEY_LEFT,	/* Nav left */
		},

	},
	.gpio_modes = {
		 GPIO_NR_PALMLD_KP_MKIN0_MD,
		 GPIO_NR_PALMLD_KP_MKIN1_MD,
		 GPIO_NR_PALMLD_KP_MKIN2_MD,
		 GPIO_NR_PALMLD_KP_MKIN3_MD,
		 GPIO_NR_PALMLD_KP_MKOUT0_MD,
		 GPIO_NR_PALMLD_KP_MKOUT1_MD,
		 GPIO_NR_PALMLD_KP_MKOUT2_MD,
	 },
};

static struct platform_device palmld_kbd = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &palmld_kbd_data,
	},
};

/*********************************************************
 * GPIO Key - Voice Memo Button *
 *********************************************************/
#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button palmld_pxa_buttons[] = {
        {KEY_F8, GPIO_NR_PALMLD_POWER_SWITCH, 0, "Power switch" },
        {KEY_F5, GPIO_NR_PALMLD_LOCK_SWITCH, 0, "Lock switch" },
};

static struct gpio_keys_platform_data palmld_pxa_keys_data = {
        .buttons = palmld_pxa_buttons,
        .nbuttons = ARRAY_SIZE(palmld_pxa_buttons),
};

static struct platform_device palmld_pxa_keys = {
        .name = "gpio-keys",
        .dev = {
                .platform_data = &palmld_pxa_keys_data,
        },
};
#endif

/*********************************************************
 * Power Management *
 *********************************************************/

struct platform_device palmld_pm = {
        .name = "palmld-pm",
        .id = -1,
        .dev = {
                .platform_data = NULL,
        },
};

/*********************************************************
 * IDE
 *********************************************************/


struct platform_device palmld_ide = {
	.name = "palmld-ide",
	.id = 0,
};

static struct platform_device *devices[] __initdata = {
	&palmld_kbd,
#ifdef CONFIG_KEYBOARD_GPIO
	&palmld_pxa_keys,
#endif
	&palmld_ac97,
	&palmld_ide,
	&palmld_backlight,
	&palmldled_device,
	&palmldusb2_device,
	&palmld_pm,
	&bcm2035_bt,
	&palmld_border,
};

/*********************************************************
 * LCD
 *********************************************************/

static struct pxafb_mode_info palmld_lcd_modes[] = {
{
	/* pixclock is set by lccr3 below */
	.pixclock		= 0,		
	.xres			= 320,
	.yres			= 480,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 1,

	.left_margin		= 32,
	.right_margin		= 1,
	.upper_margin		= 7, //5,
	.lower_margin		= 1, //3,

	.sync			= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
},
};

static struct pxafb_mach_info palmld_lcd_screen = {
	.modes			= palmld_lcd_modes,
	.num_modes		= ARRAY_SIZE(palmld_lcd_modes),
	.lccr0			= LCCR0_Color | LCCR0_Sngl | LCCR0_LDM   | LCCR0_SFM   |
				  LCCR0_IUM   | LCCR0_EFM  | LCCR0_Act   | LCCR0_QDM   |
				  LCCR0_BM    | LCCR0_OUM  | LCCR0_RDSTM | LCCR0_CMDIM |
				  LCCR0_OUC   | LCCR0_LDDALT,
	/* Set divisor to 2 to get rid of screen whining */
	.lccr3			= LCCR3_PixClkDiv(2) | LCCR3_HorSnchL | LCCR3_VrtSnchL |
				  LCCR3_PixFlEdg     | LCCR3_Bpp(4),
	
	.pxafb_backlight_power	= NULL,
};


static struct map_desc palmld_io_desc[] __initdata = {
  	{	/* Devs */
		.virtual	= PALMLD_USB_VIRT,
		.pfn		= __phys_to_pfn(PALMLD_USB_PHYS),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
	{
          .virtual	= PALMLD_IDE_VIRT,
          .pfn		= __phys_to_pfn(PALMLD_IDE_PHYS),
          .length	= 0x00100000,
          .type		= MT_DEVICE
        },
	{ /* pccard 1 attribute mem for debugging */
          .virtual	= 0xe0000000,
	  .pfn		= __phys_to_pfn(0x38000000),
          .length	= 0x00100000,
          .type		= MT_DEVICE
        },

//  { 0XF0000000, 0x08000000, 0x00100000, MT_DEVICE }, /* PCCard 0 */
//  { 0xe0000000, 0x20000000, 0x00100000, MT_DEVICE }, /* PCCard 0 */
  
  //{ PALMLD_USB_VIRT, PALMLD_USB_PHYS, PALMLD_USB_SIZE, MT_DEVICE }, /* EZ-USB SX2 (CS2) */
};

static void __init palmld_map_io(void)
{
	pxa_map_io();
	
	iotable_init(palmld_io_desc, ARRAY_SIZE(palmld_io_desc));
	
        /* Clear reset status */
        //RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;
}

static void __init palmld_init(void)
{
	// disable interrupt to prevent WM9712 constantly interrupting the CPU
	// and preventing the boot process to complete (Thanx Alex & Shadowmite!)

	GCR &= ~GCR_PRIRDY_IEN;

	// set AC97's GPIOs

	pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
	pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
	pxa_gpio_mode(GPIO31_SYNC_AC97_MD);
									
	set_pxa_fb_info( &palmld_lcd_screen );
	pxa_set_btuart_info(&bcm2035_pxa_bt_funcs);
	pxa_set_mci_info( &palmld_mci_platform_data );
	platform_add_devices( devices, ARRAY_SIZE(devices) );
	pxa_set_ficp_info( &palmld_ficp_platform_data );
}

MACHINE_START(XSCALE_PALMLD, "Palm LifeDrive")
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= palmld_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= palmld_init,
MACHINE_END

