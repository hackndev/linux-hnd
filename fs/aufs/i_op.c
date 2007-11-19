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

/* $Id: i_op.c,v 1.33 2007/06/04 02:17:35 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include <linux/security.h>
#include <asm/uaccess.h>
#include "aufs.h"

#ifdef CONFIG_AUFS_DLGT
struct security_inode_permission_args {
	int *errp;
	struct inode *h_inode;
	int mask;
	struct nameidata *fake_nd;
};

static void call_security_inode_permission(void *args)
{
	struct security_inode_permission_args *a = args;
	LKTRTrace("fsuid %d\n", current->fsuid);
	*a->errp = security_inode_permission(a->h_inode, a->mask, a->fake_nd);
}
#endif

static int hidden_permission(struct inode *hidden_inode, int mask,
			     struct nameidata *fake_nd, int brperm, int dlgt)
{
	int err, submask;
	const int write_mask = (mask & (MAY_WRITE | MAY_APPEND));

	LKTRTrace("ino %lu, mask 0x%x, brperm 0x%x\n",
		  hidden_inode->i_ino, mask, brperm);

	err = -EACCES;
	if (unlikely(write_mask && IS_IMMUTABLE(hidden_inode)))
		goto out;

	/* skip hidden fs test in the case of write to ro branch */
	submask = mask & ~MAY_APPEND;
	if (unlikely((write_mask && !br_writable(brperm))
		     || !hidden_inode->i_op
		     || !hidden_inode->i_op->permission)) {
		//LKTRLabel(generic_permission);
		err = generic_permission(hidden_inode, submask, NULL);
	} else {
		//LKTRLabel(h_inode->permission);
		err = hidden_inode->i_op->permission(hidden_inode, submask,
						     fake_nd);
		TraceErr(err);
	}

#if 1
	if (!err) {
#ifndef CONFIG_AUFS_DLGT
		err = security_inode_permission(hidden_inode, mask, fake_nd);
#else
		if (!dlgt)
			err = security_inode_permission(hidden_inode, mask,
							fake_nd);
		else {
			int wkq_err;
			struct security_inode_permission_args args = {
				.errp		= &err,
				.h_inode	= hidden_inode,
				.mask		= mask,
				.fake_nd	= fake_nd
			};
			wkq_err = au_wkq_wait(call_security_inode_permission,
					      &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
		}
#endif
	}
#endif

 out:
	TraceErr(err);
	return err;
}

static int silly_lock(struct inode *inode, struct nameidata *nd)
{
	int locked = 0;
	struct super_block *sb = inode->i_sb;

	LKTRTrace("i%lu, nd %p\n", inode->i_ino, nd);

#ifdef CONFIG_AUFS_FAKE_DM
	si_read_lock(sb);
	ii_read_lock_child(inode);
#else
	if (!nd || !nd->dentry) {
		si_read_lock(sb);
		ii_read_lock_child(inode);
	} else if (nd->dentry->d_inode != inode) {
		locked = 1;
		/* lock child first, then parent */
		si_read_lock(sb);
		ii_read_lock_child(inode);
		di_read_lock_parent(nd->dentry, 0);
	} else {
		locked = 2;
		aufs_read_lock(nd->dentry, AUFS_I_RLOCK);
	}
#endif
	return locked;
}

static void silly_unlock(int locked, struct inode *inode, struct nameidata *nd)
{
	struct super_block *sb = inode->i_sb;

	LKTRTrace("locked %d, i%lu, nd %p\n", locked, inode->i_ino, nd);

#ifdef CONFIG_AUFS_FAKE_DM
	ii_read_unlock(inode);
	si_read_unlock(sb);
#else
	switch (locked) {
	case 0:
		ii_read_unlock(inode);
		si_read_unlock(sb);
		break;
	case 1:
		di_read_unlock(nd->dentry, 0);
		ii_read_unlock(inode);
		si_read_unlock(sb);
		break;
	case 2:
		aufs_read_unlock(nd->dentry, AUFS_I_RLOCK);
		break;
	default:
		BUG();
	}
#endif
}

