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

/* $Id: cpup.c,v 1.40 2007/06/04 02:17:34 sfjro Exp $ */

#include <asm/uaccess.h>
#include "aufs.h"

/* violent cpup_attr_*() functions don't care inode lock */
void au_cpup_attr_timesizes(struct inode *inode)
{
	struct inode *hidden_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	hidden_inode = au_h_iptr(inode);
	AuDebugOn(!hidden_inode);
	//IMustLock(!hidden_inode);

	inode->i_atime = hidden_inode->i_atime;
	inode->i_mtime = hidden_inode->i_mtime;
	inode->i_ctime = hidden_inode->i_ctime;
	spin_lock(&inode->i_lock);
	i_size_write(inode, i_size_read(hidden_inode));
	inode->i_blocks = hidden_inode->i_blocks;
	spin_unlock(&inode->i_lock);
}

void au_cpup_attr_nlink(struct inode *inode)
{
	struct inode *h_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	AuDebugOn(!inode->i_mode);

	h_inode = au_h_iptr(inode);
	inode->i_nlink = h_inode->i_nlink;

	/*
	 * fewer nlink makes find(1) noisy, but larger nlink doesn't.
	 * it may includes whplink directory.
	 */
	if (unlikely(S_ISDIR(h_inode->i_mode))) {
		aufs_bindex_t bindex, bend;
		bend = ibend(inode);
		for (bindex = ibstart(inode) + 1; bindex <= bend; bindex++) {
			h_inode = au_h_iptr_i(inode, bindex);
			if (h_inode)
				au_add_nlink(inode, h_inode);
		}
	}
}

void au_cpup_attr_changable(struct inode *inode)
{
	struct inode *hidden_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	hidden_inode = au_h_iptr(inode);
	AuDebugOn(!hidden_inode);

	inode->i_mode = hidden_inode->i_mode;
	inode->i_uid = hidden_inode->i_uid;
	inode->i_gid = hidden_inode->i_gid;
	au_cpup_attr_timesizes(inode);

	//??
	inode->i_flags = hidden_inode->i_flags;
}

void au_cpup_igen(struct inode *inode, struct inode *h_inode)
{
	inode->i_generation = h_inode->i_generation;
	itoii(inode)->ii_hsb1 = h_inode->i_sb;
}

void au_cpup_attr_all(struct inode *inode)
{
	struct inode *hidden_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	hidden_inode = au_h_iptr(inode);
	AuDebugOn(!hidden_inode);

	au_cpup_attr_changable(inode);
	if (inode->i_nlink > 0)
		au_cpup_attr_nlink(inode);

	switch (inode->i_mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		inode->i_rdev = hidden_inode->i_rdev;
	}
	inode->i_blkbits = hidden_inode->i_blkbits;
	au_cpup_attr_blksize(inode, hidden_inode);
	au_cpup_igen(inode, hidden_inode);
}

/* ---------------------------------------------------------------------- */

/* Note: dt_dentry and dt_hidden_dentry are not dget/dput-ed */

/* keep the timestamps of the parent dir when cpup */
void dtime_store(struct dtime *dt, struct dentry *dentry,
		 struct dentry *hidden_dentry)
{
	struct inode *inode;

	TraceEnter();
	AuDebugOn(!dentry || !hidden_dentry || !hidden_dentry->d_inode);

	dt->dt_dentry = dentry;
	dt->dt_h_dentry = hidden_dentry;
	inode = hidden_dentry->d_inode;
	dt->dt_atime = inode->i_atime;
	dt->dt_mtime = inode->i_mtime;
	//smp_mb();
}

// todo: remove extra parameter
void dtime_revert(struct dtime *dt, int h_parent_is_locked)
{
	struct iattr attr;
	int err;
	struct dentry *dentry;

	LKTRTrace("h_parent locked %d\n", h_parent_is_locked);

	attr.ia_atime = dt->dt_atime;
	attr.ia_mtime = dt->dt_mtime;
	attr.ia_valid = ATTR_FORCE | ATTR_MTIME | ATTR_MTIME_SET
		| ATTR_ATIME | ATTR_ATIME_SET;
	//smp_mb();
	dentry = NULL;
	if (!h_parent_is_locked /* && !IS_ROOT(dt->dt_dentry) */)
		dentry = dt->dt_dentry;
	err = vfsub_notify_change(dt->dt_h_dentry, &attr,
				  need_dlgt(dt->dt_dentry->d_sb));
	if (unlikely(err))
		Warn("restoring timestamps failed(%d). ignored\n", err);
}

