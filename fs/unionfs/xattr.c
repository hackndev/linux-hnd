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
 * Copyright (c) 2003-2007 The Research Foundation of State University of New York*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "union.h"

/* This is lifted from fs/xattr.c */
void *unionfs_xattr_alloc(size_t size, size_t limit)
{
	void *ptr;

	if (size > limit)
		return ERR_PTR(-E2BIG);

	if (!size)		/* size request, no buffer is needed */
		return NULL;
	else if (size <= PAGE_SIZE)
		ptr = kmalloc(size, GFP_KERNEL);
	else
		ptr = vmalloc(size);
	if (!ptr)
		return ERR_PTR(-ENOMEM);
	return ptr;
}

void unionfs_xattr_free(void *ptr, size_t size)
{
	if (!size)		/* size request, no buffer was needed */
		return;
	else if (size <= PAGE_SIZE)
		kfree(ptr);
	else
		vfree(ptr);
}

/* BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
ssize_t unionfs_getxattr(struct dentry * dentry, const char *name, void *value,
		size_t size)
{
	struct dentry *hidden_dentry = NULL;
	int err = -EOPNOTSUPP;

	unionfs_lock_dentry(dentry);

	hidden_dentry = unionfs_lower_dentry(dentry);

	err = vfs_getxattr(hidden_dentry, (char*) name, value, size);

	unionfs_unlock_dentry(dentry);
	return err;
}

/* BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
int unionfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		size_t size, int flags)
{
	struct dentry *hidden_dentry = NULL;
	int err = -EOPNOTSUPP;

	unionfs_lock_dentry(dentry);
	hidden_dentry = unionfs_lower_dentry(dentry);

	err = vfs_setxattr(hidden_dentry, (char*) name, (void*) value, size, flags);

	unionfs_unlock_dentry(dentry);
	return err;
}

/* BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
int unionfs_removexattr(struct dentry *dentry, const char *name)
{
	struct dentry *hidden_dentry = NULL;
	int err = -EOPNOTSUPP;

	unionfs_lock_dentry(dentry);
	hidden_dentry = unionfs_lower_dentry(dentry);

	err = vfs_removexattr(hidden_dentry, (char*) name);

	unionfs_unlock_dentry(dentry);
	return err;
}

/* BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
ssize_t unionfs_listxattr(struct dentry * dentry, char *list, size_t size)
{
	struct dentry *hidden_dentry = NULL;
	int err = -EOPNOTSUPP;
	char *encoded_list = NULL;

	unionfs_lock_dentry(dentry);

	hidden_dentry = unionfs_lower_dentry(dentry);

	encoded_list = list;
	err = vfs_listxattr(hidden_dentry, encoded_list, size);

	unionfs_unlock_dentry(dentry);
	return err;
}

