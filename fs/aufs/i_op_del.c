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

/* $Id: i_op_del.c,v 1.39 2007/06/11 01:42:40 sfjro Exp $ */

#include "aufs.h"

/* returns,
 * 0: wh is unnecessary
 * plus: wh is necessary
 * minus: error
 */
int wr_dir_need_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		   struct dentry *locked)
{
	int need_wh, err;
	aufs_bindex_t bstart;
	struct dentry *hidden_dentry;
	struct super_block *sb;

	LKTRTrace("%.*s, isdir %d, *bcpup %d, locked %p\n",
		  DLNPair(dentry), isdir, *bcpup, locked);
	sb = dentry->d_sb;

	bstart = dbstart(dentry);
	LKTRTrace("bcpup %d, bstart %d\n", *bcpup, bstart);
	hidden_dentry = au_h_dptr(dentry);
	if (*bcpup < 0) {
		*bcpup = bstart;
		if (test_ro(sb, bstart, dentry->d_inode)) {
			*bcpup = err = find_rw_parent_br(dentry, bstart);
			//*bcpup = err = find_rw_br(sb, bstart);
			//err = -1;
			if (unlikely(err < 0))
				goto out;
		}
	} else
		AuDebugOn(bstart < *bcpup
			  || test_ro(sb, *bcpup, dentry->d_inode));
	LKTRTrace("bcpup %d, bstart %d\n", *bcpup, bstart);

	if (*bcpup != bstart) {
		err = cpup_dirs(dentry, *bcpup, locked);
		//err = -1;
		if (unlikely(err))
			goto out;
		need_wh = 1;
	} else {
		//struct nameidata nd;
		aufs_bindex_t old_bend, new_bend, bdiropq = -1;
		old_bend = dbend(dentry);
		if (isdir) {
			bdiropq = dbdiropq(dentry);
			set_dbdiropq(dentry, -1);
		}
		err = need_wh = lkup_dentry(dentry, bstart + 1, /*type*/0);
		//err = -1;
		if (isdir)
			set_dbdiropq(dentry, bdiropq);
		if (unlikely(err < 0))
			goto out;
		new_bend = dbend(dentry);
		if (!need_wh && old_bend != new_bend) {
			set_h_dptr(dentry, new_bend, NULL);
			set_dbend(dentry, old_bend);
#if 0
		} else if (!au_h_dptr_i(dentry, new_bend)->d_inode) {
			LKTRTrace("negative\n");
			set_h_dptr(dentry, new_bend, NULL);
			set_dbend(dentry, old_bend);
			need_wh = 0;
#endif
		}
	}
	LKTRTrace("need_wh %d\n", need_wh);
	err = need_wh;

 out:
	TraceErr(err);
	return err;
}

