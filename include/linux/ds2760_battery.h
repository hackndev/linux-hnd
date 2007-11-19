/*
 * Driver for batteries with DS2760 chips inside.
 *
 * Copyright (c) 2007 Anton Vorontsov
 *               2004-2007 Matt Reimer
 *               2004 Szabolcs Gyurko
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author:  Anton Vorontsov <cbou@mail.ru>
 *          February 2007
 *
 *          Matt Reimer <mreimer@vpop.net>
 *          April 2004, 2005, 2007
 *
 *          Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>
 *          September 2004
 */

#ifndef __DS2760_BATTERY__
#define __DS2760_BATTERY__

#include <linux/battery.h>

struct ds2760_platform_data {
	/* Battery information and characteristics. */
	struct battery_info battery_info;
};

#endif
