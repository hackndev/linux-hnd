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

/* $Id: finfo.c,v 1.25 2007/06/04 02:17:35 sfjro Exp $ */

#include "aufs.h"

struct aufs_finfo *ftofi(struct file *file)
{
	struct aufs_finfo *finfo = file->private_data;
	AuDebugOn(!finfo
		  || !finfo->fi_hfile
		  || (0 < finfo->fi_bend
		      && (/* stosi(file->f_dentry->d_sb)->si_bend
			     < finfo->fi_bend
			     || */ finfo->fi_bend < finfo->fi_bstart)));
	return finfo;
}

// hard/soft set
aufs_bindex_t fbstart(struct file *file)
{
	FiMustAnyLock(file);
	return ftofi(file)->fi_bstart;
}

aufs_bindex_t fbend(struct file *file)
{
	FiMustAnyLock(file);
	return ftofi(file)->fi_bend;
}

struct aufs_vdir *fvdir_cache(struct file *file)
{
	FiMustAnyLock(file);
	return ftofi(file)->fi_vdir_cache;
}

struct aufs_branch *ftobr(struct file *file, aufs_bindex_t bindex)
{
	struct aufs_finfo *finfo = ftofi(file);
	struct aufs_hfile *hf;

	FiMustAnyLock(file);
	AuDebugOn(!finfo
		  || finfo->fi_bstart < 0
		  || bindex < finfo->fi_bstart
		  || finfo->fi_bend < bindex);
	hf = finfo->fi_hfile + bindex;
	AuDebugOn(hf->hf_br && br_count(hf->hf_br) <= 0);
	return hf->hf_br;
}

struct file *au_h_fptr_i(struct file *file, aufs_bindex_t bindex)
{
	struct aufs_finfo *finfo = ftofi(file);
	struct aufs_hfile *hf;

	FiMustAnyLock(file);
	AuDebugOn(!finfo
		  || finfo->fi_bstart < 0
		  || bindex < finfo->fi_bstart
		  || finfo->fi_bend < bindex);
	hf = finfo->fi_hfile + bindex;
	AuDebugOn(hf->hf_file
		  && file_count(hf->hf_file) <= 0
		  && br_count(hf->hf_br) <= 0);
	return hf->hf_file;
}

struct file *au_h_fptr(struct file *file)
{
	return au_h_fptr_i(file, fbstart(file));
}

void set_fbstart(struct file *file, aufs_bindex_t bindex)
{
	FiMustWriteLock(file);
	AuDebugOn(sbend(file->f_dentry->d_sb) < bindex);
	ftofi(file)->fi_bstart = bindex;
}

void set_fbend(struct file *file, aufs_bindex_t bindex)
{
	FiMustWriteLock(file);
	AuDebugOn(sbend(file->f_dentry->d_sb) < bindex
		  || bindex < fbstart(file));
	ftofi(file)->fi_bend = bindex;
}

void set_fvdir_cache(struct file *file, struct aufs_vdir *vdir_cache)
{
	FiMustWriteLock(file);
	AuDebugOn(!S_ISDIR(file->f_dentry->d_inode->i_mode)
		  || (ftofi(file)->fi_vdir_cache && vdir_cache));
	ftofi(file)->fi_vdir_cache = vdir_cache;
}

void au_hfput(struct aufs_hfile *hf)
{
	fput(hf->hf_file);
	hf->hf_file = NULL;
	AuDebugOn(!hf->hf_br);
	br_put(hf->hf_br);
	hf->hf_br = NULL;
}

void set_h_fptr(struct file *file, aufs_bindex_t bindex, struct file *val)
{
	struct aufs_finfo *finfo = ftofi(file);
	struct aufs_hfile *hf;

	FiMustWriteLock(file);
	AuDebugOn(!finfo
		  || finfo->fi_bstart < 0
		  || bindex < finfo->fi_bstart
		  || finfo->fi_bend < bindex);
	AuDebugOn(val && file_count(val) <= 0);
	hf = finfo->fi_hfile + bindex;
	AuDebugOn(val && hf->hf_file);
	if (hf->hf_file)
		au_hfput(hf);
	if (val) {
		hf->hf_file = val;
		hf->hf_br = stobr(file->f_dentry->d_sb, bindex);
	}
}

void au_update_figen(struct file *file)
{
	atomic_set(&ftofi(file)->fi_generation, au_digen(file->f_dentry));
}

void au_fin_finfo(struct file *file)
{
	struct aufs_finfo *finfo;
	struct dentry *dentry;
	aufs_bindex_t bindex, bend;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(dentry));
	SiMustAnyLock(dentry->d_sb);

	fi_write_lock(file);
	bend = fbend(file);
	bindex = fbstart(file);
	if (bindex >= 0)
		for (; bindex <= bend; bindex++)
			set_h_fptr(file, bindex, NULL);

	finfo = ftofi(file);
#ifdef CONFIG_AUFS_DEBUG
	if (finfo->fi_bstart >= 0) {
		bend = fbend(file);
		for (bindex = finfo->fi_bstart; bindex <= bend; bindex++) {
			struct aufs_hfile *hf;
			hf = finfo->fi_hfile + bindex;
			AuDebugOn(hf->hf_file || hf->hf_br);
		}
	}
#endif

	kfree(finfo->fi_hfile);
	fi_write_unlock(file);
	cache_free_finfo(finfo);
	//file->private_data = NULL;
}

int au_init_finfo(struct file *file)
{
	struct aufs_finfo *finfo;
	struct dentry *dentry;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(dentry));
	AuDebugOn(!dentry->d_inode);

	finfo = cache_alloc_finfo();
	if (finfo) {
		finfo->fi_hfile = kcalloc(sbend(dentry->d_sb) + 1,
					  sizeof(*finfo->fi_hfile), GFP_KERNEL);
		if (finfo->fi_hfile) {
			rw_init_wlock(&finfo->fi_rwsem);
			finfo->fi_bstart = -1;
			finfo->fi_bend = -1;
			atomic_set(&finfo->fi_generation, au_digen(dentry));

			file->private_data = finfo;
			return 0; /* success */
		}
		cache_free_finfo(finfo);
	}

	TraceErr(-ENOMEM);
	return -ENOMEM;
}
