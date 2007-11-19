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

/* $Id: i_op_ren.c,v 1.43 2007/06/11 01:42:40 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

enum {SRC, DST};
struct rename_args {
	struct dentry *hidden_dentry[2], *parent[2], *hidden_parent[2];
	struct aufs_nhash whlist;
	aufs_bindex_t btgt, bstart[2];
	struct super_block *sb;

	unsigned int isdir:1;
	unsigned int issamedir:1;
	unsigned int whsrc:1;
	unsigned int whdst:1;
	unsigned int dlgt:1;
} __attribute__((aligned(sizeof(long))));

static int do_rename(struct inode *src_dir, struct dentry *src_dentry,
		     struct inode *dir, struct dentry *dentry,
		     struct rename_args *a)
{
	int err, need_diropq, bycpup, rerr;
	struct rmdir_whtmp_args *thargs;
	struct dentry *wh_dentry[2], *hidden_dst, *hg_parent;
	struct inode *hidden_dir[2];
	aufs_bindex_t bindex, bend;
	unsigned int flags;
	struct lkup_args lkup = {.dlgt = a->dlgt};

	LKTRTrace("%.*s/%.*s, %.*s/%.*s, "
		  "hd{%p, %p}, hp{%p, %p}, wh %p, btgt %d, bstart{%d, %d}, "
		  "flags{%d, %d, %d, %d}\n",
		  DLNPair(a->parent[SRC]), DLNPair(src_dentry),
		  DLNPair(a->parent[DST]), DLNPair(dentry),
		  a->hidden_dentry[SRC], a->hidden_dentry[DST],
		  a->hidden_parent[SRC], a->hidden_parent[DST],
		  &a->whlist, a->btgt,
		  a->bstart[SRC], a->bstart[DST],
		  a->isdir, a->issamedir, a->whsrc, a->whdst);
	hidden_dir[SRC] = a->hidden_parent[SRC]->d_inode;
	hidden_dir[DST] = a->hidden_parent[DST]->d_inode;
	IMustLock(hidden_dir[SRC]);
	IMustLock(hidden_dir[DST]);

	/* prepare workqueue args */
	hidden_dst = NULL;
	thargs = NULL;
	if (a->isdir && a->hidden_dentry[DST]->d_inode) {
		err = -ENOMEM;
		thargs = kmalloc(sizeof(*thargs), GFP_KERNEL);
		//thargs = NULL;
		if (unlikely(!thargs))
			goto out;
		hidden_dst = dget(a->hidden_dentry[DST]);
	}

	wh_dentry[SRC] = wh_dentry[DST] = NULL;
	lkup.nfsmnt = au_nfsmnt(a->sb, a->btgt);
	/* create whiteout for src_dentry */
	if (a->whsrc) {
		wh_dentry[SRC] = simple_create_wh(src_dentry, a->btgt,
						  a->hidden_parent[SRC], &lkup);
		//wh_dentry[SRC] = ERR_PTR(-1);
		err = PTR_ERR(wh_dentry[SRC]);
		if (IS_ERR(wh_dentry[SRC]))
			goto out_thargs;
	}

	/* lookup whiteout for dentry */
	if (a->whdst) {
		struct dentry *d;
		d = lkup_wh(a->hidden_parent[DST], &dentry->d_name, &lkup);
		//d = ERR_PTR(-1);
		err = PTR_ERR(d);
		if (IS_ERR(d))
			goto out_whsrc;
		if (!d->d_inode)
			dput(d);
		else
			wh_dentry[DST] = d;
	}

	/* rename dentry to tmpwh */
	if (thargs) {
		err = rename_whtmp(dentry, a->btgt);
		//err = -1;
		if (unlikely(err))
			goto out_whdst;
		set_h_dptr(dentry, a->btgt, NULL);
		err = lkup_neg(dentry, a->btgt);
		//err = -1;
		if (unlikely(err))
			goto out_whtmp;
		a->hidden_dentry[DST] = au_h_dptr_i(dentry, a->btgt);
	}

	/* cpup src */
	if (a->hidden_dentry[DST]->d_inode && a->bstart[SRC] != a->btgt) {
		flags = au_flags_cpup(!CPUP_DTIME, a->parent[SRC]);
		hg_parent = a->hidden_parent[SRC]->d_parent;
		if (!(flags & CPUP_LOCKED_GHDIR)
		    && hg_parent == a->hidden_parent[DST])
			flags |= CPUP_LOCKED_GHDIR;

		hi_lock_child(a->hidden_dentry[SRC]->d_inode);
		err = sio_cpup_simple(src_dentry, a->btgt, -1, flags);
		//err = -1; // untested dir
		i_unlock(a->hidden_dentry[SRC]->d_inode);
		if (unlikely(err))
			goto out_whtmp;
	}

