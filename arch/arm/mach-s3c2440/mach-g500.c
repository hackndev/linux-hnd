/* linux/arch/arm/mach-s3c2440/mach-g500.c
 *
 * http://www.pierrox.net/G500 or
 * http://www.handhelds.org/moin/moin.cgi/EtenG500Home
 * pierrox@pierrox.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Origin:
 * Copied and adapted from mach-rx3715.c, mach-h1940.c and mach-smdk2440.c.
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

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/hardware/iomd.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/arch/regs-serial.h>
#include <asm/arch/regs-timer.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-irq.h>

#include <asm/arch/idle.h>
#include <asm/arch/nand.h>
#include <asm/arch/fb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/ts.h>
#include <asm/arch/lcd.h>
#include <asm/arch/leds-gpio.h>
#include <asm/arch/h1940.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/protocol.h>

#include <asm/plat-s3c24xx/clock.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>


static struct map_desc g500_iodesc[] __initdata = {
	/* nothing */
};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg g500_uartcfgs[] = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x9fc5,
		.ulcon	     = 0x2b,
		.ufcon	     = 0x11,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x1c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x31,
	},
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = 0x8245,
		.ulcon	     = 0x43,
		.ufcon	     = 0x00,
	}
};

/* LCD driver info */

static struct s3c2410fb_mach_info g500_lcd_cfg __initdata = {
	.regs	= {

		.lcdcon1	= S3C2410_LCDCON1_CLKVAL(0x04) | 
				  S3C2410_LCDCON1_TFT | 
				  S3C2410_LCDCON1_TFT16BPP,
                                
		.lcdcon2	= S3C2410_LCDCON2_VBPD(5) | 
				  S3C2410_LCDCON2_LINEVAL(319) | 
				  S3C2410_LCDCON2_VFPD(6) | 
				  S3C2410_LCDCON2_VSPW(1),
			
		.lcdcon3	= S3C2410_LCDCON3_HBPD(4) | 
				  S3C2410_LCDCON3_HOZVAL(239) | 
				  S3C2410_LCDCON3_HFPD(7),

		.lcdcon4	= S3C2410_LCDCON4_MVAL(0) |
				  S3C2410_LCDCON4_HSPW(2),

		.lcdcon5	= S3C2410_LCDCON5_FRM565 |
				  S3C2410_LCDCON5_INVVCLK |
				  S3C2410_LCDCON5_INVVLINE |
				  S3C2410_LCDCON5_INVVFRAME |
				  S3C2410_LCDCON5_INVVDEN |
				  S3C2410_LCDCON5_PWREN |
				  S3C2410_LCDCON5_HWSWP
	},

#if 0
	/* currently setup by wince, TODO find exact values */
	.gpccon		= 0xaa940659,
	.gpccon_mask	= 0xffffffff,
	.gpcup		= 0x0000ffff,
	.gpcup_mask	= 0xffffffff,
	.gpdcon		= 0xaa84aaa0,
	.gpdcon_mask	= 0xffffffff,
	.gpdup		= 0x0000faff,
	.gpdup_mask	= 0xffffffff,
#endif

