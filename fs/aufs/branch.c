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

/* $Id: branch.c,v 1.51 2007/06/04 02:15:32 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

static void free_branch(struct aufs_branch *br)
{
	TraceEnter();

	if (br->br_xino)
		fput(br->br_xino);
	dput(br->br_wh);
	dput(br->br_plink);
	mntput(br->br_mnt);
	AuDebugOn(br_count(br) || atomic_read(&br->br_wh_running));
	kfree(br);
}

/*
 * frees all branches
 */
void free_branches(struct aufs_sbinfo *sbinfo)
{
	aufs_bindex_t bmax;
	struct aufs_branch **br;

	TraceEnter();
	bmax = sbinfo->si_bend + 1;
	br = sbinfo->si_branch;
	while (bmax--)
		free_branch(*br++);
}

/*
 * find the index of a branch which is specified by @br_id.
 */
int find_brindex(struct super_block *sb, aufs_bindex_t br_id)
{
	aufs_bindex_t bindex, bend;

	TraceEnter();

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++)
		if (sbr_id(sb, bindex) == br_id)
			return bindex;
	return -1;
}

/*
 * test if the @br is readonly or not.
 */
int br_rdonly(struct aufs_branch *br)
{
	return ((br->br_mnt->mnt_sb->s_flags & MS_RDONLY)
		|| !br_writable(br->br_perm))
		? -EROFS : 0;
}

/*
 * returns writable branch index, otherwise an error.
 * todo: customizable writable-branch-policy
 */
static int find_rw_parent(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;
	aufs_bindex_t bindex, candidate;
	struct super_block *sb;
	struct dentry *parent, *hidden_parent;

	err = bend;
	sb = dentry->d_sb;
	parent = dget_parent(dentry);
#if 1 // branch policy
	hidden_parent = au_h_dptr_i(parent, bend);
	if (hidden_parent && !br_rdonly(stobr(sb, bend)))
		goto out; /* success */
#endif

	candidate = -1;
	for (bindex = dbstart(parent); bindex <= bend; bindex++) {
		hidden_parent = au_h_dptr_i(parent, bindex);
		if (hidden_parent && !br_rdonly(stobr(sb, bindex))) {
#if 0 // branch policy
			if (candidate == -1)
				candidate = bindex;
			if (!au_test_perm(hidden_parent->d_inode, MAY_WRITE))
				return bindex;
#endif
			err = bindex;
			goto out; /* success */
		}
	}
#if 0 // branch policy
	err = candidate;
	if (candidate != -1)
		goto out; /* success */
#endif
	err = -EROFS;

 out:
	dput(parent);
	return err;
}

int find_rw_br(struct super_block *sb, aufs_bindex_t bend)
{
	aufs_bindex_t bindex;

	for (bindex = bend; bindex >= 0; bindex--)
		if (!br_rdonly(stobr(sb, bindex)))
			return bindex;
	return -EROFS;
}

int find_rw_parent_br(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;

	err = find_rw_parent(dentry, bend);
	if (err >= 0)
		return err;
	return find_rw_br(dentry->d_sb, bend);
}

/* ---------------------------------------------------------------------- */

/*
 * test if two hidden_dentries have overlapping branches.
 */
static int do_is_overlap(struct super_block *sb, struct dentry *hidden_d1,
			 struct dentry *hidden_d2)
{
	int err;

	LKTRTrace("%.*s, %.*s\n", DLNPair(hidden_d1), DLNPair(hidden_d2));

	err = au_is_subdir(hidden_d1, hidden_d2);
	TraceErr(err);
	return err;
}

#if defined(CONFIG_BLK_DEV_LOOP) || defined(CONFIG_BLK_DEV_LOOP_MODULE)
#include <linux/loop.h>
static int is_overlap_loopback(struct super_block *sb, struct dentry *hidden_d1,
			       struct dentry *hidden_d2)
{
	struct inode *hidden_inode;
	struct loop_device *l;

	hidden_inode = hidden_d1->d_inode;
	if (MAJOR(hidden_inode->i_sb->s_dev) != LOOP_MAJOR)
		return 0;