static int aufs_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	int err, locked, dlgt;
	aufs_bindex_t bindex, bend;
	struct inode *hidden_inode;
	struct super_block *sb;
	struct nameidata fake_nd, *p;
	const int write_mask = (mask & (MAY_WRITE | MAY_APPEND));
	const int nondir = !S_ISDIR(inode->i_mode);

	LKTRTrace("ino %lu, mask 0x%x, nondir %d, write_mask %d, "
		  "nd %p{%p, %p}\n",
		  inode->i_ino, mask, nondir, write_mask,
		  nd, nd ? nd->dentry : NULL, nd ? nd->mnt : NULL);

	sb = inode->i_sb;
	locked = silly_lock(inode, nd);
	dlgt = need_dlgt(sb);

	if (nd)
		fake_nd = *nd;
	if (/* unlikely */(nondir || write_mask)) {
		hidden_inode = au_h_iptr(inode);
		AuDebugOn(!hidden_inode
			  || ((hidden_inode->i_mode & S_IFMT)
			      != (inode->i_mode & S_IFMT)));
		err = 0;
		bindex = ibstart(inode);
		p = fake_dm(&fake_nd, nd, sb, bindex);
		/* actual test will be delegated to LSM */
		if (IS_ERR(p))
			AuDebugOn(PTR_ERR(p) != -ENOENT);
		else {
			err = hidden_permission(hidden_inode, mask, p,
						sbr_perm(sb, bindex), dlgt);
			fake_dm_release(p);
		}
		if (write_mask && !err) {
			err = find_rw_br(sb, bindex);
			if (err >= 0)
				err = 0;
		}
		goto out;
	}

	/* non-write to dir */
	err = 0;
	bend = ibend(inode);
	for (bindex = ibstart(inode); !err && bindex <= bend; bindex++) {
		hidden_inode = au_h_iptr_i(inode, bindex);
		if (!hidden_inode)
			continue;
		AuDebugOn(!S_ISDIR(hidden_inode->i_mode));

		p = fake_dm(&fake_nd, nd, sb, bindex);
		/* actual test will be delegated to LSM */
		if (IS_ERR(p))
			AuDebugOn(PTR_ERR(p) != -ENOENT);
		else {
			err = hidden_permission(hidden_inode, mask, p,
						sbr_perm(sb, bindex), dlgt);
			fake_dm_release(p);
		}
	}

 out:
	silly_unlock(locked, inode, nd);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static struct dentry *aufs_lookup(struct inode *dir, struct dentry *dentry,
				  struct nameidata *nd)
{
	struct dentry *ret, *parent;
	int err, npositive;
	struct inode *inode;

	LKTRTrace("dir %lu, %.*s\n", dir->i_ino, DLNPair(dentry));
	AuDebugOn(IS_ROOT(dentry));
	IMustLock(dir);

	// nd may be NULL
	parent = dget_parent(dentry);
	aufs_read_lock(parent, !AUFS_I_RLOCK);
	err = au_alloc_dinfo(dentry);
	//if (LktrCond) err = -1;
	ret = ERR_PTR(err);
	if (unlikely(err))
		goto out;

	err = npositive = lkup_dentry(dentry, dbstart(parent), /*type*/0);
	//err = -1;
	ret = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out_unlock;
	inode = NULL;
	if (npositive) {
		inode = au_new_inode(dentry);
		ret = (void*)inode;
	}
	if (!IS_ERR(inode)) {
#if 1
		/* d_splice_alias() also supports d_add() */
		ret = d_splice_alias(inode, dentry);
		if (unlikely(IS_ERR(ret) && inode))
			ii_write_unlock(inode);
#else
		d_add(dentry, inode);
#endif
	}

 out_unlock:
	di_write_unlock(dentry);
 out:
	aufs_read_unlock(parent, !AUFS_I_RLOCK);
	dput(parent);
	TraceErrPtr(ret);
	return ret;
}

