/*
 * Copyright (C) 2007 Junjiro Okajima
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

/* $Id: vfsub.c,v 1.8 2007/06/04 02:17:35 sfjro Exp $ */
// I'm going to slightly mad

#include "aufs.h"

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DLGT
struct permission_args {
	int *errp;
	struct inode *inode;
	int mask;
	struct nameidata *nd;
};

static void call_permission(void *args)
{
	struct permission_args *a = args;
	*a->errp = do_vfsub_permission(a->inode, a->mask, a->nd);
}

int vfsub_permission(struct inode *inode, int mask, struct nameidata *nd,
		     int dlgt)
{
	if (!dlgt)
		return do_vfsub_permission(inode, mask, nd);
	else {
		int err, wkq_err;
		struct permission_args args = {
			.errp	= &err,
			.inode	= inode,
			.mask	= mask,
			.nd	= nd
		};
		wkq_err = au_wkq_wait(call_permission, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

/* ---------------------------------------------------------------------- */

struct create_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	struct nameidata *nd;
};

static void call_create(void *args)
{
	struct create_args *a = args;
	*a->errp = do_vfsub_create(a->dir, a->dentry, a->mode, a->nd);
}

int vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		 struct nameidata *nd, int dlgt)
{
	if (!dlgt)
		return do_vfsub_create(dir, dentry, mode, nd);
	else {
		int err, wkq_err;
		struct create_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode,
			.nd	= nd
		};
		wkq_err = au_wkq_wait(call_create, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct symlink_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	const char *symname;
	int mode;
};

static void call_symlink(void *args)
{
	struct symlink_args *a = args;
	*a->errp = do_vfsub_symlink(a->dir, a->dentry, a->symname, a->mode);
}

int vfsub_symlink(struct inode *dir, struct dentry *dentry, const char *symname,
		  int mode, int dlgt)
{
	if (!dlgt)
		return do_vfsub_symlink(dir, dentry, symname, mode);
	else {
		int err, wkq_err;
		struct symlink_args args = {
			.errp		= &err,
			.dir		= dir,
			.dentry		= dentry,
			.symname	= symname,
			.mode		= mode
		};
		wkq_err = au_wkq_wait(call_symlink, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct mknod_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	dev_t dev;
};

static void call_mknod(void *args)
{
	struct mknod_args *a = args;
	*a->errp = do_vfsub_mknod(a->dir, a->dentry, a->mode, a->dev);
}

int vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev,
		int dlgt)
{
	if (!dlgt)
		return do_vfsub_mknod(dir, dentry, mode, dev);
	else {
		int err, wkq_err;
		struct mknod_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode,
			.dev	= dev
		};
		wkq_err = au_wkq_wait(call_mknod, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct mkdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
};

static void call_mkdir(void *args)
{
	struct mkdir_args *a = args;
	*a->errp = do_vfsub_mkdir(a->dir, a->dentry, a->mode);
}

int vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode, int dlgt)
{
	if (!dlgt)
		return do_vfsub_mkdir(dir, dentry, mode);
	else {
		int err, wkq_err;
		struct mkdir_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode
		};
		wkq_err = au_wkq_wait(call_mkdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

/* ---------------------------------------------------------------------- */

struct link_args {
	int *errp;
	struct inode *dir;
	struct dentry *src_dentry, *dentry;
};

static void call_link(void *args)
{
	struct link_args *a = args;
	*a->errp = do_vfsub_link(a->src_dentry, a->dir, a->dentry);
}

int vfsub_link(struct dentry *src_dentry, struct inode *dir,
	       struct dentry *dentry, int dlgt)
{
	if (!dlgt)
		return do_vfsub_link(src_dentry, dir, dentry);
	else {
		int err, wkq_err;
		struct link_args args = {
			.errp		= &err,
			.src_dentry	= src_dentry,
			.dir		= dir,
			.dentry		= dentry
		};
		wkq_err = au_wkq_wait(call_link, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct rename_args {
	int *errp;
	struct inode *src_dir, *dir;
	struct dentry *src_dentry, *dentry;
};

static void call_rename(void *args)
{
	struct rename_args *a = args;
	*a->errp = do_vfsub_rename(a->src_dir, a->src_dentry, a->dir,
				   a->dentry);
}

int vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		 struct inode *dir, struct dentry *dentry, int dlgt)
{
	if (!dlgt)
		return do_vfsub_rename(src_dir, src_dentry, dir, dentry);
	else {
		int err, wkq_err;
		struct rename_args args = {
			.errp		= &err,
			.src_dir	= src_dir,
			.src_dentry	= src_dentry,
			.dir		= dir,
			.dentry		= dentry
		};
		wkq_err = au_wkq_wait(call_rename, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct rmdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
};

static void call_rmdir(void *args)
{
	struct rmdir_args *a = args;
	*a->errp = do_vfsub_rmdir(a->dir, a->dentry);
}

int vfsub_rmdir(struct inode *dir, struct dentry *dentry, int dlgt)
{
	if (!dlgt)
		return do_vfsub_rmdir(dir, dentry);
	else {
		int err, wkq_err;
		struct rmdir_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry
		};
		wkq_err = au_wkq_wait(call_rmdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

/* ---------------------------------------------------------------------- */

struct read_args {
	ssize_t *errp;
	struct file *file;
	union {
		void *kbuf;
		char __user *ubuf;
	};
	size_t count;
	loff_t *ppos;
};

static void call_read_k(void *args)
{
	struct read_args *a = args;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  DLNPair(a->file->f_dentry), (unsigned long)a->count,
		  *a->ppos);
	*a->errp = do_vfsub_read_k(a->file, a->kbuf, a->count, a->ppos);
}

ssize_t vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
		     loff_t *ppos, int dlgt)
{
	if (!dlgt)
		return do_vfsub_read_u(file, ubuf, count, ppos);
	else {
		int wkq_err;
		ssize_t err, read;
		struct read_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};

		if (unlikely(!count))
			return 0;

		/*
		 * workaround an application bug.
		 * generally, read(2) or write(2) may return the value shorter
		 * than requested. But many applications don't support it,
		 * for example bash.
		 */
		err = -ENOMEM;
		if (args.count > PAGE_SIZE)
			args.count = PAGE_SIZE;
		args.kbuf = kmalloc(args.count, GFP_KERNEL);
		if (unlikely(!args.kbuf))
			goto out;

		read = 0;
		do {
			wkq_err = au_wkq_wait(call_read_k, &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
			if (unlikely(err > 0
				     && copy_to_user(ubuf, args.kbuf, err))) {
				err = -EFAULT;
				goto out_free;
			} else if (!err)
				break;
			else if (unlikely(err < 0))
				goto out_free;
			count -= err;
			/* do not read too much because of file i/o pointer */
			if (unlikely(count < args.count))
				args.count = count;
			ubuf += err;
			read += err;
		} while (count);
		smp_mb();
		err = read;

	out_free:
		kfree(args.kbuf);
	out:
		return err;
	}
}

ssize_t vfsub_read_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		     int dlgt)
{
	if (!dlgt)
		return do_vfsub_read_k(file, kbuf, count, ppos);
	else {
		ssize_t err;
		int wkq_err;
		struct read_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};
		args.kbuf = kbuf;
		wkq_err = au_wkq_wait(call_read_k, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct write_args {
	ssize_t *errp;
	struct file *file;
	union {
		void *kbuf;
		const char __user *ubuf;
	};
	void *buf;
	size_t count;
	loff_t *ppos;
};

static void call_write_k(void *args)
{
	struct write_args *a = args;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  DLNPair(a->file->f_dentry), (unsigned long)a->count,
		  *a->ppos);
	*a->errp = do_vfsub_write_k(a->file, a->kbuf, a->count, a->ppos);
}

ssize_t vfsub_write_u(struct file *file, const char __user *ubuf, size_t count,
		      loff_t *ppos, int dlgt)
{
	if (!dlgt)
		return do_vfsub_write_u(file, ubuf, count, ppos);
	else {
		ssize_t err, written;
		int wkq_err;
		struct write_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};

		if (unlikely(!count))
			return 0;

		/*
		 * workaround an application bug.
		 * generally, read(2) or write(2) may return the value shorter
		 * than requested. But many applications don't support it,
		 * for example bash.
		 */
		err = -ENOMEM;
		if (args.count > PAGE_SIZE)
			args.count = PAGE_SIZE;
		args.kbuf = kmalloc(args.count, GFP_KERNEL);
		if (unlikely(!args.kbuf))
			goto out;

		written = 0;
		do {
			if (unlikely(copy_from_user(args.kbuf, ubuf,
						    args.count))) {
				err = -EFAULT;
				goto out_free;
			}

			wkq_err = au_wkq_wait(call_write_k, &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
			if (err > 0) {
				count -= err;
				if (count < args.count)
					args.count = count;
				ubuf += err;
				written += err;
			} else if (!err)
				break;
			else if (unlikely(err < 0))
				goto out_free;
		} while (count);
		err = written;

	out_free:
		kfree(args.kbuf);
	out:
		return err;
	}
}

ssize_t vfsub_write_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		      int dlgt)
{
	if (!dlgt)
		return do_vfsub_write_k(file, kbuf, count, ppos);
	else {
		ssize_t err;
		int wkq_err;
		struct write_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};
		args.kbuf = kbuf;
		wkq_err = au_wkq_wait(call_write_k, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct readdir_args {
	int *errp;
	struct file *file;
	filldir_t filldir;
	void *arg;
};

static void call_readdir(void *args)
{
	struct readdir_args *a = args;
	*a->errp = do_vfsub_readdir(a->file, a->filldir, a->arg);
}

int vfsub_readdir(struct file *file, filldir_t filldir, void *arg, int dlgt)
{
	if (!dlgt)
		return do_vfsub_readdir(file, filldir, arg);
	else {
		int err, wkq_err;
		struct readdir_args args = {
			.errp		= &err,
			.file		= file,
			.filldir	= filldir,
			.arg		= arg
		};
		wkq_err = au_wkq_wait(call_readdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}
#endif /* CONFIG_AUFS_DLGT */

/* ---------------------------------------------------------------------- */

struct notify_change_args {
	int *errp;
	struct dentry *h_dentry;
	struct iattr *ia;
};

static void call_notify_change(void *args)
{
	struct notify_change_args *a = args;
	struct inode *h_inode;

	LKTRTrace("%.*s, ia_valid 0x%x\n",
		  DLNPair(a->h_dentry), a->ia->ia_valid);
	h_inode = a->h_dentry->d_inode;
	IMustLock(h_inode);

	*a->errp = -EPERM;
	if (!IS_IMMUTABLE(h_inode) && !IS_APPEND(h_inode)) {
		lockdep_off();
		*a->errp = notify_change(a->h_dentry, a->ia);
		lockdep_on();
	}
	TraceErr(*a->errp);
}

int vfsub_notify_change(struct dentry *dentry, struct iattr *ia, int dlgt)
{
	int err;
	struct notify_change_args args = {
		.errp		= &err,
		.h_dentry	= dentry,
		.ia		= ia
	};

#ifndef CONFIG_AUFS_DLGT
	call_notify_change(&args);
#else
	if (!dlgt)
		call_notify_change(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_notify_change, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
#endif

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct unlink_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
};

static void call_unlink(void *args)
{
	struct unlink_args *a = args;
	struct inode *h_inode;
	const int stop_sillyrename = (au_is_nfs(a->dentry->d_sb)
				      && atomic_read(&a->dentry->d_count) == 1);

	LKTRTrace("%.*s, stop_silly %d, cnt %d\n",
		  DLNPair(a->dentry), stop_sillyrename,
		  atomic_read(&a->dentry->d_count));
	IMustLock(a->dir);

	if (!stop_sillyrename)
		dget(a->dentry);
	h_inode = a->dentry->d_inode;
	if (h_inode)
		atomic_inc(&h_inode->i_count);
#if 0 // partial testing
	{
		struct qstr *name = &a->dentry->d_name;
		if (name->len == sizeof(AUFS_XINO_FNAME) - 1
		    && !strncmp(name->name, AUFS_XINO_FNAME, name->len)) {
			lockdep_off();
			*a->errp = do_vfsub_unlink(a->dir, a->dentry);
			lockdep_on();
		} else
			err = -1;
	}
#else
	// vfs_unlink() locks inode
	lockdep_off();
	*a->errp = do_vfsub_unlink(a->dir, a->dentry);
	lockdep_on();
#endif

	if (!stop_sillyrename)
		dput(a->dentry);
	if (h_inode)
		iput(h_inode);

	TraceErr(*a->errp);
}

/*
 * @dir: must be locked.
 * @dentry: target dentry.
 */
int vfsub_unlink(struct inode *dir, struct dentry *dentry, int dlgt)
{
	int err;
	struct unlink_args args = {
		.errp	= &err,
		.dir	= dir,
		.dentry	= dentry
	};

#ifndef CONFIG_AUFS_DLGT
	call_unlink(&args);
#else
	if (!dlgt)
		call_unlink(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_unlink, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
#endif
	return err;
}

/* ---------------------------------------------------------------------- */

struct statfs_args {
	int *errp;
	void *arg;
	struct kstatfs *buf;
};

static void call_statfs(void *args)
{
	struct statfs_args *a = args;
	*a->errp = vfs_statfs(a->arg, a->buf);
}

int vfsub_statfs(void *arg, struct kstatfs *buf, int dlgt)
{
	int err;
	struct statfs_args args = {
		.errp	= &err,
		.arg	= arg,
		.buf	= buf
	};

#ifndef CONFIG_AUFS_DLGT
	call_statfs(&args);
#else
	if (!dlgt)
		call_statfs(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_statfs, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
#endif
	return err;
}
