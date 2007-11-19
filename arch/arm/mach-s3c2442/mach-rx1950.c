/* linux/arch/arm/mach-s3c2440/mach-rx1950.c
 *
 * Copyright (c) 2003-2005 Simtec Electronics
 *   Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Originally for smdk2440 modified for rx1950 by Denis Grigoriev <dgreenday@gmail.com>
 * and Victor Chukhantsev
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <linux/mmc/protocol.h>

#include <linux/input_pda.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/hardware/iomd.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/arch/regs-serial.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-irq.h>
#include <asm/arch/regs-timer.h>
#include <asm/arch/ts.h>
#include <asm/arch/mmc.h>
#include <asm/arch/fb.h>
#include <asm/arch/lcd.h>
#include <asm/arch/udc.h>
#include <asm/arch/nand.h>

#include <linux/serial_core.h>

#include <asm/plat-s3c24xx/clock.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>
#include <asm/plat-s3c24xx/irq.h>

static struct map_desc rx1950_iodesc[] __initdata = {
	        /* dump ISA space somewhere unused */
	        {
			.virtual        = (u32)S3C24XX_VA_ISA_WORD,
			.pfn            = __phys_to_pfn(S3C2410_CS3),
			.length         = SZ_1M,
			.type           = MT_DEVICE,
		}, {
			.virtual        = (u32)S3C24XX_VA_ISA_BYTE,
			.pfn            = __phys_to_pfn(S3C2410_CS3),
			.length         = SZ_1M,
			.type           = MT_DEVICE,
		},
		};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c24xx_uart_clksrc rx1950_serial_clocks[] = {
        [0] = {
                .name           = "fclk",
                .divisor        = 0x0a,
                .min_baud       = 0,
                .max_baud       = 0,
        }
};


static struct s3c2410_uartcfg rx1950_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
                .clocks      = rx1950_serial_clocks,
                .clocks_size = ARRAY_SIZE(rx1950_serial_clocks),
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x245,
		.ulcon	     = 0x03,
		.ufcon	     = 0x00,
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.uart_flags  = UPF_CONS_FLOW,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x43,
		.ufcon	     = 0x51,
	}
};

/* Board control latch control */

/* Does rx1950 have any latches? */
/*
//static unsigned int latch_state = H1940_LATCH_DEFAULT;

void rx1950_latch_control(unsigned int clear, unsigned int set)
{
//	unsigned long flags;

//	local_irq_save(flags);

//	latch_state &= ~clear;
//	latch_state |= set;

//	__raw_writel(latch_state, H1940_LATCH);

//	local_irq_restore(flags);
}

EXPORT_SYMBOL_GPL(rx1950_latch_control);
*/

static struct s3c2410fb_mach_info rx1950_lcd_cfg __initdata = {
	//.type = 		S3C2410_LCDCON1_TFT,
	.fixed_syncs=		1,
	.regs	= {
		.lcdcon1=	S3C2410_LCDCON1_TFT16BPP | \
				S3C2410_LCDCON1_TFT | \
				S3C2410_LCDCON1_CLKVAL(0x07),

		.lcdcon2=	S3C2410_LCDCON2_VBPD(1) | \
				S3C2410_LCDCON2_LINEVAL(319) | \
				S3C2410_LCDCON2_VFPD(1) | \
				S3C2410_LCDCON2_VSPW(1),

		.lcdcon3=	S3C2410_LCDCON3_HBPD(19) | \
				S3C2410_LCDCON3_HOZVAL(239) | \
				S3C2410_LCDCON3_HFPD(9),

		.lcdcon4=	S3C2410_LCDCON4_MVAL(0) | \
				S3C2410_LCDCON4_HSPW(9),

		.lcdcon5=	S3C2410_LCDCON5_FRM565 | \
				S3C2410_LCDCON5_INVVCLK | \
				S3C2410_LCDCON5_INVVLINE | \
				S3C2410_LCDCON5_INVVFRAME | \
				S3C2410_LCDCON5_HWSWP,
	},


	/* it seems correct */
	.gpccon		= 0xaa9556a9,
	.gpccon_mask	= 0xffffffff,
	.gpcup		= 0x0000ffff,
	.gpcup_mask	= 0xffffffff,
	.gpdcon		= 0xaa90aaa1,
	.gpdcon_mask	= 0xffffffff,
	.gpdup		= 0x0000fcfd,
	.gpdup_mask	= 0xffffffff,