	l = hidden_inode->i_sb->s_bdev->bd_disk->private_data;
	hidden_d1 = l->lo_backing_file->f_dentry;
	if (unlikely(hidden_d1->d_sb == sb))
		return 1;
	return do_is_overlap(sb, hidden_d1, hidden_d2);
}
#else
#define is_overlap_loopback(sb, hidden_d1, hidden_d2) 0
#endif

static int is_overlap(struct super_block *sb, struct dentry *hidden_d1,
		      struct dentry *hidden_d2)
{
	LKTRTrace("d1 %.*s, d2 %.*s\n", DLNPair(hidden_d1), DLNPair(hidden_d2));
	if (unlikely(hidden_d1 == hidden_d2))
		return 1;
	return do_is_overlap(sb, hidden_d1, hidden_d2)
		|| do_is_overlap(sb, hidden_d2, hidden_d1)
		|| is_overlap_loopback(sb, hidden_d1, hidden_d2)
		|| is_overlap_loopback(sb, hidden_d2, hidden_d1);
}

/* ---------------------------------------------------------------------- */

static int init_br_wh(struct super_block *sb, aufs_bindex_t bindex,
		      struct aufs_branch *br, int new_perm,
		      struct dentry *h_root, struct vfsmount *h_mnt)
{
	int err, old_perm;
	struct inode *dir = sb->s_root->d_inode,
		*h_dir = h_root->d_inode;
	const int new = (bindex < 0);

	LKTRTrace("b%d, new_perm %d\n", bindex, new_perm);

	if (new)
		hi_lock_parent(h_dir);
	else
		hdir_lock(h_dir, dir, bindex);

	br_wh_write_lock(br);
	old_perm = br->br_perm;
	br->br_perm = new_perm;
	err = init_wh(h_root, br, au_do_nfsmnt(h_mnt), sb);
	br->br_perm = old_perm;
	br_wh_write_unlock(br);

	if (new)
		i_unlock(h_dir);
	else
		hdir_unlock(h_dir, dir, bindex);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * returns a newly allocated branch. @new_nbranch is a number of branches
 * after adding a branch.
 */
static struct aufs_branch *alloc_addbr(struct super_block *sb, int new_nbranch)
{
	struct aufs_branch **branchp, *add_branch;
	int sz;
	void *p;
	struct dentry *root;
	struct inode *inode;
	struct aufs_hinode *hinodep;
	struct aufs_hdentry *hdentryp;

	LKTRTrace("new_nbranch %d\n", new_nbranch);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	add_branch = kmalloc(sizeof(*add_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(add_branch); add_branch = NULL;}
	if (unlikely(!add_branch))
		goto out;

	sz = sizeof(*branchp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*branchp);
	p = stosi(sb)->si_branch;
	branchp = au_kzrealloc(p, sz, sizeof(*branchp) * new_nbranch,
			       GFP_KERNEL);
	//if (LktrCond) branchp = NULL;
	if (unlikely(!branchp))
		goto out;
	stosi(sb)->si_branch = branchp;

	sz = sizeof(*hdentryp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hdentryp);
	p = dtodi(root)->di_hdentry;
	hdentryp = au_kzrealloc(p, sz, sizeof(*hdentryp) * new_nbranch,
				GFP_KERNEL);
	//if (LktrCond) hdentryp = NULL;
	if (unlikely(!hdentryp))
		goto out;
	dtodi(root)->di_hdentry = hdentryp;

	sz = sizeof(*hinodep) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hinodep);
	p = itoii(inode)->ii_hinode;
	hinodep = au_kzrealloc(p, sz, sizeof(*hinodep) * new_nbranch,
			       GFP_KERNEL);
	//if (LktrCond) hinodep = NULL; // unavailable test
	if (unlikely(!hinodep))
		goto out;
	itoii(inode)->ii_hinode = hinodep;
	return add_branch; /* success */

