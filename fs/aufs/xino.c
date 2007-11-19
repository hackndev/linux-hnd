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

/* $Id: xino.c,v 1.29 2007/05/27 23:03:46 sfjro Exp $ */

//#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <asm/uaccess.h>
#include "aufs.h"

static readf_t find_readf(struct file *h_file)
{
	const struct file_operations *fop = h_file->f_op;

	if (fop) {
		if (fop->read)
			return fop->read;
		if (fop->aio_read)
			return do_sync_read;
	}
	return ERR_PTR(-ENOSYS);
}

static writef_t find_writef(struct file *h_file)
{
	const struct file_operations *fop = h_file->f_op;

	if (fop) {
		if (fop->write)
			return fop->write;
		if (fop->aio_write)
			return do_sync_write;
	}
	return ERR_PTR(-ENOSYS);
}

/* ---------------------------------------------------------------------- */

static ssize_t xino_fread(readf_t func, struct file *file, void *buf,
			  size_t size, loff_t *pos)
{
	ssize_t err;
	mm_segment_t oldfs;

	LKTRTrace("%.*s, sz %lu, *pos %Ld\n",
		  DLNPair(file->f_dentry), (unsigned long)size, *pos);

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	do {
		err = func(file, (char __user*)buf, size, pos);
	} while (err == -EAGAIN || err == -EINTR);
	set_fs(oldfs);

#if 0
	if (err > 0)
		fsnotify_access(file->f_dentry);
#endif

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static ssize_t do_xino_fwrite(writef_t func, struct file *file, void *buf,
			      size_t size, loff_t *pos)
{
	ssize_t err;
	mm_segment_t oldfs;

	lockdep_off();
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	do {
		err = func(file, (const char __user*)buf, size, pos);
	} while (err == -EAGAIN || err == -EINTR);
	set_fs(oldfs);
	lockdep_on();

#if 0
	if (err > 0)
		fsnotify_modify(file->f_dentry);
#endif

	TraceErr(err);
	return err;
}

struct do_xino_fwrite_args {
	ssize_t *errp;
	writef_t func;
	struct file *file;
	void *buf;
	size_t size;
	loff_t *pos;
};

static void call_do_xino_fwrite(void *args)
{
	struct do_xino_fwrite_args *a = args;
	*a->errp = do_xino_fwrite(a->func, a->file, a->buf, a->size, a->pos);
}

static ssize_t xino_fwrite(writef_t func, struct file *file, void *buf,
			   size_t size, loff_t *pos)
{
	ssize_t err;

	LKTRTrace("%.*s, sz %lu, *pos %Ld\n",
		  DLNPair(file->f_dentry), (unsigned long)size, *pos);

	// signal block and no wkq?
	/*
	 * it breaks RLIMIT_FSIZE and normal user's limit,
	 * users should care about quota and real 'filesystem full.'
	 */
	if (!is_au_wkq(current)) {
		int wkq_err;
		struct do_xino_fwrite_args args = {
			.errp	= &err,
			.func	= func,
			.file	= file,
			.buf	= buf,
			.size	= size,
			.pos	= pos
		};
		wkq_err = au_wkq_wait(call_do_xino_fwrite, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	} else
		err = do_xino_fwrite(func, file, buf, size, pos);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * write @ino to the xinofile for the specified branch{@sb, @bindex}
 * at the position of @_ino.
 * when @ino is zero, it is written to the xinofile and means no entry.
 */
int xino_write(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	       struct xino *xino)
{
	struct aufs_branch *br;
	loff_t pos;
	ssize_t sz;

	LKTRTrace("b%d, hi%lu, i%lu\n", bindex, h_ino, xino->ino);
	//AuDebugOn(!xino->ino /* || !xino->h_gen */);
	//WARN_ON(bindex == 0 && h_ino == 31);

	if (unlikely(!au_flag_test(sb, AuFlag_XINO)))
		return 0;

	br = stobr(sb, bindex);
	AuDebugOn(!br || !br->br_xino);
	pos = h_ino * sizeof(*xino);
	sz = xino_fwrite(br->br_xino_write, br->br_xino, xino, sizeof(*xino),
			 &pos);
	//if (LktrCond) sz = 1;
	if (sz == sizeof(*xino))
		return 0; /* success */

	IOErr("write failed (%ld)\n", (long)sz);
	return -EIO;
}

int xino_write0(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino)
{
	struct xino xino = {
		.ino	= 0
	};
	return xino_write(sb, bindex, h_ino, &xino);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
// why is not atomic_long_inc_return defined?
static DEFINE_SPINLOCK(alir_lock);
static long atomic_long_inc_return(atomic_long_t *a)
{
	long l;

	spin_lock(&alir_lock);
	atomic_long_inc(a);
	l = atomic_long_read(a);
	spin_unlock(&alir_lock);
	return l;
}
#endif

ino_t xino_new_ino(struct super_block *sb)
{
	ino_t ino;

	TraceEnter();
	ino = atomic_long_inc_return(&stosi(sb)->si_xino);
	BUILD_BUG_ON(AUFS_FIRST_INO < AUFS_ROOT_INO);
	if (ino >= AUFS_ROOT_INO)
		return ino;
	else {
		atomic_long_dec(&stosi(sb)->si_xino);
		IOErr1("inode number overflow\n");
		return 0;
	}
}

/*
 * read @ino from xinofile for the specified branch{@sb, @bindex}
 * at the position of @h_ino.
 * if @ino does not exist and @do_new is true, get new one.
 */
int xino_read(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	      struct xino *xino)
{
	int err;
	struct aufs_branch *br;
	struct file *file;
	loff_t pos;
	ssize_t sz;

	LKTRTrace("b%d, hi%lu\n", bindex, h_ino);

	err = 0;
	xino->ino = 0;
	if (unlikely(!au_flag_test(sb, AuFlag_XINO)))
		return 0; /* no ino */

	br = stobr(sb, bindex);
	file = br->br_xino;
	AuDebugOn(!file);
	pos = h_ino * sizeof(*xino);
	if (i_size_read(file->f_dentry->d_inode) < pos + sizeof(*xino))
		return 0; /* no ino */

	sz = xino_fread(br->br_xino_read, file, xino, sizeof(*xino), &pos);
	if (sz == sizeof(*xino))
		return 0; /* success */

	err = sz;
	if (unlikely(sz >= 0)) {
		err = -EIO;
		IOErr("xino read error (%ld)\n", (long)sz);
	}

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct file *xino_create(struct super_block *sb, char *fname, int silent,
			 struct dentry *parent)
{
	struct file *file;
	int err;
	struct dentry *hidden_parent;
	struct inode *hidden_dir;
	//const int udba = au_flag_test(sb, AuFlag_UDBA_INOTIFY);

	LKTRTrace("%s\n", fname);
	//AuDebugOn(!au_flag_test(sb, AuFlag_XINO));

	// LSM may detect it
	// use sio?
	file = vfsub_filp_open(fname, O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE,
			       S_IRUGO | S_IWUGO);
	//file = ERR_PTR(-1);
	if (IS_ERR(file)) {
		if (!silent)
			Err("open %s(%ld)\n", fname, PTR_ERR(file));
		return file;
	}
#if 0
	if (unlikely(udba && parent))
		au_direval_dec(parent);
#endif

	/* keep file count */
	hidden_parent = dget_parent(file->f_dentry);
	hidden_dir = hidden_parent->d_inode;
	hi_lock_parent(hidden_dir);
	err = vfsub_unlink(hidden_dir, file->f_dentry, /*dlgt*/0);
#if 0
	if (unlikely(!err && udba && parent))
		au_direval_dec(parent);
#endif
	i_unlock(hidden_dir);
	dput(hidden_parent);
	if (unlikely(err)) {
		if (!silent)
			Err("unlink %s(%d)\n", fname, err);
		goto out;
	}
	if (sb != file->f_dentry->d_sb)
		return file; /* success */

	if (!silent)
		Err("%s must be outside\n", fname);
	err = -EINVAL;

 out:
	fput(file);
	file = ERR_PTR(err);
	return file;
}

/*
 * find another branch who is on the same filesystem of the specified
 * branch{@btgt}. search until @bend.
 */
static int is_sb_shared(struct super_block *sb, aufs_bindex_t btgt,
			aufs_bindex_t bend)
{
	aufs_bindex_t bindex;
	struct super_block *tgt_sb = sbr_sb(sb, btgt);

	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(btgt != bindex && tgt_sb == sbr_sb(sb, bindex)))
			return bindex;
	return -1;
}

/*
 * create a new xinofile at the same place/path as @base_file.
 */
static struct file *xino_create2(struct file *base_file)
{
	struct file *file;
	int err;
	struct dentry *base, *dentry, *parent;
	struct inode *dir;
	struct qstr *name;
	struct lkup_args lkup = {
		.nfsmnt	= NULL,
		.dlgt	= 0
	};

	base = base_file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(base));
	parent = dget_parent(base);
	dir = parent->d_inode;
	IMustLock(dir);

	file = ERR_PTR(-EINVAL);
	if (unlikely(au_is_nfs(parent->d_sb)))
		goto out;

	// do not superio, nor NFS.
	name = &base->d_name;
	dentry = lkup_one(name->name, parent, name->len, &lkup);
	//if (LktrCond) {dput(dentry); dentry = ERR_PTR(-1);}
	if (IS_ERR(dentry)) {
		file = (void*)dentry;
		Err("%.*s lookup err %ld\n", LNPair(name), PTR_ERR(dentry));
		goto out;
	}
	err = vfsub_create(dir, dentry, S_IRUGO | S_IWUGO, NULL, /*dlgt*/0);
	//if (LktrCond) {vfs_unlink(dir, dentry); err = -1;}
	if (unlikely(err)) {
		file = ERR_PTR(err);
		Err("%.*s create err %d\n", LNPair(name), err);
		goto out_dput;
	}
	file = dentry_open(dget(dentry), mntget(base_file->f_vfsmnt),
			   O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE);
	//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
	if (IS_ERR(file)) {
		Err("%.*s open err %ld\n", LNPair(name), PTR_ERR(file));
		goto out_dput;
	}
	err = vfsub_unlink(dir, dentry, /*dlgt*/0);
	//if (LktrCond) err = -1;
	if (!err)
		goto out_dput; /* success */

	Err("%.*s unlink err %d\n", LNPair(name), err);
	fput(file);
	file = ERR_PTR(err);

 out_dput:
	dput(dentry);
 out:
	dput(parent);
	TraceErrPtr(file);
	return file;
}

/*
 * initialize the xinofile for the specified branch{@sb, @bindex}
 * at the place/path where @base_file indicates.
 * test whether another branch is on the same filesystem or not,
 * if @do_test is true.
 */
int xino_init(struct super_block *sb, aufs_bindex_t bindex,
	      struct file *base_file, int do_test)
{
	int err;
	struct aufs_branch *br;
	aufs_bindex_t bshared, bend;
	struct file *file;
	struct inode *inode, *hidden_inode;
	struct xino xino;

	LKTRTrace("b%d, base_file %p, do_test %d\n",
		  bindex, base_file, do_test);
	SiMustWriteLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_XINO));
	br = stobr(sb, bindex);
	AuDebugOn(br->br_xino);

	file = NULL;
	bshared = -1;
	bend = sbend(sb);
	if (do_test)
		bshared = is_sb_shared(sb, bindex, bend);
	if (unlikely(bshared >= 0)) {
		struct aufs_branch *shared_br = stobr(sb, bshared);
		if (shared_br->br_xino) {
			file = shared_br->br_xino;
			get_file(file);
		}
	}

	if (!file) {
		struct dentry *parent = dget_parent(base_file->f_dentry);
		struct inode *dir = parent->d_inode;

		hi_lock_parent(dir);
		file = xino_create2(base_file);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		i_unlock(dir);
		dput(parent);
		err = PTR_ERR(file);
		if (IS_ERR(file))
			goto out;
	}
	br->br_xino_read = find_readf(file);
	err = PTR_ERR(br->br_xino_read);
	if (IS_ERR(br->br_xino_read))
		goto out_put;
	br->br_xino_write = find_writef(file);
	err = PTR_ERR(br->br_xino_write);
	if (IS_ERR(br->br_xino_write))
		goto out_put;
	br->br_xino = file;

	inode = sb->s_root->d_inode;
	hidden_inode = au_h_iptr_i(inode, bindex);
	xino.ino = inode->i_ino;
	//xino.h_gen = hidden_inode->i_generation;
	//WARN_ON(xino.h_gen == AuXino_INVALID_HGEN);
	err = xino_write(sb, bindex, hidden_inode->i_ino, &xino);
	//if (LktrCond) err = -1;
	if (!err)
		return 0; /* success */

	br->br_xino = NULL;

 out_put:
	fput(file);
 out:
	TraceErr(err);
	return err;
}

/*
 * set xino mount option.
 */
int xino_set(struct super_block *sb, struct opt_xino *xino, int remount)
{
	int err, sparse;
	aufs_bindex_t bindex, bend;
	struct aufs_branch *br;
	struct dentry *parent, *cur_parent;
	struct qstr *name;
	struct file *cur_xino;
	struct inode *dir;

	LKTRTrace("%s\n", xino->path);

	err = 0;
	name = &xino->file->f_dentry->d_name;
	parent = dget_parent(xino->file->f_dentry);
	dir = parent->d_inode;
	cur_parent = NULL;
	cur_xino = stobr(sb, 0)->br_xino;
	if (cur_xino)
		cur_parent = dget_parent(cur_xino->f_dentry);
	if (remount
	    && cur_xino
	    && cur_parent == parent
	    && name->len == cur_xino->f_dentry->d_name.len
	    && !memcmp(name->name, cur_xino->f_dentry->d_name.name, name->len))
		goto out;

	au_flag_set(sb, AuFlag_XINO);
	bend = sbend(sb);
	for (bindex = bend; bindex >= 0; bindex--) {
		br = stobr(sb, bindex);
		if (unlikely(br->br_xino && file_count(br->br_xino) > 1)) {
			fput(br->br_xino);
			br->br_xino = NULL;
		}
	}

	for (bindex = 0; bindex <= bend; bindex++) {
		struct file *file;
		struct inode *inode;

		br = stobr(sb, bindex);
		if (unlikely(!br->br_xino))
			continue;

		AuDebugOn(file_count(br->br_xino) != 1);
		hi_lock_parent(dir);
		file = xino_create2(xino->file);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		err = PTR_ERR(file);
		if (IS_ERR(file)) {
			i_unlock(dir);
			break;
		}
		inode = br->br_xino->f_dentry->d_inode;
		err = au_copy_file(file, br->br_xino, i_size_read(inode), sb,
				   &sparse);
		//if (LktrCond) err = -1;
		i_unlock(dir);
		if (unlikely(err)) {
			fput(file);
			break;
		}
		fput(br->br_xino);
		br->br_xino = file;
		br->br_xino_read = find_readf(file);
		AuDebugOn(IS_ERR(br->br_xino_read));
		br->br_xino_write = find_writef(file);
		AuDebugOn(IS_ERR(br->br_xino_write));
	}

	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(!stobr(sb, bindex)->br_xino)) {
			err = xino_init(sb, bindex, xino->file, /*do_test*/1);
			//if (LktrCond) {fput(stobr(sb, bindex)->br_xino);
			//stobr(sb, bindex)->br_xino = NULL; err = -1;}
			if (!err)
				continue;
			IOErr("creating xino for branch %d(%d), "
			      "forcing noxino\n", bindex, err);
			err = -EIO;
			break;
		}
 out:
	dput(cur_parent);
	dput(parent);
	if (!err)
		au_flag_set(sb, AuFlag_XINO);
	else
		au_flag_clr(sb, AuFlag_XINO);
	TraceErr(err);
	return err;
}

