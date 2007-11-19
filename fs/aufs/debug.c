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

/* $Id: debug.c,v 1.28 2007/06/04 02:17:34 sfjro Exp $ */

#include "aufs.h"

atomic_t aufs_cond = ATOMIC_INIT(0);

#if defined(CONFIG_LKTR) || defined(CONFIG_LKTR_MODULE)
#define dpri(fmt, arg...) \
	do {if (LktrCond) printk(KERN_DEBUG fmt, ##arg);} while (0)
#else
#define dpri(fmt, arg...)	printk(KERN_DEBUG fmt, ##arg)
#endif

/* ---------------------------------------------------------------------- */

void au_dpri_whlist(struct aufs_nhash *whlist)
{
	int i;
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos;

	for (i = 0; i < AUFS_NHASH_SIZE; i++) {
		head = whlist->heads + i;
		hlist_for_each_entry(tpos, pos, head, wh_hash)
			dpri("b%d, %.*s, %d\n",
			     tpos->wh_bindex,
			     tpos->wh_str.len, tpos->wh_str.name,
			     tpos->wh_str.len);
	}
}

void au_dpri_vdir(struct aufs_vdir *vdir)
{
	int i;
	union aufs_deblk_p p;
	unsigned char *o;

	if (!vdir || IS_ERR(vdir)) {
		dpri("err %ld\n", PTR_ERR(vdir));
		return;
	}

	dpri("nblk %d, deblk %p %d, last{%d, %p}, ver %lu\n",
	     vdir->vd_nblk, vdir->vd_deblk, ksize(vdir->vd_deblk),
	     vdir->vd_last.i, vdir->vd_last.p.p, vdir->vd_version);
	for (i = 0; i < vdir->vd_nblk; i++) {
		p.deblk = vdir->vd_deblk[i];
		o = p.p;
		dpri("[%d]: %p %d\n", i, o, ksize(o));
#if 0 // verbose
		int j;
		for (j = 0; j < 8; j++) {
			dpri("%p(+%d) {%02x %02x %02x %02x %02x %02x %02x %02x "
			     "%02x %02x %02x %02x %02x %02x %02x %02x}\n",
			     p.p, p.p - o,
			     p.p[0], p.p[1], p.p[2], p.p[3],
			     p.p[4], p.p[5], p.p[6], p.p[7],
			     p.p[8], p.p[9], p.p[10], p.p[11],
			     p.p[12], p.p[13], p.p[14], p.p[15]);
			p.p += 16;
		}
#endif
	}
}

static int do_pri_inode(aufs_bindex_t bindex, struct inode *inode)
{
	if (!inode || IS_ERR(inode)) {
		dpri("i%d: err %ld\n", bindex, PTR_ERR(inode));
		return -1;
	}

	/* the type of i_blocks depends upon CONFIG_LSF */
	BUILD_BUG_ON(sizeof(inode->i_blocks) != sizeof(unsigned long)
		     && sizeof(inode->i_blocks) != sizeof(u64));
	dpri("i%d: i%lu, %s, cnt %d, nl %u, 0%o, sz %Lu, blk %Lu,"
	     " ct %Ld, np %lu, st 0x%lx, g %x\n",
	     bindex,
	     inode->i_ino, inode->i_sb ? au_sbtype(inode->i_sb) : "??",
	     atomic_read(&inode->i_count), inode->i_nlink, inode->i_mode,
	     i_size_read(inode), (u64)inode->i_blocks,
	     timespec_to_ns(&inode->i_ctime) & 0x0ffff,
	     inode->i_mapping ? inode->i_mapping->nrpages : 0,
	     inode->i_state, inode->i_generation);
	return 0;
}

void au_dpri_inode(struct inode *inode)
{
	struct aufs_iinfo *iinfo;
	aufs_bindex_t bindex;
	int err;

	err = do_pri_inode(-1, inode);
	if (err || !au_is_aufs(inode->i_sb))
		return;

	iinfo = itoii(inode);
	if (!iinfo)
		return;
	dpri("i-1: bstart %d, bend %d, gen %d\n",
	     iinfo->ii_bstart, iinfo->ii_bend, au_iigen(inode));
	if (iinfo->ii_bstart < 0)
		return;
	for (bindex = iinfo->ii_bstart; bindex <= iinfo->ii_bend; bindex++)
		do_pri_inode(bindex, iinfo->ii_hinode[0 + bindex].hi_inode);
}

static int do_pri_dentry(aufs_bindex_t bindex, struct dentry *dentry)
{
	if (!dentry || IS_ERR(dentry)) {
		dpri("d%d: err %ld\n", bindex, PTR_ERR(dentry));
		return -1;
	}
	dpri("d%d: %.*s/%.*s, %s, cnt %d, flags 0x%x\n",
	     bindex,
	     DLNPair(dentry->d_parent), DLNPair(dentry),
	     dentry->d_sb ? au_sbtype(dentry->d_sb) : "??",
	     atomic_read(&dentry->d_count), dentry->d_flags);
	do_pri_inode(bindex, dentry->d_inode);
	return 0;
}

void au_dpri_dentry(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bindex;
	int err;

	err = do_pri_dentry(-1, dentry);
	if (err || !au_is_aufs(dentry->d_sb))
		return;

	dinfo = dtodi(dentry);
	if (!dinfo)
		return;
	dpri("d-1: bstart %d, bend %d, bwh %d, bdiropq %d, gen %d\n",
	     dinfo->di_bstart, dinfo->di_bend,
	     dinfo->di_bwh, dinfo->di_bdiropq, au_digen(dentry));
	if (dinfo->di_bstart < 0)
		return;
	for (bindex = dinfo->di_bstart; bindex <= dinfo->di_bend; bindex++)
		do_pri_dentry(bindex, dinfo->di_hdentry[0 + bindex].hd_dentry);
}

static int do_pri_file(aufs_bindex_t bindex, struct file *file)
{
	char a[32];

	if (!file || IS_ERR(file)) {
		dpri("f%d: err %ld\n", bindex, PTR_ERR(file));
		return -1;
	}
	a[0] = 0;
	if (bindex == -1 && ftofi(file))
		snprintf(a, sizeof(a), ", mmapped %d", au_is_mmapped(file));
	dpri("f%d: mode 0x%x, flags 0%o, cnt %d, pos %Lu%s\n",
	     bindex, file->f_mode, file->f_flags, file_count(file),
	     file->f_pos, a);
	do_pri_dentry(bindex, file->f_dentry);
	return 0;
}

void au_dpri_file(struct file *file)
{
	struct aufs_finfo *finfo;
	aufs_bindex_t bindex;
	int err;

	err = do_pri_file(-1, file);
	if (err || !file->f_dentry || !au_is_aufs(file->f_dentry->d_sb))
		return;

	finfo = ftofi(file);
	if (!finfo)
		return;
	if (finfo->fi_bstart < 0)
		return;
	for (bindex = finfo->fi_bstart; bindex <= finfo->fi_bend; bindex++) {
		struct aufs_hfile *hf;
		//dpri("bindex %d\n", bindex);
		hf = finfo->fi_hfile + bindex;
		do_pri_file(bindex, hf ? hf->hf_file : NULL);
	}
}

static int do_pri_br(aufs_bindex_t bindex, struct aufs_branch *br)
{
	struct vfsmount *mnt;
	struct super_block *sb;

	if (!br || IS_ERR(br)
	    || !(mnt = br->br_mnt) || IS_ERR(mnt)
	    || !(sb = mnt->mnt_sb) || IS_ERR(sb)) {
		dpri("s%d: err %ld\n", bindex, PTR_ERR(br));
		return -1;
	}

	dpri("s%d: {perm 0x%x, cnt %d}, "
	     "%s, flags 0x%lx, cnt(BIAS) %d, active %d, xino %p %p\n",
	     bindex, br->br_perm, br_count(br),
	     au_sbtype(sb), sb->s_flags, sb->s_count - S_BIAS,
	     atomic_read(&sb->s_active), br->br_xino,
	     br->br_xino ? br->br_xino->f_dentry : NULL);
	return 0;
}

void au_dpri_sb(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	aufs_bindex_t bindex;
	int err;
	struct vfsmount mnt = {.mnt_sb = sb};
	struct aufs_branch fake = {
		.br_perm = 0,
		.br_mnt = &mnt,
		.br_count = ATOMIC_INIT(0),
		.br_xino = NULL
	};

	atomic_set(&fake.br_count, 0);
	err = do_pri_br(-1, &fake);
	dpri("dev 0x%x\n", sb->s_dev);
	if (err || !au_is_aufs(sb))
		return;

	sbinfo = stosi(sb);
	if (!sbinfo)
		return;
	for (bindex = 0; bindex <= sbinfo->si_bend; bindex++) {
		//dpri("bindex %d\n", bindex);
		do_pri_br(bindex, sbinfo->si_branch[0 + bindex]);
	}
}

/* ---------------------------------------------------------------------- */

void au_dbg_sleep(int sec)
{
	static DECLARE_WAIT_QUEUE_HEAD(wq);
	wait_event_timeout(wq, 0, sec * HZ);
}