/* ---------------------------------------------------------------------- */

static int cpup_iattr(struct dentry *hidden_dst, struct dentry *hidden_src,
		      int dlgt)
{
	int err;
	struct iattr ia;
	struct inode *hidden_isrc, *hidden_idst;

	LKTRTrace("%.*s\n", DLNPair(hidden_dst));
	hidden_idst = hidden_dst->d_inode;
	//IMustLock(hidden_idst);
	hidden_isrc = hidden_src->d_inode;
	//IMustLock(hidden_isrc);

	ia.ia_valid = ATTR_FORCE | ATTR_MODE | ATTR_UID | ATTR_GID
		| ATTR_ATIME | ATTR_MTIME
		| ATTR_ATIME_SET | ATTR_MTIME_SET;
	ia.ia_mode = hidden_isrc->i_mode;
	ia.ia_uid = hidden_isrc->i_uid;
	ia.ia_gid = hidden_isrc->i_gid;
	ia.ia_atime = hidden_isrc->i_atime;
	ia.ia_mtime = hidden_isrc->i_mtime;
	err = vfsub_notify_change(hidden_dst, &ia, dlgt);
	//if (LktrCond) err = -1;
	if (!err)
		hidden_idst->i_flags = hidden_isrc->i_flags; //??

	TraceErr(err);
	return err;
}

/*
 * to support a sparse file which is opened with O_APPEND,
 * we need to close the file.
 */
static int cpup_regular(struct dentry *dentry, aufs_bindex_t bdst,
			aufs_bindex_t bsrc, loff_t len)
{
	int err, i, sparse;
	struct super_block *sb;
	struct inode *hidden_inode;
	enum {SRC, DST};
	struct {
		aufs_bindex_t bindex;
		unsigned int flags;
		struct dentry *dentry;
		struct file *file;
		void *label, *label_file;
	} *h, hidden[] = {
		{
			.bindex = bsrc,
			.flags = O_RDONLY | O_NOATIME | O_LARGEFILE,
			.file = NULL,
			.label = &&out,
			.label_file = &&out_src_file
		},
		{
			.bindex = bdst,
			.flags = O_WRONLY | O_NOATIME | O_LARGEFILE,
			.file = NULL,
			.label = &&out_src_file,
			.label_file = &&out_dst_file
		}
	};

	LKTRTrace("dentry %.*s, bdst %d, bsrc %d, len %lld\n",
		  DLNPair(dentry), bdst, bsrc, len);
	AuDebugOn(bsrc <= bdst);
	AuDebugOn(!len);
	sb = dentry->d_sb;
	AuDebugOn(test_ro(sb, bdst, dentry->d_inode));
	// bsrc branch can be ro/rw.

	h = hidden;
	for (i = 0; i < 2; i++, h++) {
		h->dentry = au_h_dptr_i(dentry, h->bindex);
		AuDebugOn(!h->dentry);
		hidden_inode = h->dentry->d_inode;
		AuDebugOn(!hidden_inode || !S_ISREG(hidden_inode->i_mode));
		h->file = hidden_open(dentry, h->bindex, h->flags);
		//if (LktrCond)
		//{fput(h->file); sbr_put(sb, h->bindex); h->file = ERR_PTR(-1);}
		err = PTR_ERR(h->file);
		if (IS_ERR(h->file))
			goto *h->label;
		err = -EINVAL;
		if (unlikely(!h->file->f_op))
			goto *h->label_file;
	}

	/* stop updating while we copyup */
	IMustLock(hidden[SRC].dentry->d_inode);
	sparse = 0;
	err = au_copy_file(hidden[DST].file, hidden[SRC].file, len, sb,
			   &sparse);

	/* sparse file: update i_blocks next time */
	if (unlikely(!err && sparse))
		d_drop(dentry);

 out_dst_file:
	fput(hidden[DST].file);
	sbr_put(sb, hidden[DST].bindex);
 out_src_file:
	fput(hidden[SRC].file);
	sbr_put(sb, hidden[SRC].bindex);
 out:
	TraceErr(err);
	return err;
}

