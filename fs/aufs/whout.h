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

/* $Id: whout.h,v 1.9 2007/05/21 02:57:31 sfjro Exp $ */

#ifndef __AUFS_WHOUT_H__
#define __AUFS_WHOUT_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/aufs_type.h>

int au_alloc_whname(const char *name, int len, struct qstr *wh);
void au_free_whname(struct qstr *wh);

struct lkup_args;
int is_wh(struct dentry *h_parent, struct qstr *wh_name, int try_sio,
	  struct lkup_args *lkup);
int is_diropq(struct dentry *h_dentry, struct lkup_args *lkup);

struct dentry *lkup_whtmp(struct dentry *h_parent, struct qstr *prefix,
			  struct lkup_args *lkup);
int rename_whtmp(struct dentry *dentry, aufs_bindex_t bindex);
int au_unlink_wh_dentry(struct inode *h_dir, struct dentry *wh_dentry,
			struct dentry *dentry, int dlgt);

struct aufs_branch;
int init_wh(struct dentry *h_parent, struct aufs_branch *br,
	    struct vfsmount *nfsmnt, struct super_block *sb);

struct dentry *sio_diropq(struct dentry *dentry, aufs_bindex_t bindex,
			  int do_create, int dlgt);

struct dentry *lkup_wh(struct dentry *h_parent, struct qstr *base_name,
		       struct lkup_args *lkup);
struct dentry *simple_create_wh(struct dentry *dentry, aufs_bindex_t bindex,
				struct dentry *h_parent,
				struct lkup_args *lkup);

/* real rmdir the whiteout-ed dir */
struct rmdir_whtmp_args {
	struct dentry *h_dentry;
	struct aufs_nhash whlist;
	aufs_bindex_t bindex;
	struct inode *dir, *inode;
};

struct aufs_nhash;
int rmdir_whtmp(struct dentry *h_dentry, struct aufs_nhash *whlist,
		aufs_bindex_t bindex, struct inode *dir, struct inode *inode);
void kick_rmdir_whtmp(struct dentry *h_dentry, struct aufs_nhash *whlist,
		      aufs_bindex_t bindex, struct inode *dir,
		      struct inode *inode, struct rmdir_whtmp_args *args);

/* ---------------------------------------------------------------------- */

static inline
struct dentry *create_diropq(struct dentry *dentry, aufs_bindex_t bindex,
			     int dlgt)
{
	return sio_diropq(dentry, bindex, /*do_create*/1, dlgt);
}

static inline
int remove_diropq(struct dentry *dentry, aufs_bindex_t bindex, int dlgt)
{
	return PTR_ERR(sio_diropq(dentry, bindex, /*do_create*/0, dlgt));
}

#endif /* __KERNEL__ */
#endif /* __AUFS_WHOUT_H__ */
