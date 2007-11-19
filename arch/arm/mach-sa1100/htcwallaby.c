/*
   linux/arch/arm/mach-sa1100/xda.c
   Copyright (C) 2005 Henk Vergonet

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/
/* CREDITS:
 *   w4xy at xanadux dot org 
 *	For making the xda-linux port on linux 2.4.
 *   www.xda-developers.com
 *	For the excellent documentation provided on their site.
*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>

#include <asm/irq.h>
#include <asm/setup.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <asm/mach/flash.h>
#include <asm/arch/htcwallaby.h>

#include "generic.h"

/* copy of the ASIC_A6 register */
uint __xda_save_asic_a6;
EXPORT_SYMBOL(__xda_save_asic_a6);

/***********************************
 * XDA system flash                *
 ***********************************/

static struct mtd_partition xda_partitions[] = {
	{
		.name		= "bootldr",
		.size		= 0x00040000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "rootfs",
		.size		= MTDPART_SIZ_FULL,
		.offset		= MTDPART_OFS_APPEND,
	}
};

static void xda_set_vpp(int vpp)
{
//	if (vpp)   FIXME
//		PPSR |= PPC_LDD7;
//	else
//		PPSR &= ~PPC_LDD7;
//	PPDR |= PPC_LDD7;
};

static struct flash_platform_data xda_flash_data = {
	.map_name	= "cfi_probe",
	.parts		= xda_partitions,
	.nr_parts	= ARRAY_SIZE(xda_partitions),
	.set_vpp	= xda_set_vpp,
};

static struct resource xda_flash_resource = {
	.start	= SA1100_CS0_PHYS,
	.end	= SA1100_CS0_PHYS + SZ_32M - 1,
	.flags	= IORESOURCE_MEM,
};

/***********************************
 * XDA uart2  irDA / rs232	   *
 ***********************************/

static struct resource xda_uart_resources[] = {
	[0] = {
		.start	= 0x80030000,
		.end	= 0x8003ffff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device xda_uart_device = {
	.name		= "sa11x0-uart",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(xda_uart_resources),
	.resource	= xda_uart_resources,
};


/***********************************
 * XDA system functions            *
 ***********************************/

static struct map_desc xda_io_desc[] __initdata = {
 /* virtual     physical    length      type */
  { 0xe8000000, 0x00000000, 0x02000000, MT_DEVICE	}, /* Flash 0 */
  { 0xf0000000, 0x18000000, 0x00100000, MT_DEVICE	}, /* EGPIO? */
  { 0xf1000000, 0x19000000, 0x00100000, MT_DEVICE	}, /* SD card related */
  { 0xf2000000, 0x40000000, 0x00100000, MT_DEVICE	}, /* EGPIO? */
  { 0xf3000000, 0x49000000, 0x00100000, MT_DEVICE	}, /* EGPIO? */
  { 0xf4000000, 0x4a000000, 0x00100000, MT_DEVICE	}, /* GSM/UART? */
};

static void __init xda_map_io(void)
{
	sa1100_map_io();
	iotable_init(xda_io_desc, ARRAY_SIZE(xda_io_desc));

	sa1100_register_uart(0, 1);	/* gsm bootloader */
	sa1100_register_uart(1, 2);	/* irDA / external rs232 */
	sa1100_register_uart(2, 3);	/* gsm multiplexed protocols */

	Ser1SDCR0 |= SDCR0_UART;
	/* disable IRDA -- UART2 is used as a normal serial port */
	Ser2UTCR4 = 0;
	Ser2HSCR0 = 0;
}

static struct platform_device *xda_devices[] __initdata = {
	&xda_uart_device,
};

static void __init xda_init(void)
{
	platform_add_devices(xda_devices, ARRAY_SIZE(xda_devices));
	sa11x0_set_flash_data(&xda_flash_data, &xda_flash_resource, 1);
}

MACHINE_START(XDA, "HTC Wallaby")
	/* MAINTAINER("henk dot vergonet at gmail dot com") */
//	.phys_ram       = 0xc0000000,
	.phys_io        = 0x80000000,
	.io_pg_offst    = ((0xf8000000) >> 18) & 0xfffc,
	.boot_params    = 0xc0000100,
	.map_io         = xda_map_io,
	.init_irq	= sa1100_init_irq,
	.timer		= &sa1100_timer,
	.init_machine	= xda_init,
MACHINE_END