static struct dentry *
lock_hdir_create_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		    struct dtime *dt)
{
	struct dentry *wh_dentry;
	int err, need_wh;
	struct dentry *hidden_parent, *parent;
	struct inode *dir, *h_dir;
	struct lkup_args lkup;

	LKTRTrace("%.*s, isdir %d\n", DLNPair(dentry), isdir);

	err = need_wh = wr_dir_need_wh(dentry, isdir, bcpup, NULL);
	//err = -1;
	wh_dentry = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out;

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	hidden_parent = au_h_dptr_i(parent, *bcpup);
	h_dir = hidden_parent->d_inode;
	hdir_lock(h_dir, dir, *bcpup);
	dtime_store(dt, parent, hidden_parent);
	wh_dentry = NULL;
	if (!need_wh)
		goto out_dput; /* success, no need to create whiteout */

	lkup.nfsmnt = au_nfsmnt(dentry->d_sb, *bcpup);
	lkup.dlgt = need_dlgt(dentry->d_sb);
	wh_dentry = simple_create_wh(dentry, *bcpup, hidden_parent, &lkup);
	//wh_dentry = ERR_PTR(-1);
	if (!IS_ERR(wh_dentry))
		goto out_dput; /* success */
	/* returns with the parent is locked and wh_dentry is DGETed */

	hdir_unlock(h_dir, dir, *bcpup);

 out_dput:
	dput(parent);
 out:
	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

static int renwh_and_rmdir(struct dentry *dentry, aufs_bindex_t bindex,
			   struct aufs_nhash *whlist, struct inode *dir)
{
	int rmdir_later, err;
	struct dentry *hidden_dentry;

	LKTRTrace("%.*s, b%d\n", DLNPair(dentry), bindex);

	err = rename_whtmp(dentry, bindex);
	//err = -1;
#if 0
	//todo: bug
	if (unlikely(err)) {
		au_direval_inc(dentry->d_parent);
		return err;
	}
#endif

	hidden_dentry = au_h_dptr_i(dentry, bindex);
	if (!au_is_nfs(hidden_dentry->d_sb)) {
		const int dirwh = stosi(dentry->d_sb)->si_dirwh;
		rmdir_later = (dirwh <= 1);
		if (!rmdir_later)
			rmdir_later = is_longer_wh(whlist, bindex, dirwh);
		if (rmdir_later)
			return rmdir_later;
	}

	err = rmdir_whtmp(hidden_dentry, whlist, bindex, dir, dentry->d_inode);
	//err = -1;
	if (unlikely(err)) {
		IOErr("rmdir %.*s, b%d failed, %d. ignored\n",
		      DLNPair(hidden_dentry), bindex, err);
		err = 0;
	}
	TraceErr(err);
	return err;
}

static void epilog(struct inode *dir, struct dentry *dentry,
		   aufs_bindex_t bindex)
{
	d_drop(dentry);
	dentry->d_inode->i_ctime = dir->i_ctime;
	if (atomic_read(&dentry->d_count) == 1) {
		set_h_dptr(dentry, dbstart(dentry), NULL);
		au_update_dbstart(dentry);
	}
	if (ibstart(dir) == bindex)
		au_cpup_attr_timesizes(dir);
	dir->i_version++;
}

static int do_revert(int err, struct dentry *wh_dentry, struct dentry *dentry,
		     aufs_bindex_t bwh, struct dtime *dt, int dlgt)
{
	int rerr;
	struct dentry *parent;

	parent = dget_parent(wh_dentry);
	rerr = au_unlink_wh_dentry(parent->d_inode, wh_dentry, dentry, dlgt);
	dput(parent);
	//rerr = -1;
	if (!rerr) {
		set_dbwh(dentry, bwh);
		dtime_revert(dt, !CPUP_LOCKED_GHDIR);
		return 0;
	}

	IOErr("%.*s reverting whiteout failed(%d, %d)\n",
	      DLNPair(dentry), err, rerr);
	return -EIO;
}

/* ---------------------------------------------------------------------- */

int aufs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err, dlgt;
	struct inode *inode, *hidden_dir;
	struct dentry *parent, *wh_dentry, *hidden_dentry, *hidden_parent;
	struct dtime dt;
	aufs_bindex_t bwh, bindex, bstart;
	struct super_block *sb;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, DLNPair(dentry));
	IMustLock(dir);
	inode = dentry->d_inode;
	if (unlikely(!inode))
		return -ENOENT; // possible?
	IMustLock(inode);

	aufs_read_lock(dentry, AUFS_D_WLOCK);
	parent = dget_parent(dentry);
	di_write_lock_parent(parent);

	bstart = dbstart(dentry);
	bwh = dbwh(dentry);
	bindex = -1;
	wh_dentry = lock_hdir_create_wh(dentry, /*isdir*/0, &bindex, &dt);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	sb = dir->i_sb;
	dlgt = need_dlgt(sb);
	hidden_dentry = au_h_dptr(dentry);
	dget(hidden_dentry);
	hidden_parent = dget_parent(hidden_dentry);
	hidden_dir = hidden_parent->d_inode;

	if (bindex == bstart) {
		err = vfsub_unlink(hidden_dir, hidden_dentry, dlgt);
		//err = -1;
	} else {
		AuDebugOn(!wh_dentry);
		dput(hidden_parent);
		hidden_parent = dget_parent(wh_dentry);
		AuDebugOn(hidden_parent != au_h_dptr_i(parent, bindex));
		hidden_dir = hidden_parent->d_inode;
		IMustLock(hidden_dir);
		err = 0;
	}

	if (!err) {
		inode->i_nlink--;
		epilog(dir, dentry, bindex);
#if 0
		xino_write0(sb, bstart, hidden_dentry->d_inode->i_ino);
		/* ignore this error */
#endif
		goto out_unlock; /* success */
	}

	/* revert */
	if (wh_dentry) {
		int rerr;
		rerr = do_revert(err, wh_dentry, dentry, bwh, &dt, dlgt);
		if (rerr)
			err = rerr;
	}

 out_unlock:
	hdir_unlock(hidden_dir, dir, bindex);
	dput(wh_dentry);
	dput(hidden_parent);
	dput(hidden_dentry);
 out:
	di_write_unlock(parent);
	dput(parent);
	aufs_read_unlock(dentry, AUFS_D_WLOCK);
	TraceErr(err);
	return err;
}

