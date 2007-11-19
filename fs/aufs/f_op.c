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

/* $Id: f_op.c,v 1.28 2007/05/27 23:03:45 sfjro Exp $ */

#include <linux/fsnotify.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/security.h>
#include <linux/version.h>
#include "aufs.h"

/* common function to regular file and dir */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define FlushArgs	hidden_file, id
int aufs_flush(struct file *file, fl_owner_t id)
#else
#define FlushArgs	hidden_file
int aufs_flush(struct file *file)
#endif
{
	int err;
	struct dentry *dentry;
	aufs_bindex_t bindex, bend;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(dentry));

	// aufs_read_lock_file()
	si_read_lock(dentry->d_sb);
	fi_read_lock(file);
	di_read_lock_child(dentry, !AUFS_I_RLOCK);

	err = 0;
	bend = fbend(file);
	for (bindex = fbstart(file); !err && bindex <= bend; bindex++) {
		struct file *hidden_file;
		hidden_file = au_h_fptr_i(file, bindex);
		if (hidden_file && hidden_file->f_op
		    && hidden_file->f_op->flush)
			err = hidden_file->f_op->flush(FlushArgs);
	}

	di_read_unlock(dentry, !AUFS_I_RLOCK);
	fi_read_unlock(file);
	si_read_unlock(dentry->d_sb);
	TraceErr(err);
	return err;
}
#undef FlushArgs

/* ---------------------------------------------------------------------- */

static int do_open_nondir(struct file *file, int flags)
{
	int err;
	aufs_bindex_t bindex;
	struct super_block *sb;
	struct file *hidden_file;
	struct dentry *dentry;
	struct inode *inode;
	struct aufs_finfo *finfo;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, flags 0%o\n", DLNPair(dentry), flags);
	FiMustWriteLock(file);
	inode = dentry->d_inode;
	AuDebugOn(!inode || S_ISDIR(inode->i_mode));

	err = 0;
	finfo = ftofi(file);
	finfo->fi_h_vm_ops = NULL;
	sb = dentry->d_sb;
	bindex = dbstart(dentry);
	AuDebugOn(!au_h_dptr(dentry)->d_inode);
	/* O_TRUNC is processed already */
	BUG_ON(test_ro(sb, bindex, inode) && (flags & O_TRUNC));

	hidden_file = hidden_open(dentry, bindex, flags);
	//if (LktrCond) {fput(hidden_file); br_put(stobr(dentry->d_sb, bindex));
	//hidden_file = ERR_PTR(-1);}
	if (!IS_ERR(hidden_file)) {
		set_fbstart(file, bindex);
		set_fbend(file, bindex);
		set_h_fptr(file, bindex, hidden_file);
		return 0; /* success */
	}
	err = PTR_ERR(hidden_file);
	TraceErr(err);
	return err;
}

static int aufs_open_nondir(struct inode *inode, struct file *file)
{
	return au_do_open(inode, file, do_open_nondir);
}

static int aufs_release_nondir(struct inode *inode, struct file *file)
{
	struct super_block *sb = file->f_dentry->d_sb;

	LKTRTrace("i%lu, %.*s\n", inode->i_ino, DLNPair(file->f_dentry));

	si_read_lock(sb);
	au_fin_finfo(file);
	si_read_unlock(sb);
	return 0;
}

/* ---------------------------------------------------------------------- */

