/*
 * Bluetooth support file for calling bluetooth configuration functions
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-06	Todd Blumer             Initial Revision
 */

#ifndef _HTCUNIVERSAL_BT_H
#define _HTCUNIVERSAL_BT_H

struct htcuniversal_bt_funcs {
	void (*configure) ( int state );
};


#endif
