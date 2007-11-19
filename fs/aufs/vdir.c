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

/* $Id: vdir.c,v 1.24 2007/06/04 02:17:35 sfjro Exp $ */

#include "aufs.h"

static int calc_size(int namelen)
{
	int sz;

	sz = sizeof(struct aufs_de) + namelen;
	if (sizeof(ino_t) == sizeof(long)) {
		const int mask = sizeof(ino_t) - 1;
		if (sz & mask) {
			sz += sizeof(ino_t);
			sz &= ~mask;
		}
	} else {
#if 0 // remove
		BUG();
		// this block will be discarded by optimizer.
		int m;
		m = sz % sizeof(ino_t);
		if (m)
			sz += sizeof(ino_t) - m;
#endif
	}

	AuDebugOn(sz % sizeof(ino_t));
	return sz;
}

static int set_deblk_end(union aufs_deblk_p *p, union aufs_deblk_p *deblk_end)
{
	if (calc_size(0) <= deblk_end->p - p->p) {
		p->de->de_str.len = 0;
		//smp_mb();
		return 0;
	}
	return -1; // error
}

/* returns true or false */
static int is_deblk_end(union aufs_deblk_p *p, union aufs_deblk_p *deblk_end)
{
	if (calc_size(0) <= deblk_end->p - p->p)
		return !p->de->de_str.len;
	return 1;
}

static aufs_deblk_t *last_deblk(struct aufs_vdir *vdir)
{
	return vdir->vd_deblk[vdir->vd_nblk - 1];
}

void nhash_init(struct aufs_nhash *nhash)
{
	int i;
	for (i = 0; i < AUFS_NHASH_SIZE; i++)
		INIT_HLIST_HEAD(nhash->heads + i);
}

struct aufs_nhash *nhash_new(gfp_t gfp)
{
	struct aufs_nhash *nhash;

	nhash = kmalloc(sizeof(*nhash), gfp);
	if (nhash) {
		nhash_init(nhash);
		return nhash;
	}
	return ERR_PTR(-ENOMEM);
}

void nhash_del(struct aufs_nhash *nhash)
{
	nhash_fin(nhash);
	kfree(nhash);
}

void nhash_move(struct aufs_nhash *dst, struct aufs_nhash *src)
{
	int i;

	TraceEnter();

	//DbgWhlist(src);
	*dst = *src;
	for (i = 0; i < AUFS_NHASH_SIZE; i++) {
		struct hlist_head *h;
		h = dst->heads + i;
		if (h->first)
			h->first->pprev = &h->first;
		INIT_HLIST_HEAD(src->heads + i);
	}
	//DbgWhlist(src);
	//DbgWhlist(dst);
	//smp_mb();
}

/* ---------------------------------------------------------------------- */

void nhash_fin(struct aufs_nhash *whlist)
{
	int i;
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos, *n;

	TraceEnter();

	for (i = 0; i < AUFS_NHASH_SIZE; i++) {
		head = whlist->heads + i;
		hlist_for_each_entry_safe(tpos, pos, n, head, wh_hash) {
			//hlist_del(pos);
			kfree(tpos);
		}
	}
}

int is_longer_wh(struct aufs_nhash *whlist, aufs_bindex_t btgt, int limit)
{
	int n, i;
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos;

	LKTRTrace("limit %d\n", limit);
	//return 1;

	n = 0;
	for (i = 0; i < AUFS_NHASH_SIZE; i++) {
		head = whlist->heads + i;
		hlist_for_each_entry(tpos, pos, head, wh_hash)
			if (tpos->wh_bindex == btgt && ++n > limit)
				return 1;
	}
	return 0;
}

/* returns found(true) or not */
int test_known_wh(struct aufs_nhash *whlist, char *name, int namelen)
{
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos;
	struct aufs_destr *str;

	LKTRTrace("%.*s\n", namelen, name);

	head = whlist->heads + au_name_hash(name, namelen);
	hlist_for_each_entry(tpos, pos, head, wh_hash) {
		str = &tpos->wh_str;
		LKTRTrace("%.*s\n", str->len, str->name);
		if (str->len == namelen && !memcmp(str->name, name, namelen))
			return 1;
	}
	return 0;
}

