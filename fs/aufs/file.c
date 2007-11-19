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

/* $Id: file.c,v 1.46 2007/06/11 01:41:50 sfjro Exp $ */

//#include <linux/fsnotify.h>
#include <linux/pagemap.h>
//#include <linux/poll.h>
//#include <linux/security.h>
#include "aufs.h"

/* drop flags for writing */
unsigned int au_file_roflags(unsigned int flags)
{
	flags &= ~(O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC);
	flags |= O_RDONLY | O_NOATIME;
	return flags;
}

/* common functions to regular file and dir */
struct file *hidden_open(struct dentry *dentry, aufs_bindex_t bindex, int flags)
{
	struct dentry *hidden_dentry;
	struct inode *hidden_inode;
	struct super_block *sb;
	struct vfsmount *hidden_mnt;
	struct file *hidden_file;
	struct aufs_branch *br;
	loff_t old_size;
	int udba;

	LKTRTrace("%.*s, b%d, flags 0%o\n", DLNPair(dentry), bindex, flags);
	AuDebugOn(!dentry);
	hidden_dentry = au_h_dptr_i(dentry, bindex);
	AuDebugOn(!hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	AuDebugOn(!hidden_inode);

	sb = dentry->d_sb;
	udba = au_flag_test(sb, AuFlag_UDBA_INOTIFY);
	if (unlikely(udba)) {
		// test here?
	}

	br = stobr(sb, bindex);
	br_get(br);
	/* drop flags for writing */
	if (test_ro(sb, bindex, dentry->d_inode))
		flags = au_file_roflags(flags);
	flags &= ~O_CREAT;
	spin_lock(&hidden_inode->i_lock);
	old_size = i_size_read(hidden_inode);
	spin_unlock(&hidden_inode->i_lock);

	//DbgSleep(3);

	dget(hidden_dentry);
	hidden_mnt = mntget(br->br_mnt);
	hidden_file = dentry_open(hidden_dentry, hidden_mnt, flags);
	//if (LktrCond) {fput(hidden_file); hidden_file = ERR_PTR(-1);}

	if (!IS_ERR(hidden_file)) {
#if 0 // remove this
		if (/* old_size && */ (flags & O_TRUNC)) {
			au_direval_dec(dentry);
			if (!IS_ROOT(dentry))
				au_direval_dec(dentry->d_parent);
		}
#endif
		return hidden_file;
	}

	br_put(br);
	TraceErrPtr(hidden_file);
	return hidden_file;
}

#if 0
static int do_coo(struct dentry *dentry, aufs_bindex_t bstart)
{
	int err;
	struct dentry *parent, *h_parent, *h_dentry;
	aufs_bindex_t bcpup;
	struct inode *h_dir, *h_inode, *dir;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	AuDebugOn(IS_ROOT(dentry));
	DiMustWriteLock(dentry);

	parent = dget_parent(dentry);
	di_write_lock_parent(parent);
	bcpup = err = find_rw_parent_br(dentry, bstart);
	//bcpup = err = find_rw_br(sb, bstart);
	if (unlikely(err < 0)) {
		err = 0; // stop copyup, it is not an error
		goto out;
	}
	err = 0;

	h_parent = au_h_dptr_i(parent, bcpup);
	if (!h_parent) {
		err = cpup_dirs(dentry, bcpup, NULL);
		if (unlikely(err))
			goto out;
		h_parent = au_h_dptr_i(parent, bcpup);
	}

	h_dir = h_parent->d_inode;
	h_dentry = au_h_dptr_i(dentry, bstart);
	h_inode = h_dentry->d_inode;
	dir = parent->d_inode;
	hdir_lock(h_dir, dir, bcpup);
	hi_lock_child(h_inode);
	AuDebugOn(au_h_dptr_i(dentry, bcpup));
	err = sio_cpup_simple(dentry, bcpup, -1,
			      au_flags_cpup(CPUP_DTIME, parent));
	TraceErr(err);
	i_unlock(h_inode);
	hdir_unlock(h_dir, dir, bcpup);

 out:
	di_write_unlock(parent);
	dput(parent);
	TraceErr(err);
	return err;
}
#endif

int au_do_open(struct inode *inode, struct file *file,
	       int (*open)(struct file *file, int flags))
{
	int err, coo;
	struct dentry *dentry;
	struct super_block *sb;
	aufs_bindex_t bstart;
	//struct inode *h_dir, *dir;

	dentry = file->f_dentry;
	LKTRTrace("i%lu, %.*s\n", inode->i_ino, DLNPair(dentry));

	sb = dentry->d_sb;
	si_read_lock(sb);
	coo = 0;
#if 0
	switch (au_flag_test_coo(sb)) {
	case AuFlag_COO_LEAF:
		coo = !S_ISDIR(inode->i_mode);
		break;
	case AuFlag_COO_ALL:
		coo = 1;
		break;
	}
#endif
	err = au_init_finfo(file);
	//if (LktrCond) {fi_write_unlock(file); fin_finfo(file); err = -1;}
	if (unlikely(err))
		goto out;

	if (!coo) {
		di_read_lock_child(dentry, AUFS_I_RLOCK);
		bstart = dbstart(dentry);
#if 0
	} else {
		di_write_lock_child(dentry);
		bstart = dbstart(dentry);
		if (test_ro(sb, bstart, dentry->d_inode)) {
			err = do_coo(dentry, bstart);
			if (err) {
				di_write_unlock(dentry);
				goto out_finfo;
			}
			bstart = dbstart(dentry);
		}
		di_downgrade_lock(dentry, AUFS_I_RLOCK);
#endif
	}

	// todo: remove this extra locks
#if 0
	dir = dentry->d_parent->d_inode;
	if (!IS_ROOT(dentry))
		ii_read_lock_parent(dir);
	h_dir = au_h_iptr_i(dir, bstart);
	hdir_lock(h_dir, dir, bstart);
#endif
	err = open(file, file->f_flags);
	//if (LktrCond) err = -1;
#if 0
	hdir_unlock(h_dir, dir, bstart);
	if (!IS_ROOT(dentry))
		ii_read_unlock(dir);
#endif
	di_read_unlock(dentry, AUFS_I_RLOCK);

// out_finfo:
	fi_write_unlock(file);
	if (unlikely(err))
		au_fin_finfo(file);
	//DbgFile(file);
 out:
	si_read_unlock(sb);
	TraceErr(err);
	return err;
}

int au_reopen_nondir(struct file *file)
{
	int err;
	struct dentry *dentry;
	aufs_bindex_t bstart, bindex, bend;
	struct file *hidden_file, *h_file_tmp;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(dentry));
	AuDebugOn(S_ISDIR(dentry->d_inode->i_mode)
		  || !au_h_dptr(dentry)->d_inode);
	bstart = dbstart(dentry);

