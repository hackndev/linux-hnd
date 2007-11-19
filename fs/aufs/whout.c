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

/* $Id: whout.c,v 1.17 2007/06/04 02:17:35 sfjro Exp $ */

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/random.h>
#include <linux/security.h>
#include "aufs.h"

#define WH_MASK			S_IRUGO

/* If a directory contains this file, then it is opaque.  We start with the
 * .wh. flag so that it is blocked by lookup.
 */
static struct qstr diropq_name = {
	.name = AUFS_WH_DIROPQ,
	.len = sizeof(AUFS_WH_DIROPQ) - 1
};

/*
 * generate whiteout name, which is NOT terminated by NULL.
 * @name: original d_name.name
 * @len: original d_name.len
 * @wh: whiteout qstr
 * returns zero when succeeds, otherwise error.
 * succeeded value as wh->name should be freed by au_free_whname().
 */
int au_alloc_whname(const char *name, int len, struct qstr *wh)
{
	char *p;

	AuDebugOn(!name || !len || !wh);

	if (unlikely(len > PATH_MAX - AUFS_WH_PFX_LEN))
		return -ENAMETOOLONG;

	wh->len = len + AUFS_WH_PFX_LEN;
	wh->name = p = kmalloc(wh->len, GFP_KERNEL);
	//if (LktrCond) {kfree(p); wh->name = p = NULL;}
	if (p) {
		memcpy(p, AUFS_WH_PFX, AUFS_WH_PFX_LEN);
		memcpy(p + AUFS_WH_PFX_LEN, name, len);
		//smp_mb();
		return 0;
	}
	return -ENOMEM;
}

void au_free_whname(struct qstr *wh)
{
	AuDebugOn(!wh || !wh->name);
	kfree(wh->name);
#ifdef CONFIG_AUFS_DEBUG
	wh->name = NULL;
#endif
}

/* ---------------------------------------------------------------------- */

/*
 * test if the @wh_name exists under @hidden_parent.
 * @try_sio specifies the necessary of super-io.
 */
int is_wh(struct dentry *hidden_parent, struct qstr *wh_name, int try_sio,
	  struct lkup_args *lkup)
{
	int err;
	struct dentry *wh_dentry;
	struct inode *hidden_dir;

