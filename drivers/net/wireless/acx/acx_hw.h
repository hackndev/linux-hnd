/*
 * Interface for ACX slave memory driver
 *
 * Copyright (c) 2006 SDG Systems, LLC
 *
 * GPL
 *
 */

#ifndef _ACX_HW_H
#define _ACX_HW_H

struct acx_hardware_data {
	int (*start_hw)( void );
	int (*stop_hw)( void );
};

#endif /* _ACX_HW_H */
