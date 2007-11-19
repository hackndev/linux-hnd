/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restrutured June 2002
*/

#ifndef H3600_ASIC_CORE_H
#define H3600_ASIC_CORE_H

/* Statistics */
struct asic_statistics {
	u32  spi_bytes;   /* Bytes handled by SPI */
	u32  spi_timeout; /* Timeouts in transmission */
	u32  spi_wip;     /* Write in process */

	u32  mmc_insert;
	u32  mmc_eject;
	u32  mmc_reset;    /* Reset commands issued */
	u32  mmc_command;  /* General commands issued */
	u32  mmc_read;     /* Blocks */
	u32  mmc_written;  /* Blocks */
	u32  mmc_timeout;  /* Timeout interrupts */
	u32  mmc_error;    /* Response errors */
};

struct owm_statistics {
	u32  owm_timeout;
	u32  owm_reset;
	u32  owm_written;
	u32  owm_read;
	u32  owm_valid_isr[5];
	u32  owm_invalid_isr[5];
	u32  owm_post_isr[5];
};

extern struct asic_statistics g_h3600_asic_statistics;

#include "../common/ipaq/asic2_shared.h"

extern int battery_sample_interval;
extern int ds2760_sample_interval;
extern int sleep_battery_interval;
extern int owm_sample_delay;
extern int owm_reset_delay;
extern int owm_rw_delay;


#endif /* H3600_ASIC_CORE_H */