 out:
	kfree(add_branch);
	TraceErr(-ENOMEM);
	return ERR_PTR(-ENOMEM);
}

/*
 * test if the branch permission is legal or not.
 */
static int test_br(struct super_block *sb, struct inode *inode, int brperm,
		   char *path)
{
	int err;

	err = 0;
	if (unlikely(br_writable(brperm) && IS_RDONLY(inode))) {
		Err("write permission for readonly fs or inode, %s\n", path);
		err = -EINVAL;
	}

	TraceErr(err);
	return err;
}

/*
 * retunrs,,,
 * 0: success, the caller will add it
 * plus: success, it is already unified, the caller should ignore it
 * minus: error
 */
static int test_add(struct super_block *sb, struct opt_add *add, int remount)
{
	int err;
	struct dentry *root;
	struct inode *inode, *hidden_inode;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%s, remo%d\n", add->path, remount);

	root = sb->s_root;
	if (unlikely(au_find_dbindex(root, add->nd.dentry) != -1)) {
		err = 1;
		if (!remount) {
			err = -EINVAL;
			Err("%s duplicated\n", add->path);
		}
		goto out;
	}

	err = -ENOSPC; //-E2BIG;
	bend = sbend(sb);
	//if (LktrCond) bend = AUFS_BRANCH_MAX;
	if (unlikely(AUFS_BRANCH_MAX <= add->bindex
		     || AUFS_BRANCH_MAX - 1 <= bend)) {
		Err("number of branches exceeded %s\n", add->path);
		goto out;
	}

	err = -EDOM;
	if (unlikely(add->bindex < 0 || bend + 1 < add->bindex)) {
		Err("bad index %d\n", add->bindex);
		goto out;
	}

	inode = add->nd.dentry->d_inode;
	AuDebugOn(!inode || !S_ISDIR(inode->i_mode));
	err = -ENOENT;
	if (unlikely(!inode->i_nlink)) {
		Err("no existence %s\n", add->path);
		goto out;
	}

	err = -EINVAL;
	if (unlikely(inode->i_sb == sb)) {
		Err("%s must be outside\n", add->path);
		goto out;
	}

#if 1 //ndef CONFIG_AUFS_ROBR
	if (unlikely(au_is_aufs(inode->i_sb)
		     || !strcmp(au_sbtype(inode->i_sb), "unionfs"))) {
		Err("nested " AUFS_NAME " %s\n", add->path);
		goto out;
	}
#endif

#ifdef AuNoNfsBranch
	if (unlikely(au_is_nfs(inode->i_sb))) {
		Err(AuNoNfsBranchMsg ". %s\n", add->path);
		goto out;
	}
#endif

	err = test_br(sb, add->nd.dentry->d_inode, add->perm, add->path);
	if (unlikely(err))
		goto out;

	if (unlikely(bend == -1))
		return 0; /* success */

	hidden_inode = au_h_dptr(root)->d_inode;
	if (unlikely(au_flag_test(sb, AuFlag_WARN_PERM)
		     && ((hidden_inode->i_mode & S_IALLUGO)
			 != (inode->i_mode & S_IALLUGO)
			 || hidden_inode->i_uid != inode->i_uid
			 || hidden_inode->i_gid != inode->i_gid)))
		Warn("uid/gid/perm %s %u/%u/0%o, %u/%u/0%o\n",
		     add->path,
		     inode->i_uid, inode->i_gid, (inode->i_mode & S_IALLUGO),
		     hidden_inode->i_uid, hidden_inode->i_gid,
		     (hidden_inode->i_mode & S_IALLUGO));

	err = -EINVAL;
	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(is_overlap(sb, add->nd.dentry,
					au_h_dptr_i(root, bindex)))) {
			Err("%s is overlapped\n", add->path);
			goto out;
		}
	err = 0;

 out:
	TraceErr(err);
	return err;
}

