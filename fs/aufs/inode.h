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

/* $Id: inode.h,v 1.34 2007/06/04 02:17:35 sfjro Exp $ */

#ifndef __AUFS_INODE_H__
#define __AUFS_INODE_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/inotify.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"
#include "vfsub.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#else
struct inotify_watch {};
#endif

/* ---------------------------------------------------------------------- */

struct aufs_hinotify {
	struct inotify_watch	hin_watch;
	struct inode		*hin_aufs_inode;	/* no get/put */
};

struct aufs_hinode {
	struct inode		*hi_inode;
	aufs_bindex_t		hi_id;
	struct aufs_hinotify	*hi_notify;
};

struct aufs_vdir;
struct aufs_iinfo {
	atomic_t		ii_generation;
	struct super_block	*ii_hsb1;	/* no get/put */

	struct aufs_rwsem	ii_rwsem;
	aufs_bindex_t		ii_bstart, ii_bend;
	struct aufs_hinode	*ii_hinode;
	struct aufs_vdir	*ii_vdir;
};

struct aufs_icntnr {
	struct aufs_iinfo iinfo;
	struct inode vfs_inode;
};

/* ---------------------------------------------------------------------- */

/* inode.c */
int au_refresh_hinode(struct inode *inode, struct dentry *dentry);
struct inode *au_new_inode(struct dentry *dentry);

/* i_op.c */
extern struct inode_operations aufs_iop, aufs_symlink_iop, aufs_dir_iop;
int wr_dir(struct dentry *dentry, int negative, struct dentry *src_dentry,
	   aufs_bindex_t force_btgt, int do_lock_srcdir);

/* i_op_add.c */
int aufs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
int aufs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd);
int aufs_link(struct dentry *src_dentry, struct inode *dir,
	      struct dentry *dentry);
int aufs_mkdir(struct inode *dir, struct dentry *dentry, int mode);

/* i_op_del.c */
int wr_dir_need_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		   struct dentry *locked);
int aufs_unlink(struct inode *dir, struct dentry *dentry);
int aufs_rmdir(struct inode *dir, struct dentry *dentry);

/* i_op_ren.c */
int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry);

/* iinfo.c */
struct aufs_iinfo *itoii(struct inode *inode);
aufs_bindex_t ibstart(struct inode *inode);
aufs_bindex_t ibend(struct inode *inode);
struct aufs_vdir *ivdir(struct inode *inode);
struct inode *au_h_iptr_i(struct inode *inode, aufs_bindex_t bindex);
struct inode *au_h_iptr(struct inode *inode);
aufs_bindex_t itoid_index(struct inode *inode, aufs_bindex_t bindex);

void set_ibstart(struct inode *inode, aufs_bindex_t bindex);
void set_ibend(struct inode *inode, aufs_bindex_t bindex);
void set_ivdir(struct inode *inode, struct aufs_vdir *vdir);
void aufs_hiput(struct aufs_hinode *hinode);
#define AUFS_HI_XINO	1
#define AUFS_HI_NOTIFY	2
unsigned int au_hi_flags(struct inode *inode, int isdir);
void set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		struct inode *h_inode, unsigned int flags);
void au_update_iigen(struct inode *inode);
void au_update_brange(struct inode *inode, int do_put_zero);

int au_iinfo_init(struct inode *inode);
void au_iinfo_fin(struct inode *inode);

/* plink.c */
#ifdef CONFIG_AUFS_DEBUG
void au_list_plink(struct super_block *sb);
#else
static inline void au_list_plink(struct super_block *sb)
{
	/* nothing */
}
#endif
int au_is_plinked(struct super_block *sb, struct inode *inode);
struct dentry *lkup_plink(struct super_block *sb, aufs_bindex_t bindex,
			  struct inode *inode);
void append_plink(struct super_block *sb, struct inode *inode,
		  struct dentry *h_dentry, aufs_bindex_t bindex);
void au_put_plink(struct super_block *sb);
void half_refresh_plink(struct super_block *sb, aufs_bindex_t br_id);

/* ---------------------------------------------------------------------- */

/* lock subclass for hidden inode */
/* default MAX_LOCKDEP_SUBCLASSES(8) is not enough */
// todo: reduce it by dcsub.
enum {
	AuLsc_Begin = I_MUTEX_QUOTA,
	AuLsc_HI_GPARENT,	/* setattr with inotify */
	AuLsc_HI_PARENT,	/* hidden inode, parent first */
	AuLsc_HI_CHILD,
	AuLsc_HI_PARENT2,	/* copyup dirs */
	AuLsc_HI_CHILD2,
	AuLsc_End
};

/* simple abstraction */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static inline void i_lock(struct inode *i)
{
	down(&i->i_sem);
}

static inline void i_unlock(struct inode *i)
{
	up(&i->i_sem);
}

static inline int i_trylock(struct inode *i)
{
	return down_trylock(&i->i_sem);
}

static inline void hi_lock(struct inode *i, unsigned int lsc)
{
	i_lock(i);
}

#define IMustLock(i)	AuDebugOn(!down_trylock(&(i)->i_sem))
#else
static inline void i_lock(struct inode *i)
{
	mutex_lock(&i->i_mutex);
}

static inline void i_unlock(struct inode *i)
{
	mutex_unlock(&i->i_mutex);
}

static inline int i_trylock(struct inode *i)
{
	return mutex_trylock(&i->i_mutex);
}

static inline void hi_lock(struct inode *i, unsigned int lsc)
{
	mutex_lock_nested(&i->i_mutex, lsc);
}

