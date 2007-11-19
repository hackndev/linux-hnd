/*
 * Machine initialization for Rover P1
 *
 * Authors: Konstantine A. Beklemishev konstantine@r66.ru
 * LCD initialization based on h1900.c code and adapted for Rover P1 platform
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/delay.h>
/* #include <linux/fb.h> */

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/memory.h>
#include <asm/hardware.h>

#include <asm/arch/roverp1-init.h>
#include <asm/arch/roverp1-gpio.h>

#include <asm/arch/pxa-dmabounce.h>
/* #include <asm/arch/pxafb.h> */
#include <asm/arch/udc.h>
#include <asm/arch/pxa-regs.h>


#include "../generic.h"

#define DEBUG 1

#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/* static void roverp1_backlight_power(int on)
 * {
 *         |+ TODO +|
 * }

 * static struct pxafb_mach_info roverp1lcd = {
 *         .pixclock   =	156551,   // --
 *         .bpp	    =	16,  // ??
 *         .xres	    =	240, // PPL + 1
 *         .yres	    =	320, // LPP + 1
 *         .hsync_len  =	24,  // HSW + 1
 *         .vsync_len  =	2,   // VSW + 1
 *         .left_margin=	20,  // BLW + 1
 *         .upper_margin = 5,   // BFW
 *         .right_margin = 12,  // ELW + 1
 *         .lower_margin = 5,   // EFW
 *         .sync	    =	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
 *         .lccr0 = 0x003008f0,
 *         .lccr3 = 0x04400008,
 *         .pxafb_backlight_power	= roverp1_backlight_power
 * };                                                                       */


static int roverp1_udc_is_connected(void) {
	DPRINTK("roverp1_udc_is_connected\n");
	return GET_ROVERP1_GPIO(USB_DETECT);
}

/* TODO unified eGPIO write/get function. static var */

static void roverp1_udc_command(int cmd) {

	DPRINTK("roverp1_udc_command: %d\n", cmd);
	
	switch(cmd){
	    case PXA2XX_UDC_CMD_DISCONNECT:
/*                 SET_ROVERP1_eGPIO(eGPIO_NR_ROVERP1_USB_EN, 0) */
/*                 writel(0x8021,ROVERP1_eGPIO_VIRT); */
/*                 printk("usb disconnected\n"); */
		break;
	    case PXA2XX_UDC_CMD_CONNECT:
/*                 SET_ROVERP1_eGPIO(eGPIO_NR_ROVERP1_USB_EN, 1) */
/*                 writel(0x8220,ROVERP1_eGPIO_VIRT); */
/*                 printk("usb connected\n"); */
		break;
	    default:
		DPRINTK("roverp1_udc_control: unknown command!\n");
		break;
	}
}

static struct pxa2xx_udc_mach_info roverp1_udc_mach_info = {
	.udc_is_connected = roverp1_udc_is_connected,
	.udc_command      = roverp1_udc_command,
};

static void __init roverp1_init_irq (void)
{
	/* Initialize standard IRQs */
	pxa_init_irq();

}

static struct map_desc roverp1_io_desc[] __initdata = {
	/* virtual            physical           length      domain     r  w  c  b */
	{ ROVERP1_eGPIO_VIRT, ROVERP1_eGPIO_PHYS, 0x00001000, MT_DEVICE}, /* CS#2 eGPIO*/
};
                                                                               
static void __init roverp1_map_io(void)
{
	/* Initialize standard IO maps */
	pxa_map_io();
	iotable_init(roverp1_io_desc, ARRAY_SIZE(roverp1_io_desc));
/*         writel(0x8228,ROVERP1_eGPIO_VIRT); */
	PWER = PWER_GPIO0 | PWER_RTC;
	PFER = PWER_GPIO0 | PWER_RTC;
	PRER = 0;
	PCFR = PCFR_OPDE;
/*         CKEN = CKEN0_PWM0    | CKEN1_PWM1     | CKEN2_AC97    | CKEN3_SSP  |
 *                CKEN4_SSP3    | CKEN5_STUART   | CKEN6_FFUART  | CKEN8_I2S  |
 *                CKEN9_OSTIMER | CKEN10_USBHOST | CKEN11_USB    | CKEN12_MMC |
 *                CKEN13_FICP   | CKEN14_I2C     | CKEN15_PWRI2C | CKEN16_LCD | 
 *                CKEN20_IM     | CKEN22_MEMC;                                   */

	PGSR0 = GPSRx_SleepValue;
	PGSR1 = GPSRy_SleepValue;
	PGSR2 = GPSRz_SleepValue;

/*         Set up GPIO direction and alternate function registers */
	GAFR0_L = GAFR0x_InitValue;
	GAFR0_U = GAFR1x_InitValue;
	GAFR1_L = GAFR0y_InitValue;
	GAFR1_U = GAFR1y_InitValue;
	GAFR2_L = GAFR0z_InitValue;
	GAFR2_U = GAFR1z_InitValue;

	GPDR0 = GPDRx_InitValue;
	GPDR1 = GPDRy_InitValue;
	GPDR2 = GPDRz_InitValue;

	GPSR0 = GPSRx_InitValue;
	GPSR1 = GPSRy_InitValue;
	GPSR2 = GPSRz_InitValue;

	GPCR0 = ~GPSRx_InitValue;
	GPCR1 = ~GPSRy_InitValue;
	GPCR2 = ~GPSRz_InitValue;

	REINIT_ROVERP1_eGPIO;

/*         if GET_ROVERP1_GPIO(USB_DETECT)
 *              printk("\n\nusb connected\n\n");
 *         else
 *              printk("\n\nusb disconnected\n\n");
 *         printk("eGPIO reinited\n");              */

}

/* static struct platform_device *devices[] __initdata = {
 *         &pxafb_device,
 * };                                                      */

static void __init roverp1_init (void)
{
	pxa_set_udc_info (&roverp1_udc_mach_info);
/*         set_pxa_fb_info(&roverp1lcd); */
}

MACHINE_START(ROVERP1, "Rover Rover P1")
	/* Maintainer Konstantine A. Beklemishev */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= roverp1_map_io,
	.init_irq	= roverp1_init_irq,
	.timer = &pxa_timer,
	.init_machine = roverp1_init,
MACHINE_END