static ssize_t aufs_read(struct file *file, char __user *buf, size_t count,
			 loff_t *ppos)
{
	ssize_t err;
	struct dentry *dentry;
	struct file *hidden_file;
	struct super_block *sb;
	struct inode *h_inode;
	int dlgt;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  DLNPair(dentry), (unsigned long)count, *ppos);

	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/0,
				      /*locked*/0);
	//if (LktrCond) {fi_read_unlock(file); err = -1;}
	if (unlikely(err))
		goto out;

	/* support LSM and notify */
	dlgt = need_dlgt(sb);
	hidden_file = au_h_fptr(file);
	h_inode = hidden_file->f_dentry->d_inode;
	if (!au_flag_test(sb, AuFlag_UDBA_INOTIFY))
		err = vfsub_read_u(hidden_file, buf, count, ppos, dlgt);
	else {
		struct dentry *parent = dget_parent(dentry),
			*h_parent = dget_parent(hidden_file->f_dentry);
		struct inode *dir = parent->d_inode,
			*h_dir = h_parent->d_inode;
		aufs_bindex_t bstart = fbstart(file);
		hdir_lock(h_dir, dir, bstart);
		err = vfsub_read_u(hidden_file, buf, count, ppos, dlgt);
		hdir_unlock(h_dir, dir, bstart);
		dput(parent);
		dput(h_parent);
	}
	memcpy(&file->f_ra, &hidden_file->f_ra, sizeof(file->f_ra)); //??
	dentry->d_inode->i_atime = hidden_file->f_dentry->d_inode->i_atime;

	fi_read_unlock(file);
 out:
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

static ssize_t aufs_write(struct file *file, const char __user *__buf,
			  size_t count, loff_t *ppos)
{
	ssize_t err;
	struct dentry *dentry;
	struct inode *inode;
	struct super_block *sb;
	struct file *hidden_file;
	char __user *buf = (char __user*)__buf;
	struct inode *h_inode;
	int dlgt;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  DLNPair(dentry), (unsigned long)count, *ppos);

	inode = dentry->d_inode;
	i_lock(inode);
	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/1,
				      /*locked*/1);
	//if (LktrCond) {fi_write_unlock(file); err = -1;}
	if (unlikely(err))
		goto out;
	err = au_ready_to_write(file, -1);
	//if (LktrCond) err = -1;
	if (unlikely(err))
		goto out_unlock;

	/* support LSM and notify */
	dlgt = need_dlgt(sb);
	hidden_file = au_h_fptr(file);
	h_inode = hidden_file->f_dentry->d_inode;
	if (!au_flag_test(sb, AuFlag_UDBA_INOTIFY))
		err = vfsub_write_u(hidden_file, buf, count, ppos, dlgt);
	else {
		struct dentry *parent = dget_parent(dentry),
			*h_parent = dget_parent(hidden_file->f_dentry);
		struct inode *dir = parent->d_inode,
			*h_dir = h_parent->d_inode;
		aufs_bindex_t bstart = fbstart(file);
		hdir_lock(h_dir, dir, bstart);
		err = vfsub_write_u(hidden_file, buf, count, ppos, dlgt);
		hdir_unlock(h_dir, dir, bstart);
		dput(parent);
		dput(h_parent);
	}
	ii_write_lock_child(inode);
	au_cpup_attr_timesizes(inode);
	ii_write_unlock(inode);

 out_unlock:
	fi_write_unlock(file);
 out:
	si_read_unlock(sb);
	i_unlock(inode);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#if 0 //def CONFIG_AUFS_ROBR
struct lvma {
	struct list_head list;
	struct vm_area_struct *vma;
};

static struct file *safe_file(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct super_block *sb = file->f_dentry->d_sb;
	struct lvma *lvma, *entry;
	struct aufs_sbinfo *sbinfo;
	int found, warn;

	TraceEnter();
	AuDebugOn(!SB_AUFS(sb));

	warn = 0;
	found = 0;
	sbinfo = stosi(sb);
	spin_lock(&sbinfo->si_lvma_lock);
	list_for_each_entry(entry, &sbinfo->si_lvma, list) {
		found = (entry->vma == vma);
		if (unlikely(found))
			break;
	}
	if (!found) {
		lvma = kmalloc(sizeof(*lvma), GFP_ATOMIC);
		if (lvma) {
			lvma->vma = vma;
			list_add(&lvma->list, &sbinfo->si_lvma);
		} else {
			warn = 1;
			file = NULL;
		}
	} else
		file = NULL;
	spin_unlock(&sbinfo->si_lvma_lock);

	if (unlikely(warn))
		Warn1("no memory for lvma\n");
	return file;
}