int aufs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err, rmdir_later;
	struct inode *inode, *hidden_dir;
	struct dentry *parent, *wh_dentry, *hidden_dentry, *hidden_parent;
	struct dtime dt;
	aufs_bindex_t bwh, bindex, bstart;
	struct rmdir_whtmp_args *args;
	struct aufs_nhash *whlist;
	struct super_block *sb;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, DLNPair(dentry));
	IMustLock(dir);
	inode = dentry->d_inode;
	if (unlikely(!inode))
		return -ENOENT; // possible?
	IMustLock(inode);

	whlist = nhash_new(GFP_KERNEL);
	err = PTR_ERR(whlist);
	if (IS_ERR(whlist))
		goto out;

	err = -ENOMEM;
	args = kmalloc(sizeof(*args), GFP_KERNEL);
	//args = NULL;
	if (unlikely(!args))
		goto out_whlist;

	aufs_read_lock(dentry, AUFS_D_WLOCK);
	parent = dget_parent(dentry);
	di_write_lock_parent(parent);
	err = test_empty(dentry, whlist);
	//err = -1;
	if (unlikely(err))
		goto out_args;

	bstart = dbstart(dentry);
	bwh = dbwh(dentry);
	bindex = -1;
	wh_dentry = lock_hdir_create_wh(dentry, /*isdir*/ 1, &bindex, &dt);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out_args;

	hidden_dentry = au_h_dptr(dentry);
	dget(hidden_dentry);
	hidden_parent = dget_parent(hidden_dentry);
	hidden_dir = hidden_parent->d_inode;

	rmdir_later = 0;
	if (bindex == bstart) {
		IMustLock(hidden_dir);
		/* this will fire IN_MOVE_SELF */
		err = renwh_and_rmdir(dentry, bstart, whlist, dir);
		//err = -1;
		if (err > 0) {
			rmdir_later = err;
			err = 0;
		}
	} else {
		AuDebugOn(!wh_dentry);
		dput(hidden_parent);
		hidden_parent = dget_parent(wh_dentry);;
		AuDebugOn(hidden_parent != au_h_dptr_i(parent, bindex));
		hidden_dir = hidden_parent->d_inode;
		IMustLock(hidden_dir);
		err = 0;
	}

	sb = dentry->d_sb;
	if (!err) {
		//aufs_bindex_t bi, bend;

		au_reset_hinotify(inode, /*flags*/0);
		inode->i_nlink = 0;
		set_dbdiropq(dentry, -1);
		epilog(dir, dentry, bindex);

		if (rmdir_later) {
			kick_rmdir_whtmp(hidden_dentry, whlist, bstart, dir,
					 inode, args);
			args = NULL;
		}

#if 0
		bend = dbend(dentry);
		for (bi = bstart; bi <= bend; bi++) {
			struct dentry *hd;
			hd = au_h_dptr_i(dentry, bi);
			if (hd && hd->d_inode)
				xino_write0(sb, bi, hd->d_inode->i_ino);
			/* ignore this error */
		}
#endif

		goto out_unlock; /* success */
	}

	/* revert */
	LKTRLabel(revert);
	if (wh_dentry) {
		int rerr;
		rerr = do_revert(err, wh_dentry, dentry, bwh, &dt,
				 need_dlgt(sb));
		if (rerr)
			err = rerr;
	}

 out_unlock:
	hdir_unlock(hidden_dir, dir, bindex);
	dput(wh_dentry);
	dput(hidden_parent);
	dput(hidden_dentry);
 out_args:
	di_write_unlock(parent);
	dput(parent);
	aufs_read_unlock(dentry, AUFS_D_WLOCK);
	kfree(args);
 out_whlist:
	nhash_del(whlist);
 out:
	TraceErr(err);
	return err;
}