int append_wh(struct aufs_nhash *whlist, char *name, int namelen,
	      aufs_bindex_t bindex)
{
	int err;
	struct aufs_destr *str;
	struct aufs_wh *wh;

	LKTRTrace("%.*s\n", namelen, name);

	err = -ENOMEM;
	wh = kmalloc(sizeof(*wh) + namelen, GFP_KERNEL);
	if (unlikely(!wh))
		goto out;
	err = 0;
	wh->wh_bindex = bindex;
	str = &wh->wh_str;
	str->len = namelen;
	memcpy(str->name, name, namelen);
	hlist_add_head(&wh->wh_hash,
		       whlist->heads + au_name_hash(name, namelen));
	//smp_mb();

 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

void free_vdir(struct aufs_vdir *vdir)
{
	aufs_deblk_t **deblk;

	TraceEnter();

	deblk = vdir->vd_deblk;
	while (vdir->vd_nblk--) {
		kfree(*deblk);
		deblk++;
	}
	kfree(vdir->vd_deblk);
	cache_free_vdir(vdir);
}

static int append_deblk(struct aufs_vdir *vdir)
{
	int err, sz, i;
	aufs_deblk_t **o;
	union aufs_deblk_p p, deblk_end;

	TraceEnter();

	err = -ENOMEM;
	sz = sizeof(*o) * vdir->vd_nblk;
	o = au_kzrealloc(vdir->vd_deblk, sz, sz + sizeof(*o), GFP_KERNEL);
	if (unlikely(!o))
		goto out;
	vdir->vd_deblk = o;
	p.deblk = kmalloc(sizeof(*p.deblk), GFP_KERNEL);
	if (p.deblk) {
		i = vdir->vd_nblk++;
		vdir->vd_deblk[i] = p.deblk;
		vdir->vd_last.i = i;
		vdir->vd_last.p.p = p.p;
		deblk_end.deblk = p.deblk + 1;
		err = set_deblk_end(&p, &deblk_end);
		AuDebugOn(err);
	}

 out:
	TraceErr(err);
	return err;
}

static struct aufs_vdir *alloc_vdir(void)
{
	struct aufs_vdir *vdir;
	int err;

	TraceEnter();

	err = -ENOMEM;
	vdir = cache_alloc_vdir();
	if (unlikely(!vdir))
		goto out;
	vdir->vd_deblk = kzalloc(sizeof(*vdir->vd_deblk), GFP_KERNEL);
	if (unlikely(!vdir->vd_deblk))
		goto out_free;

	vdir->vd_nblk = 0;
	vdir->vd_version = 0;
	vdir->vd_jiffy = 0;
	err = append_deblk(vdir);
	if (!err)
		return vdir; /* success */

	kfree(vdir->vd_deblk);

 out_free:
	cache_free_vdir(vdir);
 out:
	vdir = ERR_PTR(err);
	TraceErrPtr(vdir);
	return vdir;
}

static int reinit_vdir(struct aufs_vdir *vdir)
{
	int err;
	union aufs_deblk_p p, deblk_end;

	TraceEnter();

	while (vdir->vd_nblk > 1) {
		kfree(vdir->vd_deblk[vdir->vd_nblk - 1]);
		vdir->vd_deblk[vdir->vd_nblk - 1] = NULL;
		vdir->vd_nblk--;
	}
	p.deblk = vdir->vd_deblk[0];
	deblk_end.deblk = p.deblk + 1;
	err = set_deblk_end(&p, &deblk_end);
	AuDebugOn(err);
	vdir->vd_version = 0;
	vdir->vd_jiffy = 0;
	vdir->vd_last.i = 0;
	vdir->vd_last.p.deblk = vdir->vd_deblk[0];
	//smp_mb();
	//DbgVdir(vdir);
	return err;
}

/* ---------------------------------------------------------------------- */

static void free_dehlist(struct aufs_nhash *dehlist)
{
	int i;
	struct hlist_head *head;
	struct aufs_dehstr *tpos;
	struct hlist_node *pos, *n;

	TraceEnter();

	for (i = 0; i < AUFS_NHASH_SIZE; i++) {
		head = dehlist->heads + i;
		hlist_for_each_entry_safe(tpos, pos, n, head, hash) {
			//hlist_del(pos);
			cache_free_dehstr(tpos);
		}
	}
}

/* returns found(true) or not */
static int test_known(struct aufs_nhash *delist, char *name, int namelen)
{
	struct hlist_head *head;
	struct aufs_dehstr *tpos;
	struct hlist_node *pos;
	struct aufs_destr *str;

	LKTRTrace("%.*s\n", namelen, name);

	head = delist->heads + au_name_hash(name, namelen);
	hlist_for_each_entry(tpos, pos, head, hash) {
		str = tpos->str;
		LKTRTrace("%.*s\n", str->len, str->name);
		if (str->len == namelen && !memcmp(str->name, name, namelen))
			return 1;
	}
	return 0;

}

static int append_de(struct aufs_vdir *vdir, char *name, int namelen, ino_t ino,
		     unsigned int d_type, struct aufs_nhash *delist)
{
	int err, sz;
	union aufs_deblk_p p, *room, deblk_end;
	struct aufs_dehstr *dehstr;

	LKTRTrace("%.*s %d, i%lu, dt%u\n", namelen, name, namelen, ino, d_type);

	p.deblk = last_deblk(vdir);
	deblk_end.deblk = p.deblk + 1;
	room = &vdir->vd_last.p;
	AuDebugOn(room->p < p.p || deblk_end.p <= room->p
		  || !is_deblk_end(room, &deblk_end));

	sz = calc_size(namelen);
	if (unlikely(sz > deblk_end.p - room->p)) {
		err = append_deblk(vdir);
		if (unlikely(err))
			goto out;
		p.deblk = last_deblk(vdir);
		deblk_end.deblk = p.deblk + 1;
		//smp_mb();
		AuDebugOn(room->p != p.p);
	}

	err = -ENOMEM;
	dehstr = cache_alloc_dehstr();
	if (unlikely(!dehstr))
		goto out;
	dehstr->str = &room->de->de_str;
	hlist_add_head(&dehstr->hash,
		       delist->heads + au_name_hash(name, namelen));

	room->de->de_ino = ino;
	room->de->de_type = d_type;
	room->de->de_str.len = namelen;
	memcpy(room->de->de_str.name, name, namelen);

	err = 0;
	room->p += sz;
	if (unlikely(set_deblk_end(room, &deblk_end)))
		err = append_deblk(vdir);
	//smp_mb();

 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct fillvdir_arg {
	struct file		*file;
	struct aufs_vdir 	*vdir;
	struct aufs_nhash	*delist;
	struct aufs_nhash	*whlist;
	aufs_bindex_t		bindex;
	int			err;
	int			called;
};

static int fillvdir(void *__arg, const char *__name, int namelen, loff_t offset,
		    filldir_ino_t h_ino, unsigned int d_type)
{
	struct fillvdir_arg *arg = __arg;
	char *name = (void*)__name;
	aufs_bindex_t bindex, bend;
	struct xino xino;
	struct super_block *sb;

	LKTRTrace("%.*s, namelen %d, i%Lu, dt%u\n",
		  namelen, name, namelen, (u64)h_ino, d_type);

	sb = arg->file->f_dentry->d_sb;
	bend = arg->bindex;
	arg->err = 0;
	arg->called++;
	//smp_mb();
	if (namelen <= AUFS_WH_PFX_LEN
	    || memcmp(name, AUFS_WH_PFX, AUFS_WH_PFX_LEN)) {
		for (bindex = 0; bindex < bend; bindex++)
			if (test_known(arg->delist + bindex, name, namelen)
			    || test_known_wh(arg->whlist + bindex, name,
					     namelen))
				goto out; /* already exists or whiteouted */

		arg->err = xino_read(sb, bend, h_ino, &xino);
		if (!arg->err && !xino.ino) {
			//struct inode *h_inode;
			xino.ino = xino_new_ino(sb);
			if (unlikely(!xino.ino))
				arg->err = -EIO;
#if 0
			//xino.h_gen = AuXino_INVALID_HGEN;
			h_inode = ilookup(sbr_sb(sb, bend), h_ino);
			if (h_inode) {
				if (!is_bad_inode(h_inode)) {
					xino.h_gen = h_inode->i_generation;
					WARN_ON(xino.h_gen
						== AuXino_INVALID_HGEN);
				}
				iput(h_inode);
			}
#endif
			arg->err = xino_write(sb, bend, h_ino, &xino);
		}
		if (!arg->err)
			arg->err = append_de(arg->vdir, name, namelen, xino.ino,
					     d_type, arg->delist + bend);
	} else {
		name += AUFS_WH_PFX_LEN;
		namelen -= AUFS_WH_PFX_LEN;
		for (bindex = 0; bindex < bend; bindex++)
			if (test_known_wh(arg->whlist + bend, name, namelen))
				goto out; /* already whiteouted */
		arg->err = append_wh(arg->whlist + bend, name, namelen, bend);
	}

 out:
	if (!arg->err)
		arg->vdir->vd_jiffy = jiffies;
	//smp_mb();
	TraceErr(arg->err);
	return arg->err;
}

static int read_vdir(struct file *file, int may_read)
{
	int err, do_read, dlgt;
	struct dentry *dentry;
	struct inode *inode;
	struct aufs_vdir *vdir, *allocated;
	unsigned long expire;
	struct fillvdir_arg arg;
	aufs_bindex_t bindex, bend, bstart;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s,  may %d\n", DLNPair(dentry), may_read);
	FiMustWriteLock(file);
	inode = dentry->d_inode;
	IMustLock(inode);
	IiMustWriteLock(inode);
	AuDebugOn(!S_ISDIR(inode->i_mode));

	err = 0;
	allocated = NULL;
	do_read = 0;
	sb = inode->i_sb;
	expire = stosi(sb)->si_rdcache;
	vdir = ivdir(inode);
	if (!vdir) {
		AuDebugOn(fvdir_cache(file));
		do_read = 1;
		vdir = alloc_vdir();
		err = PTR_ERR(vdir);
		if (IS_ERR(vdir))
			goto out;
		err = 0;
		allocated = vdir;
	} else if (may_read
		   && (inode->i_version != vdir->vd_version
		       || time_after(jiffies, vdir->vd_jiffy + expire))) {
		LKTRTrace("iver %lu, vdver %lu, exp %lu\n",
			  inode->i_version, vdir->vd_version,
			  vdir->vd_jiffy + expire);
		do_read = 1;
		err = reinit_vdir(vdir);
		if (unlikely(err))
			goto out;
	}
	//DbgVdir(vdir); goto out;

	if (!do_read)
		return 0; /* success */

	err = -ENOMEM;
	bend = fbend(file);
	arg.delist = kmalloc(sizeof(*arg.delist) * (bend + 1), GFP_KERNEL);
	if (unlikely(!arg.delist))
		goto out_vdir;
	arg.whlist = kmalloc(sizeof(*arg.whlist) * (bend + 1), GFP_KERNEL);
	if (unlikely(!arg.whlist))
		goto out_delist;
	err = 0;
	for (bindex = 0; bindex <= bend; bindex++) {
		nhash_init(arg.delist + bindex);
		nhash_init(arg.whlist + bindex);
	}

	dlgt = need_dlgt(sb);
	arg.file = file;
	arg.vdir = vdir;
	bstart = fbstart(file);
	for (bindex = bstart; !err && bindex <= bend; bindex++) {
		struct file *hf;
		struct inode *h_inode;

		hf = au_h_fptr_i(file, bindex);
		if (!hf)
			continue;

		h_inode = hf->f_dentry->d_inode;
		//hf->f_pos = 0;
		arg.bindex = bindex;
		do {
			arg.err = 0;
			arg.called = 0;
			//smp_mb();
			err = vfsub_readdir(hf, fillvdir, &arg, dlgt);
			if (err >= 0)
				err = arg.err;
		} while (!err && arg.called);
	}

	for (bindex = bstart; bindex <= bend; bindex++) {
		free_dehlist(arg.delist + bindex);
		nhash_fin(arg.whlist + bindex);
	}
	kfree(arg.whlist);

 out_delist:
	kfree(arg.delist);
 out_vdir:
	if (!err) {
		//file->f_pos = 0;
		vdir->vd_version = inode->i_version;
		vdir->vd_last.i = 0;
		vdir->vd_last.p.deblk = vdir->vd_deblk[0];
		if (allocated)
			set_ivdir(inode, allocated);
	} else if (allocated)
		free_vdir(allocated);
	//DbgVdir(vdir); goto out;

 out:
	TraceErr(err);
	return err;
}

static int copy_vdir(struct aufs_vdir *tgt, struct aufs_vdir *src)
{
	int err, i, rerr, n;

	TraceEnter();
	AuDebugOn(tgt->vd_nblk != 1);
	//DbgVdir(tgt);

	err = -ENOMEM;
	if (tgt->vd_nblk < src->vd_nblk) {
		aufs_deblk_t **p;
		p = au_kzrealloc(tgt->vd_deblk, sizeof(*p) * tgt->vd_nblk,
				 sizeof(*p) * src->vd_nblk, GFP_KERNEL);
		if (unlikely(!p))
			goto out;
		tgt->vd_deblk = p;
	}

	n = tgt->vd_nblk = src->vd_nblk;
	memcpy(tgt->vd_deblk[0], src->vd_deblk[0], AUFS_DEBLK_SIZE);
	//tgt->vd_last.i = 0;
	//tgt->vd_last.p.deblk = tgt->vd_deblk[0];
	tgt->vd_version = src->vd_version;
	tgt->vd_jiffy = src->vd_jiffy;

	for (i = 1; i < n; i++) {
		tgt->vd_deblk[i] = kmalloc(AUFS_DEBLK_SIZE, GFP_KERNEL);
		if (tgt->vd_deblk[i])
			memcpy(tgt->vd_deblk[i], src->vd_deblk[i],
			       AUFS_DEBLK_SIZE);
		else
			goto out;
	}
	//smp_mb();
	//DbgVdir(tgt);
	return 0; /* success */

 out:
	rerr = reinit_vdir(tgt);
	BUG_ON(rerr);
	TraceErr(err);
	return err;
}

int au_init_vdir(struct file *file)
{
	int err;
	struct dentry *dentry;
	struct inode *inode;
	struct aufs_vdir *vdir_cache, *allocated;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, pos %Ld\n", DLNPair(dentry), file->f_pos);
	FiMustWriteLock(file);
	inode = dentry->d_inode;
	IiMustWriteLock(inode);
	AuDebugOn(!S_ISDIR(inode->i_mode));

	err = read_vdir(file, !file->f_pos);
	if (unlikely(err))
		goto out;
	//DbgVdir(ivdir(inode)); goto out;

	allocated = NULL;
	vdir_cache = fvdir_cache(file);
	if (!vdir_cache) {
		vdir_cache = alloc_vdir();
		err = PTR_ERR(vdir_cache);
		if (IS_ERR(vdir_cache))
			goto out;
		allocated = vdir_cache;
	} else if (!file->f_pos && vdir_cache->vd_version != file->f_version) {
		err = reinit_vdir(vdir_cache);
		if (unlikely(err))
			goto out;
	} else
		return 0; /* success */
	//err = 0; DbgVdir(vdir_cache); goto out;

	err = copy_vdir(vdir_cache, ivdir(inode));
	if (!err) {
		file->f_version = inode->i_version;
		if (allocated)
			set_fvdir_cache(file, allocated);
	} else if (allocated)
		free_vdir(allocated);

 out:
	TraceErr(err);
	return err;
}

static loff_t calc_offset(struct aufs_vdir *vdir)
{
	loff_t offset;
	union aufs_deblk_p p;

	p.deblk = vdir->vd_deblk[vdir->vd_last.i];
	offset = vdir->vd_last.p.p - p.p;
	offset += sizeof(*p.deblk) * vdir->vd_last.i;
	return offset;
}

/* returns true or false */
static int seek_vdir(struct file *file)
{
	int valid, i, n;
	struct dentry *dentry;
	struct aufs_vdir *vdir_cache;
	loff_t offset;
	union aufs_deblk_p p, deblk_end;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, pos %Ld\n", DLNPair(dentry), file->f_pos);
	vdir_cache = fvdir_cache(file);
	AuDebugOn(!vdir_cache);
	//DbgVdir(vdir_cache);

	valid = 1;
	offset = calc_offset(vdir_cache);
	LKTRTrace("offset %Ld\n", offset);
	if (file->f_pos == offset)
		goto out;

	vdir_cache->vd_last.i = 0;
	vdir_cache->vd_last.p.deblk = vdir_cache->vd_deblk[0];
	if (!file->f_pos)
		goto out;

	valid = 0;
	i = file->f_pos / AUFS_DEBLK_SIZE;
	LKTRTrace("i %d\n", i);
	if (i >= vdir_cache->vd_nblk)
		goto out;

	n = vdir_cache->vd_nblk;
	//DbgVdir(vdir_cache);
	for (; i < n; i++) {
		p.deblk = vdir_cache->vd_deblk[i];
		deblk_end.deblk = p.deblk + 1;
		offset = i * AUFS_DEBLK_SIZE;
		while (!is_deblk_end(&p, &deblk_end) && offset < file->f_pos) {
			int l;
			l = calc_size(p.de->de_str.len);
			offset += l;
			p.p += l;
		}
		if (!is_deblk_end(&p, &deblk_end)) {
			valid = 1;
			vdir_cache->vd_last.i = i;
			vdir_cache->vd_last.p = p;
			break;
		}
	}

 out:
	//smp_mb();
	//DbgVdir(vdir_cache);
	TraceErr(!valid);
	return valid;
}

int au_fill_de(struct file *file, void *dirent, filldir_t filldir)
{
	int err, l;
	struct dentry *dentry;
	struct aufs_vdir *vdir_cache;
	struct aufs_de *de;
	union aufs_deblk_p deblk_end;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, pos %Ld\n", DLNPair(dentry), file->f_pos);
	vdir_cache = fvdir_cache(file);
	AuDebugOn(!vdir_cache);
	//DbgVdir(vdir_cache);

	if (!seek_vdir(file))
		return 0;

	while (1) {
		deblk_end.deblk
			= vdir_cache->vd_deblk[vdir_cache->vd_last.i] + 1;
		while (!is_deblk_end(&vdir_cache->vd_last.p, &deblk_end)) {
			de = vdir_cache->vd_last.p.de;
			LKTRTrace("%.*s, off%Ld, i%lu, dt%d\n",
				  de->de_str.len, de->de_str.name,
				  file->f_pos, de->de_ino, de->de_type);
			err = filldir(dirent, de->de_str.name, de->de_str.len,
				      file->f_pos, de->de_ino, de->de_type);
			if (unlikely(err)) {
				TraceErr(err);
				//return err;
				//todo: ignore the error caused by udba.
				return 0;
			}

			l = calc_size(de->de_str.len);
			vdir_cache->vd_last.p.p += l;
			file->f_pos += l;
		}
		if (vdir_cache->vd_last.i < vdir_cache->vd_nblk - 1) {
			vdir_cache->vd_last.i++;
			vdir_cache->vd_last.p.deblk
				= vdir_cache->vd_deblk[vdir_cache->vd_last.i];
			file->f_pos = sizeof(*vdir_cache->vd_last.p.deblk)
				* vdir_cache->vd_last.i;
			continue;
		}
		break;
	}

	//smp_mb();
	return 0;
}