	.lpcsel=	0x02,

	.width		= 240,
	.height		= 320,

	.xres		= {
		.min	= 240,
		.max	= 240,
		.defval	= 240,
	},

	.yres		= {
		.min	= 320,
		.max	= 320,
		.defval = 320,
	},

	.bpp		= {
		.min	= 16,
		.max	= 16,
		.defval = 16,
	},
};

static void rx1950_backlight_power(int on)
{
	/* Need check if this correct */
	//s3c2410_gpio_setpin(S3C2410_GPB0, 0);
	//s3c2410_gpio_pullup(S3C2410_GPB0, 0);

	//s3c2410_gpio_cfgpin(S3C2410_GPB0,
	//		(on) ? S3C2410_GPB0_TOUT0 : S3C2410_GPB0_OUTP);
}

static void rx1950_lcd_power(int on)
{
	//s3c2410_gpio_setpin(S3C2410_GPC0, on);
}

static void rx1950_set_brightness(int tcmpb0)
{
	unsigned long tcfg0;
	unsigned long tcfg1;
	unsigned long tcon;

	/* configure power on/off */
	rx1950_backlight_power(tcmpb0 ? 1 : 0);


	tcfg0=readl(S3C2410_TCFG0);
	tcfg1=readl(S3C2410_TCFG1);

	tcfg0 &= ~S3C2410_TCFG_PRESCALER0_MASK;
	tcfg0 |= 0x18;

	tcfg1 &= ~S3C2410_TCFG1_MUX0_MASK;
	tcfg1 |= S3C2410_TCFG1_MUX0_DIV2;

	writel(tcfg0, S3C2410_TCFG0);
	writel(tcfg1, S3C2410_TCFG1);
	writel(0x31, S3C2410_TCNTB(0));

	tcon = readl(S3C2410_TCON);
	tcon &= ~0x0F;
	tcon |= S3C2410_TCON_T0RELOAD;
	tcon |= S3C2410_TCON_T0MANUALUPD;

	writel(tcon, S3C2410_TCON);
	writel(0x31, S3C2410_TCNTB(0));
	writel(tcmpb0, S3C2410_TCMPB(0));

	/* start the timer running */
	tcon |= S3C2410_TCON_T0START;
	tcon &= ~S3C2410_TCON_T0MANUALUPD;
	writel(tcon, S3C2410_TCON);
}

static struct s3c2410_bl_mach_info rx1950_bl_cfg __initdata = {

	.backlight_max          = 0x2c,
	.backlight_default      = 0x0a,
	.backlight_power	= rx1950_backlight_power,
	.set_brightness		= rx1950_set_brightness,
	.lcd_power		= rx1950_lcd_power
};

static void s3c2410_mmc_def_setpower(unsigned int to)
{
	s3c2410_gpio_cfgpin(S3C2410_GPH8, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPH8, to);
}

static struct s3c24xx_mmc_platdata rx1950_mmc_cfg = {
	/* please comment MMC_CAP_MULTIWRITE in drivers/mmc/s3c2440mci.c avoid kernel hang
	 * and set mmc->max_blk_size = 512 instead of 64 in drivers/mmc/s3c2440mci.c
	 * othewise we have kernel BUG() in drivers/mmc/mmc.c with ext2 fs
	 */
	.gpio_detect  = S3C2410_GPF5,
	.set_power  = s3c2410_mmc_def_setpower,
	.ocr_avail  = MMC_VDD_32_33,
	.f_max = 3000000,
};

static struct mtd_partition rx1950_nand_part[] = {
        [0] = {
                .name           = "Whole Flash",
                .offset         = 0,
                .size           = MTDPART_SIZ_FULL,
                .mask_flags     = MTD_WRITEABLE,
        }
};

static struct s3c2410_nand_set rx1950_nand_sets[] = {
        [0] = {
                .name           = "Internal",
                .nr_chips       = 1,
                .nr_partitions  = ARRAY_SIZE(rx1950_nand_part),
                .partitions     = rx1950_nand_part,
        },
};

static struct s3c2410_platform_nand rx1950_nand_info = {
        .tacls          = 25,
        .twrph0         = 50,
        .twrph1         = 15,
        .nr_sets        = ARRAY_SIZE(rx1950_nand_sets),
        .sets           = rx1950_nand_sets,
};