// unnecessary?
unsigned int au_flags_cpup(unsigned int init, struct dentry *parent)
{
	if (unlikely(parent && IS_ROOT(parent)))
		init |= CPUP_LOCKED_GHDIR;
	return init;
}

/* return with hidden dst inode is locked */
static int cpup_entry(struct dentry *dentry, aufs_bindex_t bdst,
		      aufs_bindex_t bsrc, loff_t len, unsigned int flags,
		      int dlgt)
{
	int err, isdir, symlen;
	struct dentry *hidden_src, *hidden_dst, *hidden_parent, *parent;
	struct inode *hidden_inode, *hidden_dir, *dir;
	struct dtime dt;
	umode_t mode;
	char *sym;
	mm_segment_t old_fs;
	const int do_dt = flags & CPUP_DTIME;
	struct super_block *sb;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, flags 0x%x\n",
		  DLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  flags);
	sb = dentry->d_sb;
	AuDebugOn(bdst >= bsrc || test_ro(sb, bdst, NULL));
	// bsrc branch can be ro/rw.

	hidden_src = au_h_dptr_i(dentry, bsrc);
	AuDebugOn(!hidden_src);
	hidden_inode = hidden_src->d_inode;
	AuDebugOn(!hidden_inode);

	/* stop refrencing while we are creating */
	parent = dget_parent(dentry);
	dir = parent->d_inode;
	hidden_dst = au_h_dptr_i(dentry, bdst);
	AuDebugOn(hidden_dst && hidden_dst->d_inode);
	hidden_parent = dget_parent(hidden_dst);
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	if (do_dt)
		dtime_store(&dt, parent, hidden_parent);

	isdir = 0;
	mode = hidden_inode->i_mode;
	switch (mode & S_IFMT) {
	case S_IFREG:
		/* stop updating while we are referencing */
		IMustLock(hidden_inode);
		err = vfsub_create(hidden_dir, hidden_dst, mode | S_IWUSR, NULL,
				   dlgt);
		//if (LktrCond) {vfs_unlink(hidden_dir, hidden_dst); err = -1;}
		if (!err) {
			loff_t l = i_size_read(hidden_inode);
			if (len == -1 || l < len)
				len = l;
			if (len) {
				err = cpup_regular(dentry, bdst, bsrc, len);
				//if (LktrCond) err = -1;
			}
			if (unlikely(err)) {
				int rerr;
				rerr = vfsub_unlink(hidden_dir, hidden_dst,
						    dlgt);
				if (rerr) {
					IOErr("failed unlinking cpup-ed %.*s"
					      "(%d, %d)\n",
					      DLNPair(hidden_dst), err, rerr);
					err = -EIO;
				}
			}
		}
		break;
	case S_IFDIR:
		isdir = 1;
		err = vfsub_mkdir(hidden_dir, hidden_dst, mode, dlgt);
		//if (LktrCond) {vfs_rmdir(hidden_dir, hidden_dst); err = -1;}
		if (!err) {
			/* setattr case: dir is not locked */
			if (0 && ibstart(dir) == bdst)
				au_cpup_attr_nlink(dir);
			au_cpup_attr_nlink(dentry->d_inode);
		}
		break;
	case S_IFLNK:
		err = -ENOMEM;
		sym = __getname();
		//if (LktrCond) {__putname(sym); sym = NULL;}
		if (unlikely(!sym))
			break;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		err = symlen = hidden_inode->i_op->readlink
			(hidden_src, (char __user*)sym, PATH_MAX);
		//if (LktrCond) err = symlen = -1;
		set_fs(old_fs);
		if (symlen > 0) {
			sym[symlen] = 0;
			err = vfsub_symlink(hidden_dir, hidden_dst, sym, mode,
					    dlgt);
			//if (LktrCond)
			//{vfs_unlink(hidden_dir, hidden_dst); err = -1;}
		}
		__putname(sym);
		break;
	case S_IFCHR:
	case S_IFBLK:
		AuDebugOn(!capable(CAP_MKNOD));
		/*FALLTHROUGH*/
	case S_IFIFO:
	case S_IFSOCK:
		err = vfsub_mknod(hidden_dir, hidden_dst, mode,
				  hidden_inode->i_rdev, dlgt);
		//if (LktrCond) {vfs_unlink(hidden_dir, hidden_dst); err = -1;}
		break;
	default:
		IOErr("Unknown inode type 0%o\n", mode);
		err = -EIO;
	}

	if (do_dt)
		dtime_revert(&dt, flags & CPUP_LOCKED_GHDIR);
	dput(parent);
	dput(hidden_parent);
	TraceErr(err);
	return err;
}

