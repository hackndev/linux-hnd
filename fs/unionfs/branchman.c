/*
 * Copyright (c) 2003-2007 Erez Zadok
 * Copyright (c) 2003-2006 Charles P. Wright
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
 * Copyright (c) 2005-2006 Junjiro Okajima
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

#include "union.h"

/* increase the superblock generation count; effectively invalidating every
 * upper inode, dentry and file object */
int unionfs_ioctl_incgen(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct super_block *sb;
	int gen;

	sb = file->f_dentry->d_sb;

	unionfs_write_lock(sb);

	gen = atomic_inc_return(&UNIONFS_SB(sb)->generation);

	atomic_set(&UNIONFS_D(sb->s_root)->generation, gen);
	atomic_set(&UNIONFS_I(sb->s_root->d_inode)->generation, gen);

	unionfs_write_unlock(sb);

	return gen;
}

/* return to userspace the branch indices containing the file in question
 *
 * We use fd_set and therefore we are limited to the number of the branches
 * to FD_SETSIZE, which is currently 1024 - plenty for most people
 */
int unionfs_ioctl_queryfile(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int err = 0;
	fd_set branchlist;

	int bstart = 0, bend = 0, bindex = 0;
	struct dentry *dentry, *hidden_dentry;

	dentry = file->f_dentry;
	unionfs_lock_dentry(dentry);
	if ((err = unionfs_partial_lookup(dentry)))
		goto out;
	bstart = dbstart(dentry);
	bend = dbend(dentry);

	FD_ZERO(&branchlist);

	for (bindex = bstart; bindex <= bend; bindex++) {
		hidden_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!hidden_dentry)
			continue;
		if (hidden_dentry->d_inode)
			FD_SET(bindex, &branchlist);
	}

	err = copy_to_user((void __user *)arg, &branchlist, sizeof(fd_set));
	if (err)
		err = -EFAULT;

out:
	unionfs_unlock_dentry(dentry);
	return err < 0 ? err : bend;
}

