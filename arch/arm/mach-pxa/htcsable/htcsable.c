/*
 * linux/arch/arm/mach-pxa/htcsable/htcsable.c
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
#include <linux/fb.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/serial.h>

#include <asm/arch/htcsable.h>
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>

#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>
#include <linux/mfd/tmio_mmc.h>

#include "../generic.h"

#ifdef CONFIG_HTCSABLE_BT
#include "htcsable_bt.h"
#endif

#ifdef CONFIG_HTCSABLE_PHONE
#include "htcsable_phone.h"
#endif

/*
 *  * Bluetooth - Relies on other loadable modules, like ASIC3 and Core,
 *   * so make the calls indirectly through pointers. Requires that the
 *    * hx4700 bluetooth module be loaded before any attempt to use
 *     * bluetooth (obviously).
 *      */

#ifdef CONFIG_HTCSABLE_BT
static struct htcsable_bt_funcs bt_funcs;

static void
htcsable_bt_configure( int state )
{
  if (bt_funcs.configure != NULL)
    bt_funcs.configure( state );
}
#endif

#ifdef CONFIG_HTCSABLE_PHONE
static struct htcsable_phone_funcs phone_funcs;

static void
htcsable_phone_configure( int state )
{
  if (phone_funcs.configure != NULL)
    phone_funcs.configure( state );
}
#endif

static struct pxafb_mode_info htcsable_lcd_modes[] = {
{
	.pixclock		= 480769, // LCCR4 bit is set!
	.xres			= 240,
	.yres			= 240,
	.bpp			= 16,
	.hsync_len		= 4,
	.vsync_len		= 2,
	.left_margin		= 12,
	.right_margin		= 8,
	.upper_margin		= 3,
	.lower_margin		= 3,

//	.sync			= FB_SYNC_HOR_LOW_ACT|FB_SYNC_VERT_LOW_ACT,
},
};


static struct pxafb_mach_info htcsable_lcd_screen = {
        .modes		= htcsable_lcd_modes,
        .num_modes	= ARRAY_SIZE(htcsable_lcd_modes), 

	.lccr0			= 0x042000b1,
	.lccr3			= 0x04700019,
//	.lccr4			= 0x80000000,

};

#ifdef CONFIG_HTCSABLE_BACKLIGHT
/* Backlight */ 
extern struct platform_device htcsable_bl;
#endif
extern struct platform_device htcsable_keyboard;
static struct platform_device htcsable_udc       = { .name = "htcsable_udc", };
static struct platform_device htcsable_ts        = { .name = "htcsable_ts", };
static struct platform_device htcsable_kbd        = { .name = "htcsable_kbd", };
static struct platform_device htcsable_buttons   = { .name = "htcsable_buttons", };
static struct platform_device htcsable_lcd        = { .name = "htcsable_lcd", };

/* Bluetooth */

#ifdef CONFIG_HTCSABLE_BT
static struct platform_device htcsable_bt = {
	.name = "htcsable_bt",
	.id = -1,
	.dev = {
		.platform_data = &bt_funcs,
	},
};
#endif

#ifdef CONFIG_HTCSABLE_PHONE
static struct platform_device htcsable_phone = {
	.name = "htcsable_phone",
	.id = -1,
	.dev = {
		.platform_data = &phone_funcs,
	},
};
#endif

static struct platform_device *htcsable_asic3_devices[] __initdata = {
	&htcsable_ts,
	&htcsable_kbd,
	&htcsable_lcd,
	&htcsable_keyboard,
	&htcsable_buttons,
#ifdef CONFIG_HTCSABLE_BACKLIGHT
	&htcsable_bl,
#endif
#ifdef CONFIG_HTCSABLE_BT
	&htcsable_bt,
#endif
#ifdef CONFIG_HTCSABLE_PHONE
	&htcsable_phone,
#endif
	&htcsable_udc,
};

static struct tmio_mmc_hwconfig htcsable_mmc_hwconfig = {
	.mmc_get_ro = TMIO_WP_ALWAYS_RW,
};

static struct asic3_platform_data htcsable_asic3_platform_data = {