	h_file_tmp = NULL;
	if (fbstart(file) == bstart) {
		hidden_file = au_h_fptr(file);
		if (file->f_mode == hidden_file->f_mode)
			return 0; /* success */
		h_file_tmp = hidden_file;
		get_file(h_file_tmp);
		set_h_fptr(file, bstart, NULL);
	}
	AuDebugOn(fbstart(file) < bstart
		  || ftofi(file)->fi_hfile[0 + bstart].hf_file);

	hidden_file = hidden_open(dentry, bstart, file->f_flags & ~O_TRUNC);
	//if (LktrCond) {fput(hidden_file); br_put(stobr(dentry->d_sb, bstart));
	//hidden_file = ERR_PTR(-1);}
	err = PTR_ERR(hidden_file);
	if (IS_ERR(hidden_file))
		goto out; // close all?
	err = 0;
	//cpup_file_flags(hidden_file, file);
	set_fbstart(file, bstart);
	set_h_fptr(file, bstart, hidden_file);
	memcpy(&hidden_file->f_ra, &file->f_ra, sizeof(file->f_ra)); //??

	/* close lower files */
	bend = fbend(file);
	for (bindex = bstart + 1; bindex <= bend; bindex++)
		set_h_fptr(file, bindex, NULL);
	set_fbend(file, bstart);

 out:
	if (h_file_tmp)
		fput(h_file_tmp);
	TraceErr(err);
	return err;
}

/*
 * copyup the deleted file for writing.
 */
