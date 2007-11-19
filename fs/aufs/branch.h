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

/* $Id: branch.h,v 1.30 2007/05/14 03:41:51 sfjro Exp $ */

#ifndef __AUFS_BRANCH_H__
#define __AUFS_BRANCH_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"
#include "super.h"

/* protected by superblock rwsem */
struct aufs_branch {
	struct file		*br_xino;
	readf_t			br_xino_read;
	writef_t		br_xino_write;

	aufs_bindex_t		br_id;

	int			br_perm;
	struct vfsmount		*br_mnt;
	atomic_t		br_count;

	/* whiteout base */
	struct aufs_rwsem	br_wh_rwsem;
	struct dentry		*br_wh;
	atomic_t 		br_wh_running;

	/* pseudo-link dir */
	struct dentry		*br_plink;
};

/* ---------------------------------------------------------------------- */

/* branch permission and attribute */
enum {
	AuBr_RW,		/* writable, linkable wh */
	AuBr_RO,		/* readonly, no wh */
	AuBr_RR,		/* natively readonly, no wh */

	AuBr_RWNoLinkWH,	/* un-linkable whiteouts */

	AuBr_ROWH,
	AuBr_RRWH,		/* whiteout-able */

	AuBr_Last
};

static inline int br_writable(int brperm)
{
	return (brperm == AuBr_RW
		|| brperm == AuBr_RWNoLinkWH);
}

static inline int br_whable(int brperm)
{
	return (brperm == AuBr_RW
		|| brperm == AuBr_ROWH
		|| brperm == AuBr_RRWH);
}

static inline int br_linkable_wh(int brperm)
{
	return (brperm == AuBr_RW);
}

/* ---------------------------------------------------------------------- */

#define _AuNoNfsBranchMsg "NFS branch is not supported"
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,15)
#define AuNoNfsBranch
#define AuNoNfsBranchMsg _AuNoNfsBranchMsg
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19) \
	&& !defined(CONFIG_AUFS_LHASH_PATCH)
#define AuNoNfsBranch
#define AuNoNfsBranchMsg _AuNoNfsBranchMsg \
	", try lhash.patch and CONFIG_AUFS_LHASH_PATCH"
#endif

/* ---------------------------------------------------------------------- */

struct aufs_sbinfo;
void free_branches(struct aufs_sbinfo *sinfo);
int br_rdonly(struct aufs_branch *br);
int find_brindex(struct super_block *sb, aufs_bindex_t br_id);
int find_rw_br(struct super_block *sb, aufs_bindex_t bend);
int find_rw_parent_br(struct dentry *dentry, aufs_bindex_t bend);
struct opt_add;
int br_add(struct super_block *sb, struct opt_add *add, int remount);
struct opt_del;
int br_del(struct super_block *sb, struct opt_del *del, int remount);
struct opt_mod;
int br_mod(struct super_block *sb, struct opt_mod *mod, int remount,
	   int *do_update);

/* ---------------------------------------------------------------------- */

static inline int br_count(struct aufs_branch *br)
{
	return atomic_read(&br->br_count);
}

static inline void br_get(struct aufs_branch *br)
{
	atomic_inc(&br->br_count);
}

static inline void br_put(struct aufs_branch *br)
{
	atomic_dec(&br->br_count);
}

/* ---------------------------------------------------------------------- */

/* Superblock to branch */
static inline aufs_bindex_t sbr_id(struct super_block *sb, aufs_bindex_t bindex)
{
	return stobr(sb, bindex)->br_id;
}

static inline
struct vfsmount *sbr_mnt(struct super_block *sb, aufs_bindex_t bindex)
{
	return stobr(sb, bindex)->br_mnt;
}

static inline
struct super_block *sbr_sb(struct super_block *sb, aufs_bindex_t bindex)
{
	return sbr_mnt(sb, bindex)->mnt_sb;
}

#if 0
static inline int sbr_count(struct super_block *sb, aufs_bindex_t bindex)
{
	return br_count(stobr(sb, bindex));
}

static inline void sbr_get(struct super_block *sb, aufs_bindex_t bindex)
{
	br_get(stobr(sb, bindex));
}
#endif

static inline void sbr_put(struct super_block *sb, aufs_bindex_t bindex)
{
	br_put(stobr(sb, bindex));
}

static inline int sbr_perm(struct super_block *sb, aufs_bindex_t bindex)
{
	return stobr(sb, bindex)->br_perm;
}

static inline int sbr_is_whable(struct super_block *sb, aufs_bindex_t bindex)
{
	return br_whable(sbr_perm(sb, bindex));
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_LHASH_PATCH
static inline struct vfsmount *au_do_nfsmnt(struct vfsmount *h_mnt)
{
	if (!au_is_nfs(h_mnt->mnt_sb))
		return NULL;
	return h_mnt;
}

/* it doesn't mntget() */
static inline
struct vfsmount *au_nfsmnt(struct super_block *sb, aufs_bindex_t bindex)
{
	return au_do_nfsmnt(sbr_mnt(sb, bindex));
}
#else
static inline struct vfsmount *au_do_nfsmnt(struct vfsmount *h_mnt)
{
	return NULL;
}

static inline
struct vfsmount *au_nfsmnt(struct super_block *sb, aufs_bindex_t bindex)
{
	return NULL;
}
#endif /* CONFIG_AUFS_LHASH_PATCH */

/* ---------------------------------------------------------------------- */

/*
 * br_wh_read_lock, br_wh_write_lock
 * br_wh_read_unlock, br_wh_write_unlock, br_wh_downgrade_lock
 */
SimpleRwsemFuncs(br_wh, struct aufs_branch *br, br->br_wh_rwsem);

/* to debug easier, do not make them inlined functions */
#define BrWhMustReadLock(br) do { \
	/* SiMustAnyLock(sb); */ \
	RwMustReadLock(&(br)->br_wh_rwsem); \
} while (0)

#define BrWhMustWriteLock(br) do { \
	/* SiMustAnyLock(sb); */ \
	RwMustWriteLock(&(br)->br_wh_rwsem); \
} while (0)

#define BrWhMustAnyLock(br) do { \
	/* SiMustAnyLock(sb); */ \
	RwMustAnyLock(&(br)->br_wh_rwsem); \
} while (0)

#endif /* __KERNEL__ */
#endif /* __AUFS_BRANCH_H__ */
