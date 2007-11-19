/*
 * Copyright (C) 2005, 2006, 2007 Junjiro Okajima
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

/* $Id: aufs.h,v 1.26 2007/06/04 02:15:32 sfjro Exp $ */

#ifndef __AUFS_H__
#define __AUFS_H__

#ifdef __KERNEL__

#include <linux/version.h>

/* limited support before 2.6.16, curretly 2.6.15 only. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define atomic_long_t		atomic_t
#define atomic_long_set		atomic_set
#define timespec_to_ns(ts)	({(long long)(ts)->tv_sec;})
#define D_CHILD			d_child
#else
#define D_CHILD			d_u.d_child
#endif

/* ---------------------------------------------------------------------- */

#include "debug.h"

#include "branch.h"
#include "cpup.h"
#include "dcsub.h"
#include "dentry.h"
#include "dir.h"
#include "file.h"
#include "inode.h"
#include "misc.h"
#include "module.h"
#include "opts.h"
#include "super.h"
#include "sysaufs.h"
#include "vfsub.h"
#include "whout.h"
#include "wkq.h"
//#include "xattr.h"

#define AuUse_ISSUBDIR
#ifdef CONFIG_AUFS_MODULE

/* call ksize() or not */
#ifndef CONFIG_AUFS_KSIZE_PATCH
#define ksize(p)	(0U)
#endif

/* call is_subdir() or not */
#ifndef CONFIG_AUFS_ISSUBDIR_PATCH
#undef AuUse_ISSUBDIR
#endif

#endif /* CONFIG_AUFS_MODULE */

#endif /* __KERNEL__ */
#endif /* __AUFS_H__ */
