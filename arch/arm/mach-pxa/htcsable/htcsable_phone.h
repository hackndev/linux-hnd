/*
 * Bluetooth support file for calling bluetooth configuration functions
 *
 * Copyright (c) 2005 SDG Systems, LLC
 * Copyright (C) 2006 Luke Kenneth Casson Leighton
 *
 * 2005-06	Todd Blumer             Initial Revision
 */

#ifndef _HTCSABLE_PHONE_H
#define _HTCSABLE_PHONE_H

struct uart_port;

struct htcsable_phone_funcs {
	void (*configure) ( int state );
	int (*ioctl)(struct uart_port *port, unsigned int cmd, unsigned long arg);

};

#endif