/* ---------------------------------------------------------------------- */

/*
 * decide the branch and the parent dir where we will create a new entry.
 * returns new bindex or an error.
 * copyup the parent dir if needed.
 */
int wr_dir(struct dentry *dentry, int add_entry, struct dentry *src_dentry,
	   aufs_bindex_t force_btgt, int do_lock_srcdir)
{
	int err;
	aufs_bindex_t bcpup, bstart, src_bstart;
	struct dentry *hidden_parent;
	struct super_block *sb;
	struct dentry *parent, *src_parent = NULL;
	struct inode *dir, *src_dir = NULL;

	LKTRTrace("%.*s, add %d, src %p, force %d, lock_srcdir %d\n",
		  DLNPair(dentry), add_entry, src_dentry, force_btgt,
		  do_lock_srcdir);

	sb = dentry->d_sb;
	parent = dget_parent(dentry);
	bcpup = bstart = dbstart(dentry);
	if (force_btgt < 0) {
		if (src_dentry) {
			src_bstart = dbstart(src_dentry);
			if (src_bstart < bstart)
				bcpup = src_bstart;
		}
		if (test_ro(sb, bcpup, dentry->d_inode)) {
			if (!add_entry)
				di_read_lock_parent(parent, !AUFS_I_RLOCK);
			bcpup = err = find_rw_parent_br(dentry, bcpup);
			//bcpup = err = find_rw_br(sb, bcpup);
			if (!add_entry)
				di_read_unlock(parent, !AUFS_I_RLOCK);
			//err = -1;
			if (unlikely(err < 0))
				goto out;
		}
	} else {
		AuDebugOn(bstart <= force_btgt
			  || test_ro(sb, force_btgt, dentry->d_inode));
		bcpup = force_btgt;
	}
	LKTRTrace("bstart %d, bcpup %d\n", bstart, bcpup);

	err = bcpup;
	if (bcpup == bstart)
		goto out; /* success */

	/* copyup the new parent into the branch we process */
	hidden_parent = dget_parent(au_h_dptr(dentry));
	if (src_dentry) {
		src_parent = dget_parent(src_dentry);
		src_dir = src_parent->d_inode;
		if (do_lock_srcdir)
			di_write_lock_parent2(src_parent);
	}

	dir = parent->d_inode;
	if (add_entry) {
		au_update_dbstart(dentry);
		IMustLock(dir);
		DiMustWriteLock(parent);
		IiMustWriteLock(dir);
	} else
		di_write_lock_parent(parent);

	err = 0;
	if (!au_h_dptr_i(parent, bcpup))
		err = cpup_dirs(dentry, bcpup, src_parent);
	//err = -1;
	if (!err && add_entry) {
		dput(hidden_parent);
		hidden_parent = dget(au_h_dptr_i(parent, bcpup));
		AuDebugOn(!hidden_parent || !hidden_parent->d_inode);
		hi_lock_parent(hidden_parent->d_inode);
		err = lkup_neg(dentry, bcpup);
		//err = -1;
		i_unlock(hidden_parent->d_inode);
	}
	dput(hidden_parent);

	if (!add_entry)
		di_write_unlock(parent);
	if (do_lock_srcdir)
		di_write_unlock(src_parent);
	dput(src_parent);
	if (!err)
		err = bcpup; /* success */
	//err = -EPERM;
 out:
	dput(parent);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int aufs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err, isdir;
	aufs_bindex_t bstart, bcpup;
	struct inode *hidden_inode, *inode, *dir, *h_dir, *gh_dir, *gdir;
	struct dentry *hidden_dentry, *parent;
	unsigned int udba;

	LKTRTrace("%.*s, ia_valid 0x%x\n", DLNPair(dentry), ia->ia_valid);
	inode = dentry->d_inode;
	IMustLock(inode);

	aufs_read_lock(dentry, AUFS_D_WLOCK);
	bstart = dbstart(dentry);
	bcpup = err = wr_dir(dentry, /*add*/0, /*src_dentry*/NULL,
			     /*force_btgt*/-1, /*do_lock_srcdir*/0);
	//err = -1;
	if (unlikely(err < 0))
		goto out;

	/* crazy udba locks */
	udba = au_flag_test(dentry->d_sb, AuFlag_UDBA_INOTIFY);
	parent = NULL;
	gdir = gh_dir = dir = h_dir = NULL;
	if ((udba || bstart != bcpup) && !IS_ROOT(dentry)) {
		parent = dentry->d_parent; // dget_parent()
		dir = parent->d_inode;
		di_read_lock_parent(parent, AUFS_I_RLOCK);
		h_dir = au_h_iptr_i(dir, bcpup);
	}
	if (parent) {
		if (unlikely(udba && !IS_ROOT(parent))) {
			gdir = parent->d_parent->d_inode;  // dget_parent()
			ii_read_lock_parent2(gdir);
			gh_dir = au_h_iptr_i(gdir, bcpup);
			hgdir_lock(gh_dir, gdir, bcpup);
		}
		hdir_lock(h_dir, dir, bcpup);
	}

	isdir = S_ISDIR(inode->i_mode);
	hidden_dentry = au_h_dptr(dentry);
	hidden_inode = hidden_dentry->d_inode;
	AuDebugOn(!hidden_inode);

#define HiLock(bindex) do {\
	if (!isdir) \
		hi_lock_child(hidden_inode); \
	else \
		hdir2_lock(hidden_inode, inode, bindex); \
	} while (0)
