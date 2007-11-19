/* linux/arch/arm/mach-s3c2440/mach-rx3715.c
 *
 * Copyright 2006 Roman Moravcik <roman.moravcik@gmail.com>
 * Copyright (c) 2003,2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://www.handhelds.org/projects/rx3715.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#include <linux/mfd/asic3_base.h>
#include <asm/hardware/asic3_leds.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/arch/regs-serial.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/rx3000.h>
#include <asm/arch/rx3000-asic3.h>

#include <asm/arch/h1940.h>
#include <asm/arch/irqs.h>
#include <asm/arch/nand.h>
#include <asm/arch/fb.h>

#include <asm/hardware/ipaq-asic3.h>

#include <asm/plat-s3c24xx/clock.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>

DEFINE_LED_TRIGGER_SHARED_GLOBAL(rx3000_radio_trig);
EXPORT_LED_TRIGGER_SHARED(rx3000_radio_trig);

static struct map_desc rx3715_iodesc[] __initdata = {
	/* dump ISA space somewhere unused */

	{
		.virtual	= (u32)S3C24XX_VA_ISA_WORD,
		.pfn		= __phys_to_pfn(S3C2410_CS3),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_BYTE,
		.pfn		= __phys_to_pfn(S3C2410_CS3),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	},
};


static struct s3c24xx_uart_clksrc rx3715_serial_clocks[] = {
	[0] = {
		.name		= "fclk",
		.divisor	= 0,
		.min_baud	= 0,
		.max_baud	= 0,
	}
};

static struct s3c2410_uartcfg rx3715_uartcfgs[] = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
		.clocks	     = rx3715_serial_clocks,
		.clocks_size = ARRAY_SIZE(rx3715_serial_clocks),
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
		.clocks	     = rx3715_serial_clocks,
		.clocks_size = ARRAY_SIZE(rx3715_serial_clocks),
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.uart_flags  = UPF_CONS_FLOW,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x43,
		.ufcon	     = 0x51,
		.clocks	     = rx3715_serial_clocks,
		.clocks_size = ARRAY_SIZE(rx3715_serial_clocks),
	}
};

static struct platform_device rx3000_battery = {
	.name           = "rx3000-battery",
	.id             = -1,
};

static struct platform_device rx3000_bl = {
	.name           = "rx3000-bl",
	.id             = -1,
};

static struct platform_device rx3000_bt = {
	.name           = "rx3000-bt",
	.id             = -1,
};

static struct platform_device rx3000_buttons = {
	.name           = "rx3000-buttons",
	.id             = -1,
};

static struct platform_device rx3000_serial = {
	.name           = "rx3000-serial",
	.id             = -1,
};

static struct platform_device rx3000_udc = {
	.name		= "rx3000-udc",
	.id		= -1,
};

/* LEDs */
static struct asic3_led rx3000_leds[] = {
	{
		.led_cdev  = {
			.name	         = "rx3000:green",
			.default_trigger = "ds2760-battery.0-full",
			.flags		 = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 0,

	},
	{
		.led_cdev  = {
			.name	         = "rx3000:red",
			.default_trigger = "ds2760-battery.0-charging",
			.flags		 = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 1,
	},
	{
		.led_cdev  = {
			.name	         = "rx3000:blue",
			.default_trigger = "rx3000-radio",
			.flags		 = LED_SUPPORTS_HWTIMER,
		},
		.hw_num = 2,
	},
};

static struct asic3_leds_machinfo rx3000_leds_machinfo = {
	.num_leds = ARRAY_SIZE(rx3000_leds),
	.leds = rx3000_leds,
	.asic3_pdev = &s3c_device_asic3,
};

static struct platform_device rx3000_leds_pdev = {
	.name = "asic3-leds",
	.dev = {
		.platform_data = &rx3000_leds_machinfo,
	},
};


static struct platform_device *child_devices[] __initdata = {
 	&rx3000_udc,
	&rx3000_buttons,
	&rx3000_bl,
	&rx3000_bt,
 	&rx3000_serial,
	&rx3000_battery,
	&rx3000_leds_pdev,
};

static struct asic3_platform_data rx3715_asic3_cfg = {
        .gpio_a = {
                .dir            = 0xffff,
                .init           = 0x0020,
                .sleep_mask     = 0xffff,
                .sleep_out      = 0x0030,
                .batt_fault_out = 0x0030,
                .alt_function   = 0x0000,
                .sleep_conf     = 0x0008,
        },
        .gpio_b = {
                .dir            = 0xffff,
                .init           = 0x1a02,
                .sleep_mask     = 0xffff,
                .sleep_out      = 0x0402,
                .batt_fault_out = 0x0402,
                .alt_function   = 0x0000,
                .sleep_conf     = 0x0008,
        },
        .gpio_c = {
                .dir            = 0xffff,
                .init           = 0x0600,
                .sleep_mask     = 0xffff,
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
                .alt_function   = 0x0007,
                .sleep_conf     = 0x0008,
        },
        .gpio_d = {
                .dir            = 0xfff0,
                .init           = 0x0040,
                .sleep_mask     = 0xfff0,
                .sleep_out      = 0x0000,
                .batt_fault_out = 0x0000,
                .alt_function   = 0x0000,
                .sleep_conf     = 0x0008,
        },
        .irq_base = RX3000_ASIC3_IRQ_BASE,