	LKTRTrace("%.*s/%.*s, lkup{%p, %d}\n", DLNPair(hidden_parent),
		  wh_name->len, wh_name->name, lkup->nfsmnt, lkup->dlgt);
	hidden_dir = hidden_parent->d_inode;
	AuDebugOn(!S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	if (!try_sio)
		wh_dentry = lkup_one(wh_name->name, hidden_parent,
				     wh_name->len, lkup);
	else
		wh_dentry = sio_lkup_one(wh_name->name, hidden_parent,
					 wh_name->len, lkup);
	//if (LktrCond) {dput(wh_dentry); wh_dentry = ERR_PTR(-1);}
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	err = 0;
	if (!wh_dentry->d_inode)
		goto out_wh; /* success */

	err = 1;
	if (S_ISREG(wh_dentry->d_inode->i_mode))
		goto out_wh; /* success */

	err = -EIO;
	IOErr("%.*s Invalid whiteout entry type 0%o.\n",
	      DLNPair(wh_dentry), wh_dentry->d_inode->i_mode);

 out_wh:
	dput(wh_dentry);
 out:
	TraceErr(err);
	return err;
}

/*
 * test if the @hidden_dentry sets opaque or not.
 */
int is_diropq(struct dentry *hidden_dentry, struct lkup_args *lkup)
{
	int err;
	struct inode *hidden_dir;

	LKTRTrace("dentry %.*s\n", DLNPair(hidden_dentry));
	hidden_dir = hidden_dentry->d_inode;
	AuDebugOn(!S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	err = is_wh(hidden_dentry, &diropq_name, /*try_sio*/1, lkup);
	TraceErr(err);
	return err;
}

/*
 * returns a negative dentry whose name is unique and temporary.
 */
struct dentry *lkup_whtmp(struct dentry *hidden_parent, struct qstr *prefix,
			  struct lkup_args *lkup)
{
#define HEX_LEN 4
	struct dentry *dentry;
	int len, i;
	char defname[AUFS_WH_PFX_LEN * 2 + DNAME_INLINE_LEN_MIN + 1
		     + HEX_LEN + 1], *name, *p;
	static unsigned char cnt;

	LKTRTrace("hp %.*s, prefix %.*s\n",
		  DLNPair(hidden_parent), prefix->len, prefix->name);
	AuDebugOn(!hidden_parent->d_inode);
	IMustLock(hidden_parent->d_inode);

	name = defname;
	len = sizeof(defname) - DNAME_INLINE_LEN_MIN + prefix->len - 1;
	if (unlikely(prefix->len > DNAME_INLINE_LEN_MIN)) {
		dentry = ERR_PTR(-ENAMETOOLONG);
		if (unlikely(len >= PATH_MAX))
			goto out;
		dentry = ERR_PTR(-ENOMEM);
		name = kmalloc(len + 1, GFP_KERNEL);
		//if (LktrCond) {kfree(name); name = NULL;}
		if (unlikely(!name))
			goto out;
	}

	// doubly whiteout-ed
	memcpy(name, AUFS_WH_PFX AUFS_WH_PFX, AUFS_WH_PFX_LEN * 2);
	p = name + AUFS_WH_PFX_LEN * 2;
	memcpy(p, prefix->name, prefix->len);
	p += prefix->len;
	*p++ = '.';
	AuDebugOn(name + len + 1 - p <= HEX_LEN);

	for (i = 0; i < 3; i++) {
		sprintf(p, "%.*d", HEX_LEN, cnt++);
		dentry = sio_lkup_one(name, hidden_parent, len, lkup);
		//if (LktrCond) {dput(dentry); dentry = ERR_PTR(-1);}
		if (unlikely(IS_ERR(dentry) || !dentry->d_inode))
			goto out_name;
		dput(dentry);
	}
	//Warn("could not get random name\n");
	dentry = ERR_PTR(-EEXIST);
	Dbg("%.*s\n", len, name);
	BUG();

 out_name:
	if (unlikely(name != defname))
		kfree(name);
 out:
	TraceErrPtr(dentry);
	return dentry;
#undef HEX_LEN
}

/*
 * rename the @dentry of @bindex to the whiteouted temporary name.
 */
int rename_whtmp(struct dentry *dentry, aufs_bindex_t bindex)
{
	int err;
	struct inode *hidden_dir;
	struct dentry *hidden_dentry, *hidden_parent, *tmp_dentry;
	struct super_block *sb;
	struct lkup_args lkup;

	LKTRTrace("%.*s, b%d\n", DLNPair(dentry), bindex);
	hidden_dentry = au_h_dptr_i(dentry, bindex);
	AuDebugOn(!hidden_dentry || !hidden_dentry->d_inode);
	hidden_parent = dget_parent(hidden_dentry);
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	sb = dentry->d_sb;
	lkup.nfsmnt = au_nfsmnt(sb, bindex);
	lkup.dlgt = need_dlgt(sb);
	tmp_dentry = lkup_whtmp(hidden_parent, &hidden_dentry->d_name, &lkup);
	//if (LktrCond) {dput(tmp_dentry); tmp_dentry = ERR_PTR(-1);}
	err = PTR_ERR(tmp_dentry);
	if (!IS_ERR(tmp_dentry)) {
		/* under the same dir, no need to lock_rename() */
		err = vfsub_rename(hidden_dir, hidden_dentry,
				   hidden_dir, tmp_dentry, lkup.dlgt);
		//if (LktrCond) err = -1; //unavailable
		TraceErr(err);
		dput(tmp_dentry);
	}
	dput(hidden_parent);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int au_unlink_wh_dentry(struct inode *hidden_dir, struct dentry *wh_dentry,
			struct dentry *dentry, int dlgt)
{
	int err;

	LKTRTrace("hi%lu, wh %.*s, d %p\n", hidden_dir->i_ino,
		  DLNPair(wh_dentry), dentry);
	AuDebugOn((dentry && dbwh(dentry) == -1)
		  || !wh_dentry->d_inode
		  || !S_ISREG(wh_dentry->d_inode->i_mode));
	IMustLock(hidden_dir);

	err = vfsub_unlink(hidden_dir, wh_dentry, dlgt);
	//if (LktrCond) err = -1; // unavailable
	if (!err && dentry)
		set_dbwh(dentry, -1);

	TraceErr(err);
	return err;
}

static int unlink_wh_name(struct dentry *hidden_parent, struct qstr *wh,
			  struct lkup_args *lkup)
{
	int err;
	struct inode *hidden_dir;
	struct dentry *hidden_dentry;

	LKTRTrace("%.*s/%.*s\n", DLNPair(hidden_parent), LNPair(wh));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	// au_test_perm() is already done
	hidden_dentry = lkup_one(wh->name, hidden_parent, wh->len, lkup);
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	if (!IS_ERR(hidden_dentry)) {
		err = 0;
		if (hidden_dentry->d_inode)
			err = vfsub_unlink(hidden_dir, hidden_dentry,
					   lkup->dlgt);
		dput(hidden_dentry);
	} else
		err = PTR_ERR(hidden_dentry);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static void clean_wh(struct inode *h_dir, struct dentry *wh)
{
	TraceEnter();
	if (wh->d_inode) {
		int err = vfsub_unlink(h_dir, wh, /*dlgt*/0);
		if (unlikely(err))
			Warn("failed unlink %.*s (%d), ignored.\n",
			     DLNPair(wh), err);
	}
}

static void clean_plink(struct inode *h_dir, struct dentry *plink)
{
	TraceEnter();
	if (plink->d_inode) {
		int err = vfsub_rmdir(h_dir, plink, /*dlgt*/0);
		if (unlikely(err))
			Warn("failed rmdir %.*s (%d), ignored.\n",
			     DLNPair(plink), err);
	}
}

static int test_linkable(struct inode *h_dir)
{
	if (h_dir->i_op && h_dir->i_op->link)
		return 0;
	return -ENOSYS;
}

static int plink_dir(struct inode *h_dir, struct dentry *plink)
{
	int err;

	err = -EEXIST;
	if (!plink->d_inode) {
		int mode = S_IRWXU;
		if (unlikely(au_is_nfs(plink->d_sb)))
			mode |= S_IXUGO;
		err = vfsub_mkdir(h_dir, plink, mode, /*dlgt*/0);
	} else if (S_ISDIR(plink->d_inode->i_mode))
		err = 0;
	else
		Err("unknown %.*s exists\n", DLNPair(plink));

	return err;
}

/*
 * initialize the whiteout base file/dir for @br.
 */
int init_wh(struct dentry *h_root, struct aufs_branch *br,
	    struct vfsmount *nfsmnt, struct super_block *sb)
{
	int err;
	struct dentry *wh, *plink;
	struct inode *h_dir;
	static struct qstr base_name[] = {
		{.name = AUFS_WH_BASENAME, .len = sizeof(AUFS_WH_BASENAME) - 1},
		{.name = AUFS_WH_PLINKDIR, .len = sizeof(AUFS_WH_PLINKDIR) - 1}
	};
	struct lkup_args lkup = {
		.nfsmnt	= nfsmnt,
		.dlgt	= 0 // always no dlgt
	};
	const int do_plink = au_flag_test(sb, AuFlag_PLINK);

	LKTRTrace("nfsmnt %p\n", nfsmnt);
	BrWhMustWriteLock(br);
	SiMustWriteLock(sb);
	h_dir = h_root->d_inode;
	IMustLock(h_dir);

	// doubly whiteouted
	wh = lkup_wh(h_root, base_name + 0, &lkup);
	//if (LktrCond) {dput(wh); wh = ERR_PTR(-1);}
	err = PTR_ERR(wh);
	if (IS_ERR(wh))
		goto out;
	AuDebugOn(br->br_wh && br->br_wh != wh);

	plink = lkup_wh(h_root, base_name + 1, &lkup);
	err = PTR_ERR(plink);
	if (IS_ERR(plink))
		goto out_dput_wh;
	AuDebugOn(br->br_plink && br->br_plink != plink);

	dput(br->br_wh);
	dput(br->br_plink);
	br->br_wh = br->br_plink = NULL;

	err = 0;
	switch (br->br_perm) {
	case AuBr_RR:
	case AuBr_RO:
	case AuBr_RRWH:
	case AuBr_ROWH:
		clean_wh(h_dir, wh);
		clean_plink(h_dir, plink);
		break;

	case AuBr_RWNoLinkWH:
		clean_wh(h_dir, wh);
		if (do_plink) {
			err = test_linkable(h_dir);
			if (unlikely(err))
				goto out_nolink;

			err = plink_dir(h_dir, plink);
			if (unlikely(err))
				goto out_err;
			br->br_plink = dget(plink);
		} else
			clean_plink(h_dir, plink);
		break;

	case AuBr_RW:
		/*
		 * for the moment, aufs supports the branch filesystem
		 * which does not support link(2).
		 * testing on FAT which does not support i_op->setattr() fully either,
		 * copyup failed.
		 * finally, such filesystem will not be used as the writable branch.
		 */
		err = test_linkable(h_dir);
		if (unlikely(err))
			goto out_nolink;

		err = -EEXIST;
		if (!wh->d_inode)
			err = vfsub_create(h_dir, wh, WH_MASK, NULL, /*dlgt*/0);
		else if (S_ISREG(wh->d_inode->i_mode))
			err = 0;
		else
			Err("unknown %.*s/%.*s exists\n",
			    DLNPair(h_root), DLNPair(wh));
		if (unlikely(err))
			goto out_err;

		if (do_plink) {
			err = plink_dir(h_dir, plink);
			if (unlikely(err))
				goto out_err;
			br->br_plink = dget(plink);
		} else
			clean_plink(h_dir, plink);
		br->br_wh = dget(wh);
		break;

	default:
		BUG();
	}

 out_dput:
	dput(plink);
 out_dput_wh:
	dput(wh);
 out:
	TraceErr(err);
	return err;
 out_nolink:
	Err("%.*s doesn't support link(2), use noplink and rw+nolwh\n",
	    DLNPair(h_root));
	goto out_dput;
 out_err:
	Err("an error(%d) on the writable branch %.*s(%s)\n",
	    err, DLNPair(h_root), au_sbtype(h_root->d_sb));
	goto out_dput;
}

struct reinit_br_wh {
	struct super_block *sb;
	struct aufs_branch *br;
};

static void reinit_br_wh(void *arg)
{
	int err;
	struct reinit_br_wh *a = arg;
	struct inode *hidden_dir, *dir;
	struct dentry *hidden_root;
	aufs_bindex_t bindex;

	TraceEnter();
	AuDebugOn(!a->br->br_wh || !a->br->br_wh->d_inode || current->fsuid);

	err = 0;
	/* aufs big lock */
	si_write_lock(a->sb);
	if (unlikely(!br_writable(a->br->br_perm)))
		goto out;
	bindex = find_brindex(a->sb, a->br->br_id);
	if (unlikely(bindex < 0))
		goto out;

	dir = a->sb->s_root->d_inode;
	hidden_root = dget_parent(a->br->br_wh);
	hidden_dir = hidden_root->d_inode;
	AuDebugOn(!hidden_dir->i_op || !hidden_dir->i_op->link);
	hdir_lock(hidden_dir, dir, bindex);
	br_wh_write_lock(a->br);
	err = vfsub_unlink(hidden_dir, a->br->br_wh, /*dlgt*/0);
	//if (LktrCond) err = -1;
	dput(a->br->br_wh);
	a->br->br_wh = NULL;
	if (!err)
		err = init_wh(hidden_root, a->br, au_do_nfsmnt(a->br->br_mnt),
			      a->sb);
	br_wh_write_unlock(a->br);
	hdir_unlock(hidden_dir, dir, bindex);
	dput(hidden_root);

 out:
	atomic_dec(&a->br->br_wh_running);
	br_put(a->br);
	si_write_unlock(a->sb);
	kfree(arg);
	if (unlikely(err))
		IOErr("err %d\n", err);
}

static void kick_reinit_br_wh(struct super_block *sb, struct aufs_branch *br)
{
	int do_dec, wkq_err;
	struct reinit_br_wh *arg;

	do_dec = 1;
	if (atomic_inc_return(&br->br_wh_running) != 1)
		goto out;

	// ignore ENOMEM
	arg = kmalloc(sizeof(*arg), GFP_KERNEL);
	if (arg) {
		// dec(wh_running), kfree(arg) and br_put() in reinit function
		arg->sb = sb;
		arg->br = br;
		br_get(br);
		wkq_err = au_wkq_nowait(reinit_br_wh, arg, sb, /*dlgt*/0);
		if (unlikely(wkq_err)) {
			atomic_dec(&br->br_wh_running);
			br_put(br);
			kfree(arg);
		}
		do_dec = 0;
	}

 out:
	if (do_dec)
		atomic_dec(&br->br_wh_running);
}

/*
 * create the whiteoute @wh.
 */
static int link_or_create_wh(struct dentry *wh, struct super_block *sb,
			     aufs_bindex_t bindex)
{
	int err, dlgt;
	struct aufs_branch *br;
	struct dentry *hidden_parent;
	struct inode *hidden_dir;

	LKTRTrace("%.*s\n", DLNPair(wh));
	SiMustReadLock(sb);
	hidden_parent = dget_parent(wh);
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	dlgt = need_dlgt(sb);
	br = stobr(sb, bindex);
	br_wh_read_lock(br);
	if (br->br_wh) {
		err = vfsub_link(br->br_wh, hidden_dir, wh, dlgt);
		if (!err || err != -EMLINK)
			goto out;

		// link count full. re-initialize br_wh.
		kick_reinit_br_wh(sb, br);
	}

	// return this error in this context
	err = vfsub_create(hidden_dir, wh, WH_MASK, NULL, dlgt);

 out:
	br_wh_read_unlock(br);
	dput(hidden_parent);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * create or remove the diropq.
 */
static struct dentry *do_diropq(struct dentry *dentry, aufs_bindex_t bindex,
				int do_create, int dlgt)
{
	struct dentry *opq_dentry, *hidden_dentry;
	struct inode *hidden_dir;
	int err;
	struct super_block *sb;
	struct lkup_args lkup;

	LKTRTrace("%.*s, bindex %d, do_create %d\n", DLNPair(dentry),
		  bindex, do_create);
	hidden_dentry = au_h_dptr_i(dentry, bindex);
	AuDebugOn(!hidden_dentry);
	hidden_dir = hidden_dentry->d_inode;
	AuDebugOn(!hidden_dir || !S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	// already checked by au_test_perm().
	sb = dentry->d_sb;
	lkup.nfsmnt = au_nfsmnt(sb, bindex);
	lkup.dlgt = dlgt;
	opq_dentry = lkup_one(diropq_name.name, hidden_dentry, diropq_name.len,
			      &lkup);
	//if (LktrCond) {dput(opq_dentry); opq_dentry = ERR_PTR(-1);}
	if (IS_ERR(opq_dentry))
		goto out;

	if (do_create) {
		AuDebugOn(opq_dentry->d_inode);
		err = link_or_create_wh(opq_dentry, sb, bindex);
		//if (LktrCond) {vfs_unlink(hidden_dir, opq_dentry); err = -1;}
		if (!err) {
			set_dbdiropq(dentry, bindex);
			goto out; /* success */
		}
	} else {
		AuDebugOn(/* !S_ISDIR(dentry->d_inode->i_mode)
			   * ||  */!opq_dentry->d_inode);
		err = vfsub_unlink(hidden_dir, opq_dentry, lkup.dlgt);
		//if (LktrCond) err = -1;
		if (!err)
			set_dbdiropq(dentry, -1);
	}
	dput(opq_dentry);
	opq_dentry = ERR_PTR(err);

 out:
	TraceErrPtr(opq_dentry);
	return opq_dentry;
}

struct do_diropq_args {
	struct dentry **errp;
	struct dentry *dentry;
	aufs_bindex_t bindex;
	int do_create, dlgt;
};

static void call_do_diropq(void *args)
{
	struct do_diropq_args *a = args;
	*a->errp = do_diropq(a->dentry, a->bindex, a->do_create, a->dlgt);
}

struct dentry *sio_diropq(struct dentry *dentry, aufs_bindex_t bindex,
			  int do_create, int dlgt)
{
	struct dentry *diropq, *hidden_dentry;

	LKTRTrace("%.*s, bindex %d, do_create %d\n",
		  DLNPair(dentry), bindex, do_create);

	hidden_dentry = au_h_dptr_i(dentry, bindex);
	if (!au_test_perm(hidden_dentry->d_inode, MAY_EXEC | MAY_WRITE, dlgt))
		diropq = do_diropq(dentry, bindex, do_create, dlgt);
	else {
		int wkq_err;
		struct do_diropq_args args = {
			.errp		= &diropq,
			.dentry		= dentry,
			.bindex		= bindex,
			.do_create	= do_create,
			.dlgt		= dlgt
		};
		wkq_err = au_wkq_wait(call_do_diropq, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			diropq = ERR_PTR(wkq_err);
	}

	TraceErrPtr(diropq);
	return diropq;
}

/* ---------------------------------------------------------------------- */

/*
 * lookup whiteout dentry.
 * @hidden_parent: hidden parent dentry which must exist and be locked
 * @base_name: name of dentry which will be whiteouted
 * returns dentry for whiteout.
 */
struct dentry *lkup_wh(struct dentry *hidden_parent, struct qstr *base_name,
		       struct lkup_args *lkup)
{
	int err;
	struct qstr wh_name;
	struct dentry *wh_dentry;

	LKTRTrace("%.*s/%.*s\n", DLNPair(hidden_parent), LNPair(base_name));
	IMustLock(hidden_parent->d_inode);

	err = au_alloc_whname(base_name->name, base_name->len, &wh_name);
	//if (LktrCond) {au_free_whname(&wh_name); err = -1;}
	wh_dentry = ERR_PTR(err);
	if (!err) {
		// do not superio.
		wh_dentry = lkup_one(wh_name.name, hidden_parent, wh_name.len,
				     lkup);
		au_free_whname(&wh_name);
	}
	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

/*
 * link/create a whiteout for @dentry on @bindex.
 */
struct dentry *simple_create_wh(struct dentry *dentry, aufs_bindex_t bindex,
				struct dentry *hidden_parent,
				struct lkup_args *lkup)
{
	struct dentry *wh_dentry;
	int err;
	struct super_block *sb;

	LKTRTrace("%.*s/%.*s on b%d\n", DLNPair(hidden_parent),
		  DLNPair(dentry), bindex);

	sb = dentry->d_sb;
	wh_dentry = lkup_wh(hidden_parent, &dentry->d_name, lkup);
	//au_nfsmnt(sb, bindex), need_dlgt(sb));
	//if (LktrCond) {dput(wh_dentry); wh_dentry = ERR_PTR(-1);}
	if (!IS_ERR(wh_dentry) && !wh_dentry->d_inode) {
		IMustLock(hidden_parent->d_inode);
		err = link_or_create_wh(wh_dentry, sb, bindex);
		if (!err)
			set_dbwh(dentry, bindex);
		else {
			dput(wh_dentry);
			wh_dentry = ERR_PTR(err);
		}
	}

	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

/* ---------------------------------------------------------------------- */

/* Delete all whiteouts in this directory in branch bindex. */
static int del_wh_children(struct aufs_nhash *whlist,
			   struct dentry *hidden_parent, aufs_bindex_t bindex,
			   struct lkup_args *lkup)
{
	int err, i;
	struct qstr wh_name;
	char *p;
	struct inode *hidden_dir;
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos;
	struct aufs_destr *str;

	LKTRTrace("%.*s\n", DLNPair(hidden_parent));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);
	AuDebugOn(IS_RDONLY(hidden_dir));
	//SiMustReadLock(??);

	err = -ENOMEM;
	wh_name.name = p = __getname();
	//if (LktrCond) {__putname(p); wh_name.name = p = NULL;}
	if (unlikely(!wh_name.name))
		goto out;
	memcpy(p, AUFS_WH_PFX, AUFS_WH_PFX_LEN);
	p += AUFS_WH_PFX_LEN;

	// already checked by au_test_perm().
	err = 0;
	for (i = 0; !err && i < AUFS_NHASH_SIZE; i++) {
		head = whlist->heads + i;
		hlist_for_each_entry(tpos, pos, head, wh_hash) {
			if (tpos->wh_bindex != bindex)
				continue;
			str = &tpos->wh_str;
			if (str->len + AUFS_WH_PFX_LEN <= PATH_MAX) {
				memcpy(p, str->name, str->len);
				wh_name.len = AUFS_WH_PFX_LEN + str->len;
				err = unlink_wh_name(hidden_parent, &wh_name,
						     lkup);
				//if (LktrCond) err = -1;
				if (!err)
					continue;
				break;
			}
			IOErr("whiteout name too long %.*s\n",
			      str->len, str->name);
			err = -EIO;
			break;
		}
	}
	__putname(wh_name.name);

 out:
	TraceErr(err);
	return err;
}

struct del_wh_children_args {
	int *errp;
	struct aufs_nhash *whlist;
	struct dentry *hidden_parent;
	aufs_bindex_t bindex;
	struct lkup_args *lkup;
};

static void call_del_wh_children(void *args)
{
	struct del_wh_children_args *a = args;
	*a->errp = del_wh_children(a->whlist, a->hidden_parent, a->bindex,
				   a->lkup);
}

/* ---------------------------------------------------------------------- */

/*
 * rmdir the whiteouted temporary named dir @hidden_dentry.
 * @whlist: whiteouted children.
 */
int rmdir_whtmp(struct dentry *hidden_dentry, struct aufs_nhash *whlist,
		aufs_bindex_t bindex, struct inode *dir, struct inode *inode)
{
	int err;
	struct inode *hidden_inode, *hidden_dir;
	struct dentry *h_parent;
	struct lkup_args lkup;
	struct super_block *sb;

	LKTRTrace("hd %.*s, b%d, i%lu\n",
		  DLNPair(hidden_dentry), bindex, dir->i_ino);
	IMustLock(dir);
	IiMustAnyLock(dir);
	h_parent = dget_parent(hidden_dentry);
	hidden_dir = h_parent->d_inode;
	IMustLock(hidden_dir);

	sb = inode->i_sb;
	lkup.nfsmnt = au_nfsmnt(sb, bindex);
	lkup.dlgt = need_dlgt(sb);
	hidden_inode = hidden_dentry->d_inode;
	AuDebugOn(hidden_inode != au_h_iptr_i(inode, bindex));
	hdir2_lock(hidden_inode, inode, bindex);
	if (!au_test_perm(hidden_inode, MAY_EXEC | MAY_WRITE, lkup.dlgt))
		err = del_wh_children(whlist, hidden_dentry, bindex, &lkup);
	else {
		int wkq_err;
		// ugly
		int dlgt = lkup.dlgt;
		struct del_wh_children_args args = {
			.errp		= &err,
			.whlist		= whlist,
			.hidden_parent	= hidden_dentry,
			.bindex		= bindex,
			.lkup		= &lkup
		};

		lkup.dlgt = 0;
		wkq_err = au_wkq_wait(call_del_wh_children, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
		lkup.dlgt = dlgt;
	}
	hdir_unlock(hidden_inode, inode, bindex);

	if (!err) {
		err = vfsub_rmdir(hidden_dir, hidden_dentry, lkup.dlgt);
		//d_drop(hidden_dentry);
		//if (LktrCond) err = -1;
	}
	dput(h_parent);

	if (!err) {
		if (ibstart(dir) == bindex) {
			au_cpup_attr_timesizes(dir);
			//au_cpup_attr_nlink(dir);
			dir->i_nlink--;
		}
		return 0; /* success */
	}

	Warn("failed removing %.*s(%d), ignored\n",
	     DLNPair(hidden_dentry), err);
	return err;
}

static void rmdir_whtmp_free_args(struct rmdir_whtmp_args *args)
{
	dput(args->h_dentry);
	nhash_fin(&args->whlist);
	iput(args->inode);
	i_unlock(args->dir);
	iput(args->dir);
	kfree(args);
}

static void do_rmdir_whtmp(void *args)
{
	int err;
	struct rmdir_whtmp_args *a = args;
	struct super_block *sb;

	LKTRTrace("%.*s, b%d, dir i%lu\n",
		  DLNPair(a->h_dentry), a->bindex, a->dir->i_ino);

	i_lock(a->dir);
	sb = a->dir->i_sb;
	//DbgSleep(3);
	si_read_lock(sb);
	err = test_ro(sb, a->bindex, NULL);
	if (!err) {
		struct dentry *h_parent = dget_parent(a->h_dentry);
		struct inode *hidden_dir = h_parent->d_inode;

		ii_write_lock_child(a->inode);
		ii_write_lock_parent(a->dir);
		hdir_lock(hidden_dir, a->dir, a->bindex);
		err = rmdir_whtmp(a->h_dentry, &a->whlist, a->bindex,
				  a->dir, a->inode);
		hdir_unlock(hidden_dir, a->dir, a->bindex);
		ii_write_unlock(a->dir);
		ii_write_unlock(a->inode);
		dput(h_parent);
	}
	si_read_unlock(sb);
	rmdir_whtmp_free_args(a);
	if (unlikely(err))
		IOErr("err %d\n", err);
}

void kick_rmdir_whtmp(struct dentry *h_dentry, struct aufs_nhash *whlist,
		      aufs_bindex_t bindex, struct inode *dir,
		      struct inode *inode, struct rmdir_whtmp_args *args)
{
	int wkq_err;

	LKTRTrace("%.*s\n", DLNPair(h_dentry));
	IMustLock(dir);

	// all post-process will be done in do_rmdir_whtmp().
	args->h_dentry = dget(h_dentry);
	nhash_init(&args->whlist);
	nhash_move(&args->whlist, whlist);
	args->bindex = bindex;
	args->dir = igrab(dir);
	args->inode = igrab(inode);
	wkq_err = au_wkq_nowait(do_rmdir_whtmp, args, dir->i_sb, /*dlgt*/0);
	if (unlikely(wkq_err)) {
		Warn("rmdir error %.*s (%d), ignored\n",
		     DLNPair(h_dentry), wkq_err);
		rmdir_whtmp_free_args(args);
	}
}
