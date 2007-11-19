/*
 * Copyright 2005 Erik Hovland
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef _SAMCOP_FSI_H_
#define _SAMCOP_FSI_H_

struct samcop_fsi {
	struct device 		*parent;	// parent struct for access to
						// samcop registers
	void			*map;		// fsi register map
	int			irq;		// data ready irq
	struct clk		*clk;		// samcop fsi clk
};

extern void samcop_fsi_set_control (struct samcop_fsi *fsi, u32 val);
extern u32 samcop_fsi_get_control (struct samcop_fsi *fsi);
extern void samcop_fsi_set_status (struct samcop_fsi *fsi, u32 val);
extern u32 samcop_fsi_get_status (struct samcop_fsi *fsi);
extern u32 samcop_fsi_read_data (struct samcop_fsi *fsi);
extern void samcop_fsi_up (struct samcop_fsi *fsi);
extern void samcop_fsi_down (struct samcop_fsi *fsi);
extern void samcop_fsi_set_fifo_control (struct samcop_fsi *fsi, u32 val);
extern void samcop_fsi_set_prescaler (struct samcop_fsi *fsi, u32 val);
extern void samcop_fsi_set_dmc (struct samcop_fsi *fsi, u32 val);

extern int fsi_attach (struct samcop_fsi *fsi);
extern void fsi_detach (void);

#endif /* _SAMCOP_FSI_H_ */
