/*
 * vsfb.h
 *
 * (c) 2006 Ian Molton <spyro@f2s.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _VSFB_H_INCLUDED
#define _VSFB_H_INCLUDED

#include <linux/fb.h>

struct vsfb_deviceinfo
{
	struct fb_var_screeninfo *var;
	struct fb_fix_screeninfo *fix;
};

#endif
