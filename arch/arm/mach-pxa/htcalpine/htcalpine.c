/*
 * linux/arch/arm/mach-pxa/htcalpine/htcalpine.c
 *
 *  Support for the Intel XScale based Palm PDAs. Only the LifeDrive is
 *  supported at the moment.
 *
 *  Author: Alex Osborne <bobofdoom@gmail.com>
 *
 *  USB stubs based on aximx30.c (Michael Opdenacker)
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/udc.h>

#include <asm/arch/htcalpine-gpio.h>

#include "tsc2046_ts.h"

#include "../generic.h"

#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

static struct pxafb_mode_info htcalpine_lcd_modes[] = {
{
	.pixclock		= 115384,
	.xres			= 240,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 34,
	.vsync_len		= 8,
	.left_margin		= 34,
	.right_margin		= 34,
	.upper_margin		= 7,
	.lower_margin		= 7,

//	.sync			= FB_SYNC_HOR_LOW_ACT|FB_SYNC_VERT_LOW_ACT,
},
};

static struct pxafb_mach_info htcalpine_lcd = {
        .modes		= htcalpine_lcd_modes,
        .num_modes	= ARRAY_SIZE(htcalpine_lcd_modes), 

	/* fixme: this is a hack, use constants instead. */
//	.lccr0			= 0x04000081,
//	.lccr3			= 0x04400000,
	.lccr0			= 0x04000080,
	.lccr3			= 0x04400000,

};

/*
 * CPLD
 */

static struct resource htcalpine_cpld_resources[] = {
	[0] = {
		.start	= PXA_CS3_PHYS,
		.end	= PXA_CS3_PHYS + 0x1000,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device htcalpine_cpld = {
	.name		= "htcalpine-cpld",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(htcalpine_cpld_resources),
	.resource	= htcalpine_cpld_resources,
};

/*
 * Touchscreen
 */

static struct tsc2046_mach_info htcalpine_ts_platform_data = {
       .port     = 2,
       .clock    = CKEN3_SSP2,
       .pwrbit_X = 1,
       .pwrbit_Y = 1,
       .irq	 = HTCALPINE_IRQ(TOUCHPANEL_IRQ_N)
};

static struct platform_device htcalpine_ts        = {
       .name = "htcuniversal_ts",
       .dev  = {
              .platform_data = &htcalpine_ts_platform_data,
       },
};

/*
 * Phone
 */

struct htcalpine_phone_funcs {
        void (*configure) (int state);
        int (*suspend) (struct platform_device *dev, pm_message_t state);
        int (*resume) (struct platform_device *dev);
};

static struct htcalpine_phone_funcs phone_funcs;

static void htcalpine_phone_configure (int state)
{
	if (phone_funcs.configure)
		phone_funcs.configure (state);
}

static int htcalpine_phone_suspend (struct platform_device *dev, pm_message_t state)
{
	if (phone_funcs.suspend)
		return phone_funcs.suspend (dev, state);
	else
		return 0; /* ? */
}

static int htcalpine_phone_resume (struct platform_device *dev)
{
	if (phone_funcs.resume)
		return phone_funcs.resume (dev);
	else
		return 0; /* ? */
}

static struct platform_pxa_serial_funcs htcalpine_pxa_phone_funcs = {
	.configure = htcalpine_phone_configure,
	.suspend   = htcalpine_phone_suspend,
	.resume    = htcalpine_phone_resume,
};


static struct platform_device htcalpine_phone = {
	.name = "htcalpine-phone",
	.dev  = {
		.platform_data = &phone_funcs,
		},
	.id   = -1,
};

static struct platform_device *devices[] __initdata = {
	&htcalpine_cpld,
	&htcalpine_ts,
	&htcalpine_phone,
//	&htcalpine_flash,
//	&htcalpine_pxa_keys,
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
                        GPCR_BIT(GPIO_NR_HTCALPINE_USB_PUEN);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			printk(KERN_NOTICE "USB cmd connect\n");
                        GPSR_BIT(GPIO_NR_HTCALPINE_USB_PUEN);
			break;
	}
}

static struct pxa2xx_udc_mach_info htcalpine_udc_mach_info = {
	.udc_command      = udc_command,
};

static void __init htcalpine_init(void)
{
	set_pxa_fb_info( &htcalpine_lcd );
	platform_add_devices( devices, ARRAY_SIZE(devices) );
	pxa_set_btuart_info(&htcalpine_pxa_phone_funcs);
	pxa_set_udc_info( &htcalpine_udc_mach_info );
}

MACHINE_START(HTCALPINE, "HTC Alpine")
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcalpine_init,
MACHINE_END