static void reset_file(struct vm_area_struct *vma, struct file *file)
{
	struct super_block *sb = file->f_dentry->d_sb;
	struct lvma *entry, *found;
	struct aufs_sbinfo *sbinfo;

	TraceEnter();
	AuDebugOn(!SB_AUFS(sb));

	vma->vm_file = file;

	found = NULL;
	sbinfo = stosi(sb);
	spin_lock(&sbinfo->si_lvma_lock);
	list_for_each_entry(entry, &sbinfo->si_lvma, list)
		if (entry->vma == vma){
			found = entry;
			break;
		}
	AuDebugOn(!found);
	list_del(&found->list);
	spin_unlock(&sbinfo->si_lvma_lock);
	kfree(found);
}

#else

static struct file *safe_file(struct vm_area_struct *vma)
{
	struct file *file;

	file = vma->vm_file;
	if (file->private_data && au_is_aufs(file->f_dentry->d_sb))
		return file;
	return NULL;
}

static void reset_file(struct vm_area_struct *vma, struct file *file)
{
	vma->vm_file = file;
	smp_mb();
}
#endif /* CONFIG_AUFS_ROBR */

static struct page *aufs_nopage(struct vm_area_struct *vma, unsigned long addr,
				int *type)
{
	struct page *page;
	struct dentry *dentry;
	struct file *file, *hidden_file;
	struct inode *inode;
	static DECLARE_WAIT_QUEUE_HEAD(wq);
	struct aufs_finfo *finfo;

	TraceEnter();
	AuDebugOn(!vma || !vma->vm_file);
	wait_event(wq, (file = safe_file(vma)));
	AuDebugOn(!au_is_aufs(file->f_dentry->d_sb));
	dentry = file->f_dentry;
	LKTRTrace("%.*s, addr %lx\n", DLNPair(dentry), addr);
	inode = dentry->d_inode;
	AuDebugOn(!S_ISREG(inode->i_mode));

	// do not revalidate, nor lock
	finfo = ftofi(file);
	hidden_file = finfo->fi_hfile[0 + finfo->fi_bstart].hf_file;
	AuDebugOn(!hidden_file || !au_is_mmapped(file));
	vma->vm_file = hidden_file;
	//smp_mb();
	page = finfo->fi_h_vm_ops->nopage(vma, addr, type);
	reset_file(vma, file);
#if 0 //def CONFIG_SMP
	//wake_up_nr(&wq, online_cpu - 1);
	wake_up_all(&wq);
#else
	wake_up(&wq);
#endif
	if (!IS_ERR(page)) {
		//page->mapping = file->f_mapping;
		//get_page(page);
		//file->f_mapping = hidden_file->f_mapping;
		//touch_atime(NULL, dentry);
		//inode->i_atime = hidden_file->f_dentry->d_inode->i_atime;
	}
	TraceErrPtr(page);
	return page;
}

static int aufs_populate(struct vm_area_struct *vma, unsigned long addr,
			 unsigned long len, pgprot_t prot, unsigned long pgoff,
			 int nonblock)
{
	Err("please report me this application\n");
	BUG();
	return ftofi(vma->vm_file)->fi_h_vm_ops->populate
		(vma, addr, len, prot, pgoff, nonblock);
}

static struct vm_operations_struct aufs_vm_ops = {
	//.open		= aufs_vmaopen,
	//.close		= aufs_vmaclose,
	.nopage		= aufs_nopage,
	.populate	= aufs_populate,
	//page_mkwrite(struct vm_area_struct *vma, struct page *page)
};

/* ---------------------------------------------------------------------- */

