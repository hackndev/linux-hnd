/* linux/arch/arm/mach-s3c2442/mach-htctrinity.c
 *
 * Based on the code by Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
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
#include <linux/input_pda.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <linux/mfd/asic3_base.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

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
#include <asm/arch/regs-gpio.h>

#include <asm/arch/irqs.h>
#include <asm/arch/nand.h>
#include <asm/arch/fb.h>

#include <asm/plat-s3c24xx/clock.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>

static struct map_desc htctrinity_iodesc[] __initdata = {
};


#if 0
static struct s3c24xx_uart_clksrc htctrinity_serial_clocks[] = {
	[0] = {
		.name		= "fclk",
		.divisor	= 0,
		.min_baud	= 0,
		.max_baud	= 0,
	}
};
#endif

static struct s3c2410_uartcfg htctrinity_uartcfgs[] = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3fc5,
		.ulcon	     = 0x03,
		.ufcon	     = 0xf1,
//		.clocks	     = htctrinity_serial_clocks,
//		.clocks_size = ARRAY_SIZE(htctrinity_serial_clocks),
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0xfc5,
		.ulcon	     = 0x03,
		.ufcon	     = 0xf1,
//		.clocks	     = htctrinity_serial_clocks,
//		.clocks_size = ARRAY_SIZE(htctrinity_serial_clocks),
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.uart_flags  = UPF_CONS_FLOW,
		.ucon	     = 0x8fc5,
		.ulcon	     = 0x43,
		.ufcon	     = 0xf1,
//		.clocks	     = htctrinity_serial_clocks,
//		.clocks_size = ARRAY_SIZE(htctrinity_serial_clocks),
	}
};

static struct mtd_partition htctrinity_nand_part[] = {
	[0] = {
		.name		= "Whole Flash",
		.offset		= 0,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= MTD_WRITEABLE,
	}
};

static struct s3c2410_nand_set htctrinity_nand_sets[] = {
	[0] = {
		.name		= "Internal",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(htctrinity_nand_part),
		.partitions	= htctrinity_nand_part,
	},
};

static struct s3c2410_platform_nand htctrinity_nand_info = {
	.tacls		= 0,	// 1
	.twrph0		= 20,	// 3
	.twrph1		= 0,	// 1
	.nr_sets	= ARRAY_SIZE(htctrinity_nand_sets),
	.sets		= htctrinity_nand_sets,
};

#if 0
static struct gpio_keys_button htctrinity_gpio_keys_table[] = {
	{KEY_POWER,	S3C2410_GPF0,	1,	"Power button"}, /* power*/
	{KEY_RECORD,	S3C2410_GPF7,	1,	"Record button"}, /* camera */
	{KEY_CALENDAR,	S3C2410_GPG0,	1,	"Calendar button"}, /* play */
	{KEY_CONTACTS,	S3C2410_GPG2,	1,	"Contacts button"}, /* person */
	{KEY_MAIL,	S3C2410_GPG3,	1,	"Mail button"}, /* nevo */
	{KEY_HOMEPAGE,	S3C2410_GPG7,	1,	"Home button"}, /* arrow */
	{KEY_ENTER,	S3C2410_GPG9,	1,	"Ok button"}, /* ok */
};

static struct gpio_keys_platform_data htctrinity_gpio_keys_data = {
	.buttons	= htctrinity_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(htctrinity_gpio_keys_table),
};

static struct platform_device s3c_device_gpiokeys = {
	.name		= "gpio-keys",
	.dev = { .platform_data = &htctrinity_gpio_keys_data, }
};
#endif

static struct platform_device *htctrinity_devices[] __initdata = {
//	&s3c_device_usb,
//	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_iis,
	&s3c_device_nand,
//	&s3c_device_gpiokeys,
};

static struct s3c24xx_board htctrinity_board __initdata = {
	.devices       = htctrinity_devices,
	.devices_count = ARRAY_SIZE(htctrinity_devices)
};

static void __init htctrinity_map_io(void)
{
	s3c_device_nand.dev.platform_data = &htctrinity_nand_info;

	s3c24xx_init_io(htctrinity_iodesc, ARRAY_SIZE(htctrinity_iodesc));
	s3c24xx_init_clocks(0);
	s3c24xx_init_uarts(htctrinity_uartcfgs, ARRAY_SIZE(htctrinity_uartcfgs));
	s3c24xx_set_board(&htctrinity_board);
}

static void __init htctrinity_init_irq(void)
{
	s3c24xx_init_irq();
}

static void __init htctrinity_init_machine(void)
{
	s3c2410_pm_init();
//	s3c24xx_fb_set_platdata(&htctrinity_lcdcfg);
}


MACHINE_START(HTC_TRINITY, "HTC Trinity")
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.map_io		= htctrinity_map_io,
	.init_irq	= htctrinity_init_irq,
	.init_machine	= htctrinity_init_machine,
	.timer		= &s3c24xx_timer,
MACHINE_END
