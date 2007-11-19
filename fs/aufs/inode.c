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

/* $Id: inode.c,v 1.24 2007/06/04 02:17:35 sfjro Exp $ */

#include "aufs.h"

int au_refresh_hinode(struct inode *inode, struct dentry *dentry)
{
	int err, new_sz, update, isdir;
	struct inode *first;
	struct aufs_hinode *p, *q, tmp;
	struct super_block *sb;
	struct aufs_iinfo *iinfo;
	aufs_bindex_t bindex, bend, new_bindex;
	unsigned int flags;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	IiMustWriteLock(inode);

	err = -ENOMEM;
	sb = dentry->d_sb;
	bend = sbend(sb);
	new_sz = sizeof(*iinfo->ii_hinode) * (bend + 1);
	iinfo = itoii(inode);
	p = au_kzrealloc(iinfo->ii_hinode, sizeof(*p) * (iinfo->ii_bend + 1),
			 new_sz, GFP_KERNEL);
	//p = NULL;
	if (unlikely(!p))
		goto out;

	iinfo->ii_hinode = p;
	err = 0;
	update = 0;
	p = iinfo->ii_hinode + iinfo->ii_bstart;
	first = p->hi_inode;
	for (bindex = iinfo->ii_bstart; bindex <= iinfo->ii_bend;
	     bindex++, p++) {
		if (unlikely(!p->hi_inode))
			continue;

		new_bindex = find_brindex(sb, p->hi_id);
		if (new_bindex == bindex)
			continue;
		if (new_bindex < 0) {
			update++;
			aufs_hiput(p);
			p->hi_inode = NULL;
			continue;
		}

		if (new_bindex < iinfo->ii_bstart)
			iinfo->ii_bstart = new_bindex;
		if (iinfo->ii_bend < new_bindex)
			iinfo->ii_bend = new_bindex;
		/* swap two hidden inode, and loop again */
		q = iinfo->ii_hinode + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hi_inode) {
			bindex--;
			p--;
		}
	}

	isdir = S_ISDIR(inode->i_mode);
	flags = au_hi_flags(inode, isdir);
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
		struct inode *hi;
		struct dentry *hd;

		hd = au_h_dptr_i(dentry, bindex);
		if (!hd || !hd->d_inode)
			continue;

		if (iinfo->ii_bstart <= bindex && bindex <= iinfo->ii_bend) {
			hi = au_h_iptr_i(inode, bindex);
			if (hi) {
				if (hi == hd->d_inode)
					continue;
				//Dbg("here\n");
				err = -ESTALE;
				break;
			}
		}
		if (bindex < iinfo->ii_bstart)
			iinfo->ii_bstart = bindex;
		if (iinfo->ii_bend < bindex)
			iinfo->ii_bend = bindex;
		set_h_iptr(inode, bindex, igrab(hd->d_inode), flags);
		update++;
	}

	bend = iinfo->ii_bend;
	p = iinfo->ii_hinode;
	for (bindex = 0; bindex <= bend; bindex++, p++)
		if (p->hi_inode) {
			iinfo->ii_bstart = bindex;
			break;
		}
	p = iinfo->ii_hinode + bend;
	for (bindex = bend; bindex > iinfo->ii_bstart; bindex--, p--)
		if (p->hi_inode) {
			iinfo->ii_bend = bindex;
			break;
		}
	AuDebugOn(iinfo->ii_bstart > bend || iinfo->ii_bend < 0);

	if (unlikely(err))
		goto out;

	if (1 || first != au_h_iptr(inode))
		au_cpup_attr_all(inode);
	if (update && isdir)
		inode->i_version++;
	au_update_iigen(inode);

 out:
	//au_debug_on();
	TraceErr(err);
	//au_debug_off();
	return err;
}