static int aufs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err, wlock, mmapped;
	struct dentry *dentry;
	struct super_block *sb;
	struct file *h_file;
	struct vm_operations_struct *vm_ops;
	unsigned long flags;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, %lx, len %lu\n",
		  DLNPair(dentry), vma->vm_start, vma->vm_end - vma->vm_start);
	AuDebugOn(!S_ISREG(dentry->d_inode->i_mode));
	AuDebugOn(down_write_trylock(&vma->vm_mm->mmap_sem));

	mmapped = au_is_mmapped(file);
	wlock = 0;
	if (file->f_mode & FMODE_WRITE) {
		flags = VM_SHARED | VM_WRITE;
		wlock = ((flags & vma->vm_flags) == flags);
	}

	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir,
				      wlock | !mmapped, /*locked*/0);
	//err = -1;
	if (unlikely(err))
		goto out;

	if (wlock) {
		err = au_ready_to_write(file, -1);
		//err = -1;
		if (unlikely(err))
			goto out_unlock;
	}

	h_file = au_h_fptr(file);
	vm_ops = ftofi(file)->fi_h_vm_ops;
	if (unlikely(!mmapped)) {
		// nfs uses some locks
		lockdep_off();
		err = h_file->f_op->mmap(h_file, vma);
		lockdep_on();
		if (unlikely(err))
			goto out_unlock;
		vm_ops = vma->vm_ops;
		AuDebugOn(!vm_ops);
		err = do_munmap(current->mm, vma->vm_start,
				vma->vm_end - vma->vm_start);
		if (unlikely(err)) {
			IOErr("failed internal unmapping %.*s, %d\n",
			      DLNPair(h_file->f_dentry), err);
			err = -EIO;
			goto out_unlock;
		}
	}
	AuDebugOn(!vm_ops);

	err = generic_file_mmap(file, vma);
	if (!err) {
		file_accessed(h_file);
		dentry->d_inode->i_atime = h_file->f_dentry->d_inode->i_atime;
		vma->vm_ops = &aufs_vm_ops;
		if (unlikely(!mmapped))
			ftofi(file)->fi_h_vm_ops = vm_ops;
	}

 out_unlock:
	if (!wlock && mmapped)
		fi_read_unlock(file);
	else
		fi_write_unlock(file);
 out:
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

// todo: try do_sendfile() in fs/read_write.c
static ssize_t aufs_sendfile(struct file *file, loff_t *ppos,
			     size_t count, read_actor_t actor, void *target)
{
	ssize_t err;
	struct file *h_file;
	const char c = current->comm[4];
	/* true if a kernel thread named 'loop[0-9].*' accesses a file */
	const int loopback = (current->mm == NULL
			      && '0' <= c && c <= '9'
			      && strncmp(current->comm, "loop", 4) == 0);
	struct dentry *dentry;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, pos %Ld, cnt %lu, loopback %d\n",
		  DLNPair(dentry), *ppos, (unsigned long)count, loopback);

	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/0,
				      /*locked*/0);
	if (unlikely(err))
		goto out;

	err = -EINVAL;
	h_file = au_h_fptr(file);
	if (h_file->f_op && h_file->f_op->sendfile) {
		if (/* unlikely */(loopback)) {
			file->f_mapping = h_file->f_mapping;
			smp_mb(); //??
		}
		// nfs uses some locks
		lockdep_off();
		err = h_file->f_op->sendfile
			(h_file, ppos, count, actor, target);
		lockdep_on();
		dentry->d_inode->i_atime = h_file->f_dentry->d_inode->i_atime;
	}
	fi_read_unlock(file);

 out:
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/* copied from linux/fs/select.h, must match */
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

static unsigned int aufs_poll(struct file *file, poll_table *wait)
{
	unsigned int mask;
	struct file *hidden_file;
	int err;
	struct dentry *dentry;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, wait %p\n", DLNPair(dentry), wait);
	AuDebugOn(S_ISDIR(dentry->d_inode->i_mode));

	/* We should pretend an error happend. */
	mask = POLLERR /* | POLLIN | POLLOUT */;
	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/0,
				      /*locked*/0);
	//err = -1;
	if (unlikely(err))
		goto out;

	/* it is not an error of hidden_file has no operation */
	mask = DEFAULT_POLLMASK;
	hidden_file = au_h_fptr(file);
	if (hidden_file->f_op && hidden_file->f_op->poll)
		mask = hidden_file->f_op->poll(hidden_file, wait);
	fi_read_unlock(file);

 out:
	si_read_unlock(sb);
	TraceErr((int)mask);
	return mask;
}

