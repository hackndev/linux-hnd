/* linux/include/asm/hardware/hamcop-sdi.h
 *
 * Copyright (c) 2004 Simtec Electronics <linux@simtec.co.uk>
 *		      http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * HAMCOP MMC/SDIO register definitions
 * Adapted from S3C2410 driver by Matt Reimer.
*/

#ifndef __ASM_HARDWARE_HAMCOP_SDI
#define __ASM_HARDWARE_HAMCOP_SDI "hamcop-sdi.h"

#define HAMCOP_SDICON                (0x00)
#define HAMCOP_SDIPRE                (0x02)
#define HAMCOP_SDICMDARGH            (0x04)
#define HAMCOP_SDICMDARGL            (0x06)
#define HAMCOP_SDICMDCON             (0x08)
#define HAMCOP_SDICMDSTAT            (0x0A)
#define HAMCOP_SDIRSP0H              (0x0C)
#define HAMCOP_SDIRSP0L              (0x0E)
#define HAMCOP_SDIRSP1H              (0x10)
#define HAMCOP_SDIRSP1L              (0x12)
#define HAMCOP_SDIRSP2H              (0x14)
#define HAMCOP_SDIRSP2L              (0x16)
#define HAMCOP_SDIRSP3H              (0x18)
#define HAMCOP_SDIRSP3L              (0x1A)
#define HAMCOP_SDITIMER1             (0x1C)
#define HAMCOP_SDITIMER2             (0x1E)
#define HAMCOP_SDIBSIZE              (0x20)
#define HAMCOP_SDIBNUM               (0x22)
#define HAMCOP_SDIDCON               (0x24)
#define HAMCOP_SDIDCNT1              (0x26)
#define HAMCOP_SDIDCNT2              (0x28)
#define HAMCOP_SDIDSTA               (0x2A)
#define HAMCOP_SDIFSTA               (0x2C)
#define HAMCOP_SDIDATA               (0x60)
#define HAMCOP_SDIIMSK1              (0x30)
#define HAMCOP_SDIIMSK2              (0x32)

#define HAMCOP_SDICON_CLOCKTYPE_SD   (0 << 0)
#define HAMCOP_SDICON_CLOCKTYPE_MMC  (1 << 0)

#define HAMCOP_SDIBNUM_MASK	     (0xfff)

#endif /* __ASM_HARDWARE_HAMCOP_SDI */
