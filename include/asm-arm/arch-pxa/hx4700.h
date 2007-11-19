/*
 * HP iPaq hx4700 general definitions.
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-06	Todd Blumer             Initial Revision
 * 2007-04	Paul Sokolovsky		Converted to general hx4700 header.
 */

#ifndef _HX4700_H
#define _HX4700_H

#include <asm/arch/irqs.h>
#include <linux/gpiodev2.h>

#define HX4700_ASIC3_IRQ_BASE IRQ_BOARD_START
#define HX4700_ASIC3_GPIO_BASE GPIO_BASE_INCREMENT


struct hx4700_bt_funcs {
	void (*configure) ( int state );
};


#endif
