/*
 * Copyright (c) 2003-2007 Erez Zadok
 * Copyright (c) 2003-2006 Charles P. Wright
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
 * Copyright (c) 2005      Arun M. Krishnakumar
 * Copyright (c) 2004-2006 David P. Quigley
 * Copyright (c) 2003-2004 Mohammad Nayyer Zubair
 * Copyright (c) 2003      Puja Gupta
 * Copyright (c) 2003      Harikesavan Krishnan
 * Copyright (c) 2003-2007 Stony Brook University
 * Copyright (c) 2003-2007 The Research Foundation of State University of New York
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _FANOUT_H_
#define _FANOUT_H_

/* Inode to private data */
static inline struct unionfs_inode_info *UNIONFS_I(const struct inode *inode)
{
	return container_of(inode, struct unionfs_inode_info, vfs_inode);
}

#define ibstart(ino) (UNIONFS_I(ino)->bstart)
#define ibend(ino) (UNIONFS_I(ino)->bend)

/* Superblock to private data */
#define UNIONFS_SB(super) ((struct unionfs_sb_info *)(super)->s_fs_info)
#define sbstart(sb) 0
#define sbend(sb) (UNIONFS_SB(sb)->bend)
#define sbmax(sb) (UNIONFS_SB(sb)->bend + 1)

/* File to private Data */
#define UNIONFS_F(file) ((struct unionfs_file_info *)((file)->private_data))
#define fbstart(file) (UNIONFS_F(file)->bstart)
#define fbend(file) (UNIONFS_F(file)->bend)

/* File to lower file. */
static inline struct file *unionfs_lower_file(const struct file *f)
{
	return UNIONFS_F(f)->lower_files[fbstart(f)];
}

static inline struct file *unionfs_lower_file_idx(const struct file *f, int index)
{
	return UNIONFS_F(f)->lower_files[index];
}

static inline void unionfs_set_lower_file_idx(struct file *f, int index, struct file *val)
{
	UNIONFS_F(f)->lower_files[index] = val;
}

static inline void unionfs_set_lower_file(struct file *f, struct file *val)
{
	UNIONFS_F(f)->lower_files[fbstart(f)] = val;
}

/* Inode to lower inode. */
static inline struct inode *unionfs_lower_inode(const struct inode *i)
{
	return UNIONFS_I(i)->lower_inodes[ibstart(i)];
}

static inline struct inode *unionfs_lower_inode_idx(const struct inode *i, int index)
{
	return UNIONFS_I(i)->lower_inodes[index];
}

static inline void unionfs_set_lower_inode_idx(struct inode *i, int index,
				   struct inode *val)
{
	UNIONFS_I(i)->lower_inodes[index] = val;
}

static inline void unionfs_set_lower_inode(struct inode *i, struct inode *val)
{
	UNIONFS_I(i)->lower_inodes[ibstart(i)] = val;
}

/* Superblock to lower superblock. */
static inline struct super_block *unionfs_lower_super(const struct super_block *sb)
{
	return UNIONFS_SB(sb)->data[sbstart(sb)].sb;
}

static inline struct super_block *unionfs_lower_super_idx(const struct super_block *sb, int index)
{
	return UNIONFS_SB(sb)->data[index].sb;
}

static inline void unionfs_set_lower_super_idx(struct super_block *sb, int index,
				   struct super_block *val)
{
	UNIONFS_SB(sb)->data[index].sb = val;
}

static inline void unionfs_set_lower_super(struct super_block *sb, struct super_block *val)
{
	UNIONFS_SB(sb)->data[sbstart(sb)].sb = val;
}

/* Branch count macros. */
static inline int branch_count(const struct super_block *sb, int index)
{
	return atomic_read(&UNIONFS_SB(sb)->data[index].sbcount);
}

static inline void set_branch_count(struct super_block *sb, int index, int val)
{
	atomic_set(&UNIONFS_SB(sb)->data[index].sbcount, val);
}

static inline void branchget(struct super_block *sb, int index)
{
	atomic_inc(&UNIONFS_SB(sb)->data[index].sbcount);
}

static inline void branchput(struct super_block *sb, int index)
{
	atomic_dec(&UNIONFS_SB(sb)->data[index].sbcount);
}

/* Dentry macros */
static inline struct unionfs_dentry_info *UNIONFS_D(const struct dentry *dent)
{
	return dent->d_fsdata;
}

static inline int dbstart(const struct dentry *dent)
{
	return UNIONFS_D(dent)->bstart;
}

static inline void set_dbstart(struct dentry *dent, int val)
{
	UNIONFS_D(dent)->bstart = val;
}

static inline int dbend(const struct dentry *dent)
{
	return UNIONFS_D(dent)->bend;
}

static inline void set_dbend(struct dentry *dent, int val)
{
	UNIONFS_D(dent)->bend = val;
}

static inline int dbopaque(const struct dentry *dent)
{
	return UNIONFS_D(dent)->bopaque;
}

static inline void set_dbopaque(struct dentry *dent, int val)
{
	UNIONFS_D(dent)->bopaque = val;
}

static inline void unionfs_set_lower_dentry_idx(struct dentry *dent, int index,
				   struct dentry *val)
{
	UNIONFS_D(dent)->lower_paths[index].dentry = val;
}

static inline struct dentry *unionfs_lower_dentry_idx(const struct dentry *dent, int index)
{
	return UNIONFS_D(dent)->lower_paths[index].dentry;
}

static inline struct dentry *unionfs_lower_dentry(const struct dentry *dent)
{
	return unionfs_lower_dentry_idx(dent, dbstart(dent));
}

static inline void unionfs_set_lower_mnt_idx(struct dentry *dent, int index,
				   struct vfsmount *mnt)
{
	UNIONFS_D(dent)->lower_paths[index].mnt = mnt;
}

static inline struct vfsmount *unionfs_lower_mnt_idx(const struct dentry *dent, int index)
{
	return UNIONFS_D(dent)->lower_paths[index].mnt;
}

static inline struct vfsmount *unionfs_lower_mnt(const struct dentry *dent)
{
	return unionfs_lower_mnt_idx(dent,dbstart(dent));
}

/* Macros for locking a dentry. */
static inline void unionfs_lock_dentry(struct dentry *d)
{
	mutex_lock(&UNIONFS_D(d)->lock);
}

static inline void unionfs_unlock_dentry(struct dentry *d)
{
	mutex_unlock(&UNIONFS_D(d)->lock);
}

static inline void verify_locked(struct dentry *d)
{
}

#endif	/* _FANOUT_H */
