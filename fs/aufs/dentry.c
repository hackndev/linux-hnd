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

/* $Id: dentry.c,v 1.44 2007/06/04 02:17:34 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

#ifdef CONFIG_AUFS_LHASH_PATCH

#ifdef CONFIG_AUFS_DLGT
struct lookup_hash_args {
	struct dentry **errp;
	struct qstr *name;
	struct dentry *base;
	struct nameidata *nd;
};

static void call_lookup_hash(void *args)
{
	struct lookup_hash_args *a = args;
	*a->errp = __lookup_hash(a->name, a->base, a->nd);
}
#endif /* CONFIG_AUFS_DLGT */

static struct dentry *lkup_hash(const char *name, struct dentry *parent,
				int len, struct lkup_args *lkup)
{
	struct dentry *dentry;
	char *p;
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	struct nameidata tmp_nd;

	dentry = ERR_PTR(-EACCES);
	this.name = name;
	this.len = len;
	if (unlikely(!len))
		goto out;

	p = (void*)name;
	hash = init_name_hash();
	while (len--) {
		c = *p++;
		if (unlikely(c == '/' || c == '\0'))
			goto out;
		hash = partial_name_hash(c, hash);
	}
	this.hash = end_name_hash(hash);

	memset(&tmp_nd, 0, sizeof(tmp_nd));
	tmp_nd.dentry = dget(parent);
	tmp_nd.mnt = mntget(lkup->nfsmnt);
#ifndef CONFIG_AUFS_DLGT
	dentry = __lookup_hash(&this, parent, &tmp_nd);
#else
	if (!lkup->dlgt)
		dentry = __lookup_hash(&this, parent, &tmp_nd);
	else {
		int wkq_err;
		struct lookup_hash_args args = {
			.errp	= &dentry,
			.name	= &this,
			.base	= parent,
			.nd	= &tmp_nd
		};
		wkq_err = au_wkq_wait(call_lookup_hash, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			dentry = ERR_PTR(wkq_err);
	}
#endif
	path_release(&tmp_nd);

 out:
	TraceErrPtr(dentry);
	return dentry;
}
#elif defined(CONFIG_AUFS_DLGT)
static struct dentry *lkup_hash(const char *name, struct dentry *parent,
				int len, struct lkup_args *lkup)
{
	return ERR_PTR(-ENOSYS);
}
#endif

#ifdef CONFIG_AUFS_DLGT
struct lookup_one_len_args {
	struct dentry **errp;
	const char *name;
	struct dentry *parent;
	int len;
};

static void call_lookup_one_len(void *args)
{
	struct lookup_one_len_args *a = args;
	*a->errp = lookup_one_len(a->name, a->parent, a->len);
}
#endif /* CONFIG_AUFS_DLGT */

#if defined(CONFIG_AUFS_LHASH_PATCH) || defined(CONFIG_AUFS_DLGT)
/* cf. lookup_one_len() in linux/fs/namei.c */
struct dentry *lkup_one(const char *name, struct dentry *parent, int len,
			struct lkup_args *lkup)
{
	struct dentry *dentry;

	LKTRTrace("%.*s/%.*s, lkup{%p, %d}\n",
		  DLNPair(parent), len, name, lkup->nfsmnt, lkup->dlgt);

	if (!lkup->nfsmnt) {
#ifndef CONFIG_AUFS_DLGT
		dentry = lookup_one_len(name, parent, len);
#else
		if (!lkup->dlgt)
			dentry = lookup_one_len(name, parent, len);
		else {
			int wkq_err;
			struct lookup_one_len_args args = {
				.errp	= &dentry,
				.name	= name,
				.parent	= parent,
				.len	= len
			};
			wkq_err = au_wkq_wait(call_lookup_one_len, &args,
					      /*dlgt*/1);
			if (unlikely(wkq_err))
				dentry = ERR_PTR(wkq_err);
		}
#endif
	} else
		dentry = lkup_hash(name, parent, len, lkup);

	TraceErrPtr(dentry);
	return dentry;
}
#endif

struct lkup_one_args {
	struct dentry **errp;
	const char *name;
	struct dentry *parent;
	int len;
	struct lkup_args *lkup;
};

static void call_lkup_one(void *args)
{
	struct lkup_one_args *a = args;
	*a->errp = lkup_one(a->name, a->parent, a->len, a->lkup);
}

/*
 * returns positive/negative dentry, NULL or an error.
 * NULL means whiteout-ed or not-found.
 */
static struct dentry *do_lookup(struct dentry *hidden_parent,
				struct dentry *dentry, aufs_bindex_t bindex,
				struct qstr *wh_name, int allow_neg,
				mode_t type, int dlgt)
{
	struct dentry *hidden_dentry;
	int wh_found, wh_able, opq;
	struct inode *hidden_dir, *hidden_inode;
	struct qstr *name;
	struct super_block *sb;
	struct lkup_args lkup = {.dlgt = dlgt};

	LKTRTrace("%.*s/%.*s, b%d, allow_neg %d, type 0%o, dlgt %d\n",
		  DLNPair(hidden_parent), DLNPair(dentry), bindex, allow_neg,
		  type, dlgt);
	AuDebugOn(IS_ROOT(dentry));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	wh_found = 0;
	sb = dentry->d_sb;
	wh_able = sbr_is_whable(sb, bindex);
	lkup.nfsmnt = au_nfsmnt(sb, bindex);
	name = &dentry->d_name;
	if (unlikely(wh_able)) {
#if 0 //def CONFIG_AUFS_ROBR
		if (strncmp(name->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN))
			wh_found = is_wh(hidden_parent, wh_name, /*try_sio*/0,
					 &lkup);
		else
			wh_found = -EPERM;
#else
		wh_found = is_wh(hidden_parent, wh_name, /*try_sio*/0, &lkup);
#endif
	}
	//if (LktrCond) wh_found = -1;
	hidden_dentry = ERR_PTR(wh_found);
	if (!wh_found)
		goto real_lookup;
	if (unlikely(wh_found < 0))
		goto out;

	/* We found a whiteout */
	//set_dbend(dentry, bindex);
	set_dbwh(dentry, bindex);
	if (!allow_neg)
		return NULL; /* success */

 real_lookup:
	// do not superio.
	hidden_dentry = lkup_one(name->name, hidden_parent, name->len, &lkup);
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	if (IS_ERR(hidden_dentry))
		goto out;
	AuDebugOn(d_unhashed(hidden_dentry));
	hidden_inode = hidden_dentry->d_inode;
	if (!hidden_inode) {
		if (!allow_neg)
			goto out_neg;
	} else if (wh_found
		   || (type && type != (hidden_inode->i_mode & S_IFMT)))
		goto out_neg;

	if (dbend(dentry) <= bindex)
		set_dbend(dentry, bindex);
	if (dbstart(dentry) == -1 || bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	set_h_dptr(dentry, bindex, hidden_dentry);

	if (!hidden_inode || !S_ISDIR(hidden_inode->i_mode) || !wh_able)
		return hidden_dentry; /* success */

	hi_lock_child(hidden_inode);
	opq = is_diropq(hidden_dentry, &lkup);
	//if (LktrCond) opq = -1;
	i_unlock(hidden_inode);
	if (opq > 0)
		set_dbdiropq(dentry, bindex);
	else if (unlikely(opq < 0)) {
		set_h_dptr(dentry, bindex, NULL);
		hidden_dentry = ERR_PTR(opq);
	}
	goto out;

 out_neg:
	dput(hidden_dentry);
	hidden_dentry = NULL;
 out:
	TraceErrPtr(hidden_dentry);
	return hidden_dentry;
}

/*
 * returns the number of hidden positive dentries,
 * otherwise an error.
 */
int lkup_dentry(struct dentry *dentry, aufs_bindex_t bstart, mode_t type)
{
	int npositive, err, allow_neg, dlgt;
	struct dentry *parent;
	aufs_bindex_t bindex, btail;
	const struct qstr *name = &dentry->d_name;
	struct qstr whname;
	struct super_block *sb;

	LKTRTrace("%.*s, b%d, type 0%o\n", LNPair(name), bstart, type);
	AuDebugOn(bstart < 0 || IS_ROOT(dentry));
	parent = dget_parent(dentry);

#if 1 //ndef CONFIG_AUFS_ROBR
	err = -EPERM;
	if (unlikely(!strncmp(name->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN)))
		goto out;
#endif

	err = au_alloc_whname(name->name, name->len, &whname);
	//if (LktrCond) {au_free_whname(&whname); err = -1;}
	if (unlikely(err))
		goto out;

	sb = dentry->d_sb;
	dlgt = need_dlgt(sb);
	allow_neg = !type;
	npositive = 0;
	btail = dbtaildir(parent);
	for (bindex = bstart; bindex <= btail; bindex++) {
		struct dentry *hidden_parent, *hidden_dentry;
		struct inode *hidden_inode;
		struct inode *hidden_dir;

		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (hidden_dentry) {
			if (hidden_dentry->d_inode)
				npositive++;
			if (type != S_IFDIR)
				break;
			continue;
		}
		hidden_parent = au_h_dptr_i(parent, bindex);
		if (!hidden_parent)
			continue;
		hidden_dir = hidden_parent->d_inode;
		if (!hidden_dir || !S_ISDIR(hidden_dir->i_mode))
			continue;

		hi_lock_parent(hidden_dir);
		hidden_dentry = do_lookup(hidden_parent, dentry, bindex,
					  &whname, allow_neg, type, dlgt);
		// do not dput for testing
		//if (LktrCond) {hidden_dentry = ERR_PTR(-1);}
		i_unlock(hidden_dir);
		err = PTR_ERR(hidden_dentry);
		if (IS_ERR(hidden_dentry))
			goto out_wh;
		allow_neg = 0;

		if (dbwh(dentry) != -1)
			break;
		if (!hidden_dentry)
			continue;
		hidden_inode = hidden_dentry->d_inode;
		if (!hidden_inode)
			continue;
		npositive++;
		if (!type)
			type = hidden_inode->i_mode & S_IFMT;
		if (type != S_IFDIR)
			break;
		else if (dbdiropq(dentry) != -1)
			break;
	}

	if (npositive) {
		LKTRLabel(positive);
		au_update_dbstart(dentry);
	}
	err = npositive;

 out_wh:
	au_free_whname(&whname);
 out:
	dput(parent);
	TraceErr(err);
	return err;
}

struct dentry *sio_lkup_one(const char *name, struct dentry *parent, int len,
			    struct lkup_args *lkup)
{
	struct dentry *dentry;
	int wkq_err;

	LKTRTrace("%.*s/%.*s\n", DLNPair(parent), len, name);
	IMustLock(parent->d_inode);

	if (!au_test_perm(parent->d_inode, MAY_EXEC, lkup->dlgt))
		dentry = lkup_one(name, parent, len, lkup);
	else {
		// ugly
		int dlgt = lkup->dlgt;
		struct lkup_one_args args = {
			.errp	= &dentry,
			.name	= name,
			.parent	= parent,
			.len	= len,
			.lkup	= lkup
		};

		lkup->dlgt = 0;
		wkq_err = au_wkq_wait(call_lkup_one, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			dentry = ERR_PTR(wkq_err);
		lkup->dlgt = dlgt;
	}

	TraceErrPtr(dentry);
	return dentry;
}

/*
 * lookup @dentry on @bindex which should be negative.
 */
int lkup_neg(struct dentry *dentry, aufs_bindex_t bindex)
{
	int err;
	struct dentry *parent, *hidden_parent, *hidden_dentry;
	struct inode *hidden_dir;
	struct lkup_args lkup;

	LKTRTrace("%.*s, b%d\n", DLNPair(dentry), bindex);
	parent = dget_parent(dentry);
	AuDebugOn(!parent || !parent->d_inode
		  || !S_ISDIR(parent->d_inode->i_mode));
	hidden_parent = au_h_dptr_i(parent, bindex);
	AuDebugOn(!hidden_parent);
	hidden_dir = hidden_parent->d_inode;
	AuDebugOn(!hidden_dir || !S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	lkup.nfsmnt = au_nfsmnt(dentry->d_sb, bindex);
	lkup.dlgt = need_dlgt(dentry->d_sb);
	hidden_dentry = sio_lkup_one(dentry->d_name.name, hidden_parent,
				     dentry->d_name.len, &lkup);
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	err = PTR_ERR(hidden_dentry);
	if (IS_ERR(hidden_dentry))
		goto out;
	if (unlikely(hidden_dentry->d_inode)) {
		err = -EIO;
		IOErr("b%d %.*s should be negative.%s\n",
		      bindex, DLNPair(hidden_dentry),
		      au_flag_test(dentry->d_sb, AuFlag_UDBA_INOTIFY) ? "" :
		      " Try udba=inotify.");
		dput(hidden_dentry);
		goto out;
	}

	if (bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	if (dbend(dentry) < bindex)
		set_dbend(dentry, bindex);
	set_h_dptr(dentry, bindex, hidden_dentry);
	err = 0;

 out:
	dput(parent);
	TraceErr(err);
	return err;
}

/*
 * returns the number of found hidden positive dentries,
 * otherwise an error.
 */
int au_refresh_hdentry(struct dentry *dentry, mode_t type)
{
	int npositive, pgen, new_sz, sgen, dgen;
	struct aufs_dinfo *dinfo;
	struct super_block *sb;
	struct dentry *parent;
	aufs_bindex_t bindex, parent_bend, parent_bstart, bwh, bdiropq, bend;
	struct aufs_hdentry *p;
	//struct nameidata nd;

	LKTRTrace("%.*s, type 0%o\n", DLNPair(dentry), type);
	DiMustWriteLock(dentry);
	sb = dentry->d_sb;
	AuDebugOn(IS_ROOT(dentry));
	parent = dget_parent(dentry);
	pgen = au_digen(parent);
	sgen = au_sigen(sb);
	dgen = au_digen(dentry);
	AuDebugOn(pgen != sgen);

	npositive = -ENOMEM;
	new_sz = sizeof(*dinfo->di_hdentry) * (sbend(sb) + 1);
	dinfo = dtodi(dentry);
	p = au_kzrealloc(dinfo->di_hdentry, sizeof(*p) * (dinfo->di_bend + 1),
			 new_sz, GFP_KERNEL);
	//p = NULL;
	if (unlikely(!p))
		goto out;
	dinfo->di_hdentry = p;

	bend = dinfo->di_bend;
	bwh = dinfo->di_bwh;
	bdiropq = dinfo->di_bdiropq;
	p += dinfo->di_bstart;
	for (bindex = dinfo->di_bstart; bindex <= bend; bindex++, p++) {
		struct dentry *hd, *hdp;
		struct aufs_hdentry tmp, *q;
		aufs_bindex_t new_bindex;

		hd = p->hd_dentry;
		if (!hd)
			continue;
		hdp = dget_parent(hd);
		if (hdp == au_h_dptr_i(parent, bindex)) {
			dput(hdp);
			continue;
		}

		new_bindex = au_find_dbindex(parent, hdp);
		dput(hdp);
		AuDebugOn(new_bindex == bindex);
		if (dinfo->di_bwh == bindex)
			bwh = new_bindex;
		if (dinfo->di_bdiropq == bindex)
			bdiropq = new_bindex;
		if (new_bindex < 0) { // test here
			hdput(p);
			p->hd_dentry = NULL;
			continue;
		}
		/* swap two hidden dentries, and loop again */
		q = dinfo->di_hdentry + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hd_dentry) {
			bindex--;
			p--;
		}
	}

	// test here
	dinfo->di_bwh = -1;
	if (unlikely(bwh != -1 && bwh <= sbend(sb) && sbr_is_whable(sb, bwh)))
		dinfo->di_bwh = bwh;
	dinfo->di_bdiropq = -1;
	if (unlikely(bdiropq != -1 && bdiropq <= sbend(sb)
		     && sbr_is_whable(sb, bdiropq)))
		dinfo->di_bdiropq = bdiropq;
	parent_bend = dbend(parent);
	p = dinfo->di_hdentry;
	for (bindex = 0; bindex <= parent_bend; bindex++, p++)
		if (p->hd_dentry) {
			dinfo->di_bstart = bindex;
			break;
		}
	p = dinfo->di_hdentry + parent_bend;
	//for (bindex = parent_bend; bindex > dinfo->di_bstart; bindex--, p--)
	for (bindex = parent_bend; bindex >= 0; bindex--, p--)
		if (p->hd_dentry) {
			dinfo->di_bend = bindex;
			break;
		}

	npositive = 0;
	parent_bstart = dbstart(parent);
	if (type != S_IFDIR && dinfo->di_bstart == parent_bstart)
		goto out_dgen; /* success */

#if 0
	nd.last_type = LAST_ROOT;
	nd.flags = LOOKUP_FOLLOW;
	nd.depth = 0;
	nd.mnt = mntget(??);
	nd.dentry = dget(parent);
#endif
	npositive = lkup_dentry(dentry, parent_bstart, type);
	//if (LktrCond) npositive = -1;
	if (npositive < 0)
		goto out;

 out_dgen:
	au_update_digen(dentry);
 out:
	dput(parent);
	TraceErr(npositive);
	return npositive;
}

static int h_d_revalidate(struct dentry *dentry, struct nameidata *nd,
			  int do_udba)
{
	int err, plus, locked, unhashed, is_root, h_plus, is_nfs;
	struct nameidata fake_nd, *p;
	aufs_bindex_t bindex, btail, bstart, ibs, ibe;
	struct super_block *sb;
	struct inode *inode, *first, *h_inode, *h_cached_inode;
	umode_t mode, h_mode;
	struct dentry *h_dentry;
	int (*reval)(struct dentry *, struct nameidata *);
	struct qstr *name;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	inode = dentry->d_inode;
	AuDebugOn(inode && au_digen(dentry) != au_iigen(inode));
	//DbgDentry(dentry);
	//DbgInode(inode);

	err = 0;
	sb = dentry->d_sb;
	plus = 0;
	mode = 0;
	first = NULL;
	ibs = ibe = -1;
	unhashed = d_unhashed(dentry);
	is_root = IS_ROOT(dentry);
	name = &dentry->d_name;

	/*
	 * Theoretically, REVAL test should be unnecessary in case of INOTIFY.
	 * But inotify doesn't fire some necessary events,
	 *	IN_ATTRIB for atime/nlink/pageio
	 *	IN_DELETE for NFS dentry
	 * Let's do REVAL test too.
	 */
	if (do_udba && inode) {
		mode = (inode->i_mode & S_IFMT);
		plus = (inode->i_nlink > 0);
		first = au_h_iptr(inode);
		ibs = ibstart(inode);
		ibe = ibend(inode);
	}

	btail = bstart = dbstart(dentry);
	if (inode && S_ISDIR(inode->i_mode))
		btail = dbtaildir(dentry);
	locked = 0;
	if (nd) {
		fake_nd = *nd;
#ifndef CONFIG_AUFS_FAKE_DM
		if (dentry != nd->dentry) {
			di_read_lock_parent(nd->dentry, 0);
			locked = 1;
		}
#endif
	}
	for (bindex = bstart; bindex <= btail; bindex++) {
		h_dentry = au_h_dptr_i(dentry, bindex);
		if (unlikely(!h_dentry))
			continue;
		if (unlikely(do_udba
			     && !is_root
			     && (unhashed != d_unhashed(h_dentry)
#if 1
				 || name->len != h_dentry->d_name.len
				 || memcmp(name->name, h_dentry->d_name.name,
					   name->len)
#endif
				     ))) {
			LKTRTrace("unhash 0x%x 0x%x, %.*s %.*s\n",
				  unhashed, d_unhashed(h_dentry),
				  DLNPair(dentry), DLNPair(h_dentry));
			goto err;
		}

		reval = NULL;
		if (h_dentry->d_op)
			reval = h_dentry->d_op->d_revalidate;
		if (unlikely(reval)) {
			//LKTRLabel(hidden reval);
			p = fake_dm(&fake_nd, nd, sb, bindex);
			AuDebugOn(IS_ERR(p));
			err = !reval(h_dentry, p);
			fake_dm_release(p);
			if (unlikely(err)) {
				//Dbg("here\n");
				goto err;
			}
		}

		if (unlikely(!do_udba))
			continue;

		/* UDBA tests */
		h_inode = h_dentry->d_inode;
		if (unlikely(!!inode != !!h_inode)) {
			//Dbg("here\n");
			goto err;
		}

		h_plus = plus;
		h_mode = mode;
		h_cached_inode = h_inode;
		is_nfs = 0;
		if (h_inode) {
			h_mode = (h_inode->i_mode & S_IFMT);
			h_plus = (h_inode->i_nlink > 0);
		}
		if (inode && ibs <= bindex && bindex <= ibe) {
			h_cached_inode = au_h_iptr_i(inode, bindex);
			//is_nfs = au_is_nfs(h_cached_inode->i_sb);
		}

		LKTRTrace("{%d, 0%o, %p}, h{%d, 0%o, %p}\n",
			  plus, mode, h_cached_inode,
			  h_plus, h_mode, h_inode);
		if (unlikely(plus != h_plus || mode != h_mode
			     || (h_cached_inode != h_inode /* && !is_nfs */))) {
			//Dbg("here\n");
			goto err;
		}
		continue;

	err:
		err = -EINVAL;
		break;
	}
#ifndef CONFIG_AUFS_FAKE_DM
	if (unlikely(locked))
		di_read_unlock(nd->dentry, 0);
#endif

#if 0
	// some filesystem uses CURRENT_TIME_SEC instead of CURRENT_TIME.
	// NFS may stop IN_DELETE because of DCACHE_NFSFS_RENAMED.
#if 0
		     && (!timespec_equal(&inode->i_ctime, &first->i_ctime)
			 || !timespec_equal(&inode->i_atime, &first->i_atime))
#endif
	if (unlikely(!err && udba && first))
		au_cpup_attr_all(inode);
#endif

	TraceErr(err);
	return err;
}

static int simple_reval_dpath(struct dentry *dentry, int sgen)
{
	int err;
	mode_t type;
	struct dentry *parent;
	struct inode *inode;

	LKTRTrace("%.*s, sgen %d\n", DLNPair(dentry), sgen);
	SiMustAnyLock(dentry->d_sb);
	DiMustWriteLock(dentry);
	inode = dentry->d_inode;
	AuDebugOn(!inode);

	if (au_digen(dentry) == sgen)
		return 0;

	parent = dget_parent(dentry);
	di_read_lock_parent(parent, AUFS_I_RLOCK);
	AuDebugOn(au_digen(parent) != sgen);
#ifdef CONFIG_AUFS_DEBUG
	{
		int i, j;
		struct au_dcsub_pages dpages;
		struct au_dpage *dpage;
		struct dentry **dentries;

		err = au_dpages_init(&dpages, GFP_KERNEL);
		AuDebugOn(err);
		err = au_dcsub_pages_rev(&dpages, parent, /*do_include*/1, NULL,
					 NULL);
		AuDebugOn(err);
		for (i = dpages.ndpage - 1; !err && i >= 0; i--) {
			dpage = dpages.dpages + i;
			dentries = dpage->dentries;
			for (j = dpage->ndentry - 1; !err && j >= 0; j--)
				AuDebugOn(au_digen(dentries[j]) != sgen);
		}
		au_dpages_free(&dpages);
	}
#endif
	type = (inode->i_mode & S_IFMT);
	/* returns a number of positive dentries */
	err = au_refresh_hdentry(dentry, type);
	if (err >= 0)
		err = au_refresh_hinode(inode, dentry);
	di_read_unlock(parent, AUFS_I_RLOCK);
	dput(parent);
	TraceErr(err);
	return err;
}

int au_reval_dpath(struct dentry *dentry, int sgen)
{
	int err;
	struct dentry *d, *parent;
	struct inode *inode;

	LKTRTrace("%.*s, sgen %d\n", DLNPair(dentry), sgen);
	AuDebugOn(!dentry->d_inode);
	DiMustWriteLock(dentry);

	if (!stosi(dentry->d_sb)->si_failed_refresh_dirs)
		return simple_reval_dpath(dentry, sgen);

	/* slow loop, keep it simple and stupid */
	/* cf: cpup_dirs() */
	err = 0;
	while (au_digen(dentry) != sgen) {
		d = dentry;
		while (1) {
			parent = d->d_parent; // dget_parent()
			if (au_digen(parent) == sgen)
				break;
			d = parent;
		}

		inode = d->d_inode;
		if (d != dentry) {
			//i_lock(inode);
			di_write_lock_child(d);
		}

		/* someone might update our dentry while we were sleeping */
		if (au_digen(d) != sgen) {
			di_read_lock_parent(parent, AUFS_I_RLOCK);
			/* returns a number of positive dentries */
			err = au_refresh_hdentry(d, inode->i_mode & S_IFMT);
			//err = -1;
			if (err >= 0)
				err = au_refresh_hinode(inode, d);
			//err = -1;
			di_read_unlock(parent, AUFS_I_RLOCK);
		}

		if (d != dentry) {
			di_write_unlock(d);
			//i_unlock(inode);
		}
		if (unlikely(err))
			break;
	}

	TraceErr(err);
	return err;
}

/*
 * THIS IS A BOOLEAN FUNCTION: returns 1 if valid, 0 otherwise.
 * nfsd passes NULL as nameidata.
 */
static int aufs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int valid, sgen, err, do_udba;
	struct super_block *sb;
	struct inode *inode;

	LKTRTrace("dentry %.*s\n", DLNPair(dentry));
	if (nd && nd->dentry)
		LKTRTrace("nd %.*s\n", DLNPair(nd->dentry));
	//dir case: AuDebugOn(dentry->d_parent != nd->dentry);
	//remove failure case: AuDebugOn(!IS_ROOT(dentry) && d_unhashed(dentry));
	AuDebugOn(!dentry->d_fsdata);
	//DbgDentry(dentry);

	err = -EINVAL;
	inode = dentry->d_inode;
	//DbgInode(inode);
	sb = dentry->d_sb;
	si_read_lock(sb);
	sgen = au_sigen(sb);
	if (au_digen(dentry) == sgen)
		di_read_lock_child(dentry, !AUFS_I_RLOCK);
	else {
		AuDebugOn(IS_ROOT(dentry));
#ifdef ForceInotify
		Dbg("UDBA or digen, %.*s\n", DLNPair(dentry));
#endif
		//i_lock(inode);
		di_write_lock_child(dentry);
		if (inode)
			err = au_reval_dpath(dentry, sgen);
		//err = -1;
		di_downgrade_lock(dentry, AUFS_I_RLOCK);
		//i_unlock(inode);
		if (unlikely(err))
			goto out;
		ii_read_unlock(inode);
		AuDebugOn(au_iigen(inode) != sgen);
	}

	if (inode) {
		if (au_iigen(inode) == sgen)
			ii_read_lock_child(inode);
		else {
			AuDebugOn(IS_ROOT(dentry));
#ifdef ForceInotify
			Dbg("UDBA or survived, %.*s\n", DLNPair(dentry));
#endif
			ii_write_lock_child(inode);
			err = au_refresh_hinode(inode, dentry);
			ii_downgrade_lock(inode);
			if (unlikely(err))
				goto out;
			AuDebugOn(au_iigen(inode) != sgen);
		}
	}

#if 0 // fix it
	/* parent dir i_nlink is not updated in the case of setattr */
	if (S_ISDIR(inode->i_mode)) {
		i_lock(inode);
		ii_write_lock(inode);
		au_cpup_attr_nlink(inode);
		ii_write_unlock(inode);
		i_unlock(inode);
	}
#endif

	err = -EINVAL;
	do_udba = !au_flag_test(sb, AuFlag_UDBA_NONE);
	if (do_udba && inode && ibstart(inode) >= 0
	    && au_test_higen(inode, au_h_iptr(inode)))
		goto out;
	err = h_d_revalidate(dentry, nd, do_udba);
	//err = -1;

 out:
	aufs_read_unlock(dentry, AUFS_I_RLOCK);
	TraceErr(err);
	valid = !err;
	//au_debug_on();
	if (!valid)
		LKTRTrace("%.*s invalid\n", DLNPair(dentry));
	//au_debug_off();
	return valid;
}

static void aufs_d_release(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	AuDebugOn(!d_unhashed(dentry));

	dinfo = dentry->d_fsdata;
	if (unlikely(!dinfo))
		return;

	/* dentry may not be revalidated */
	bindex = dinfo->di_bstart;
	if (bindex >= 0) {
		struct aufs_hdentry *p;
		bend = dinfo->di_bend;
		AuDebugOn(bend < bindex);
		p = dinfo->di_hdentry + bindex;
		while (bindex++ <= bend) {
			if (p->hd_dentry)
				hdput(p);
			p++;
		}
	}
	kfree(dinfo->di_hdentry);
	cache_free_dinfo(dinfo);
}

#if 0
/* it may be called at remount time, too */
static void aufs_d_iput(struct dentry *dentry, struct inode *inode)
{
	struct super_block *sb;

	LKTRTrace("%.*s, i%lu\n", DLNPair(dentry), inode->i_ino);

	sb = dentry->d_sb;
#if 0
	si_read_lock(sb);
	if (unlikely(au_flag_test(sb, AuFlag_PLINK)
		     && au_is_plinked(sb, inode))) {
		ii_write_lock(inode);
		au_update_brange(inode, 1);
		ii_write_unlock(inode);
	}
	si_read_unlock(sb);
#endif
	iput(inode);
}
#endif

struct dentry_operations aufs_dop = {
	.d_revalidate	= aufs_d_revalidate,
	.d_release	= aufs_d_release
	//.d_iput		= aufs_d_iput
};