static int cpup_wh_file(struct file *file, aufs_bindex_t bdst, loff_t len)
{
	int err;
	struct dentry *dentry, *parent, *hidden_parent, *tmp_dentry;
	struct dentry *hidden_dentry_bstart, *hidden_dentry_bdst;
	struct inode *hidden_dir;
	aufs_bindex_t bstart;
	struct aufs_dinfo *dinfo;
	struct dtime dt;
	struct lkup_args lkup;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, bdst %d, len %Lu\n", DLNPair(dentry), bdst, len);
	AuDebugOn(S_ISDIR(dentry->d_inode->i_mode)
		  || !(file->f_mode & FMODE_WRITE));
	DiMustWriteLock(dentry);
	parent = dget_parent(dentry);
	IiMustAnyLock(parent->d_inode);
	hidden_parent = au_h_dptr_i(parent, bdst);
	AuDebugOn(!hidden_parent);
	hidden_dir = hidden_parent->d_inode;
	AuDebugOn(!hidden_dir);
	IMustLock(hidden_dir);

	sb = parent->d_sb;
	lkup.nfsmnt = au_nfsmnt(sb, bdst);
	lkup.dlgt = need_dlgt(sb);
	tmp_dentry = lkup_whtmp(hidden_parent, &dentry->d_name, &lkup);
	//if (LktrCond) {dput(tmp_dentry); tmp_dentry = ERR_PTR(-1);}
	err = PTR_ERR(tmp_dentry);
	if (IS_ERR(tmp_dentry))
		goto out;

	dtime_store(&dt, parent, hidden_parent);
	dinfo = dtodi(dentry);
	bstart = dinfo->di_bstart;
	hidden_dentry_bdst = dinfo->di_hdentry[0 + bdst].hd_dentry;
	hidden_dentry_bstart = dinfo->di_hdentry[0 + bstart].hd_dentry;
	dinfo->di_bstart = bdst;
	dinfo->di_hdentry[0 + bdst].hd_dentry = tmp_dentry;
	dinfo->di_hdentry[0 + bstart].hd_dentry = au_h_fptr(file)->f_dentry;
	err = cpup_single(dentry, bdst, bstart, len,
			  au_flags_cpup(!CPUP_DTIME, parent));
	//if (LktrCond) err = -1;
	if (!err)
		err = au_reopen_nondir(file);
		//err = -1;
	dinfo->di_hdentry[0 + bstart].hd_dentry = hidden_dentry_bstart;
	dinfo->di_hdentry[0 + bdst].hd_dentry = hidden_dentry_bdst;
	dinfo->di_bstart = bstart;
	if (unlikely(err))
		goto out_tmp;

	AuDebugOn(!d_unhashed(dentry));
	err = vfsub_unlink(hidden_dir, tmp_dentry, lkup.dlgt);
	//if (LktrCond) err = -1;
	if (unlikely(err)) {
		IOErr("failed remove copied-up tmp file %.*s(%d)\n",
		      DLNPair(tmp_dentry), err);
		err = -EIO;
	}
	dtime_revert(&dt, !CPUP_LOCKED_GHDIR);

 out_tmp:
	dput(tmp_dentry);
 out:
	dput(parent);
	TraceErr(err);
	return err;
}

struct cpup_wh_file_args {
	int *errp;
	struct file *file;
	aufs_bindex_t bdst;
	loff_t len;
};

static void call_cpup_wh_file(void *args)
{
	struct cpup_wh_file_args *a = args;
	*a->errp = cpup_wh_file(a->file, a->bdst, a->len);
}

/*
 * prepare the @file for writing.
 */