#if 0
	/* clear the target ino in xino */
	LKTRTrace("dir %d, dst inode %p\n",
		  a->isdir, a->hidden_dentry[DST]->d_inode);
	if (a->isdir && a->hidden_dentry[DST]->d_inode) {
		Dbg("here\n");
		err = xino_write(a->sb, a->btgt,
				 a->hidden_dentry[DST]->d_inode->i_ino, 0);
		if (unlikely(err))
			goto out_whtmp;
	}
#endif

	/* rename by vfs_rename or cpup */
	need_diropq = a->isdir
		&& (wh_dentry[DST]
		    || dbdiropq(dentry) == a->btgt
		    || au_flag_test(a->sb, AuFlag_ALWAYS_DIROPQ));
	bycpup = 0;
	if (dbstart(src_dentry) == a->btgt) {
		if (need_diropq && dbdiropq(src_dentry) == a->btgt)
			need_diropq = 0;
		err = vfsub_rename(hidden_dir[SRC], au_h_dptr(src_dentry),
				   hidden_dir[DST], a->hidden_dentry[DST],
				   a->dlgt);
		//err = -1;
	} else {
		bycpup = 1;
		flags = au_flags_cpup(!CPUP_DTIME, a->parent[DST]);
		hg_parent = a->hidden_parent[DST]->d_parent;
		if (!(flags & CPUP_LOCKED_GHDIR)
		    && hg_parent == a->hidden_parent[SRC])
			flags |= CPUP_LOCKED_GHDIR;

		hi_lock_child(a->hidden_dentry[SRC]->d_inode);
		set_dbstart(src_dentry, a->btgt);
		set_h_dptr(src_dentry, a->btgt, dget(a->hidden_dentry[DST]));
		//DbgDentry(src_dentry);
		//DbgInode(src_dentry->d_inode);
		err = sio_cpup_single(src_dentry, a->btgt, a->bstart[SRC], -1,
				      flags);
		//err = -1; // untested dir
		if (unlikely(err)) {
			set_h_dptr(src_dentry, a->btgt, NULL);
			set_dbstart(src_dentry, a->bstart[SRC]);
		}
		i_unlock(a->hidden_dentry[SRC]->d_inode);
	}
	if (unlikely(err))
		goto out_whtmp;

	/* make dir opaque */
	if (need_diropq) {
		struct dentry *diropq;
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		diropq = create_diropq(src_dentry, a->btgt, a->dlgt);
		//diropq = ERR_PTR(-1);
		hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		err = PTR_ERR(diropq);
		if (IS_ERR(diropq))
			goto out_rename;
		dput(diropq);
	}

	/* remove whiteout for dentry */
	if (wh_dentry[DST]) {
		err = au_unlink_wh_dentry(hidden_dir[DST], wh_dentry[DST],
					  dentry, a->dlgt);
		//err = -1;
		if (unlikely(err))
			goto out_diropq;
	}

	/* remove whtmp */
	if (thargs) {
		if (au_is_nfs(hidden_dst->d_sb)
		    || !is_longer_wh(&a->whlist, a->btgt,
				     stosi(a->sb)->si_dirwh)) {
			err = rmdir_whtmp(hidden_dst, &a->whlist, a->btgt, dir,
					  dentry->d_inode);
			if (unlikely(err))
				Warn("failed removing whtmp dir %.*s (%d), "
				     "ignored.\n", DLNPair(hidden_dst), err);
		} else {
			kick_rmdir_whtmp(hidden_dst, &a->whlist, a->btgt, dir,
					 dentry->d_inode, thargs);
			dput(hidden_dst);
			thargs = NULL;
		}
	}
	err = 0;
	goto out_success;

