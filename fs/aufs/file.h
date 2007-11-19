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

/* $Id: file.h,v 1.25 2007/05/14 03:41:52 sfjro Exp $ */

#ifndef __AUFS_FILE_H__
#define __AUFS_FILE_H__

#ifdef __KERNEL__

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
// SEEK_xxx are defined in linux/fs.h
#else
enum {SEEK_SET, SEEK_CUR, SEEK_END};
#endif

/* ---------------------------------------------------------------------- */

struct aufs_branch;
struct aufs_hfile {
	struct file		*hf_file;
	struct aufs_branch	*hf_br;
};

struct aufs_vdir;
struct aufs_finfo {
	atomic_t			fi_generation;

	struct aufs_rwsem		fi_rwsem;
	struct aufs_hfile		*fi_hfile;
	aufs_bindex_t			fi_bstart, fi_bend;

	union {
		struct vm_operations_struct	*fi_h_vm_ops;
		struct aufs_vdir		*fi_vdir_cache;
	};
};

/* ---------------------------------------------------------------------- */

/* file.c */
extern struct address_space_operations aufs_aop;
unsigned int au_file_roflags(unsigned int flags);
struct file *hidden_open(struct dentry *dentry, aufs_bindex_t bindex,
			 int flags);
int au_do_open(struct inode *inode, struct file *file,
	       int (*open)(struct file *file, int flags));
int au_reopen_nondir(struct file *file);
int au_ready_to_write(struct file *file, loff_t len);
int au_reval_and_lock_finfo(struct file *file, int (*reopen)(struct file *file),
			    int wlock, int locked);

/* f_op.c */
extern struct file_operations aufs_file_fop;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
int aufs_flush(struct file *file, fl_owner_t id);
#else
int aufs_flush(struct file *file);
#endif

/* finfo.c */
struct aufs_finfo *ftofi(struct file *file);
aufs_bindex_t fbstart(struct file *file);
aufs_bindex_t fbend(struct file *file);
struct aufs_vdir *fvdir_cache(struct file *file);
struct aufs_branch *ftobr(struct file *file, aufs_bindex_t bindex);
struct file *au_h_fptr_i(struct file *file, aufs_bindex_t bindex);
struct file *au_h_fptr(struct file *file);

void set_fbstart(struct file *file, aufs_bindex_t bindex);
void set_fbend(struct file *file, aufs_bindex_t bindex);
void set_fvdir_cache(struct file *file, struct aufs_vdir *vdir_cache);
void au_hfput(struct aufs_hfile *hf);
void set_h_fptr(struct file *file, aufs_bindex_t bindex, struct file *h_file);
void au_update_figen(struct file *file);

void au_fin_finfo(struct file *file);
int au_init_finfo(struct file *file);

/* ---------------------------------------------------------------------- */

static inline int au_figen(struct file *f)
{
	return atomic_read(&ftofi(f)->fi_generation);
}

static inline int au_is_mmapped(struct file *f)
{
	return !!(ftofi(f)->fi_h_vm_ops);
}

/* ---------------------------------------------------------------------- */

/*
 * fi_read_lock, fi_write_lock,
 * fi_read_unlock, fi_write_unlock, fi_downgrade_lock
 */
SimpleRwsemFuncs(fi, struct file *f, ftofi(f)->fi_rwsem);

/* to debug easier, do not make them inlined functions */
#define FiMustReadLock(f) do {\
	SiMustAnyLock((f)->f_dentry->d_sb); \
	RwMustReadLock(&ftofi(f)->fi_rwsem); \
} while (0)

#define FiMustWriteLock(f) do { \
	SiMustAnyLock((f)->f_dentry->d_sb); \
	RwMustWriteLock(&ftofi(f)->fi_rwsem); \
} while (0)

#define FiMustAnyLock(f) do { \
	SiMustAnyLock((f)->f_dentry->d_sb); \
	RwMustAnyLock(&ftofi(f)->fi_rwsem); \
} while (0)

#define FiMustNoWaiters(f)	RwMustNoWaiters(&ftofi(f)->fi_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_FILE_H__ */