int au_ready_to_write(struct file *file, loff_t len)
{
	int err, wkq_err;
	struct dentry *dentry, *parent, *hidden_dentry, *hidden_parent;
	struct inode *hidden_inode, *hidden_dir, *inode, *dir;
	struct super_block *sb;
	aufs_bindex_t bstart, bcpup;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, len %Ld\n", DLNPair(dentry), len);
	FiMustWriteLock(file);

	sb = dentry->d_sb;
	bstart = fbstart(file);
	AuDebugOn(ftobr(file, bstart) != stobr(sb, bstart));

	inode = dentry->d_inode;
	ii_read_lock_child(inode);
	LKTRTrace("rdonly %d, bstart %d\n", test_ro(sb, bstart, inode), bstart);
	err = test_ro(sb, bstart, inode);
	ii_read_unlock(inode);
	if (!err && (au_h_fptr(file)->f_mode & FMODE_WRITE))
		return 0;

	/* need to cpup */
	parent = dget_parent(dentry);
	di_write_lock_child(dentry);
	di_write_lock_parent(parent);
	bcpup = err = find_rw_parent_br(dentry, bstart);
	//bcpup = err = find_rw_br(sb, bstart);
	if (unlikely(err < 0))
		goto out_unlock;
	err = 0;

	hidden_parent = au_h_dptr_i(parent, bcpup);
	if (!hidden_parent) {
		err = cpup_dirs(dentry, bcpup, NULL);
		//if (LktrCond) err = -1;
		if (unlikely(err))
			goto out_unlock;
		hidden_parent = au_h_dptr_i(parent, bcpup);
	}

	hidden_dir = hidden_parent->d_inode;
	hidden_dentry = au_h_fptr(file)->f_dentry;
	hidden_inode = hidden_dentry->d_inode;
	dir = parent->d_inode;
	hdir_lock(hidden_dir, dir, bcpup);
	hi_lock_child(hidden_inode);
	if (d_unhashed(dentry) || d_unhashed(hidden_dentry)
	    /* || !hidden_inode->i_nlink */) {
		if (!au_test_perm(hidden_dir, MAY_EXEC | MAY_WRITE,
				  need_dlgt(sb)))
			err = cpup_wh_file(file, bcpup, len);
		else {
			struct cpup_wh_file_args args = {
				.errp	= &err,
				.file	= file,
				.bdst	= bcpup,
				.len	= len
			};
			wkq_err = au_wkq_wait(call_cpup_wh_file, &args,
					      /*dlgt*/0);
			if (unlikely(wkq_err))
				err = wkq_err;
		}
		//if (LktrCond) err = -1;
		TraceErr(err);
	} else {
		if (!au_h_dptr_i(dentry, bcpup))
			err = sio_cpup_simple(dentry, bcpup, len,
					      au_flags_cpup(CPUP_DTIME,
							    parent));
		//if (LktrCond) err = -1;
		TraceErr(err);
		if (!err)
			err = au_reopen_nondir(file);
		//if (LktrCond) err = -1;
		TraceErr(err);
	}
	i_unlock(hidden_inode);
	hdir_unlock(hidden_dir, dir, bcpup);

 out_unlock:
	di_write_unlock(parent);
	di_write_unlock(dentry);
	dput(parent);
	TraceErr(err);
	return err;

}

/* ---------------------------------------------------------------------- */

/*
 * after branch manipulating, refresh the file.
 */
