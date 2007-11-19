/*
 * linux/drivers/video/imageon/imageon.c
 *
 * Frame Buffer Device for ATI Imageon
 *
 * Copyright (C) 2004, Damian Bentley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on:
 *  ATI's w100fb.h Wallaby code
 *  Ian Molton's vsfb.c (based on acornfb by Russell King)
 *
 * ChangeLog:
 *  2004-01-03 Created  
 */


#include <linux/init.h>
#include <linux/module.h>

#include "default.h"

#ifdef W100FB_BASE
	#include "w100.h"
#endif /* W100FB_BASE */

#ifdef W1030FB_BASE
	#include "w1030.h"
#endif /* W1030FB_BASE */

/******************************************************************************/

int __init imageonfb_init(void)
{
#ifdef W100FB_BASE
	w100fb_init();
#endif /* W100FB_BASE */

#ifdef W1030FB_BASE
	w1030fb_init();
#endif /* W1030FB_BASE */

	MOD_INC_USE_COUNT;

	return 0;
}

static void __exit imageonfb_cleanup(void)
{
#ifdef W100FB_BASE
	w100fb_cleanup();
#endif /* W100FB_BASE */


#ifdef W1030FB_BASE
	w1030fb_cleanup();
#endif /* W1030FB_BASE */

	MOD_DEC_USE_COUNT;
}

MODULE_DESCRIPTION("ATI Imageon framebuffer driver");
MODULE_SUPPORTED_DEVICE("fb");