static int aufs_fsync_nondir(struct file *file, struct dentry *dentry,
			     int datasync)
{
	int err, my_lock;
	struct inode *inode;
	struct file *hidden_file;
	struct super_block *sb;

	LKTRTrace("%.*s, %d\n", DLNPair(dentry), datasync);
	inode = dentry->d_inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	IMustLock(inode);
	my_lock = 0;
#else
	/* before 2.6.17,
	 * msync(2) calls me without locking i_sem/i_mutex, but fsync(2).
	 */
	my_lock = !i_trylock(inode);
#endif

	sb = dentry->d_sb;
	si_read_lock(sb);
	err = 0; //-EBADF; // posix?
	if (unlikely(!(file->f_mode & FMODE_WRITE)))
		goto out;
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/1,
				      /*locked*/1);
	//err = -1;
	if (unlikely(err))
		goto out;
	err = au_ready_to_write(file, -1);
	//err = -1;
	if (unlikely(err))
		goto out_unlock;

	err = -EINVAL;
	hidden_file = au_h_fptr(file);
	if (hidden_file->f_op && hidden_file->f_op->fsync) {
		// todo: apparmor thread?
		//file->f_mapping->host->i_mutex
		ii_write_lock_child(inode);
		hi_lock_child(hidden_file->f_dentry->d_inode);
		err = hidden_file->f_op->fsync
			(hidden_file, hidden_file->f_dentry, datasync);
		//err = -1;
		au_cpup_attr_timesizes(inode);
		i_unlock(hidden_file->f_dentry->d_inode);
		ii_write_unlock(inode);
	}

 out_unlock:
	fi_write_unlock(file);
 out:
	if (unlikely(my_lock))
		i_unlock(inode);
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

static int aufs_fasync(int fd, struct file *file, int flag)
{
	int err;
	struct file *hidden_file;
	struct dentry *dentry;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, %d\n", DLNPair(dentry), flag);

	sb = dentry->d_sb;
	si_read_lock(sb);
	err = au_reval_and_lock_finfo(file, au_reopen_nondir, /*wlock*/0,
				      /*locked*/0);
	//err = -1;
	if (unlikely(err))
		goto out;

	hidden_file = au_h_fptr(file);
	if (hidden_file->f_op && hidden_file->f_op->fasync)
		err = hidden_file->f_op->fasync(fd, hidden_file, flag);
	fi_read_unlock(file);

 out:
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#if 0 // comment
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*aio_read) (struct kiocb *, char __user *, size_t, loff_t);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*aio_write) (struct kiocb *, const char __user *, size_t, loff_t);
	int (*readdir) (struct file *, void *, filldir_t);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, struct dentry *, int datasync);
	int (*aio_fsync) (struct kiocb *, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*readv) (struct file *, const struct iovec *, unsigned long, loff_t *);
	ssize_t (*writev) (struct file *, const struct iovec *, unsigned long, loff_t *);
	ssize_t (*sendfile) (struct file *, loff_t *, size_t, read_actor_t, void *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*dir_notify)(struct file *file, unsigned long arg);
	int (*flock) (struct file *, int, struct file_lock *);
};
#endif

struct file_operations aufs_file_fop = {
	.read		= aufs_read,
	.write		= aufs_write,
	.poll		= aufs_poll,
	.mmap		= aufs_mmap,
	.open		= aufs_open_nondir,
	.flush		= aufs_flush,
	.release	= aufs_release_nondir,
	.fsync		= aufs_fsync_nondir,
	.fasync		= aufs_fasync,
	.sendfile	= aufs_sendfile,
};