	.lpcsel		= 0xCE6,

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

static struct mtd_partition g500_nand_part[] = {
	[0] = {
		.name		= "Whole Flash",
		.offset		= 0,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= MTD_WRITEABLE,
	}
};

static struct s3c2410_nand_set g500_nand_sets[] = {
	[0] = {
		.name		= "Internal",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(g500_nand_part),
		.partitions	= g500_nand_part,
	},
};

static struct s3c2410_platform_nand g500_nand_info = {
	.tacls		= 15, /*31*/ // 1
	.twrph0		= 30, /*63*/ // 6
	.twrph1		= 30, /*63*/ // 2
	.nr_sets	= ARRAY_SIZE(g500_nand_sets),
	.sets		= g500_nand_sets,
};

static void s3c2410_mmc_def_setpower(unsigned int to)
{
	s3c2410_gpio_cfgpin(S3C2410_GPA17, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPA17, to);
}

static struct s3c24xx_mmc_platdata g500_mmc_cfg = {
	.gpio_detect  = S3C2410_GPF6,
	.set_power  = s3c2410_mmc_def_setpower,
	.ocr_avail  = MMC_VDD_32_33,
};

static struct s3c2410_ts_mach_info g500_ts_cfg __initdata = {
#ifdef TOUCHSCREEN_S3C2410_ALT
		.xp_threshold = 940,
#endif
		.delay = 50000,
		.presc = 49,
		.oversampling_shift = 2,
};

static struct gpio_keys_button g500_gpio_keys_table[] = {
        {KEY_POWER,     S3C2410_GPF1,   1,      "Power"},
        {KEY_CAMERA,    S3C2410_GPG2,   1,      "Camera"},
        {KEY_LEFT,      S3C2410_GPG3,   1,      "Left"},
        {KEY_RIGHT,     S3C2410_GPG5,   1,      "Right"},
        {KEY_REPLY,     S3C2410_GPG6,   1,      "Pick-Up"},
};

static struct gpio_keys_platform_data g500_gpio_keys_data = {
        .buttons        = g500_gpio_keys_table,
        .nbuttons       = ARRAY_SIZE(g500_gpio_keys_table),
};

static struct platform_device s3c_device_buttons = {
        .name           = "gpio-keys",
        .dev            = {
        .platform_data  = &g500_gpio_keys_data,
         }
};

static void g500_backlight_power(int on)
{
	s3c2410_gpio_setpin(S3C2410_GPB0, 0);
	s3c2410_gpio_pullup(S3C2410_GPB0, 0);

	s3c2410_gpio_cfgpin(S3C2410_GPB0,
			(on) ? S3C2410_GPB0_TOUT0 : S3C2410_GPB0_OUTP);
}

static void g500_lcd_power(int on)
{
	s3c2410_gpio_setpin(S3C2410_GPC0, on);
}

static void g500_set_brightness(int tcmpb0)
{
	unsigned long tcfg0;
	unsigned long tcfg1;
	unsigned long tcon;

	/* configure power on/off */
	g500_backlight_power(tcmpb0 ? 1 : 0);


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

static struct s3c2410_bl_mach_info g500_bl_cfg __initdata = {

	.backlight_max          = 0x2c,
	.backlight_default      = 0x16,
	.backlight_power	= g500_backlight_power,
	.set_brightness		= g500_set_brightness,
	.backlight_power	= g500_backlight_power,
	.lcd_power		= g500_lcd_power
};

static struct s3c24xx_led_platdata g500_vibrator_pdata = {
	.gpio		= S3C2410_GPA6,
	.flags		= S3C24XX_LEDF_ACTLOW,
	.name		= "vibrator",
	.def_trigger	= "none",
};

static struct platform_device g500_vibrator = {
	.name		= "s3c24xx_led",
	.id		= 1,
	.dev		= {
		.platform_data = &g500_vibrator_pdata,
	},
};

static struct platform_device *g500_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
	&s3c_device_nand,
	&s3c_device_usbgadget,
	&s3c_device_sdi,
	&s3c_device_ts,
	&s3c_device_buttons,
	&s3c_device_bl,
	/* soon : &g500_vibrator,*/
};

static struct s3c24xx_board g500_board __initdata = {
	.devices       = g500_devices,
	.devices_count = ARRAY_SIZE(g500_devices)
};

static void __init g500_map_io(void)
{
	s3c_device_nand.dev.platform_data = &g500_nand_info;
	s3c24xx_init_io(g500_iodesc, ARRAY_SIZE(g500_iodesc));
	s3c24xx_init_clocks(0);
	s3c24xx_init_uarts(g500_uartcfgs, ARRAY_SIZE(g500_uartcfgs));
	s3c24xx_set_board(&g500_board);
}

static void __init g500_machine_init(void)
{
	s3c24xx_fb_set_platdata(&g500_lcd_cfg);
	s3c_device_sdi.dev.platform_data = &g500_mmc_cfg;
	set_s3c2410ts_info(&g500_ts_cfg);
	set_s3c2410bl_info(&g500_bl_cfg);

	s3c2410_pm_init();

	/* wake up source */
	enable_irq_wake(IRQ_EINT1);
}

MACHINE_START(G500, "G500")
	/* Maintainer: Pierre Hebert <pierrox@pierrox.net> */
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,

	.init_irq	= s3c24xx_init_irq,
	.map_io		= g500_map_io,
	.init_machine	= g500_machine_init,
	.timer		= &s3c24xx_timer,
MACHINE_END
