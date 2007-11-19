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

/* $Id: cpup.h,v 1.15 2007/05/14 03:41:52 sfjro Exp $ */

#ifndef __AUFS_CPUP_H__
#define __AUFS_CPUP_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/aufs_type.h>

static inline
void au_cpup_attr_blksize(struct inode *inode, struct inode *h_inode)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	inode->i_blksize = h_inode->i_blksize;
#endif
}

void au_cpup_attr_timesizes(struct inode *inode);
void au_cpup_attr_nlink(struct inode *inode);
void au_cpup_attr_changable(struct inode *inode);
void au_cpup_igen(struct inode *inode, struct inode *h_inode);
void au_cpup_attr_all(struct inode *inode);

#define CPUP_DTIME		1	// do dtime_store/revert
// todo: remove this
#define CPUP_LOCKED_GHDIR	2	// grand parent hidden dir is locked
unsigned int au_flags_cpup(unsigned int init, struct dentry *parent);

int cpup_single(struct dentry *dentry, aufs_bindex_t bdst, aufs_bindex_t bsrc,
		loff_t len, unsigned int flags);
int sio_cpup_single(struct dentry *dentry, aufs_bindex_t bdst,
		    aufs_bindex_t bsrc, loff_t len, unsigned int flags);
int cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		unsigned int flags);
int sio_cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		    unsigned int flags);

int cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst, struct dentry *locked);
int test_and_cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst,
		       struct dentry *locked);

/* keep timestamps when copyup */
struct dtime {
	struct dentry *dt_dentry, *dt_h_dentry;
	struct timespec dt_atime, dt_mtime;
};
void dtime_store(struct dtime *dt, struct dentry *dentry,
		 struct dentry *h_dentry);
void dtime_revert(struct dtime *dt, int h_parent_is_locked);

#endif /* __KERNEL__ */
#endif /* __AUFS_CPUP_H__ */