#define HiUnlock(bindex) do {\
	if (!isdir) \
		i_unlock(hidden_inode); \
	else \
		hdir_unlock(hidden_inode, inode, bindex); \
	} while (0)

	if (bstart != bcpup) {
		loff_t size = -1;

		if ((ia->ia_valid & ATTR_SIZE)
		    && ia->ia_size < i_size_read(inode)) {
			size = ia->ia_size;
			ia->ia_valid &= ~ATTR_SIZE;
		}
		HiLock(bstart);
		err = sio_cpup_simple(dentry, bcpup, size,
				      au_flags_cpup(CPUP_DTIME, parent));
		//err = -1;
		HiUnlock(bstart);
		if (unlikely(err || !ia->ia_valid))
			goto out_unlock;

		hidden_dentry = au_h_dptr(dentry);
		hidden_inode = hidden_dentry->d_inode;
		AuDebugOn(!hidden_inode);
	}

	HiLock(bcpup);
	err = vfsub_notify_change(hidden_dentry, ia, need_dlgt(dentry->d_sb));
	//err = -1;
	if (!err)
		au_cpup_attr_changable(inode);
	HiUnlock(bcpup);
#undef HiLock
#undef HiUnlock

 out_unlock:
	if (parent) {
		hdir_unlock(h_dir, dir, bcpup);
		di_read_unlock(parent, AUFS_I_RLOCK);
	}
	if (unlikely(gdir)) {
		hdir_unlock(gh_dir, gdir, bcpup);
		ii_read_unlock(gdir);
	}
 out:
	aufs_read_unlock(dentry, AUFS_D_WLOCK);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int hidden_readlink(struct dentry *dentry, int bindex,
			   char __user * buf, int bufsiz)
{
	struct super_block *sb;
	struct dentry *hidden_dentry;

	hidden_dentry = au_h_dptr_i(dentry, bindex);
	if (unlikely(!hidden_dentry->d_inode->i_op
		     || !hidden_dentry->d_inode->i_op->readlink))
		return -EINVAL;

	sb = dentry->d_sb;
	if (!test_ro(sb, bindex, dentry->d_inode)) {
		touch_atime(sbr_mnt(sb, bindex), hidden_dentry);
		dentry->d_inode->i_atime = hidden_dentry->d_inode->i_atime;
	}
	return hidden_dentry->d_inode->i_op->readlink
		(hidden_dentry, buf, bufsiz);
}

