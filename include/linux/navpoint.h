/*
 * Synaptics NavPoint (tm)
 * Uses Modular Embedded Protocol (MEP)
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _NAVPOINT_H
#define _NAVPOINT_H

#include <linux/input.h>
#include <linux/wait.h>

struct navpoint_features {
	unsigned char xunits_per_mm;
	unsigned char yunits_per_mm;
	unsigned char buttons;
	unsigned portrait:1;
	unsigned rot180:1;
	unsigned palm_det:1;
	unsigned multi_fing:1;
	unsigned wheel:1;
	unsigned vscroll:1;
	unsigned hscroll:1;
};

struct navpoint_dev {
	int unit;
	int (*tx_func)( u_int8_t *data, int count );
	void *priv;
	void (*cb_func)( void *priv, u_int8_t *data );
	struct navpoint_features features;
	void *driver_data;
};

int navpoint_features( struct navpoint_dev *navpt, struct navpoint_features **features );
void navpoint_reset( struct navpoint_dev *navpt );
int navpoint_init( struct navpoint_dev *navpt );
void navpoint_rx( struct navpoint_dev *navpt, unsigned char rxb, unsigned int pending );
void navpoint_exit( struct navpoint_dev *navpt );

#endif /* _NAVPOINT_H */
