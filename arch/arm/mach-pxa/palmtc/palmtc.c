/*
 * Hardware definitions for Palm Tungsten C
 *
 * Based on Palm Tungsten T|3 patch
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/arch/audio.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>
#include <asm/arch/mmc.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/corgi_bl.h>

#include <asm/arch/pxa2xx_udc_gpio.h>
#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/palmtc-gpio.h>
#include <asm/arch/palmtc-init.h>
#include <asm/arch/udc.h>

#include "../generic.h"

#define DEBUG

static struct pxafb_mode_info palmtc_lcd_modes[] = {
{
        /* pixclock is set by lccr3 below */
        .pixclock               = 0,
        .xres                   = 320,
        .yres                   = 320,
        .bpp                    = 16,
        .hsync_len              = 6,
        .vsync_len              = 1,

        /* fixme: these are the margins PalmOS has set,
          *        they seem to work but could be better.
          */
        .left_margin            = 27,
        .right_margin           = 7,
        .upper_margin           = 7,
        .lower_margin           = 8,
},
};

static struct pxafb_mach_info palmtc_lcd_screen = {
        .modes                  = palmtc_lcd_modes,
        .num_modes              = ARRAY_SIZE(palmtc_lcd_modes),
	.lccr0			= 0x07B008F9,
	.lccr3			= 0x04700003,

	.pxafb_backlight_power	= NULL,
};

static int palmtc_mci_init(struct device *dev, irqreturn_t (*palmtc_detect_int)(int, void *), void *data)
{
	int err;
	
	/**
	 * Setup an interrupt for detecting card insert/remove events
	 */
	set_irq_type(IRQ_GPIO_PALMTC_SD_DETECT, IRQT_BOTHEDGE);
	err = request_irq(IRQ_GPIO_PALMTC_SD_DETECT, palmtc_detect_int,
			SA_INTERRUPT, "SD/MMC card detect", data);
			
	if(err) {
		printk(KERN_ERR "palmld_mci_init: cannot request SD/MMC card detect IRQ\n");
		return -1;
	}
	
	
	printk("palmtc_mci_init: irq registered\n");
	
	return 0;
}

static void palmtc_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO_PALMTC_SD_DETECT, data);
}

static struct pxamci_platform_data palmtc_mci_platform_data = {
	.ocr_mask	= (MMC_VDD_32_33 | MMC_VDD_33_34),
	.init 		= palmtc_mci_init,
/*	.setpower 	= palmld_mci_setpower, */
	.exit		= palmtc_mci_exit,

};

/* USB gadget */
static int palmtc_udc_is_connected(void)
{
        int ret = GET_PALMTC_GPIO(USB_DETECT);
        if (ret)
            printk (KERN_INFO "palmtc_udc: device detected [USB_DETECT: %d]\n",ret);
        else
            printk (KERN_INFO "palmtc_udc: no device detected [USB_DETECT: %d]\n",ret);

        return ret;

}

static void palmtc_udc_command (int cmd)
{
    	switch (cmd) {
	    case PXA2XX_UDC_CMD_DISCONNECT:
		SET_PALMTC_GPIO(USB_POWER, 0);
		printk(KERN_INFO "palmtc_udc: got command PXA2XX_UDC_CMD_DISCONNECT\n");
		break;
	    case PXA2XX_UDC_CMD_CONNECT:
        	SET_PALMTC_GPIO(USB_POWER, 1);
                printk(KERN_INFO "palmtc_udc: got command PXA2XX_UDC_CMD_CONNECT\n");
                break;
	    default:
    		printk("palmtc_udc: unknown command '%d'\n", cmd);
	}
}

static struct pxa2xx_udc_mach_info palmtc_udc_mach_info __initdata = {
                .udc_is_connected = palmtc_udc_is_connected,
                .udc_command      = palmtc_udc_command,
};

/* Backlight ***/
static void palmtc_bl_power(int on)
{
/*    SET_PALMTC_GPIO(BL_POWER, on); */ /* to be determined */
    pxa_set_cken(CKEN0_PWM0, on);
    pxa_set_cken(CKEN1_PWM1, on);
    mdelay(50);
}

static void palmtc_set_bl_intensity(int intensity)
{
    palmtc_bl_power(intensity ? 1 : 0);
    if(intensity) {
        PWM_CTRL1   = 0x7;
        PWM_PERVAL1 = PALMTC_PERIOD;
	PWM_PWDUTY1 = intensity;
    }
}

static struct corgibl_machinfo palmtc_bl_machinfo = {
    .max_intensity	= PALMTC_MAX_INTENSITY,
    .default_intensity	= PALMTC_MAX_INTENSITY,
    .set_bl_intensity	= palmtc_set_bl_intensity,
    .limit_mask		= PALMTC_LIMIT_MASK,
};
 
static struct platform_device palmtc_backlight = {
    .name	= "corgi-bl",
    .id		= 0,
    .dev	= {
	    .platform_data = &palmtc_bl_machinfo,
    },
};

static pxa2xx_audio_ops_t palmtc_audio_ops = {
	/*
	.startup	= palmld_audio_startup,
	.shutdown	= mst_audio_shutdown,
	.suspend	= mst_audio_suspend,
	.resume		= mst_audio_resume,
	*/
};

static struct platform_device palmtc_ac97_device = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= {
	    .platform_data	= &palmtc_audio_ops
	},
};

static struct platform_device palmtc_keyboard_device = {
	.name		= "palmtc-kbd",
	.id		= -1,
};


static struct platform_device *devices[] __initdata = {
	&palmtc_ac97_device,
	&palmtc_keyboard_device,
	&palmtc_backlight,
};

static void __init palmtc_init(void)
{
	set_pxa_fb_info( &palmtc_lcd_screen );
	GCR &= ~GCR_PRIRDY_IEN;

	pxa_set_mci_info( &palmtc_mci_platform_data );
        pxa_set_udc_info( &palmtc_udc_mach_info );

	platform_add_devices (devices, ARRAY_SIZE (devices));

}

MACHINE_START(OMAP_PALMTC, "Palm Tungsten C")
/*	Maintainer: P3T3, Petr Blaha <p3t3@centrum.cz> */
/*	.phys_ram	= 0xa0000000, */
	.phys_io	= 0x40000000,
	.boot_params 	= 0xa0000100,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmtc_init
MACHINE_END
