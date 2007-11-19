/*
 *
 * Definitions for H3600 Handheld Computer
 *
 * Copyright 2000 Compaq Computer Corporation.
 * Copyright 2005 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 * 
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2001-10-??   Andrew Christian   Added support for iPAQ H3800
 * 2005-04-02   Holger Hans Peter Freyther migrate to own file for the 2.6 port
 *
 */

#ifndef _IPAQ_OPS_H_
#define _IPAQ_OPS_H_

#ifndef __ASSEMBLY__

enum led_color {
	BLUE_LED,
	GREEN_LED,
	YELLOW_LED,
	RED_LED
};

#define ipaq_led_on(color)  \
        ipaqsa_set_led(color, 4, 4)

#define ipaq_led_off(color)  \
        ipaqsa_set_led(color, 0, 0)

#define ipaq_led_blink(color,rate, pts)  \
        ipaqsa_set_led(color, rate, pts)

extern void ipaq_set_led (enum led_color color, int cycle_time, int duty_time);

enum ipaq_egpio_type {
	IPAQ_EGPIO_LCD_POWER,     /* Power to the LCD panel */
	IPAQ_EGPIO_CODEC_NRESET,  /* Clear to reset the audio codec (remember to return high) */
	IPAQ_EGPIO_AUDIO_ON,      /* Audio power */
	IPAQ_EGPIO_QMUTE,         /* Audio muting */
	IPAQ_EGPIO_OPT_NVRAM_ON,  /* Non-volatile RAM on extension sleeves (SPI interface) */
	IPAQ_EGPIO_OPT_ON,        /* Power to extension sleeves */
	IPAQ_EGPIO_CARD_RESET,    /* Reset PCMCIA cards on extension sleeve (???) */
	IPAQ_EGPIO_OPT_RESET,     /* Reset option pack (???) */
	IPAQ_EGPIO_IR_ON,         /* IR sensor/emitter power */
	IPAQ_EGPIO_IR_FSEL,       /* IR speed selection 1->fast, 0->slow */
	IPAQ_EGPIO_RS232_ON,      /* Maxim RS232 chip power */
	IPAQ_EGPIO_VPP_ON,        /* Turn on power to flash programming */
	IPAQ_EGPIO_LCD_ENABLE,    /* Enable/disable LCD controller */
	IPAQ_EGPIO_PCMCIA_CD0_N,  /* pcmcia socket 0 card detect (read-only) */
	IPAQ_EGPIO_PCMCIA_CD1_N,  /* pcmcia socket 1 card detect (read-only) */
	IPAQ_EGPIO_PCMCIA_IRQ0,   /* pcmcia socket 0 card irq (read-only) */
	IPAQ_EGPIO_PCMCIA_IRQ1,   /* pcmcia socket 1 card irq (read-only) */
	IPAQ_EGPIO_BLUETOOTH_ON,  /* Bluetooth module power */
};

struct ipaq_model_ops {
	const char     *generic_name;
	void          (*control)(enum ipaq_egpio_type, int);
	/* Data to be stashed at wakeup */
	u32           gedr;
	u32           icpr;

	void	      (*set_led)(enum led_color color, int duty_time, int cycle_time);
#ifndef CONFIG_LCD_DEVICE
	void	      (*backlight_power)(int on);
#endif /* CONFIG_LCD_DEVICE */
};

extern struct ipaq_model_ops ipaq_model_ops;
static __inline__ const char * ipaqsa_generic_name( void ) {
	return ipaq_model_ops.generic_name;
}

static __inline__ void assign_ipaqsa_egpio( enum ipaq_egpio_type x, int level ) {
	if (ipaq_model_ops.control)
		ipaq_model_ops.control(x,level);
}

void ipaqsa_map_io(void);
void ipaqsa_mtd_set_vpp(int vpp);
void ipaqsa_mach_init(void);
unsigned long ipaqsa_common_read_egpio( enum ipaq_egpio_type x);
#endif
#endif /* _IPAQ_OPS_H_ */