        .child_devs = child_devices,
        .num_child_devs = ARRAY_SIZE(child_devices),
};

static struct resource s3c_asic3_resources[] = {
        [0] = {
                .start  = RX3000_PA_ASIC3,
                .end    = RX3000_PA_ASIC3 + IPAQ_ASIC3_MAP_SIZE - 1,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .start  = IRQ_EINT12,
                .end    = IRQ_EINT12,
                .flags  = IORESOURCE_IRQ,
        },
        /* SD part */
    	[2] = {
    		.start	= RX3000_PA_ASIC3_SD,
		.end	= RX3000_PA_ASIC3_SD + IPAQ_ASIC3_MAP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
    	[3] = {
    		.start	= IRQ_EINT14,
    		.end	= IRQ_EINT14,
    		.flags	= IORESOURCE_IRQ,
    	},
};

struct platform_device s3c_device_asic3 = {
        .name           = "asic3",
        .id             = 0,
        .num_resources  = ARRAY_SIZE(s3c_asic3_resources),
        .resource       = s3c_asic3_resources,
        .dev = { .platform_data = &rx3715_asic3_cfg, }
};

EXPORT_SYMBOL(s3c_device_asic3);

/* framebuffer lcd controller information */

static struct s3c2410fb_mach_info rx3715_lcdcfg __initdata = {
	.regs	= {
		.lcdcon1 =	S3C2410_LCDCON1_TFT16BPP | \
				S3C2410_LCDCON1_TFT | \
				S3C2410_LCDCON1_CLKVAL(0x09),

		.lcdcon2 =	S3C2410_LCDCON2_VBPD(4) | \
				S3C2410_LCDCON2_LINEVAL(319) | \
				S3C2410_LCDCON2_VFPD(5) | \
				S3C2410_LCDCON2_VSPW(1),

		.lcdcon3 =	S3C2410_LCDCON3_HBPD(35) | \
				S3C2410_LCDCON3_HOZVAL(239) | \
				S3C2410_LCDCON3_HFPD(35),

		.lcdcon4 =	S3C2410_LCDCON4_MVAL(0) | \
				S3C2410_LCDCON4_HSPW(7),

		.lcdcon5 =	S3C2410_LCDCON5_INVVLINE |
				S3C2410_LCDCON5_FRM565 |
				S3C2410_LCDCON5_HWSWP,
	},

	.lpcsel =	0xf82,

	.gpccon =	0xaa955699,
	.gpccon_mask =	0xffc003cc,
	.gpcup =	0x0000ffff,
	.gpcup_mask =	0xffffffff,

	.gpdcon =	0xaa95aaa1,
	.gpdcon_mask =	0xffc0fff0,
	.gpdup =	0x0000ffff,
	.gpdup_mask =	0xffffffff,

	.fixed_syncs =	1,
	.width  =	240,
	.height =	320,

	.xres	= {
		.min =		240,
		.max =		240,
		.defval =	240,
	},

	.yres	= {
		.max =		320,
		.min =		320,
		.defval	=	320,
	},

	.bpp	= {
		.min =		16,
		.max =		16,
		.defval =	16,
	},
};

static struct mtd_partition rx3715_nand_part[] = {
	[0] = {
		.name		= "Whole Flash",
		.offset		= 0,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= MTD_WRITEABLE,
	}
};

static struct s3c2410_nand_set rx3715_nand_sets[] = {
	[0] = {
		.name		= "Internal",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(rx3715_nand_part),
		.partitions	= rx3715_nand_part,
	},
};

static struct s3c2410_platform_nand rx3715_nand_info = {
	.tacls		= 25,
	.twrph0		= 50,
	.twrph1		= 15,
	.nr_sets	= ARRAY_SIZE(rx3715_nand_sets),
	.sets		= rx3715_nand_sets,
};

static struct platform_device rx3000_ts = {
	.name		= "rx3000-ts",
	.id		= -1,
};

static struct platform_device *rx3715_devices[] __initdata = {
	&s3c_device_asic3,
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
	&s3c_device_rtc,
	&s3c_device_nand,
	&rx3000_ts,
};

static struct s3c24xx_board rx3715_board __initdata = {
	.devices       = rx3715_devices,
	.devices_count = ARRAY_SIZE(rx3715_devices)
};

static void __init rx3715_map_io(void)
{
	s3c_device_nand.dev.platform_data = &rx3715_nand_info;

	s3c24xx_init_io(rx3715_iodesc, ARRAY_SIZE(rx3715_iodesc));
	s3c24xx_init_clocks(16934000);
	s3c24xx_init_uarts(rx3715_uartcfgs, ARRAY_SIZE(rx3715_uartcfgs));
	s3c24xx_set_board(&rx3715_board);
}

static void __init rx3715_init_irq(void)
{
	s3c24xx_init_irq();
}

static void __init rx3715_init_machine(void)
{
#ifdef CONFIG_PM_H1940
	memcpy(phys_to_virt(H1940_SUSPEND_RESUMEAT), h1940_pm_return, 1024);
#endif
	s3c2410_pm_init();

	s3c24xx_fb_set_platdata(&rx3715_lcdcfg);

        /* turn on nand write protect */
        s3c2410_gpio_setpin(S3C2410_GPA6, 0);

        /* configure wakeup source */
        s3c2410_gpio_cfgpin(S3C2410_GPF0, S3C2410_GPF0_EINT0);
        enable_irq_wake(IRQ_EINT0);
        enable_irq_wake(IRQ_EINT2);
        enable_irq_wake(IRQ_EINT13);
	
	led_trigger_register_shared("rx3000-radio", &rx3000_radio_trig);
}


MACHINE_START(RX3715, "HP iPAQ RX3000")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.map_io		= rx3715_map_io,
	.init_irq	= rx3715_init_irq,
	.init_machine	= rx3715_init_machine,
	.timer		= &s3c24xx_timer,
MACHINE_END
