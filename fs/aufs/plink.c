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

/* $Id: plink.c,v 1.6 2007/05/27 23:05:15 sfjro Exp $ */

#include "aufs.h"

struct pseudo_link {
	struct list_head list;
	struct inode *inode;
};

#ifdef CONFIG_AUFS_DEBUG
void au_list_plink(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct pseudo_link *plink;

	TraceEnter();
	SiMustAnyLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list)
		Dbg("%lu\n", plink->inode->i_ino);
	spin_unlock(&sbinfo->si_plink_lock);
}
#endif

int au_is_plinked(struct super_block *sb, struct inode *inode)
{
	int found;
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct pseudo_link *plink;

	LKTRTrace("i%lu\n", inode->i_ino);
	SiMustAnyLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_PLINK));

	found = 0;
	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list)
		if (plink->inode == inode) {
			found = 1;
			break;
		}
	spin_unlock(&sbinfo->si_plink_lock);
	return found;
}

// 20 is max digits length of ulong 64
#define PLINK_NAME_LEN	((20 + 1) * 2)

static int plink_name(char *name, int len, struct inode *inode,
		      aufs_bindex_t bindex)
{
	int rlen;
	struct inode *h_inode;

	LKTRTrace("i%lu, b%d\n", inode->i_ino, bindex);
	AuDebugOn(len != PLINK_NAME_LEN);
	h_inode = au_h_iptr_i(inode, bindex);
	AuDebugOn(!h_inode);
	rlen = snprintf(name, len, "%lu.%lu", inode->i_ino, h_inode->i_ino);
	AuDebugOn(rlen >= len);
	return rlen;
}

struct dentry *lkup_plink(struct super_block *sb, aufs_bindex_t bindex,
			  struct inode *inode)
{
	struct dentry *h_dentry, *h_parent;
	struct aufs_branch *br;
	struct inode *h_dir;
	char tgtname[PLINK_NAME_LEN];
	int len;
	struct lkup_args lkup;

	LKTRTrace("b%d, i%lu\n", bindex, inode->i_ino);
	br = stobr(sb, bindex);
	h_parent = br->br_plink;
	AuDebugOn(!h_parent);
	h_dir = h_parent->d_inode;
	AuDebugOn(!h_dir);

	len = plink_name(tgtname, sizeof(tgtname), inode, bindex);

	// always superio.
	lkup.nfsmnt = au_do_nfsmnt(br->br_mnt);
	lkup.dlgt = need_dlgt(sb);
	hi_lock_whplink(h_dir);
	h_dentry = sio_lkup_one(tgtname, h_parent, len, &lkup);
	i_unlock(h_dir);
	return h_dentry;
}

static int do_whplink(char *tgt, int len, struct dentry *h_parent,
		      struct dentry *h_dentry, struct vfsmount *nfsmnt,
		      struct super_block *sb)
{
	int err;
	struct dentry *h_tgt;
	struct inode *h_dir;
	struct lkup_args lkup = {
		.nfsmnt = nfsmnt,
		.dlgt	= need_dlgt(sb)
	};

	h_tgt = lkup_one(tgt, h_parent, len, &lkup);
	err = PTR_ERR(h_tgt);
	if (IS_ERR(h_tgt))
		goto out;

	err = 0;
	h_dir = h_parent->d_inode;
	if (unlikely(h_tgt->d_inode && h_tgt->d_inode != h_dentry->d_inode))
		err = vfsub_unlink(h_dir, h_tgt, lkup.dlgt);
	if (!err && !h_tgt->d_inode) {
		err = vfsub_link(h_dentry, h_dir, h_tgt, lkup.dlgt);
		//inode->i_nlink++;
	}
	dput(h_tgt);

 out:
	TraceErr(err);
	return err;
}

struct do_whplink_args {
	int *errp;
	char *tgt;
	int len;
	struct dentry *h_parent;
	struct dentry *h_dentry;
	struct vfsmount *nfsmnt;
	struct super_block *sb;
};

static void call_do_whplink(void *args)
{
	struct do_whplink_args *a = args;
	*a->errp = do_whplink(a->tgt, a->len, a->h_parent, a->h_dentry,
			      a->nfsmnt, a->sb);
}