/*
 * clear xino mount option
 */
int xino_clr(struct super_block *sb)
{
	aufs_bindex_t bindex, bend;

	TraceEnter();
	SiMustWriteLock(sb);

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++) {
		struct aufs_branch *br;
		br = stobr(sb, bindex);
		if (br->br_xino) {
			fput(br->br_xino);
			br->br_xino = NULL;
		}
	}

	//todo: need to make iunique() to return the larger inode number

	au_flag_clr(sb, AuFlag_XINO);
	return 0;
}

/*
 * create a xinofile at the default place/path.
 */
struct file *xino_def(struct super_block *sb)
{
	struct file *file;
	aufs_bindex_t bend, bindex, bwr;
	char *page, *p;

	bend = sbend(sb);
	bwr = -1;
	for (bindex = 0; bindex <= bend; bindex++)
		if (br_writable(sbr_perm(sb, bindex))
		    && !au_is_nfs(au_h_dptr_i(sb->s_root, bindex)->d_sb)) {
			bwr = bindex;
			break;
		}

	if (bwr != -1) {
		// todo: rewrite with lkup_one()
		file = ERR_PTR(-ENOMEM);
		page = __getname();
		//if (LktrCond) {__putname(page); page = NULL;}
		if (unlikely(!page))
			goto out;
		p = d_path(au_h_dptr_i(sb->s_root, bwr), sbr_mnt(sb, bwr), page,
			   PATH_MAX - sizeof(AUFS_XINO_FNAME));
		//if (LktrCond) p = ERR_PTR(-1);
		file = (void*)p;
		if (p && !IS_ERR(p)) {
			strcat(p, "/" AUFS_XINO_FNAME);
			LKTRTrace("%s\n", p);
			file = xino_create(sb, p, /*silent*/0, sb->s_root);
			//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		}
		__putname(page);
	} else {
		file = xino_create(sb, AUFS_XINO_DEFPATH, /*silent*/0,
				   /*parent*/NULL);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
	}

 out:
	TraceErrPtr(file);
	return file;
}