static int aufs_readlink(struct dentry *dentry, char __user * buf, int bufsiz)
{
	int err;

	LKTRTrace("%.*s, %d\n", DLNPair(dentry), bufsiz);

	aufs_read_lock(dentry, AUFS_I_RLOCK);
	err = hidden_readlink(dentry, dbstart(dentry), buf, bufsiz);
	//err = -1;
	aufs_read_unlock(dentry, AUFS_I_RLOCK);
	TraceErr(err);
	return err;
}

static void *aufs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	int err;
	char *buf;
	mm_segment_t old_fs;

	LKTRTrace("%.*s, nd %.*s\n", DLNPair(dentry), DLNPair(nd->dentry));

	err = -ENOMEM;
	buf = __getname();
	//buf = NULL;
	if (unlikely(!buf))
		goto out;

	aufs_read_lock(dentry, AUFS_I_RLOCK);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = hidden_readlink(dentry, dbstart(dentry), (char __user *)buf,
			      PATH_MAX);
	//err = -1;
	set_fs(old_fs);
	aufs_read_unlock(dentry, AUFS_I_RLOCK);

	if (err >= 0) {
		buf[err] = 0;
		/* will be freed by put_link */
		nd_set_link(nd, buf);
		return NULL; /* success */
	}
	__putname(buf);

 out:
	path_release(nd);
	TraceErr(err);
	return ERR_PTR(err);
}

static void aufs_put_link(struct dentry *dentry, struct nameidata *nd,
			  void *cookie)
{
	LKTRTrace("%.*s\n", DLNPair(dentry));
	__putname(nd_get_link(nd));
}

/* ---------------------------------------------------------------------- */
#if 0 // comment
struct inode_operations {
	int (*create) (struct inode *,struct dentry *,int, struct nameidata *);
	struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct inode *,struct dentry *,const char *);
	int (*mkdir) (struct inode *,struct dentry *,int);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*mknod) (struct inode *,struct dentry *,int,dev_t);
	int (*rename) (struct inode *, struct dentry *,
			struct inode *, struct dentry *);
	int (*readlink) (struct dentry *, char __user *,int);
	void * (*follow_link) (struct dentry *, struct nameidata *);
	void (*put_link) (struct dentry *, struct nameidata *, void *);
	void (*truncate) (struct inode *);
	int (*permission) (struct inode *, int, struct nameidata *);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
	int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*removexattr) (struct dentry *, const char *);
	void (*truncate_range)(struct inode *, loff_t, loff_t);
};
#endif

struct inode_operations aufs_symlink_iop = {
	.permission	= aufs_permission,
	.setattr	= aufs_setattr,

	.readlink	= aufs_readlink,
	.follow_link	= aufs_follow_link,
	.put_link	= aufs_put_link
};

struct inode_operations aufs_dir_iop = {
	.create		= aufs_create,
	.lookup		= aufs_lookup,
	.link		= aufs_link,
	.unlink		= aufs_unlink,
	.symlink	= aufs_symlink,
	.mkdir		= aufs_mkdir,
	.rmdir		= aufs_rmdir,
	.mknod		= aufs_mknod,
	.rename		= aufs_rename,

	.permission	= aufs_permission,
	.setattr	= aufs_setattr,

#if 0 // xattr
	.setxattr	= aufs_setxattr,
	.getxattr	= aufs_getxattr,
	.listxattr	= aufs_listxattr,
	.removexattr	= aufs_removexattr
#endif
};

struct inode_operations aufs_iop = {
	.permission	= aufs_permission,
	.setattr	= aufs_setattr,

#if 0 // xattr
	.setxattr	= aufs_setxattr,
	.getxattr	= aufs_getxattr,
	.listxattr	= aufs_listxattr,
	.removexattr	= aufs_removexattr
#endif
};
