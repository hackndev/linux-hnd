/*
 * Copyright (C) 2007 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* $Id: module.h,v 1.8 2007/05/14 03:41:52 sfjro Exp $ */

#ifndef __AUFS_MODULE_H__
#define __AUFS_MODULE_H__

#ifdef __KERNEL__

#include <linux/slab.h>

/* ---------------------------------------------------------------------- */

/* module parameters */
extern short aufs_nwkq;
extern int sysaufs_brs;

/* ---------------------------------------------------------------------- */

extern char au_esc_chars[];
extern int au_dir_roflags;

/* kmem cache */
enum {AuCache_DINFO, AuCache_ICNTNR, AuCache_FINFO, AuCache_VDIR,
      AuCache_DEHSTR, AuCache_HINOTIFY, AuCache_Last};
extern struct kmem_cache *aufs_cachep[];

#define CacheFuncs(name, index) \
static inline void *cache_alloc_##name(void) \
{return kmem_cache_alloc(aufs_cachep[index], GFP_KERNEL);} \
static inline void cache_free_##name(void *p) \
{kmem_cache_free(aufs_cachep[index], p);}

CacheFuncs(dinfo, AuCache_DINFO);
CacheFuncs(icntnr, AuCache_ICNTNR);
CacheFuncs(finfo, AuCache_FINFO);
CacheFuncs(vdir, AuCache_VDIR);
CacheFuncs(dehstr, AuCache_DEHSTR);
CacheFuncs(hinotify, AuCache_HINOTIFY);

#undef CacheFuncs

#endif /* __KERNEL__ */
#endif /* __AUFS_MODULE_H__ */
