/*
 *  Asus MyPal A716 Hardware definitions
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (C) 2005-2007 Pawel Kolodziejski
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/apm-emulation.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/arch/udc.h>

#include "../generic.h"

int a716_wireless_switch;
extern int a716_wifi_pcmcia_detect;

extern struct platform_device a716_bl;
static struct platform_device a716_gpo = { .name = "a716-gpo", };
static struct platform_device a716_lcd = { .name = "a716-lcd", };
static struct platform_device a716_buttons = { .name = "a716-buttons", };
static struct platform_device a716_ssp = { .name = "a716-ssp", };
static struct platform_device a716_pcmcia = { .name = "a716-pcmcia", };
static struct platform_device a716_irda = { .name = "a716-irda", };
static struct platform_device a716_bt = { .name = "a716-bt", };
static struct platform_device a716_udc = { .name = "a716-udc", };
static struct platform_device a716_mci = { .name = "a716-mci", };
static struct platform_device a716_pm = { .name = "a716-pm", };

static struct platform_device *devices[] __initdata = {
	&a716_gpo,
	&a716_lcd,
	&a716_bl,
	&a716_buttons,
	&a716_ssp,
	&a716_pcmcia,
	&a716_udc,
	&a716_irda,
	&a716_bt,
	&a716_mci,
	&a716_pm,
};

static int a716_wireless_switch_read(char *buf, char **start, off_t fpos, int length, int *eof, void *data)
{
	if (a716_wireless_switch == 1)
		buf[0] = '1';
	else if (a716_wireless_switch == 2)
		buf[0] = '2';
	else
		buf[0] = '0';

	buf[1] = '\0';

	return 2;
}

static int a716_wireless_switch_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	if (count != 2)
		return -EIO;

	if (a716_wireless_switch == (buffer[0] - 0x30))
		return count;

	if (buffer[0] == '1') {
	} else if (buffer[0] == '2') {
		a716_wifi_pcmcia_detect = 1;
		a716_wireless_switch = 2;
	} else {
		if (a716_wireless_switch == 2) {
			a716_wifi_pcmcia_detect = 0;
			a716_wireless_switch = 0;
		}
	}

	return count;
}

static void a716_register_wireless_proc(void)
{
	struct proc_dir_entry *entry = NULL;

	a716_wireless_switch = 0;

	entry = create_proc_info_entry("a716_wireless_switch", S_IWUSR | S_IRUGO, entry, NULL);
	if (entry) {
		entry->read_proc = a716_wireless_switch_read;
		entry->write_proc = a716_wireless_switch_write;
        }
}

static void __init a716_init(void)
{
	PGSR0 = 0x00080000;
	PGSR1 = 0x03ff0300;
	PGSR2 = 0x00010400;

	//GPSR0 = 0x00080000; // don't set gpio(19) it hangs pda
	GPSR1 = 0x00BF0000;
	GPSR2 = 0x00000400;

	GPCR0 = 0xD3830040;
	GPCR1 = 0xFC40AB81;
	GPCR2 = 0x00013BFF;

	GPDR0 = 0xD38B0040;
	GPDR1 = 0xFCFFAB81;
	GPDR2 = 0x00013FFF;

	GAFR0_L = 0x00001004;
	GAFR0_U = 0x591A8002;
	GAFR1_L = 0x99908011;
	GAFR1_U = 0xAAA5AAAA;
	GAFR2_L = 0x0A8AAAAA;
	GAFR2_U = 0x00000002;

	PWER = PWER_RTC | PWER_GPIO0;
	PFER = PWER_GPIO0;
	PRER = 0;
	PEDR = 0;
	PCFR = PCFR_OPDE;

	platform_add_devices(devices, ARRAY_SIZE(devices));

	a716_register_wireless_proc();
}

MACHINE_START(A716, "Asus MyPal A716")
	/* Maintainer: Pawel Kolodziejski */
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params	= 0xa0000100,
	.map_io		= pxa_map_io,
	.init_irq	= pxa_init_irq,
	.timer		= &pxa_timer,
	.init_machine 	= a716_init,
MACHINE_END