/*
 * copyup the @dentry from @bsrc to @bdst.
 * the caller must set the both of hidden dentries.
 * @len is for trucating when it is -1 copyup the entire file.
 */
int cpup_single(struct dentry *dentry, aufs_bindex_t bdst, aufs_bindex_t bsrc,
		loff_t len, unsigned int flags)
{
	int err, rerr, isdir, dlgt;
	struct dentry *hidden_src, *hidden_dst, *parent, *h_parent;
	struct inode *dst_inode, *hidden_dir, *inode, *src_inode;
	struct super_block *sb;
	aufs_bindex_t old_ibstart;
	struct dtime dt;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, flags 0x%x\n",
		  DLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  flags);
	sb = dentry->d_sb;
	AuDebugOn(bsrc <= bdst);
	hidden_dst = au_h_dptr_i(dentry, bdst);
	AuDebugOn(!hidden_dst || hidden_dst->d_inode);
	h_parent = dget_parent(hidden_dst);
	hidden_dir = h_parent->d_inode;
	IMustLock(hidden_dir);
	hidden_src = au_h_dptr_i(dentry, bsrc);
	AuDebugOn(!hidden_src || !hidden_src->d_inode);
	inode = dentry->d_inode;
	IiMustWriteLock(inode);

	dlgt = need_dlgt(sb);
	dst_inode = au_h_iptr_i(inode, bdst);
	if (unlikely(dst_inode)) {
		if (unlikely(!au_flag_test(sb, AuFlag_PLINK))) {
			err = -EIO;
			IOErr("i%lu exists on a upper branch "
			      "but plink is disabled\n", inode->i_ino);
			goto out;
		}

		if (dst_inode->i_nlink) {
			hidden_src = lkup_plink(sb, bdst, inode);
			err = PTR_ERR(hidden_src);
			if (IS_ERR(hidden_src))
				goto out;
			AuDebugOn(!hidden_src->d_inode);
			// vfs_link() does lock the inode
			err = vfsub_link(hidden_src, hidden_dir, hidden_dst,
					 dlgt);
			dput(hidden_src);
			goto out;
		} else
			/* udba work */
			au_update_brange(inode, 1);
	}

	old_ibstart = ibstart(inode);
	err = cpup_entry(dentry, bdst, bsrc, len, flags, dlgt);
	if (unlikely(err))
		goto out;
	dst_inode = hidden_dst->d_inode;
	hi_lock_child2(dst_inode);

	//todo: test dlgt
	err = cpup_iattr(hidden_dst, hidden_src, dlgt);
	//if (LktrCond) err = -1;
#if 0 // xattr
	if (0 && !err)
		err = cpup_xattrs(hidden_src, hidden_dst);
#endif
	isdir = S_ISDIR(dst_inode->i_mode);
	if (!err) {
		if (bdst < old_ibstart)
			set_ibstart(inode, bdst);
		set_h_iptr(inode, bdst, igrab(dst_inode),
			   au_hi_flags(inode, isdir));
		i_unlock(dst_inode);
		src_inode = hidden_src->d_inode;
		if (!isdir) {
			if (src_inode->i_nlink > 1
			    && au_flag_test(sb, AuFlag_PLINK))
				append_plink(sb, inode, hidden_dst, bdst);
			else {
				/* braces are added to stop a warning */
				;//xino_write0(sb, bsrc, src_inode->i_ino);
				/* ignore this error */
			}
		}
		goto out; /* success */
	}

	/* revert */
	i_unlock(dst_inode);
	parent = dget_parent(dentry);
	dtime_store(&dt, parent, h_parent);
	dput(parent);
	if (!isdir)
		rerr = vfsub_unlink(hidden_dir, hidden_dst, dlgt);
	else
		rerr = vfsub_rmdir(hidden_dir, hidden_dst, dlgt);
	//rerr = -1;
	dtime_revert(&dt, flags & CPUP_LOCKED_GHDIR);
	if (rerr) {
		IOErr("failed removing broken entry(%d, %d)\n", err, rerr);
		err = -EIO;
	}

 out:
	dput(h_parent);
	TraceErr(err);
	return err;
}

