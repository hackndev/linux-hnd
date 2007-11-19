/*
 * UDC functions for the Toshiba e-series PDAs
 *
 * Copyright (c) Ian Molton 2003
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/mach/map.h>
#include <asm/domain.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/eseries-gpio.h>
#include <linux/device.h>
#include <asm/arch/udc.h>

/* local PXA generic code */
#include "../generic.h"

struct udc_info {
	int	discover;
	int	pullup;
};

/* E740 E750 and E400 all use the same GPIOs */
static struct udc_info e7xx_udc = {
	.discover      = GPIO_E7XX_USB_DISC,
	.pullup        = GPIO_E7XX_USB_PULLUP
};

/* E800 is different, just to be awkward */
static struct udc_info e800_udc = {
        .discover      = GPIO_E800_USB_DISC,
        .pullup        = GPIO_E800_USB_PULLUP
};

static struct udc_info *udc;

static int eseries_udc_is_connected(void) {
	return (GPLR(udc->discover) & GPIO_bit(udc->discover));
}

static void eseries_udc_command(int cmd) {
	switch(cmd){
		case PXA2XX_UDC_CMD_DISCONNECT:
			GPSR(udc->pullup) = GPIO_bit(udc->pullup);
			break;
		case PXA2XX_UDC_CMD_CONNECT:
			GPCR(udc->pullup) = GPIO_bit(udc->pullup);
			break;
		default:
			printk("eseries_udc_control: unknown command!\n");
			break;
	}
}

static struct pxa2xx_udc_mach_info eseries_udc_mach_info = {
	.udc_is_connected = eseries_udc_is_connected,
	.udc_command      = eseries_udc_command,
};

static int __init eseries_udc_init(void) {
	if(machine_is_e330() || machine_is_e740() || machine_is_e750() ||
	   machine_is_e400())
		udc = &e7xx_udc;
	else if(machine_is_e800())
		udc = &e800_udc;
	else
		return -ENODEV;

	pxa_set_udc_info(&eseries_udc_mach_info);
	return 0;
}

module_init(eseries_udc_init);