static void rx1950_udc_pullup(enum s3c2410_udc_cmd_e cmd)
{
	printk(KERN_DEBUG "udc: pullup(%d)\n",cmd);
	
	switch (cmd)
	{
		case S3C2410_UDC_P_ENABLE :
			//h1940_latch_control(0, H1940_LATCH_USB_DP);
			break;
		case S3C2410_UDC_P_DISABLE :
			//h1940_latch_control(H1940_LATCH_USB_DP, 0);
			break;
		case S3C2410_UDC_P_RESET :
			break;
		default:
			break;
	}
}

static struct s3c2410_udc_mach_info rx1950_udc_cfg __initdata = {
	.udc_command		= rx1950_udc_pullup,
	.vbus_pin		= S3C2410_GPG5,
	.vbus_pin_inverted	= 1,
};

static struct s3c2410_ts_mach_info rx1950_ts_cfg __initdata = {
#ifdef TOUCHSCREEN_S3C2410_ALT
		.xp_threshold = 940,
#endif
		.delay = 10000,
		.presc = 49,
		.oversampling_shift = 2,
};

static struct gpio_keys_button rx1950_gpio_keys_table[] = {
 {KEY_POWER,   S3C2410_GPF0, 1, "Power button"},
 {_KEY_RECORD, S3C2410_GPF7, 1, "Record button"},
 {_KEY_APP1, S3C2410_GPG0, 1, "Calendar button"},
 {_KEY_APP2, S3C2410_GPG2, 1, "Contacts button"},
 {_KEY_APP3, S3C2410_GPG3, 1, "Mail button"},
 {_KEY_APP4, S3C2410_GPG7, 1, "Home button"},
 {KEY_LEFT, S3C2410_GPG10, 1, "Calendar button"},
 {KEY_RIGHT, S3C2410_GPG11, 1, "Contacts button"},
 {KEY_UP, S3C2410_GPG4, 1, "Mail button"},
 {KEY_DOWN, S3C2410_GPG6, 1, "Home button"},
 {KEY_ENTER, S3C2410_GPG9, 1, "Ok button"},
};

static struct gpio_keys_platform_data rx1950_gpio_keys_data = {
	.buttons	= rx1950_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(rx1950_gpio_keys_table),
};

static struct platform_device rx1950_device_gpiokeys = {
	.name		= "gpio-keys",
	.dev = { .platform_data = &rx1950_gpio_keys_data, }
};

static struct platform_device *rx1950_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
	&s3c_device_usbgadget,
	&s3c_device_rtc,
	&s3c_device_nand,
	&s3c_device_sdi,
	&s3c_device_ts,
	&s3c_device_bl,
	&rx1950_device_gpiokeys,	
};

static struct s3c24xx_board rx1950_board __initdata = {
	.devices       = rx1950_devices,
	.devices_count = ARRAY_SIZE(rx1950_devices)
};

static void __init rx1950_map_io(void)
{
	s3c24xx_init_io(rx1950_iodesc, ARRAY_SIZE(rx1950_iodesc));
	s3c24xx_init_clocks(16934000);
	s3c24xx_init_uarts(rx1950_uartcfgs, ARRAY_SIZE(rx1950_uartcfgs));
	s3c24xx_set_board(&rx1950_board);
}

static void __init rx1950_init_machine(void)
{
	printk("rx1950 init()\n");
	s3c24xx_fb_set_platdata(&rx1950_lcd_cfg);
	
	s3c_device_nand.dev.platform_data = &rx1950_nand_info;
	s3c_device_sdi.dev.platform_data = &rx1950_mmc_cfg;
	set_s3c2410ts_info(&rx1950_ts_cfg);
	set_s3c2410bl_info(&rx1950_bl_cfg);
	s3c24xx_udc_set_platdata(&rx1950_udc_cfg);
	s3c2410_pm_init();
	s3c_irq_wake(IRQ_EINT0, 1);
}


MACHINE_START(RX1950, "HP iPAQ RX1950")
	/* Maintainer: Denis Grigoriev, Victor Chukhantsev */
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.map_io		= rx1950_map_io,
	.init_irq	= s3c24xx_init_irq,
	.init_machine   = rx1950_init_machine,
	.timer		= &s3c24xx_timer,
MACHINE_END