static int whplink(struct dentry *h_dentry, struct inode *inode,
		   aufs_bindex_t bindex, struct super_block *sb)
{
	int err, len, wkq_err;
	struct aufs_branch *br;
	struct dentry *h_parent;
	struct inode *h_dir;
	char tgtname[PLINK_NAME_LEN];

	LKTRTrace("%.*s\n", DLNPair(h_dentry));
	br = stobr(inode->i_sb, bindex);
	h_parent = br->br_plink;
	AuDebugOn(!h_parent);
	h_dir = h_parent->d_inode;
	AuDebugOn(!h_dir);

	len = plink_name(tgtname, sizeof(tgtname), inode, bindex);

	// always superio.
	hi_lock_whplink(h_dir);
	if (!is_au_wkq(current)) {
		struct do_whplink_args args = {
			.errp		= &err,
			.tgt		= tgtname,
			.len		= len,
			.h_parent	= h_parent,
			.h_dentry	= h_dentry,
			.nfsmnt		= au_do_nfsmnt(br->br_mnt),
			.sb		= sb
		};
		wkq_err = au_wkq_wait(call_do_whplink, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	} else
		err = do_whplink(tgtname, len, h_parent, h_dentry,
				 au_do_nfsmnt(br->br_mnt), sb);
	i_unlock(h_dir);

	TraceErr(err);
	return err;
}

void append_plink(struct super_block *sb, struct inode *inode,
		  struct dentry *h_dentry, aufs_bindex_t bindex)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct pseudo_link *plink;
	int found, err, cnt;

	LKTRTrace("i%lu\n", inode->i_ino);
	SiMustAnyLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_PLINK));

	cnt = 0;
	found = 0;
	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list) {
		cnt++;
		if (plink->inode == inode) {
			found = 1;
			break;
		}
	}

	err = 0;
	if (!found) {
		plink = kmalloc(sizeof(*plink), GFP_ATOMIC);
		if (plink) {
			plink->inode = igrab(inode);
			list_add(&plink->list, plink_list);
			cnt++;
		} else
			err = -ENOMEM;
	}
	spin_unlock(&sbinfo->si_plink_lock);

	if (!err)
		err = whplink(h_dentry, inode, bindex, sb);

	if (unlikely(cnt > 100))
		Warn1("unexpectedly many pseudo links, %d\n", cnt);
	if (unlikely(err))
		Warn("err %d, damaged pseudo link. ignored.\n", err);
}

static void do_put_plink(struct pseudo_link *plink, int do_del)
{
	TraceEnter();

	iput(plink->inode);
	if (do_del)
		list_del(&plink->list);
	kfree(plink);
}

void au_put_plink(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct pseudo_link *plink, *tmp;

	TraceEnter();
	SiMustWriteLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	//spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry_safe(plink, tmp, plink_list, list)
		do_put_plink(plink, 0);
	INIT_LIST_HEAD(plink_list);
	//spin_unlock(&sbinfo->si_plink_lock);
}

void half_refresh_plink(struct super_block *sb, aufs_bindex_t br_id)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct pseudo_link *plink, *tmp;
	struct inode *inode;
	aufs_bindex_t bstart, bend, bindex;
	int do_put;

	TraceEnter();
	SiMustWriteLock(sb);
	AuDebugOn(!au_flag_test(sb, AuFlag_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	//spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry_safe(plink, tmp, plink_list, list) {
		do_put = 0;
		inode = igrab(plink->inode);
		ii_write_lock_child(inode);
		bstart = ibstart(inode);
		bend = ibend(inode);
		if (bstart >= 0) {
			for (bindex = bstart; bindex <= bend; bindex++) {
				if (!au_h_iptr_i(inode, bindex)
				    || itoid_index(inode, bindex) != br_id)
					continue;
				set_h_iptr(inode, bindex, NULL, 0);
				do_put = 1;
				break;
			}
		} else
			do_put_plink(plink, 1);

		if (do_put) {
			for (bindex = bstart; bindex <= bend; bindex++)
				if (au_h_iptr_i(inode, bindex)) {
					do_put = 0;
					break;
				}
			if (do_put)
				do_put_plink(plink, 1);
		}
		ii_write_unlock(inode);
		iput(inode);
	}
	//spin_unlock(&sbinfo->si_plink_lock);
}