struct cpup_single_args {
	int *errp;
	struct dentry *dentry;
	aufs_bindex_t bdst, bsrc;
	loff_t len;
	unsigned int flags;
};

static void call_cpup_single(void *args)
{
	struct cpup_single_args *a = args;
	*a->errp = cpup_single(a->dentry, a->bdst, a->bsrc, a->len, a->flags);
}

int sio_cpup_single(struct dentry *dentry, aufs_bindex_t bdst,
		    aufs_bindex_t bsrc, loff_t len, unsigned int flags)
{
	int err, wkq_err;
	struct dentry *hidden_dentry;
	umode_t mode;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, flags 0x%x\n",
		  DLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  flags);

	hidden_dentry = au_h_dptr_i(dentry, bsrc);
	mode = hidden_dentry->d_inode->i_mode & S_IFMT;
	if ((mode != S_IFCHR && mode != S_IFBLK)
	    || capable(CAP_MKNOD))
		err = cpup_single(dentry, bdst, bsrc, len, flags);
	else {
		struct cpup_single_args args = {
			.errp	= &err,
			.dentry	= dentry,
			.bdst	= bdst,
			.bsrc	= bsrc,
			.len	= len,
			.flags	= flags
		};
		wkq_err = au_wkq_wait(call_cpup_single, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	TraceErr(err);
	return err;
}

/*
 * copyup the @dentry from the first active hidden branch to @bdst,
 * using cpup_single().
 */
int cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		unsigned int flags)
{
	int err;
	struct inode *inode;
	aufs_bindex_t bsrc, bend;

	LKTRTrace("%.*s, bdst %d, len %Ld, flags 0x%x\n",
		  DLNPair(dentry), bdst, len, flags);
	inode = dentry->d_inode;
	AuDebugOn(!S_ISDIR(inode->i_mode) && dbstart(dentry) < bdst);

	bend = dbend(dentry);
	for (bsrc = bdst + 1; bsrc <= bend; bsrc++)
		if (au_h_dptr_i(dentry, bsrc))
			break;
	AuDebugOn(!au_h_dptr_i(dentry, bsrc));

	err = lkup_neg(dentry, bdst);
	//err = -1;
	if (!err) {
		err = cpup_single(dentry, bdst, bsrc, len, flags);
		if (!err)
			return 0; /* success */

		/* revert */
		set_h_dptr(dentry, bdst, NULL);
		set_dbstart(dentry, bsrc);
	}

	TraceErr(err);
	return err;
}

struct cpup_simple_args {
	int *errp;
	struct dentry *dentry;
	aufs_bindex_t bdst;
	loff_t len;
	unsigned int flags;
};

static void call_cpup_simple(void *args)
{
	struct cpup_simple_args *a = args;
	*a->errp = cpup_simple(a->dentry, a->bdst, a->len, a->flags);
}

