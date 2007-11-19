/*
 * Bluetooth support file for calling bluetooth configuration functions
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-06	Todd Blumer             Initial Revision
 */

#ifndef _HTCSABLE_BT_H
#define _HTCSABLE_BT_H

struct htcsable_bt_funcs {
	void (*configure) ( int state );
};


#endif