static int refresh_file(struct file *file, int (*reopen)(struct file *file))
{
	int err, new_sz;
	struct dentry *dentry;
	aufs_bindex_t bend, bindex, bstart, brid;
	struct aufs_hfile *p;
	struct aufs_finfo *finfo;
	struct super_block *sb;
	struct inode *inode;
	struct file *hidden_file;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(dentry));
	FiMustWriteLock(file);
	DiMustReadLock(dentry);
	inode = dentry->d_inode;
	IiMustReadLock(inode);
	//au_debug_on();
	//DbgDentry(dentry);
	//DbgFile(file);
	//au_debug_off();

	err = -ENOMEM;
	sb = dentry->d_sb;
	finfo = ftofi(file);
	bstart = finfo->fi_bstart;
	bend = finfo->fi_bstart;
	new_sz = sizeof(*finfo->fi_hfile) * (sbend(sb) + 1);
	p = au_kzrealloc(finfo->fi_hfile, sizeof(*p) * (finfo->fi_bend + 1),
			 new_sz, GFP_KERNEL);
	//p = NULL;
	if (unlikely(!p))
		goto out;
	finfo->fi_hfile = p;
	hidden_file = p[0 + bstart].hf_file;

	p = finfo->fi_hfile + finfo->fi_bstart;
	brid = p->hf_br->br_id;
	bend = finfo->fi_bend;
	for (bindex = finfo->fi_bstart; bindex <= bend; bindex++, p++) {
		struct aufs_hfile tmp, *q;
		aufs_bindex_t new_bindex;

		if (!p->hf_file)
			continue;
		new_bindex = find_bindex(sb, p->hf_br);
		if (new_bindex == bindex)
			continue;
		if (new_bindex < 0) { // test here
			set_h_fptr(file, bindex, NULL);
			continue;
		}

		/* swap two hidden inode, and loop again */
		q = finfo->fi_hfile + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hf_file) {
			bindex--;
			p--;
		}
	}
	{
		aufs_bindex_t s = finfo->fi_bstart, e = finfo->fi_bend;
		finfo->fi_bstart = 0;
		finfo->fi_bend = sbend(sb);
		//au_debug_on();
		//DbgFile(file);
		//au_debug_off();
		finfo->fi_bstart = s;
		finfo->fi_bend = e;
	}

	p = finfo->fi_hfile;
	if (!au_is_mmapped(file) && !d_unhashed(dentry)) {
		bend = sbend(sb);
		for (finfo->fi_bstart = 0; finfo->fi_bstart <= bend;
		     finfo->fi_bstart++, p++)
			if (p->hf_file) {
				if (p->hf_file->f_dentry
				    && p->hf_file->f_dentry->d_inode)
					break;
				else
					au_hfput(p);
			}
	} else {
		bend = find_brindex(sb, brid);
		//LKTRTrace("%d\n", bend);
		for (finfo->fi_bstart = 0; finfo->fi_bstart < bend;
		     finfo->fi_bstart++, p++)
			if (p->hf_file)
				au_hfput(p);
		//LKTRTrace("%d\n", finfo->fi_bstart);
		bend = sbend(sb);
	}

	p = finfo->fi_hfile + bend;
	for (finfo->fi_bend = bend; finfo->fi_bend >= finfo->fi_bstart;
	     finfo->fi_bend--, p--)
		if (p->hf_file) {
			if (p->hf_file->f_dentry
			    && p->hf_file->f_dentry->d_inode)
				break;
			else
				au_hfput(p);
		}
	//Dbg("%d, %d\n", finfo->fi_bstart, finfo->fi_bend);
	AuDebugOn(finfo->fi_bend < finfo->fi_bstart);
	//DbgFile(file);
	//DbgDentry(file->f_dentry);

	err = 0;
#if 0 // todo:
	if (!au_h_dptr(dentry)->d_inode) {
		au_update_figen(file);
		goto out; /* success */
	}
#endif

	if (unlikely(au_is_mmapped(file) || d_unhashed(dentry)))
		goto out_update; /* success */

 again:
	bstart = ibstart(inode);
	if (bstart < finfo->fi_bstart
	    && au_flag_test(sb, AuFlag_PLINK)
	    && au_is_plinked(sb, inode)) {
		struct dentry *parent = dentry->d_parent; // dget_parent()
		struct inode *dir = parent->d_inode, *h_dir;

		if (test_ro(sb, bstart, inode)) {
			di_read_lock_parent(parent, !AUFS_I_RLOCK);
			bstart = err = find_rw_parent_br(dentry, bstart);
			//bstart = err = find_rw_br(sb, bstart);
			di_read_unlock(parent, !AUFS_I_RLOCK);
			//todo: err = -1;
			if (unlikely(err < 0))
				goto out;
		}
		di_read_unlock(dentry, AUFS_I_RLOCK);
		di_write_lock_child(dentry);
		if (bstart != ibstart(inode)) { // todo
			/* someone changed our inode while we were sleeping */
			di_downgrade_lock(dentry, AUFS_I_RLOCK);
			goto again;
		}

		di_read_lock_parent(parent, AUFS_I_RLOCK);
		err = test_and_cpup_dirs(dentry, bstart, NULL);

		// always superio.
#if 1
		h_dir = au_h_dptr_i(parent, bstart)->d_inode;
		hdir_lock(h_dir, dir, bstart);
		err = sio_cpup_simple(dentry, bstart, -1,
				      au_flags_cpup(CPUP_DTIME, parent));
		hdir_unlock(h_dir, dir, bstart);
		di_read_unlock(parent, AUFS_I_RLOCK);
#else
		if (!is_au_wkq(current)) {
			int wkq_err;
			struct cpup_pseudo_link_args args = {
				.errp		= &err,
				.dentry		= dentry,
				.bdst		= bstart,
				.do_lock	= 1
			};
			wkq_err = au_wkq_wait(call_cpup_pseudo_link, &args);
			if (unlikely(wkq_err))
				err = wkq_err;
		} else
			err = cpup_pseudo_link(dentry, bstart, /*do_lock*/1);
#endif
		di_downgrade_lock(dentry, AUFS_I_RLOCK);
		if (unlikely(err))
			goto out;
	}

	err = reopen(file);
	//err = -1;
 out_update:
	if (!err) {
		au_update_figen(file);
		//DbgFile(file);
		return 0; /* success */
	}

	/* error, close all hidden files */
	bend = fbend(file);
	for (bindex = fbstart(file); bindex <= bend; bindex++)
		set_h_fptr(file, bindex, NULL);

 out:
	TraceErr(err);
	return err;
}

