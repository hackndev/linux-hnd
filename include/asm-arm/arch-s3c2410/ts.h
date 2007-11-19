/* linux/include/asm/arch-s3c2410/ts.h
 *
 * Copyright (c) 2005 Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *  Changelog:
 *     24-Mar-2005     RTP     Created file
 *     03-Aug-2005     RTP     Renamed to ts.h
 */

#ifndef __ASM_ARM_TS_H
#define __ASM_ARM_TS_H

struct s3c2410_ts_mach_info {
       int             delay;
       int             presc;
       int             oversampling_shift;
#ifdef TOUCHSCREEN_S3C2410_ALT
       int             xp_threshold;
#endif
};

void __init set_s3c2410ts_info(struct s3c2410_ts_mach_info *hard_s3c2410ts_info);

#endif /* __ASM_ARM_TS_H */

