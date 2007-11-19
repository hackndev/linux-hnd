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

/* $Id: super.h,v 1.47 2007/06/04 02:17:35 sfjro Exp $ */

#ifndef __AUFS_SUPER_H__
#define __AUFS_SUPER_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
//#include <linux/workqueue.h>
#include <linux/aufs_type.h>
#include "misc.h"
#include "sysaufs.h"

#ifdef CONFIG_AUFS_SYSAUFS
/* entries under sysfs per mount-point */
enum {SysaufsSb_XINO, /* SysaufsSb_PLINK, SysaufsSb_files, */ SysaufsSb_Last};
struct sysaufs_sbinfo {
	au_subsys_t		subsys;
	struct sysaufs_entry	array[SysaufsSb_Last];
};
extern sysaufs_op au_si_ops[];
#else
struct sysaufs_sbinfo {};
#endif

struct aufs_sbinfo {
	struct aufs_rwsem	si_rwsem;

	/* branch management */
	/* wrap around attack by superuser? No. */
	int			si_generation;

	/*
	 * set true when refresh_dirs() at remount time failed.
	 * then try refreshing dirs at access time again.
	 * if it is false, refreshing dirs at access time is unnecesary
	 */
	unsigned int		si_failed_refresh_dirs:1;

	aufs_bindex_t		si_bend;
	aufs_bindex_t		si_last_br_id;
	struct aufs_branch	**si_branch;

	/* mount flags */
	unsigned int		si_flags;

	/* external inode number table */
	atomic_long_t		si_xino;	// time bomb
	//struct file		*si_xino_bmap;

#if 0
	/* non-nowait tasks */
	struct workqueue_struct *si_wkq;
#endif
#if 0
	/* nowait tasks in the system-wide workqueue */
	atomic_t		si_wkq_nowait;
	wait_queue_head_t	si_wkq_nowait_wq;
#endif

	/* readdir cache time, max, in HZ */
	unsigned long		si_rdcache;

	/*
	 * If the number of whiteouts are larger than si_dirwh, leave all of
	 * them after rename_whtmp to reduce the cost of rmdir(2).
	 * future fsck.aufs or kernel thread will remove them later.
	 * Otherwise, remove all whiteouts and the dir in rmdir(2).
	 */
	unsigned int		si_dirwh;

	/* pseudo_link list */ // dirty
	spinlock_t		si_plink_lock;
	struct list_head	si_plink;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
	/* super_blocks list is not exported */
	struct list_head	si_list;
	struct vfsmount		*si_mnt;	/* no get/put */
#endif

	/* sysfs */
	struct sysaufs_sbinfo	si_sysaufs;

#ifdef CONFIG_AUFS_ROBR
	/* locked vma list for mmap() */ // very dirty
	spinlock_t		si_lvma_lock;
	struct list_head	si_lvma;
#endif
};

/* an entry in a xino file */
struct xino {
	ino_t ino;
	//__u32 h_gen;
} __attribute__ ((packed));

//#define AuXino_INVALID_HGEN	(-1)

/* ---------------------------------------------------------------------- */

/* Mount flags */
#define AuFlag_XINO		1
#define AuFlag_ZXINO		(1 << 1)
#define AuFlag_PLINK		(1 << 2)
#define AuFlag_UDBA_NONE	(1 << 3)
#define AuFlag_UDBA_REVAL	(1 << 4)
#define AuFlag_UDBA_INOTIFY	(1 << 5)
#define AuFlag_WARN_PERM	(1 << 6)
#define AuFlag_COO_NONE		(1 << 7)
#define AuFlag_COO_LEAF		(1 << 8)
#define AuFlag_COO_ALL		(1 << 9)
#define AuFlag_ALWAYS_DIROPQ	(1 << 10)
#define AuFlag_DLGT		(1 << 11)

#define AuMask_UDBA		(AuFlag_UDBA_NONE | AuFlag_UDBA_REVAL \
				 | AuFlag_UDBA_INOTIFY)
#define AuMask_COO		(AuFlag_COO_NONE | AuFlag_COO_LEAF \
				 | AuFlag_COO_ALL)

#ifdef CONFIG_AUFS_COMPAT
#define AuDefFlag_DIROPQ	AuFlag_ALWAYS_DIROPQ
#else
#define AuDefFlag_DIROPQ	0
#endif

#define AuDefFlags_COMM		(AuFlag_XINO | AuFlag_UDBA_REVAL | AuFlag_WARN_PERM \
				 | AuFlag_COO_NONE | AuDefFlag_DIROPQ)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#define AuDefFlags		(AuDefFlags_COMM | AuFlag_PLINK)
#else
#define AuDefFlags		AuDefFlags_COMM
#endif

/* ---------------------------------------------------------------------- */

/* flags for aufs_read_lock()/di_read_lock() */
#define AUFS_D_WLOCK		1
#define AUFS_I_RLOCK		2
#define AUFS_I_WLOCK		4

/* ---------------------------------------------------------------------- */

/* super.c */
int au_show_brs(struct seq_file *seq, struct super_block *sb);
extern struct file_system_type aufs_fs_type;

/* xino.c */
struct file *xino_create(struct super_block *sb, char *fname, int silent,
			 struct dentry *parent);
