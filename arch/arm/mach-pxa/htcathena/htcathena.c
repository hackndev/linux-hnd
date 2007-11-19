/*
 * linux/arch/arm/mach-pxa/htcathena/htcathena.c
 *
 *  Support for the HTC Athena.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input_pda.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>
#include <asm/arch/udc.h>

#include <asm/arch/htcathena-gpio.h>

#include "../generic.h"

#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

#if 0
/*
 * CPLD
 */

static struct resource htcathena_cpld_resources[] = {
	[0] = {
		.start	= PXA_CS3_PHYS,
		.end	= PXA_CS3_PHYS + 0x1000,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device htcathena_cpld = {
	.name		= "htcathena-cpld",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(htcathena_cpld_resources),
	.resource	= htcathena_cpld_resources,
};

/*
 * Touchscreen
 */

static struct tsc2046_mach_info htcathena_ts_platform_data = {
       .port     = 2,
       .clock    = CKEN3_SSP2,
       .pwrbit_X = 1,
       .pwrbit_Y = 1,
       .irq	 = HTCATHENA_IRQ(TOUCHPANEL_IRQ_N)
};

static struct platform_device htcathena_ts        = {
       .name = "htcathena_ts",
       .dev  = {
              .platform_data = &htcathena_ts_platform_data,
       },
};

/*
 * Phone
 */

struct htcathena_phone_funcs {
        void (*configure) (int state);
        void (*suspend) (struct platform_device *dev, pm_message_t state);
        void (*resume) (struct platform_device *dev);
};

static struct htcathena_phone_funcs phone_funcs;

static void htcathena_phone_configure (int state)
{
	if (phone_funcs.configure)
		phone_funcs.configure (state);
}

static void htcathena_phone_suspend (struct platform_device *dev, pm_message_t state)
{
	if (phone_funcs.suspend)
		phone_funcs.suspend (dev, state);
}

static void htcathena_phone_resume (struct platform_device *dev)
{
	if (phone_funcs.resume)
		phone_funcs.resume (dev);
}

static struct platform_pxa_serial_funcs htcathena_pxa_phone_funcs = {
	.configure = htcathena_phone_configure,
	.suspend   = htcathena_phone_suspend,
	.resume    = htcathena_phone_resume,
};


static struct platform_device htcathena_phone = {
	.name = "htcathena-phone",
	.dev  = {
		.platform_data = &phone_funcs,
		},
	.id   = -1,
};
#endif

/*
 * GPIO Keys
 */

static struct gpio_keys_button htcathena_button_table[] = {
	{KEY_POWER,      GPIO_NR_HTCATHENA_KEY_POWER,         1, "Power button"},
	{KEY_VOLUMEUP,   GPIO_NR_HTCATHENA_KEY_VOL_UP,       0, "Volume slider (up)"},
	{KEY_CAMERA,     GPIO_NR_HTCATHENA_KEY_CAMERA,       0, "Camera button"},
	{KEY_RECORD,     GPIO_NR_HTCATHENA_KEY_RECORD,       0, "Record button"},
	{KEY_WWW,        GPIO_NR_HTCATHENA_KEY_WWW,          0, "WWW button"},
	{KEY_SEND,       GPIO_NR_HTCATHENA_KEY_SEND,         0, "Send button"},
	{KEY_END,        GPIO_NR_HTCATHENA_KEY_END,          0, "End button"},
	{KEY_VOLUMEDOWN, GPIO_NR_HTCATHENA_KEY_VOL_DOWN,    0, "Volume slider (down)"},
	{KEY_RIGHT,      GPIO_NR_HTCATHENA_KEY_RIGHT,       1, "Right button"},
	{KEY_UP,         GPIO_NR_HTCATHENA_KEY_UP,          1, "Up button"},
	{KEY_LEFT,       GPIO_NR_HTCATHENA_KEY_LEFT,        1, "Left button"},
	{KEY_DOWN,       GPIO_NR_HTCATHENA_KEY_DOWN,        1, "Down button"},
	{KEY_KPENTER,    GPIO_NR_HTCATHENA_KEY_ENTER,       1, "Action button"},
#if 0
	{KEY_F8,         GPIO37_HTCATHENA_KEY_PHONE_HANGUP, 0, "Phone hangup button"},
	{KEY_F10,        GPIO38_HTCATHENA_KEY_CONTACTS,     0, "Contacts button"},
	{KEY_F9,         GPIO90_HTCATHENA_KEY_CALENDAR,     0, "Calendar button"},
	{KEY_PHONE,      GPIO102_HTCATHENA_KEY_PHONE_LIFT,  0, "Phone lift button"},
	{KEY_F13,        GPIO99_HTCATHENA_HEADPHONE_IN,     0, "Headphone switch"},
#endif
};

static struct gpio_keys_platform_data htcathena_gpio_keys_data = {
	.buttons  = htcathena_button_table,
	.nbuttons = ARRAY_SIZE(htcathena_button_table),
};

static struct platform_device htcathena_gpio_keys = {
	.name = "gpio-keys",
	.dev  = {
		.platform_data = &htcathena_gpio_keys_data,
		},
	.id   = -1,
};

static struct platform_device *devices[] __initdata = {
//	&htcathena_cpld,
//	&htcathena_ts,
//	&htcathena_phone,
//	&htcathena_flash,
	&htcathena_gpio_keys,
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
//                        GPCR_BIT(GPIO_NR_HTCATHENA_USB_PUEN);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			printk(KERN_NOTICE "USB cmd connect\n");
//                        GPSR_BIT(GPIO_NR_HTCATHENA_USB_PUEN);
			break;
	}
}

static struct pxa2xx_udc_mach_info htcathena_udc_mach_info = {
	.udc_command      = udc_command,
};

static void __init htcathena_init(void)
{
//	set_pxa_fb_info( &htcathena_lcd );
	platform_add_devices( devices, ARRAY_SIZE(devices) );
//	pxa_set_btuart_info(&htcathena_pxa_phone_funcs);
	pxa_set_udc_info( &htcathena_udc_mach_info );
}

MACHINE_START(HTCATHENA, "HTC Athena")
	.phys_io	= 0x40000000,
	.io_pg_offst	= io_p2v(0x40000000),
	.boot_params	= 0xa0000100,
	.map_io 	= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcathena_init,
MACHINE_END