int br_add(struct super_block *sb, struct opt_add *add, int remount)
{
	int err, sz;
	aufs_bindex_t bend, add_bindex;
	struct dentry *root;
	struct aufs_iinfo *iinfo;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct inode *root_inode;
	unsigned long long maxb;
	struct aufs_branch **branchp, *add_branch;
	struct aufs_hdentry *hdentryp;
	struct aufs_hinode *hinodep;

	LKTRTrace("b%d, %s, 0x%x, %.*s\n", add->bindex, add->path,
		  add->perm, DLNPair(add->nd.dentry));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	root_inode = root->d_inode;
	IMustLock(root_inode);
	IiMustWriteLock(root_inode);

	err = test_add(sb, add, remount);
	if (unlikely(err < 0))
		goto out;
	if (unlikely(err))
		return 0; /* success */

	bend = sbend(sb);
	add_branch = alloc_addbr(sb, bend + 2);
	err = PTR_ERR(add_branch);
	if (IS_ERR(add_branch))
		goto out;

	err = 0;
	rw_init_nolock(&add_branch->br_wh_rwsem);
	add_branch->br_wh = add_branch->br_plink = NULL;
	if (unlikely(br_writable(add->perm))) {
		err = init_br_wh(sb, /*bindex*/-1, add_branch, add->perm,
				 add->nd.dentry, add->nd.mnt);
		if (unlikely(err)) {
			kfree(add_branch);
			goto out;
		}
	}
	add_branch->br_xino = NULL;
	add_branch->br_mnt = mntget(add->nd.mnt);
	atomic_set(&add_branch->br_wh_running, 0);
	add_branch->br_id = new_br_id(sb);
	add_branch->br_perm = add->perm;
	atomic_set(&add_branch->br_count, 0);

	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(root_inode);

	add_bindex = add->bindex;
	sz = sizeof(*(sbinfo->si_branch)) * (bend + 1 - add_bindex);
	branchp = sbinfo->si_branch + add_bindex;
	memmove(branchp + 1, branchp, sz);
	*branchp = add_branch;
	sz = sizeof(*hdentryp) * (bend + 1 - add_bindex);
	hdentryp = dinfo->di_hdentry + add_bindex;
	memmove(hdentryp + 1, hdentryp, sz);
	hdentryp->hd_dentry = NULL;
	sz = sizeof(*hinodep) * (bend + 1 - add_bindex);
	hinodep = iinfo->ii_hinode + add_bindex;
	memmove(hinodep + 1, hinodep, sz);
	hinodep->hi_inode = NULL;
	hinodep->hi_notify = NULL;

	sbinfo->si_bend++;
	dinfo->di_bend++;
	iinfo->ii_bend++;
	if (unlikely(bend == -1)) {
		dinfo->di_bstart = 0;
		iinfo->ii_bstart = 0;
	}
	set_h_dptr(root, add_bindex, dget(add->nd.dentry));
	set_h_iptr(root_inode, add_bindex, igrab(add->nd.dentry->d_inode), 0);
	if (!add_bindex)
		au_cpup_attr_all(root_inode);
	else
		au_add_nlink(root_inode, add->nd.dentry->d_inode);
	maxb = add->nd.dentry->d_sb->s_maxbytes;
	if (sb->s_maxbytes < maxb)
		sb->s_maxbytes = maxb;

	if (au_flag_test(sb, AuFlag_XINO)) {
		struct file *base_file = stobr(sb, 0)->br_xino;
		if (!add_bindex)
			base_file = stobr(sb, 1)->br_xino;
		err = xino_init(sb, add_bindex, base_file, /*do_test*/1);
		if (unlikely(err)) {
			AuDebugOn(add_branch->br_xino);
			Err("ignored xino err %d, force noxino\n", err);
			err = 0;
			au_flag_clr(sb, AuFlag_XINO);
		}
	}

 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * test if the branch is deletable or not.
 */
static int test_children_busy(struct dentry *root, aufs_bindex_t bindex)
{
	int err, i, j, ndentry, sigen;
	struct au_dcsub_pages dpages;

	LKTRTrace("b%d\n", bindex);
	SiMustWriteLock(root->d_sb);
	DiMustWriteLock(root);

	err = au_dpages_init(&dpages, GFP_KERNEL);
	if (unlikely(err))
		goto out;
	err = au_dcsub_pages(&dpages, root, NULL, NULL);
	if (unlikely(err))
		goto out_dpages;

	sigen = au_sigen(root->d_sb);
	DiMustNoWaiters(root);
	IiMustNoWaiters(root->d_inode);
	di_write_unlock(root);
	for (i = 0; !err && i < dpages.ndpage; i++) {
		struct au_dpage *dpage;
		dpage = dpages.dpages + i;
		ndentry = dpage->ndentry;
		for (j = 0; !err && j < ndentry; j++) {
			struct dentry *d;

			d = dpage->dentries[j];
			if (au_digen(d) == sigen)
				di_read_lock_child(d, AUFS_I_RLOCK);
			else {
				di_write_lock_child(d);
				err = au_reval_dpath(d, sigen);
				if (!err)
					di_downgrade_lock(d, AUFS_I_RLOCK);
				else {
					di_write_unlock(d);
					break;
				}
			}

			if (au_h_dptr_i(d, bindex)
			    && (!S_ISDIR(d->d_inode->i_mode)
				|| dbstart(d) == dbend(d)))
				err = -EBUSY;
			di_read_unlock(d, AUFS_I_RLOCK);
			if (err)
				LKTRTrace("%.*s\n", DLNPair(d));
		}
	}
	di_write_lock_child(root); /* aufs_write_lock() calls ..._child() */

 out_dpages:
	au_dpages_free(&dpages);
 out:
	TraceErr(err);
	return err;
}

int br_del(struct super_block *sb, struct opt_del *del, int remount)
{
	int err, do_wh, rerr;
	struct dentry *root;
	struct inode *inode, *hidden_dir;
	aufs_bindex_t bindex, bend, br_id;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct aufs_iinfo *iinfo;
	struct aufs_branch *br;

	LKTRTrace("%s, %.*s\n", del->path, DLNPair(del->h_root));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	bindex = au_find_dbindex(root, del->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			return 0; /* success */
		err = -ENOENT;
		Err("%s no such branch\n", del->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	err = -EBUSY;
	bend = sbend(sb);
	br = stobr(sb, bindex);
	if (unlikely(!bend || br_count(br))) {
		LKTRTrace("bend %d, br_count %d\n", bend, br_count(br));
		goto out;
	}

	do_wh = 0;
	hidden_dir = del->h_root->d_inode;
	if (unlikely(br->br_wh || br->br_plink)) {
#if 0
		/* remove whiteout base */
		err = init_br_wh(sb, bindex, br, AuBr_RO, del->h_root,
				 br->br_mnt);
		if (unlikely(err))
			goto out;
#else
		dput(br->br_wh);
		dput(br->br_plink);
		br->br_wh = br->br_plink = NULL;
#endif
		do_wh = 1;
	}

	err = test_children_busy(root, bindex);
	if (unlikely(err)) {
		if (unlikely(do_wh))
			goto out_wh;
		goto out;
	}

	err = 0;
	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(inode);

	dput(au_h_dptr_i(root, bindex));
	aufs_hiput(iinfo->ii_hinode + bindex);
	br_id = br->br_id;
	free_branch(br);

	//todo: realloc and shrink memeory
	if (bindex < bend) {
		const aufs_bindex_t n = bend - bindex;
		struct aufs_branch **brp;
		struct aufs_hdentry *hdp;
		struct aufs_hinode *hip;

		brp = sbinfo->si_branch + bindex;
		memmove(brp, brp + 1, sizeof(*brp) * n);
		hdp = dinfo->di_hdentry + bindex;
		memmove(hdp, hdp + 1, sizeof(*hdp) * n);
		hip = iinfo->ii_hinode + bindex;
		memmove(hip, hip + 1, sizeof(*hip) * n);
	}
	sbinfo->si_branch[0 + bend] = NULL;
	dinfo->di_hdentry[0 + bend].hd_dentry = NULL;
	iinfo->ii_hinode[0 + bend].hi_inode = NULL;
	iinfo->ii_hinode[0 + bend].hi_notify = NULL;

	sbinfo->si_bend--;
	dinfo->di_bend--;
	iinfo->ii_bend--;
	if (!bindex)
		au_cpup_attr_all(inode);
	else
		au_sub_nlink(inode, del->h_root->d_inode);
	if (au_flag_test(sb, AuFlag_PLINK))
		half_refresh_plink(sb, br_id);

	if (sb->s_maxbytes == del->h_root->d_sb->s_maxbytes) {
		bend--;
		sb->s_maxbytes = 0;
		for (bindex = 0; bindex <= bend; bindex++) {
			unsigned long long maxb;
			maxb = sbr_sb(sb, bindex)->s_maxbytes;
			if (sb->s_maxbytes < maxb)
				sb->s_maxbytes = maxb;
		}
	}
	goto out; /* success */

 out_wh:
	/* revert */
	rerr = init_br_wh(sb, bindex, br, br->br_perm, del->h_root, br->br_mnt);
	if (rerr)
		Warn("failed re-creating base whiteout, %s. (%d)\n",
		     del->path, rerr);
 out:
	TraceErr(err);
	return err;
}

static int do_need_sigen_inc(int a, int b)
{
	return (br_whable(a) && !br_whable(b));
}

static int need_sigen_inc(int old, int new)
{
	return (do_need_sigen_inc(old, new)
		|| do_need_sigen_inc(new, old));
}

int br_mod(struct super_block *sb, struct opt_mod *mod, int remount,
	   int *do_update)
{
	int err;
	struct dentry *root;
	aufs_bindex_t bindex;
	struct aufs_branch *br;
	struct inode *hidden_dir;

	LKTRTrace("%s, %.*s, 0x%x\n",
		  mod->path, DLNPair(mod->h_root), mod->perm);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	IiMustWriteLock(root->d_inode);

	bindex = au_find_dbindex(root, mod->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			return 0; /* success */
		err = -ENOENT;
		Err("%s no such branch\n", mod->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	hidden_dir = mod->h_root->d_inode;
	err = test_br(sb, hidden_dir, mod->perm, mod->path);
	if (unlikely(err))
		goto out;

	br = stobr(sb, bindex);
	if (unlikely(br->br_perm == mod->perm))
		return 0; /* success */

	if (br_writable(br->br_perm)) {
#if 1
		/* remove whiteout base */
		//todo: mod->perm?
		err = init_br_wh(sb, bindex, br, AuBr_RO, mod->h_root,
				 br->br_mnt);
		if (unlikely(err))
			goto out;
#else
		dput(br->br_wh);
		dput(br->br_plink);
		br->br_wh = br->br_plink = NULL;
#endif

		if (!br_writable(mod->perm)) {
			/* rw --> ro, file might be mmapped */
			struct file *file, *hf;

#if 1 // test here
			DiMustNoWaiters(root);
			IiMustNoWaiters(root->d_inode);
			di_write_unlock(root);

			// no need file_list_lock() since sbinfo is locked
			//file_list_lock();
			list_for_each_entry(file, &sb->s_files, f_u.fu_list) {
				LKTRTrace("%.*s\n", DLNPair(file->f_dentry));
				fi_read_lock(file);
				if (!S_ISREG(file->f_dentry->d_inode->i_mode)
				    || !(file->f_mode & FMODE_WRITE)
				    || fbstart(file) != bindex) {
					FiMustNoWaiters(file);
					fi_read_unlock(file);
					continue;
				}

				// todo: already flushed?
				hf = au_h_fptr(file);
				hf->f_flags = au_file_roflags(hf->f_flags);
				hf->f_mode &= ~FMODE_WRITE;
				FiMustNoWaiters(file);
				fi_read_unlock(file);
			}
			//file_list_unlock();

			/* aufs_write_lock() calls ..._child() */
			di_write_lock_child(root);
#endif
		}
	}

	*do_update |= need_sigen_inc(br->br_perm, mod->perm);
	br->br_perm = mod->perm;
	return err; /* success */

 out:
	TraceErr(err);
	return err;
}
