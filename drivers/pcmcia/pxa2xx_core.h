/*
 * linux/include/asm/arch/pcmcia.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 * Copyright (C) 2003 Stefan Eletzhofer <stefan.eletzhofer@inqunat.de>
 * Copyright (C) 2003 Ian Molton <spyro@f2s.com>
 *
 * This file contains definitions for the low-level PXA2xx kernel PCMCIA
 * interface. Please see linux/Documentation/arm/SA1100/PCMCIA for details.
 */
#ifndef _ASM_ARCH_PCMCIA
#define _ASM_ARCH_PCMCIA

/* include the world */
#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

#include <asm/arch/pcmcia.h>

/* Ideally, we'd support up to MAX_SOCK sockets, but the PXA only
 * has support for two. This shows up in lots of hardwired ways, such
 * as the fact that MECR only has enough bits to configure two sockets.
 * Since it's so entrenched in the hardware, limiting the software
 * in this way doesn't seem too terrible.
 */
#define PXA2XX_PCMCIA_MAX_SOCK   (2)


#endif