#define RevertFailure(fmt, args...) do { \
		IOErrWhck("revert failure: " fmt " (%d, %d)\n", \
			##args, err, rerr); \
		err = -EIO; \
	} while(0)

 out_diropq:
	if (need_diropq) {
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		// i_lock simplly since inotify is not set to h_inode.
		hi_lock_parent(h_inode);
		//hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		rerr = remove_diropq(src_dentry, a->btgt, a->dlgt);
		//rerr = -1;
		//hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		i_unlock(h_inode);
		if (rerr)
			RevertFailure("remove diropq %.*s",
				      DLNPair(src_dentry));
	}
 out_rename:
	if (!bycpup) {
		struct dentry *d;
		struct qstr *name = &src_dentry->d_name;
		d = lkup_one(name->name, a->hidden_parent[SRC], name->len,
			     &lkup);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("lkup_one %.*s", DLNPair(src_dentry));
			goto out_whtmp;
		}
		AuDebugOn(d->d_inode);
		rerr = vfsub_rename
			(hidden_dir[DST], au_h_dptr_i(src_dentry, a->btgt),
			 hidden_dir[SRC], d, a->dlgt);
		//rerr = -1;
		d_drop(d);
		dput(d);
		//set_h_dptr(src_dentry, a->btgt, NULL);
		if (rerr)
			RevertFailure("rename %.*s", DLNPair(src_dentry));
	} else {
		rerr = vfsub_unlink(hidden_dir[DST], a->hidden_dentry[DST],
				    a->dlgt);
		//rerr = -1;
		set_h_dptr(src_dentry, a->btgt, NULL);
		set_dbstart(src_dentry, a->bstart[SRC]);
		if (rerr)
			RevertFailure("unlink %.*s",
				      DLNPair(a->hidden_dentry[DST]));
	}
 out_whtmp:
	if (thargs) {
		struct dentry *d;
		struct qstr *name = &dentry->d_name;
		LKTRLabel(here);
		d = lkup_one(name->name, a->hidden_parent[DST], name->len,
			     &lkup);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("lookup %.*s", LNPair(name));
			goto out_whdst;
		}
		if (d->d_inode) {
			d_drop(d);
			dput(d);
			goto out_whdst;
		}
		AuDebugOn(d->d_inode);
		rerr = vfsub_rename(hidden_dir[DST], hidden_dst,
				    hidden_dir[DST], d, a->dlgt);
		//rerr = -1;
		d_drop(d);
		dput(d);
		if (rerr) {
			RevertFailure("rename %.*s", DLNPair(hidden_dst));
			goto out_whdst;
		}
		set_h_dptr(dentry, a->btgt, NULL);
		set_h_dptr(dentry, a->btgt, dget(hidden_dst));
	}
 out_whdst:
	dput(wh_dentry[DST]);
	wh_dentry[DST] = NULL;
 out_whsrc:
	if (wh_dentry[SRC]) {
		LKTRLabel(here);
		rerr = au_unlink_wh_dentry(hidden_dir[SRC], wh_dentry[SRC],
					   src_dentry, a->dlgt);
		//rerr = -1;
		if (rerr)
			RevertFailure("unlink %.*s", DLNPair(wh_dentry[SRC]));
	}