   /*
    * These registers are configured as they are on Wince.
    *
    * A0 is set high by default as we think it's the i2c initialisation
    * pin.  that means less worry about having to have a pxa-i2s module
    * init function on a per-device basis.   nasty hack, really.
    */
        .gpio_a = {
		.dir            = 0xbffb,
		.init           = 0xc084,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x6000, //
		.sleep_conf     = 0x000c,
        },
        .gpio_b = {
		.dir            = 0xe408,
		.init           = 0x1762,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000, //
                .sleep_conf     = 0x000c,
        },
        .gpio_c = {
                .dir            = 0xfff7,
                .init           = 0x8640,
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
		.alt_function   = 0x0038, // GPIOC_LED_RED | GPIOC_LED_GREEN | GPIOC_LED_BLUE
                .sleep_conf     = 0x000c,
        },
        .gpio_d = {
		.dir            = 0xffff,
		.init           = 0xd000,
		.sleep_out      = 0x0000,
		.batt_fault_out = 0x0000,
		.alt_function   = 0x0000, //
		.sleep_conf     = 0x0008,
        },
	.bus_shift = 1,
	.irq_base = HTCSABLE_ASIC3_IRQ_BASE,

	.child_devs     = htcsable_asic3_devices,
	.num_child_devs = ARRAY_SIZE(htcsable_asic3_devices),
	.tmio_mmc_hwconfig = &htcsable_mmc_hwconfig,
};

static struct resource htcsable_asic3_resources[] = {
	[0] = {
		.start	= HTCSABLE_ASIC3_GPIO_PHYS,
		.end	= HTCSABLE_ASIC3_GPIO_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= HTCSABLE_IRQ(ASIC3_EXT_INT),
		.end	= HTCSABLE_IRQ(ASIC3_EXT_INT),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start  = HTCSABLE_ASIC3_MMC_PHYS,
		.end    = HTCSABLE_ASIC3_MMC_PHYS + IPAQ_ASIC3_MAP_SIZE,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.start  = HTCSABLE_IRQ(ASIC3_SDIO_INT_N),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device htcsable_asic3 = {
	.name           = "asic3",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(htcsable_asic3_resources),
	.resource       = htcsable_asic3_resources,
	.dev = { .platform_data = &htcsable_asic3_platform_data, },
};
EXPORT_SYMBOL(htcsable_asic3);

/* Core Hardware Functions */

struct platform_device htcsable_core = {
  .name   = "htcsable_core",
  .id   = 0,
  .dev = {
    .platform_data = NULL,
  },
};

static struct platform_device *devices[] __initdata = {
  &htcsable_core,
};

#ifdef CONFIG_HTCSABLE_BT
static struct platform_pxa_serial_funcs htcsable_pxa_bt_funcs = {
  .configure = htcsable_bt_configure,
};
#endif

#ifdef CONFIG_HTCSABLE_PHONE
static struct platform_pxa_serial_funcs htcsable_pxa_phone_funcs = {
  .configure = htcsable_phone_configure,
#if 0
  .ioctl = htcsable_phone_ioctl,
#endif
};
#endif

static void __init htcsable_map_io(void)
{
  struct ffuart_pxa_port *sport;

  pxa_map_io();

  /* Is this correct? If yes, please add comment, that vendor was drunk when did soldering. */
#ifdef CONFIG_HTCSABLE_BT
  pxa_set_ffuart_info(&htcsable_pxa_bt_funcs);
#endif
#ifdef CONFIG_HTCSABLE_PHONE
  pxa_set_btuart_info(&htcsable_pxa_phone_funcs);
#endif

//  sport = platform_get_drvdata(&ffuart device);
//  printk("sport=0x%x\n", (unsigned int)sport);

  pxa_set_cken(CKEN8_I2S, 0);
  printk("CKEN=0x%x CKEN11_USB=0x%x\n", CKEN, CKEN11_USB);
  pxa_set_cken(CKEN11_USB, 1);
  printk("CKEN=0x%x\n", CKEN);
}


static void __init htcsable_init(void)
{
	set_pxa_fb_info( &htcsable_lcd_screen );

  pxa_gpio_mode( GPIO_NR_HTCSABLE_I2C_SCL_MD );
  pxa_gpio_mode( GPIO_NR_HTCSABLE_I2C_SDA_MD );

	platform_device_register(&htcsable_asic3);
	platform_add_devices( devices, ARRAY_SIZE(devices) );
}

#ifdef CONFIG_MACH_HTCBEETLES
MACHINE_START(HTCBEETLES, "HTC Beetles")
	.phys_io	= 0x40000000,
	.io_pg_offst    = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io 	= htcsable_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcsable_init,
MACHINE_END
#endif
#ifdef CONFIG_MACH_HW6900
MACHINE_START(HW6900, "HTC Sable")
	.phys_io	= 0x40000000,
	.io_pg_offst    = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io 	= htcsable_map_io,
	.init_irq	= pxa_init_irq,
	.timer  	= &pxa_timer,
	.init_machine	= htcsable_init,
MACHINE_END
#endif