ino_t xino_new_ino(struct super_block *sb);
int xino_write0(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino);
int xino_write(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	       struct xino *xino);
int xino_read(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	      struct xino *xino);
int xino_init(struct super_block *sb, aufs_bindex_t bindex,
	      struct file *base_file, int do_test);
struct opt_xino;
int xino_set(struct super_block *sb, struct opt_xino *xino, int remount);
int xino_clr(struct super_block *sb);
struct file *xino_def(struct super_block *sb);

/* sbinfo.c */
struct aufs_sbinfo *stosi(struct super_block *sb);
aufs_bindex_t sbend(struct super_block *sb);
struct aufs_branch *stobr(struct super_block *sb, aufs_bindex_t bindex);
int au_sigen(struct super_block *sb);
int au_sigen_inc(struct super_block *sb);
int find_bindex(struct super_block *sb, struct aufs_branch *br);

void aufs_read_lock(struct dentry *dentry, int flags);
void aufs_read_unlock(struct dentry *dentry, int flags);
void aufs_write_lock(struct dentry *dentry);
void aufs_write_unlock(struct dentry *dentry);
void aufs_read_and_write_lock2(struct dentry *d1, struct dentry *d2, int isdir);
void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2);

aufs_bindex_t new_br_id(struct super_block *sb);

/* ---------------------------------------------------------------------- */

static inline const char *au_sbtype(struct super_block *sb)
{
	return sb->s_type->name;
}

static inline int au_is_aufs(struct super_block *sb)
{
	return !strcmp(au_sbtype(sb), AUFS_FSTYPE);
}

static inline int au_is_nfs(struct super_block *sb)
{
#if defined(CONFIG_NFS_FS) || defined(CONFIG_NFS_FS_MODULE)
	return !strcmp(au_sbtype(sb), "nfs");
#else
	return 0;
#endif
}

static inline int au_is_remote(struct super_block *sb)
{
	return au_is_nfs(sb);
}

#ifdef CONFIG_AUFS_EXPORT
extern struct export_operations aufs_export_op;
static inline void au_init_export_op(struct super_block *sb)
{
	sb->s_export_op = &aufs_export_op;
}

static inline int au_is_nfsd(struct task_struct *tsk)
{
	return (!tsk->mm && !strcmp(tsk->comm, "nfsd"));
}

static inline void au_nfsd_lockdep_off(void)
{
	if (au_is_nfsd(current))
		lockdep_off();
}

static inline void au_nfsd_lockdep_on(void)
{
	if (au_is_nfsd(current))
		lockdep_on();
}
#else
static inline int au_is_nfsd(struct task_struct *tsk)
{
	return 0;
}
static inline void au_init_export_op(struct super_block *sb)
{
	/* nothing */
}
#define au_nfsd_lockdep_off()	do{}while(0)
#define au_nfsd_lockdep_on()	do{}while(0)
#endif /* CONFIG_AUFS_EXPORT */

static inline void init_lvma(struct aufs_sbinfo *sbinfo)
{
#ifdef CONFIG_AUFS_ROBR
	spin_lock_init(&sbinfo->si_lvma_lock);
	INIT_LIST_HEAD(&sbinfo->si_lvma);
#else
	/* nothing */
#endif
}

/* ---------------------------------------------------------------------- */

/* limited support before 2.6.18 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static inline void au_mntget(struct super_block *sb)
{
	mntget(stosi(sb)->si_mnt);
}

static inline void au_mntput(struct super_block *sb)
{
	mntput(stosi(sb)->si_mnt);
}
#else
static inline void au_mntget(struct super_block *sb)
{
	/* empty */
}

static inline void au_mntput(struct super_block *sb)
{
	/* empty */
}
#endif

/* ---------------------------------------------------------------------- */

static inline void au_flag_set(struct super_block *sb, unsigned int flag)
{
	//SiMustWriteLock(sb);
	stosi(sb)->si_flags |= flag;
}

static inline void au_flag_clr(struct super_block *sb, unsigned int flag)
{
	//SiMustWriteLock(sb);
	stosi(sb)->si_flags &= ~flag;
}

static inline
unsigned int au_flag_test(struct super_block *sb, unsigned int flag)
{
	//SiMustAnyLock(sb);
	return stosi(sb)->si_flags & flag;
}

static inline unsigned int au_flag_test_udba(struct super_block *sb)
{
	return au_flag_test(sb, AuMask_UDBA);
}

static inline unsigned int au_flag_test_coo(struct super_block *sb)
{
	return au_flag_test(sb, AuMask_COO);
}

/* ---------------------------------------------------------------------- */

/* lock superblock. mainly for entry point functions */
/*
 * si_read_lock, si_write_lock,
 * si_read_unlock, si_write_unlock, si_downgrade_lock
 */
SimpleRwsemFuncs(si, struct super_block *sb, stosi(sb)->si_rwsem);

/* to debug easier, do not make them inlined functions */
#define SiMustReadLock(sb)	RwMustReadLock(&stosi(sb)->si_rwsem)
#define SiMustWriteLock(sb)	RwMustWriteLock(&stosi(sb)->si_rwsem)
#define SiMustAnyLock(sb)	RwMustAnyLock(&stosi(sb)->si_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_SUPER_H__ */
