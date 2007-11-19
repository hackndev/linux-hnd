/*
 * Hardware definitions for Palm Zire 31 
 * 
 * Author: Scott Mansell <phiren@gmail.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>
#include <linux/irq.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#include <asm/arch/pxa-dmabounce.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxapwm-bl.h>

#include <asm/arch/audio.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include <linux/interrupt.h>
#include <asm/irq.h>

#include "../generic.h"
#include "palmz31-gpio.h"
#define DEBUG

/**
 * AC97 audio controller
 */

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

/**
 * Buttons
 */

struct platform_device palmz31_buttons = {
	.name 	= "palmz31-buttons",
	.id	=-1,
};

/**
 * USB
 */

/*static struct workqueue_struct *my_workqueue;
	#define MY_WORK_QUEUE_NAME "Palmz31"
static void palmz31_usb_power(void* irq)
{
	int gpn = (int)irq;

	if ((GPLR(gpn) & GPIO_bit(gpn)) == 0) // If USB detect is low
		//turn USB power off
		GPCR(GPIO_NR_PALMZ31_USB_POWER) = GPIO_bit(GPIO_NR_PALMZ31_USB_POWER);
	else // Else USB detect is high
		//Turn USB power on
		GPSR(GPIO_NR_PALMZ31_USB_POWER) = GPIO_bit(GPIO_NR_PALMZ31_USB_POWER);
}

static irqreturn_t palmz31_usb_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	static struct work_struct task;

	INIT_WORK(&task, palmz31_usb_power, dev_id);
	queue_work(my_workqueue, &task);

	return IRQ_HANDLED;
} */

static int palmz31_usb_init()
{
	int ret;
	printk(KERN_ERR "Setting up USB...");
	// First set the USB power gpio to 'Out'
	GPDR(GPIO_NR_PALMZ31_USB_POWER) |= GPIO_bit(GPIO_NR_PALMZ31_USB_POWER);
	// Make sure USB is off, to save power
	GPSR(GPIO_NR_PALMZ31_USB_POWER) = GPIO_bit(GPIO_NR_PALMZ31_USB_POWER);
	// Then attach a interupt to USB detect
	/*ret = request_irq (IRQ_GPIO(GPIO_NR_PALMLD_USB_DETECT), palmz31_usb_irq, 0, "Detect USB", NULL);
	if(ret!=0){ //Check if it worked
		printk(KERN_ERR "Whoops: Can't attach irq to USB detect");
		return 1; }
	else
	{
		printk(KERN_ERR "Attached irq to USB detect");
		return 0;
	} */
        return 0;
}

static struct platform_device *devices[] __initdata = {
	&palmld_ac97,
	&palmz31_buttons,
};

/**
 * Backlight
 */

static void zire31_backlight_power(int on)
{
	/* TODO */
	if(on) {
	//	PWM_CTRL1 = 0x1;
	//	PWM_PWDUTY1 = 0x50;
	//	PWM_PERVAL1 = 0x12b;
		CKEN |= 3;
	} else
		CKEN &= ~3;
}

static struct pxafb_mode_info zire31_lcd_modes[]  = {
{
	.pixclock	= 0,
	.xres		= 160,
	.yres		= 160,
	.bpp		= 16,
	.hsync_len	= 4,
	.left_margin	= 30,
	.right_margin	= 7,
	.vsync_len	= 1,
	.upper_margin	= 0,
	.lower_margin	= 0,
	.sync		= FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
},
};

static struct pxafb_mach_info zire31_lcd = {
	.modes			= zire31_lcd_modes,
	.num_modes		= ARRAY_SIZE(zire31_lcd_modes),
	/* fixme: this is a hack, use constants instead. */
	.lccr0			= 0x00100079,
	/* Palm sets 4700004, but with 4700002 we eliminate the annoying screen noise */
	.lccr3			= 0x03403d30,
	
	.pxafb_backlight_power	= zire31_backlight_power,
};


static void __init zire31_int(void)
{
	set_pxa_fb_info(&zire31_lcd);

	GCR &= ~GCR_PRIRDY_IEN;
	palmz31_usb_init();
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(ZIRE31, "Zire 31")
	.boot_params    = 0xa0000100,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= zire31_int
MACHINE_END