static int set_inode(struct inode *inode, struct dentry *dentry)
{
	int err, isdir;
	struct dentry *hidden_dentry;
	struct inode *hidden_inode;
	umode_t mode;
	aufs_bindex_t bindex, bstart, btail;
	struct aufs_iinfo *iinfo;
	unsigned int flags;

	LKTRTrace("i%lu, %.*s\n", inode->i_ino, DLNPair(dentry));
	AuDebugOn(!(inode->i_state & I_NEW));
	IiMustWriteLock(inode);
	hidden_dentry = au_h_dptr(dentry);
	AuDebugOn(!hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	AuDebugOn(!hidden_inode);

	err = 0;
	isdir = 0;
	bstart = dbstart(dentry);
	mode = hidden_inode->i_mode;
	switch (mode & S_IFMT) {
	case S_IFREG:
		btail = dbtail(dentry);
		break;
	case S_IFDIR:
		isdir = 1;
		btail = dbtaildir(dentry);
		inode->i_op = &aufs_dir_iop;
		inode->i_fop = &aufs_dir_fop;
		break;
	case S_IFLNK:
		btail = dbtail(dentry);
		inode->i_op = &aufs_symlink_iop;
		break;
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
	case S_IFSOCK:
		btail = dbtail(dentry);
		init_special_inode(inode, mode, hidden_inode->i_rdev);
		break;
	default:
		IOErr("Unknown file type 0%o\n", mode);
		err = -EIO;
		goto out;
	}

	flags = au_hi_flags(inode, isdir);
	iinfo = itoii(inode);
	iinfo->ii_bstart = bstart;
	iinfo->ii_bend = btail;
	for (bindex = bstart; bindex <= btail; bindex++) {
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (!hidden_dentry)
			continue;
		AuDebugOn(!hidden_dentry->d_inode);
		set_h_iptr(inode, bindex, igrab(hidden_dentry->d_inode), flags);
	}
	au_cpup_attr_all(inode);

 out:
	TraceErr(err);
	return err;
}

/* successful returns with iinfo write_locked */
//todo: return with unlocked?
static int reval_inode(struct inode *inode, struct dentry *dentry, int *matched)
{
	int err;
	struct inode *h_inode, *h_dinode;
	aufs_bindex_t bindex, bend;
	//const int udba = !au_flag_test(inode->i_sb, AuFlag_UDBA_NONE);

	LKTRTrace("i%lu, %.*s\n", inode->i_ino, DLNPair(dentry));

	*matched = 0;

	/*
	 * before this function, if aufs got any iinfo lock, it must be only
	 * one, the parent dir.
	 * it can happen by UDBA and the obsoleted inode number.
	 */
	err = -EIO;
	if (unlikely(inode->i_ino == parent_ino(dentry)))
		goto out;

	h_dinode = au_h_dptr(dentry)->d_inode;
	hi_lock_child(inode);	// bad name, this is not a hidden inode.
	ii_write_lock_new(inode);
	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		h_inode = au_h_iptr_i(inode, bindex);
		if (h_inode && h_inode == h_dinode) {
			//&& (ibs != bstart || !au_test_higen(inode, h_inode)));
			*matched = 1;
			err = 0;
			if (unlikely(au_iigen(inode) != au_digen(dentry)))
				err = au_refresh_hinode(inode, dentry);
			break;
		}
	}
	i_unlock(inode);
	if (unlikely(err))
		ii_write_unlock(inode);

 out:
	TraceErr(err);
	return err;
}

/* successful returns with iinfo write_locked */
//todo: return with unlocked?
struct inode *au_new_inode(struct dentry *dentry)
{
	struct inode *inode, *h_inode;
	struct dentry *h_dentry;
	ino_t h_ino;
	struct super_block *sb;
	int err, match;
	aufs_bindex_t bstart;
	struct xino xino;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	sb = dentry->d_sb;
	h_dentry = au_h_dptr(dentry);
	AuDebugOn(!h_dentry);
	h_inode = h_dentry->d_inode;
	AuDebugOn(!h_inode);

	bstart = dbstart(dentry);
	h_ino = h_inode->i_ino;
	err = xino_read(sb, bstart, h_ino, &xino);
	//err = -1;
	inode = ERR_PTR(err);
	if (unlikely(err))
		goto out;
 new_ino:
	if (!xino.ino) {
		xino.ino = xino_new_ino(sb);
		if (!xino.ino) {
			inode = ERR_PTR(-EIO);
			goto out;
		}
	}

	LKTRTrace("i%lu\n", xino.ino);
	err = -ENOMEM;
	inode = iget_locked(sb, xino.ino);
	if (unlikely(!inode))
		goto out;
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;
	err = -ENOMEM;
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;

	LKTRTrace("%lx, new %d\n", inode->i_state, !!(inode->i_state & I_NEW));
	if (inode->i_state & I_NEW) {
		sb->s_op->read_inode(inode);
		if (!is_bad_inode(inode)) {
			ii_write_lock_new(inode);
			err = set_inode(inode, dentry);
			//err = -1;
		}
		unlock_new_inode(inode);
		if (!err)
			goto out; /* success */
		ii_write_unlock(inode);
		goto out_iput;
	} else {
		err = reval_inode(inode, dentry, &match);
		if (!err)
			goto out; /* success */
		else if (match)
			goto out_iput;
	}

	Warn1("broken ino, b%d, %.*s, hi%lu, i%lu. Try udba=inotify.\n",
	      bstart, DLNPair(dentry), h_ino, xino.ino);
	xino.ino = 0;
	err = xino_write0(sb, bstart, h_ino);
	if (!err) {
		iput(inode);
		goto new_ino;
	}

 out_iput:
	iput(inode);
	inode = ERR_PTR(err);
 out:
	TraceErrPtr(inode);
	return inode;
}
