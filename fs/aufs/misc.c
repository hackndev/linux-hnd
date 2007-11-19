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

/* $Id: misc.c,v 1.33 2007/06/04 02:17:35 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
//#include <linux/mm.h>
//#include <asm/uaccess.h>
#include "aufs.h"

void *au_kzrealloc(void *p, unsigned int nused, unsigned int new_sz, gfp_t gfp)
{
	void *q;

	LKTRTrace("p %p, nused %d, sz %d, ksize %d\n",
		  p, nused, new_sz, ksize(p));
	AuDebugOn(new_sz <= 0);
	if (new_sz <= nused)
		return p;
	if (new_sz <= ksize(p)) {
		memset(p + nused, 0, new_sz - nused);
		return p;
	}

	q = kmalloc(new_sz, gfp);
	//q = NULL;
	if (unlikely(!q))
		return NULL;
	memcpy(q, p, nused);
	memset(q + nused, 0, new_sz - nused);
	//smp_mb();
	kfree(p);
	return q;
}

/* ---------------------------------------------------------------------- */

// todo: make it inline
struct nameidata *fake_dm(struct nameidata *fake_nd, struct nameidata *nd,
			  struct super_block *sb, aufs_bindex_t bindex)
{
	LKTRTrace("nd %p, b%d\n", nd, bindex);

	if (!nd)
		return NULL;

	fake_nd->dentry = NULL;
	fake_nd->mnt = NULL;

#ifndef CONFIG_AUFS_FAKE_DM
	DiMustAnyLock(nd->dentry);

	if (bindex <= dbend(nd->dentry))
		fake_nd->dentry = au_h_dptr_i(nd->dentry, bindex);
	if (fake_nd->dentry) {
		dget(fake_nd->dentry);
		fake_nd->mnt = sbr_mnt(sb, bindex);
		AuDebugOn(!fake_nd->mnt);
		mntget(fake_nd->mnt);
	} else
		fake_nd = ERR_PTR(-ENOENT);
#endif

	TraceErrPtr(fake_nd);
	return fake_nd;
}

void fake_dm_release(struct nameidata *fake_nd)
{
#ifndef CONFIG_AUFS_FAKE_DM
	if (fake_nd) {
		mntput(fake_nd->mnt);
		dput(fake_nd->dentry);
	}
#endif
}

/* ---------------------------------------------------------------------- */

int au_copy_file(struct file *dst, struct file *src, loff_t len,
		 struct super_block *sb, int *sparse)
{
	int err, all_zero, dlgt;
	unsigned long blksize;
	char *buf;
	/* reduce stack space */
	struct iattr *ia;

	LKTRTrace("%.*s, %.*s\n",
		  DLNPair(dst->f_dentry), DLNPair(src->f_dentry));
	AuDebugOn(!(dst->f_mode & FMODE_WRITE));
#ifdef CONFIG_AUFS_DEBUG
	{
		struct dentry *parent;
		parent = dget_parent(dst->f_dentry);
		IMustLock(parent->d_inode);
		dput(parent);
	}
#endif

	err = -ENOMEM;
	blksize = dst->f_dentry->d_sb->s_blocksize;
	if (!blksize || PAGE_SIZE < blksize)
		blksize = PAGE_SIZE;
	LKTRTrace("blksize %lu\n", blksize);
	buf = kmalloc(blksize, GFP_KERNEL);
	//buf = NULL;
	if (unlikely(!buf))
		goto out;
	ia = kmalloc(sizeof(*ia), GFP_KERNEL);
	if (unlikely(!ia))
		goto out_buf;

	dlgt = need_dlgt(sb);
	err = all_zero = 0;
	dst->f_pos = src->f_pos = 0;
	while (len) {
		size_t sz, rbytes, wbytes, i;
		char *p;

		LKTRTrace("len %lld\n", len);
		sz = blksize;
		if (len < blksize)
			sz = len;

		/* support LSM and notify */
		rbytes = 0;
		while (!rbytes || err == -EAGAIN || err == -EINTR)
			err = rbytes = vfsub_read_k(src, buf, sz, &src->f_pos,
						    dlgt);
		if (unlikely(err < 0))
			break;

		all_zero = 0;
		if (len >= rbytes && rbytes == blksize) {
			all_zero = 1;
			p = buf;
			for (i = 0; all_zero && i < rbytes; i++)
				all_zero = !*p++;
		}
		if (!all_zero) {
			wbytes = rbytes;
			p = buf;
			while (wbytes) {
				size_t b;
				/* support LSM and notify */
				err = b = vfsub_write_k(dst, p, wbytes,
							&dst->f_pos, dlgt);
				if (unlikely(err == -EAGAIN || err == -EINTR))
					continue;
				if (unlikely(err < 0))
					break;
				wbytes -= b;
				p += b;
			}
		} else {
			loff_t res;
			LKTRLabel(hole);
			*sparse = 1;
			err = res = vfsub_llseek(dst, rbytes, SEEK_CUR);
			if (unlikely(res < 0))
				break;
		}
		len -= rbytes;
		err = 0;
	}

	/* the last block may be a hole */
	if (unlikely(!err && all_zero)) {
		struct dentry *h_d = dst->f_dentry;
		struct inode *h_i = h_d->d_inode;

		LKTRLabel(last hole);
		do {
			err = vfsub_write_k(dst, "\0", 1, &dst->f_pos, dlgt);
		} while (err == -EAGAIN || err == -EINTR);
		if (err == 1) {
			ia->ia_size = dst->f_pos;
			ia->ia_valid = ATTR_SIZE | ATTR_FILE;
			ia->ia_file = dst;
			hi_lock_child2(h_i);
			err = vfsub_notify_change(h_d, ia, dlgt);
			i_unlock(h_i);
		}
	}

	kfree(ia);
 out_buf:
	kfree(buf);
 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int test_ro(struct super_block *sb, aufs_bindex_t bindex, struct inode *inode)
{
	int err;

	err = br_rdonly(stobr(sb, bindex));
	if (!err && inode) {
		struct inode *hi = au_h_iptr_i(inode, bindex);
		if (hi)
			err = IS_IMMUTABLE(hi) ? -EROFS : 0;
	}
	return err;
}

int au_test_perm(struct inode *hidden_inode, int mask, int dlgt)
{
	if (!current->fsuid)
		return 0;
	if (unlikely(au_is_nfs(hidden_inode->i_sb)
		     && (mask & MAY_WRITE)
		     && S_ISDIR(hidden_inode->i_mode)))
		mask |= MAY_READ; /* force permission check */
	return vfsub_permission(hidden_inode, mask, NULL, dlgt);
}
