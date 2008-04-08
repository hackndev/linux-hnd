/*
 * Bluetooth support file for calling bluetooth configuration functions
 *
 * Copyright (C) 2008 Marek Vasut <marek.vasut@gmail.com>
 */

#ifndef _PALMTT3_BT_H
#define _PALMTT3_BT_H

struct uart_port;

struct palmtt3_bt_funcs {
	void (*configure)(int state);
};

#endif