/* common function to regular file and dir */
int au_reval_and_lock_finfo(struct file *file, int (*reopen)(struct file *file),
			    int wlock, int locked)
{
	int err, sgen, fgen, pseudo_link;
	struct dentry *dentry;
	struct super_block *sb;
	aufs_bindex_t bstart;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, w %d, l %d\n", DLNPair(dentry), wlock, locked);
	sb = dentry->d_sb;
	SiMustAnyLock(sb);

	err = 0;
	sgen = au_sigen(sb);
	fi_write_lock(file);
	fgen = au_figen(file);
	di_read_lock_child(dentry, AUFS_I_RLOCK);
	bstart = dbstart(dentry);
	pseudo_link = (bstart != ibstart(dentry->d_inode));
	di_read_unlock(dentry, AUFS_I_RLOCK);
	if (sgen == fgen && !pseudo_link && fbstart(file) == bstart) {
		if (!wlock)
			fi_downgrade_lock(file);
		return 0; /* success */
	}

	LKTRTrace("sgen %d, fgen %d\n", sgen, fgen);
	if (sgen != au_digen(dentry)) {
		/*
		 * d_path() and path_lookup() is a simple and good approach
		 * to revalidate. but si_rwsem in DEBUG_RWSEM will cause a
		 * deadlock. removed the code.
		 */
		di_write_lock_child(dentry);
		err = au_reval_dpath(dentry, sgen);
		//if (LktrCond) err = -1;
		di_write_unlock(dentry);
		if (unlikely(err < 0))
			goto out;
		AuDebugOn(au_digen(dentry) != sgen);
	}

	di_read_lock_child(dentry, AUFS_I_RLOCK);
	err = refresh_file(file, reopen);
	//if (LktrCond) err = -1;
	di_read_unlock(dentry, AUFS_I_RLOCK);
	if (!err) {
		if (!wlock)
			fi_downgrade_lock(file);
	} else
		fi_write_unlock(file);

 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

// cf. aufs_nopage()
// for madvise(2)
static int aufs_readpage(struct file *file, struct page *page)
{
	TraceEnter();
	unlock_page(page);
	return 0;
}

// they will never be called.
#ifdef CONFIG_AUFS_DEBUG
static int aufs_prepare_write(struct file *file, struct page *page,
			      unsigned from, unsigned to)
{BUG();return 0;}
static int aufs_commit_write(struct file *file, struct page *page,
			     unsigned from, unsigned to)
{BUG();return 0;}
static int aufs_writepage(struct page *page, struct writeback_control *wbc)
{BUG();return 0;}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void aufs_sync_page(struct page *page)
{BUG();}
#else
static int aufs_sync_page(struct page *page)
{BUG(); return 0;}
#endif

#if 0 // comment
static int aufs_writepages(struct address_space *mapping,
			   struct writeback_control *wbc)
{BUG();return 0;}
static int aufs_readpages(struct file *filp, struct address_space *mapping,
			  struct list_head *pages, unsigned nr_pages)
{BUG();return 0;}
static sector_t aufs_bmap(struct address_space *mapping, sector_t block)
{BUG();return 0;}
#endif

static int aufs_set_page_dirty(struct page *page)
{BUG();return 0;}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void aufs_invalidatepage (struct page *page, unsigned long offset)
{BUG();}
#else
static int aufs_invalidatepage (struct page *page, unsigned long offset)
{BUG(); return 0;}
#endif
static int aufs_releasepage (struct page *page, gfp_t gfp)
{BUG();return 0;}
static ssize_t aufs_direct_IO(int rw, struct kiocb *iocb,
			      const struct iovec *iov, loff_t offset,
			      unsigned long nr_segs)
{BUG();return 0;}
static struct page* aufs_get_xip_page(struct address_space *mapping,
				      sector_t offset, int create)
{BUG();return NULL;}
//static int aufs_migratepage (struct page *newpage, struct page *page)
//{BUG();return 0;}
#endif

#if 0 // comment
struct address_space {
	struct inode		*host;		/* owner: inode, block_device */
	struct radix_tree_root	page_tree;	/* radix tree of all pages */
	rwlock_t		tree_lock;	/* and rwlock protecting it */
	unsigned int		i_mmap_writable;/* count VM_SHARED mappings */
	struct prio_tree_root	i_mmap;		/* tree of private and shared mappings */
	struct list_head	i_mmap_nonlinear;/*list VM_NONLINEAR mappings */
	spinlock_t		i_mmap_lock;	/* protect tree, count, list */
	unsigned int		truncate_count;	/* Cover race condition with truncate */
	unsigned long		nrpages;	/* number of total pages */
	pgoff_t			writeback_index;/* writeback starts here */
	struct address_space_operations *a_ops;	/* methods */
	unsigned long		flags;		/* error bits/gfp mask */
	struct backing_dev_info *backing_dev_info; /* device readahead, etc */
	spinlock_t		private_lock;	/* for use by the address_space */
	struct list_head	private_list;	/* ditto */
	struct address_space	*assoc_mapping;	/* ditto */
} __attribute__((aligned(sizeof(long))));

struct address_space_operations {
	int (*writepage)(struct page *page, struct writeback_control *wbc);
	int (*readpage)(struct file *, struct page *);
	void (*sync_page)(struct page *);

	/* Write back some dirty pages from this mapping. */
	int (*writepages)(struct address_space *, struct writeback_control *);

	/* Set a page dirty.  Return true if this dirtied it */
	int (*set_page_dirty)(struct page *page);

	int (*readpages)(struct file *filp, struct address_space *mapping,
			struct list_head *pages, unsigned nr_pages);

	/*
	 * ext3 requires that a successful prepare_write() call be followed
	 * by a commit_write() call - they must be balanced
	 */
	int (*prepare_write)(struct file *, struct page *, unsigned, unsigned);
	int (*commit_write)(struct file *, struct page *, unsigned, unsigned);
	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
	sector_t (*bmap)(struct address_space *, sector_t);
	void (*invalidatepage) (struct page *, unsigned long);
	int (*releasepage) (struct page *, gfp_t);
	ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *iov,
			loff_t offset, unsigned long nr_segs);
	struct page* (*get_xip_page)(struct address_space *, sector_t,
			int);
	/* migrate the contents of a page to the specified target */
	int (*migratepage) (struct page *, struct page *);
};
#endif

struct address_space_operations aufs_aop = {
	.readpage	= aufs_readpage,
#ifdef CONFIG_AUFS_DEBUG
	.writepage	= aufs_writepage,
	.sync_page	= aufs_sync_page,
	//.writepages	= aufs_writepages,
	.set_page_dirty	= aufs_set_page_dirty,
	//.readpages	= aufs_readpages,
	.prepare_write	= aufs_prepare_write,
	.commit_write	= aufs_commit_write,
	//.bmap		= aufs_bmap,
	.invalidatepage	= aufs_invalidatepage,
	.releasepage	= aufs_releasepage,
	.direct_IO	= aufs_direct_IO,
	.get_xip_page	= aufs_get_xip_page,
	//.migratepage	= aufs_migratepage
#endif
};