int sio_cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		    unsigned int flags)
{
	int err, do_sio, dlgt, wkq_err;
	struct dentry *parent;
	struct inode *hidden_dir, *dir;

	LKTRTrace("%.*s, b%d, len %Ld, flags 0x%x\n",
		  DLNPair(dentry), bdst, len, flags);

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	hidden_dir = au_h_iptr_i(dir, bdst);
	dlgt = need_dlgt(dir->i_sb);
	do_sio = au_test_perm(hidden_dir, MAY_EXEC | MAY_WRITE, dlgt);
	if (!do_sio) {
		umode_t mode = dentry->d_inode->i_mode & S_IFMT;
		do_sio = ((mode == S_IFCHR || mode == S_IFBLK)
			  && !capable(CAP_MKNOD));
	}
	if (!do_sio)
		err = cpup_simple(dentry, bdst, len, flags);
	else {
		struct cpup_simple_args args = {
			.errp	= &err,
			.dentry	= dentry,
			.bdst	= bdst,
			.len	= len,
			.flags	= flags
		};
		wkq_err = au_wkq_wait(call_cpup_simple, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	dput(parent);
	TraceErr(err);
	return err;
}

//todo: dcsub
/* cf. revalidate function in file.c */
int cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst, struct dentry *locked)
{
	int err;
	struct super_block *sb;
	struct dentry *d, *parent, *hidden_parent;
	unsigned int udba;

	LKTRTrace("%.*s, b%d, parent i%lu, locked %p\n",
		  DLNPair(dentry), bdst, parent_ino(dentry), locked);
	sb = dentry->d_sb;
	AuDebugOn(test_ro(sb, bdst, NULL));
	parent = dentry->d_parent;
	IiMustWriteLock(parent->d_inode);
	if (unlikely(IS_ROOT(parent)))
		return 0;
	if (locked) {
		DiMustAnyLock(locked);
		IiMustAnyLock(locked->d_inode);
	}

	/* slow loop, keep it simple and stupid */
	err = 0;
	udba = au_flag_test(sb, AuFlag_UDBA_INOTIFY);
	while (1) {
		parent = dentry->d_parent; // dget_parent()
		hidden_parent = au_h_dptr_i(parent, bdst);
		if (hidden_parent)
			return 0; /* success */

		/* find top dir which is needed to cpup */
		do {
			d = parent;
			parent = d->d_parent; // dget_parent()
			if (parent != locked)
				di_read_lock_parent3(parent, !AUFS_I_RLOCK);
			hidden_parent = au_h_dptr_i(parent, bdst);
			if (parent != locked)
				di_read_unlock(parent, !AUFS_I_RLOCK);
		} while (!hidden_parent);

		if (d != dentry->d_parent)
			di_write_lock_child3(d);

		/* somebody else might create while we were sleeping */
		if (!au_h_dptr_i(d, bdst) || !au_h_dptr_i(d, bdst)->d_inode) {
			struct inode *h_dir = hidden_parent->d_inode,
				*dir = parent->d_inode,
				*h_gdir, *gdir;

			if (au_h_dptr_i(d, bdst))
				au_update_dbstart(d);
			//AuDebugOn(dbstart(d) <= bdst);
			if (parent != locked)
				di_read_lock_parent3(parent, AUFS_I_RLOCK);
			h_gdir = gdir = NULL;
			if (unlikely(udba && !IS_ROOT(parent))) {
				gdir = parent->d_parent->d_inode;
				h_gdir = hidden_parent->d_parent->d_inode;
				hgdir_lock(h_gdir, gdir, bdst);
			}
			hdir_lock(h_dir, dir, bdst);
			err = sio_cpup_simple(d, bdst, -1,
					      au_flags_cpup(CPUP_DTIME,
							    parent));
			//if (LktrCond) err = -1;
			hdir_unlock(h_dir, dir, bdst);
			if (unlikely(gdir))
				hdir_unlock(h_gdir, gdir, bdst);
			if (parent != locked)
				di_read_unlock(parent, AUFS_I_RLOCK);
		}

		if (d != dentry->d_parent)
			di_write_unlock(d);
		if (unlikely(err))
			break;
	}

// out:
	TraceErr(err);
	return err;
}

int test_and_cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst,
		       struct dentry *locked)
{
	int err;
	struct dentry *parent;
	struct inode *dir;

	parent = dentry->d_parent;
	dir = parent->d_inode;
	LKTRTrace("%.*s, b%d, parent i%lu, locked %p\n",
		  DLNPair(dentry), bdst, dir->i_ino, locked);
	DiMustReadLock(parent);
	IiMustReadLock(dir);

	if (au_h_iptr_i(dir, bdst))
		return 0;

	err = 0;
	di_read_unlock(parent, AUFS_I_RLOCK);
	di_write_lock_parent(parent);
	if (au_h_iptr_i(dir, bdst))
		goto out;

	err = cpup_dirs(dentry, bdst, locked);

 out:
	di_downgrade_lock(parent, AUFS_I_RLOCK);
	TraceErr(err);
	return err;
}