#define IMustLock(i)	MtxMustLock(&(i)->i_mutex)
#endif

/*
 * hi_lock_gparent, hi_lock_parent, hi_lock_parent2, hi_lock_child,
 * hi_lock_child2, hi_lock_whplink
 */
#define LockFunc(name, lsc) \
static inline void hi_lock_##name(struct inode *h_i) \
{hi_lock(h_i, AuLsc_HI_##lsc);}

LockFunc(gparent, GPARENT);
LockFunc(parent, PARENT);
LockFunc(parent2, PARENT2);
LockFunc(child, CHILD);
LockFunc(child2, CHILD2);
LockFunc(whplink, CHILD2);	/* sharing lock-subclass */

#undef LockFunc

/* ---------------------------------------------------------------------- */

/* tiny test for inode number */
/* tmpfs generation is too rough */
static inline int au_test_higen(struct inode *inode, struct inode *h_inode)
{
	//IiMustAnyLock(inode);
	return !(itoii(inode)->ii_hsb1 == h_inode->i_sb
		 && inode->i_generation == h_inode->i_generation);
}

static inline int au_iigen(struct inode *inode)
{
	return atomic_read(&itoii(inode)->ii_generation);
}

#ifdef CONFIG_AUFS_HINOTIFY
static inline void au_iigen_dec(struct inode *inode)
{
	//Dbg("i%lu\n", inode->i_ino);
	atomic_dec(&itoii(inode)->ii_generation);
}

/* hinotify.c */
int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode);
void do_free_hinotify(struct aufs_hinode *hinode);
void do_hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		  unsigned int lsc);
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex);
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir);
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir);
void au_reset_hinotify(struct inode *inode, unsigned int flags);
int __init au_inotify_init(void);
void au_inotify_fin(void);
#else
static inline
int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode)
{
	return -EOPNOTSUPP;
}

static inline void do_free_hinotify(struct aufs_hinode *hinode)
{
	/* nothing */
}

static inline
void do_hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		  unsigned int lsc)
{
	hi_lock(h_dir, lsc);
}

static inline
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	i_unlock(h_dir);
}

static inline
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir)
{
	vfsub_lock_rename(h_parents[0], h_parents[1]);
}

static inline
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir)
{
	vfsub_unlock_rename(h_parents[0], h_parents[1]);
}

static inline void au_reset_hinotify(struct inode *inode, unsigned int flags)
{
	/* nothing */
}

#define au_inotify_init()	0
#define au_inotify_fin()	do{}while(0)
#endif /* CONFIG_AUFS_HINOTIFY */

static inline void free_hinotify(struct inode *inode, aufs_bindex_t bindex)
{
	do_free_hinotify(itoii(inode)->ii_hinode + bindex);
}

/*
 * hgdir_lock, hdir_lock, hdir2_lock
 */
#define LockFunc(name, lsc) \
static inline \
void name##_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex) \
{do_hdir_lock(h_dir, dir, bindex, AuLsc_HI_##lsc);}

LockFunc(hgdir, GPARENT);
LockFunc(hdir, PARENT);
LockFunc(hdir2, PARENT2);

#undef LockFunc

/* ---------------------------------------------------------------------- */

/* lock subclass for iinfo */
enum {
	AuLsc_II_CHILD,		/* child first */
	AuLsc_II_CHILD2,	/* rename(2), link(2), and cpup at hinotify */
	AuLsc_II_CHILD3,	/* copyup dirs */
	AuLsc_II_PARENT,
	AuLsc_II_PARENT2,
	AuLsc_II_PARENT3,
	AuLsc_II_NEW		/* new inode */
};

/*
 * ii_read_lock_child, ii_write_lock_child,
 * ii_read_lock_child2, ii_write_lock_child2,
 * ii_read_lock_child3, ii_write_lock_child3,
 * ii_read_lock_parent, ii_write_lock_parent,
 * ii_read_lock_parent2, ii_write_lock_parent2,
 * ii_read_lock_parent3, ii_write_lock_parent3,
 * ii_read_lock_new, ii_write_lock_new
 */
#define ReadLockFunc(name, lsc) \
static inline void ii_read_lock_##name(struct inode *i) \
{rw_read_lock_nested(&itoii(i)->ii_rwsem, AuLsc_II_##lsc);}

#define WriteLockFunc(name, lsc) \
static inline void ii_write_lock_##name(struct inode *i) \
{rw_write_lock_nested(&itoii(i)->ii_rwsem, AuLsc_II_##lsc);}

#define RWLockFuncs(name, lsc) \
	ReadLockFunc(name, lsc); \
	WriteLockFunc(name, lsc)

RWLockFuncs(child, CHILD);
RWLockFuncs(child2, CHILD2);
RWLockFuncs(child3, CHILD3);
RWLockFuncs(parent, PARENT);
RWLockFuncs(parent2, PARENT2);
RWLockFuncs(parent3, PARENT3);
RWLockFuncs(new, NEW);

#undef ReadLockFunc
#undef WriteLockFunc
#undef RWLockFunc

/*
 * ii_read_unlock, ii_write_unlock, ii_downgrade_lock
 */
SimpleUnlockRwsemFuncs(ii, struct inode *i, itoii(i)->ii_rwsem);

/* to debug easier, do not make them inlined functions */
#define IiMustReadLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustReadLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustWriteLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustWriteLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustAnyLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustAnyLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustNoWaiters(i)	RwMustNoWaiters(&itoii(i)->ii_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_INODE_H__ */