#undef RevertFailure
	d_drop(src_dentry);
	bend = dbend(src_dentry);
	for (bindex = dbstart(src_dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	d_drop(dentry);
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	au_update_dbstart(dentry);
	if (thargs)
		d_drop(hidden_dst);
 out_success:
	dput(wh_dentry[SRC]);
	dput(wh_dentry[DST]);
 out_thargs:
	if (thargs) {
		dput(hidden_dst);
		kfree(thargs);
	}
 out:
	TraceErr(err);
	return err;
}

/*
 * test if @dentry dir can be rename destination or not.
 * success means, it is a logically empty dir.
 */
static int may_rename_dstdir(struct dentry *dentry, aufs_bindex_t btgt,
			     struct aufs_nhash *whlist)
{
	LKTRTrace("%.*s\n", DLNPair(dentry));

	return test_empty(dentry, whlist);
}

/*
 * test if @dentry dir can be rename source or not.
 * if it can, return 0 and @children is filled.
 * success means,
 * - or, it is a logically empty dir.
 * - or, it exists on writable branch and has no children including whiteouts
 *       on the lower branch.
 */
static int may_rename_srcdir(struct dentry *dentry, aufs_bindex_t btgt)
{
	int err;
	aufs_bindex_t bstart;

	LKTRTrace("%.*s\n", DLNPair(dentry));

	bstart = dbstart(dentry);
	if (bstart != btgt) {
		struct aufs_nhash *whlist;

		whlist = nhash_new(GFP_KERNEL);
		err = PTR_ERR(whlist);
		if (IS_ERR(whlist))
			goto out;
		err = test_empty(dentry, whlist);
		nhash_del(whlist);
		goto out;
	}

	if (bstart == dbtaildir(dentry))
		return 0; /* success */

	err = au_test_empty_lower(dentry);

 out:
	if (/* unlikely */(err == -ENOTEMPTY))
		err = -EXDEV;
	TraceErr(err);
	return err;
}

int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry)
{
	int err, do_dt_dstdir;
	aufs_bindex_t bend, bindex;
	struct inode *inode, *dirs[2];
	enum {PARENT, CHILD};
	/* reduce stack space */
	struct {
		struct rename_args a;
		struct dtime dt[2][2];
	} *p;

	LKTRTrace("i%lu, %.*s, i%lu, %.*s\n",
		  src_dir->i_ino, DLNPair(src_dentry),
		  dir->i_ino, DLNPair(dentry));
	IMustLock(src_dir);
	IMustLock(dir);
	if (dentry->d_inode)
		IMustLock(dentry->d_inode);

	err = -ENOMEM;
	BUILD_BUG_ON(sizeof(*p) > PAGE_SIZE);
	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (unlikely(!p))
		goto out;

	err = -ENOTDIR;
	p->a.sb = src_dentry->d_sb;
	inode = src_dentry->d_inode;
	p->a.isdir = !!S_ISDIR(inode->i_mode);
	if (unlikely(p->a.isdir && dentry->d_inode
		     && !S_ISDIR(dentry->d_inode->i_mode)))
		goto out_free;

	aufs_read_and_write_lock2(dentry, src_dentry, p->a.isdir);
	p->a.dlgt = !!need_dlgt(p->a.sb);
	p->a.parent[SRC] = p->a.parent[DST] = dentry->d_parent;
	p->a.issamedir = (src_dir == dir);
	if (p->a.issamedir)
		di_write_lock_parent(p->a.parent[DST]);
	else {
		p->a.parent[SRC] = src_dentry->d_parent;
		di_write_lock2_parent(p->a.parent[SRC], p->a.parent[DST],
				      /*isdir*/1);
	}

	/* which branch we process */
	p->a.bstart[DST] = dbstart(dentry);
	p->a.btgt = err = wr_dir(dentry, 1, src_dentry, /*force_btgt*/-1,
			      /*do_lock_srcdir*/0);
	if (unlikely(err < 0))
		goto out_unlock;

	/* are they available to be renamed */
	err = 0;
	nhash_init(&p->a.whlist);
	if (p->a.isdir && dentry->d_inode) {
		set_dbstart(dentry, p->a.bstart[DST]);
		err = may_rename_dstdir(dentry, p->a.btgt, &p->a.whlist);
		set_dbstart(dentry, p->a.btgt);
	}
	p->a.hidden_dentry[DST] = au_h_dptr(dentry);
	if (unlikely(err))
		goto out_unlock;
	//todo: minor optimize, their sb may be same while their bindex differs.
	p->a.bstart[SRC] = dbstart(src_dentry);
	p->a.hidden_dentry[SRC] = au_h_dptr(src_dentry);
	if (p->a.isdir) {
		err = may_rename_srcdir(src_dentry, p->a.btgt);
		if (unlikely(err))
			goto out_children;
	}

	/* prepare the writable parent dir on the same branch */
	err = wr_dir_need_wh(src_dentry, p->a.isdir, &p->a.btgt,
			     p->a.issamedir ? NULL : p->a.parent[DST]);
	if (unlikely(err < 0))
		goto out_children;
	p->a.whsrc = !!err;
	p->a.whdst = (p->a.bstart[DST] == p->a.btgt);
	if (!p->a.whdst) {
		err = cpup_dirs(dentry, p->a.btgt,
				p->a.issamedir ? NULL : p->a.parent[SRC]);
		if (unlikely(err))
			goto out_children;
	}

	p->a.hidden_parent[SRC] = au_h_dptr_i(p->a.parent[SRC], p->a.btgt);
	p->a.hidden_parent[DST] = au_h_dptr_i(p->a.parent[DST], p->a.btgt);
	dirs[0] = src_dir;
	dirs[1] = dir;
	hdir_lock_rename(p->a.hidden_parent, dirs, p->a.btgt, p->a.issamedir);

	/* store timestamps to be revertible */
	dtime_store(p->dt[PARENT] + SRC, p->a.parent[SRC],
		    p->a.hidden_parent[SRC]);
	if (!p->a.issamedir)
		dtime_store(p->dt[PARENT] + DST, p->a.parent[DST],
			    p->a.hidden_parent[DST]);
	do_dt_dstdir = 0;
	if (p->a.isdir) {
		dtime_store(p->dt[CHILD] + SRC, src_dentry,
			    p->a.hidden_dentry[SRC]);
		if (p->a.hidden_dentry[DST]->d_inode) {
			do_dt_dstdir = 1;
			dtime_store(p->dt[CHILD] + DST, dentry,
				    p->a.hidden_dentry[DST]);
		}
	}

	/* this will fire IN_MOVE_SELF */
	err = do_rename(src_dir, src_dentry, dir, dentry, &p->a);
	if (unlikely(err))
		goto out_dt;
	hdir_unlock_rename(p->a.hidden_parent, dirs, p->a.btgt, p->a.issamedir);

	/* update dir attributes */
	dir->i_version++;
	if (p->a.isdir)
		au_cpup_attr_nlink(dir);
	if (ibstart(dir) == p->a.btgt)
		au_cpup_attr_timesizes(dir);

	if (!p->a.issamedir) {
		src_dir->i_version++;
		if (p->a.isdir)
			au_cpup_attr_nlink(src_dir);
		if (ibstart(src_dir) == p->a.btgt)
			au_cpup_attr_timesizes(src_dir);
	}

	// is this updating defined in POSIX?
	if (unlikely(p->a.isdir)) {
		//i_lock(inode);
		au_cpup_attr_timesizes(inode);
		//i_unlock(inode);
	}

#if 0
	d_drop(src_dentry);
#else
	/* dput/iput all lower dentries */
	set_dbwh(src_dentry, -1);
	bend = dbend(src_dentry);
	for (bindex = p->a.btgt + 1; bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			set_h_dptr(src_dentry, bindex, NULL);
	}
	set_dbend(src_dentry, p->a.btgt);

	bend = ibend(inode);
	for (bindex = p->a.btgt + 1; bindex <= bend; bindex++) {
		struct inode *hi;
		hi = au_h_iptr_i(inode, bindex);
		if (hi)
			set_h_iptr(inode, bindex, NULL, 0);
	}
	set_ibend(inode, p->a.btgt);
#endif

#if 0
	//au_debug_on();
	//DbgDentry(dentry);
	//DbgInode(dentry->d_inode);
	//au_debug_off();
	inode = dentry->d_inode;
	if (inode) {
		aufs_bindex_t bindex, bend;
		struct dentry *hd;
		bend = dbend(dentry);
		for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
			hd = au_h_dptr_i(dentry, bindex);
			if (hd && hd->d_inode)
				xino_write0(p->a.sb, bindex, hd->d_inode->i_ino);
			/* ignore this error */
		}
	}
