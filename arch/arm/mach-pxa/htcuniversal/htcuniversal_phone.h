/*
 * Bluetooth support file for calling bluetooth configuration functions
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * 2005-06	Todd Blumer             Initial Revision
 */

#ifndef _HTCUNIVERSAL_PHONE_H
#define _HTCUNIVERSAL_PHONE_H

struct htcuniversal_phone_funcs {
	void (*configure) ( int state );
};

#endif