#endif

	goto out_children; /* success */

 out_dt:
	dtime_revert(p->dt[PARENT] + SRC,
		     p->a.hidden_parent[SRC]->d_parent
		     == p->a.hidden_parent[DST]);
	if (!p->a.issamedir)
		dtime_revert(p->dt[PARENT] + DST,
			     p->a.hidden_parent[DST]->d_parent
			     == p->a.hidden_parent[SRC]);
	if (p->a.isdir && err != -EIO) {
		struct dentry *hd;

		hd = p->dt[CHILD][SRC].dt_h_dentry;
		hi_lock_child(hd->d_inode);
		dtime_revert(p->dt[CHILD] + SRC, 1);
		i_unlock(hd->d_inode);
		if (do_dt_dstdir) {
			hd = p->dt[CHILD][DST].dt_h_dentry;
			hi_lock_child(hd->d_inode);
			dtime_revert(p->dt[CHILD] + DST, 1);
			i_unlock(hd->d_inode);
		}
	}
	hdir_unlock_rename(p->a.hidden_parent, dirs, p->a.btgt, p->a.issamedir);
 out_children:
	nhash_fin(&p->a.whlist);
 out_unlock:
	//if (unlikely(err /* && p->a.isdir */)) {
	if (unlikely(err && p->a.isdir)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	if (p->a.issamedir)
		di_write_unlock(p->a.parent[DST]);
	else
		di_write_unlock2(p->a.parent[SRC], p->a.parent[DST]);
	aufs_read_and_write_unlock2(dentry, src_dentry);
 out_free:
	kfree(p);
 out:
	TraceErr(err);
	return err;
}
