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

/* $Id: dcsub.c,v 1.5 2007/06/04 02:15:32 sfjro Exp $ */

#include "aufs.h"

static void au_dpage_free(struct au_dpage *dpage)
{
	int i;

	TraceEnter();
	AuDebugOn(!dpage);

	for (i = 0; i < dpage->ndentry; i++)
		dput(dpage->dentries[i]);
	free_page((unsigned long)dpage->dentries);
}

int au_dpages_init(struct au_dcsub_pages *dpages, gfp_t gfp)
{
	int err;
	void *p;

	TraceEnter();

	err = -ENOMEM;
	dpages->dpages = kmalloc(sizeof(*dpages->dpages), gfp);
	if (unlikely(!dpages->dpages))
		goto out;
	p = (void*)__get_free_page(gfp);
	if (unlikely(!p))
		goto out_dpages;
	dpages->dpages[0].ndentry = 0;
	dpages->dpages[0].dentries = p;
	dpages->ndpage = 1;
	return 0; /* success */

 out_dpages:
	kfree(dpages->dpages);
 out:
	TraceErr(err);
	return err;
}

void au_dpages_free(struct au_dcsub_pages *dpages)
{
	int i;

	TraceEnter();

	for (i = 0; i < dpages->ndpage; i++)
		au_dpage_free(dpages->dpages + i);
	kfree(dpages->dpages);
}

static int au_dpages_append(struct au_dcsub_pages *dpages,
			    struct dentry *dentry, gfp_t gfp)
{
	int err, sz;
	struct au_dpage *dpage;
	void *p;

	//TraceEnter();

	dpage = dpages->dpages + dpages->ndpage - 1;
	AuDebugOn(!dpage);
	sz = PAGE_SIZE / sizeof(dentry);
	if (unlikely(dpage->ndentry >= sz)) {
		LKTRLabel(new dpage);
		err = -ENOMEM;
		sz = dpages->ndpage * sizeof(*dpages->dpages);
		p = au_kzrealloc(dpages->dpages, sz,
				 sz + sizeof(*dpages->dpages), gfp);
		if (unlikely(!p))
			goto out;
		dpage = dpages->dpages + dpages->ndpage;
		p = (void*)__get_free_page(gfp);
		if (unlikely(!p))
			goto out;
		dpage->ndentry = 0;
		dpage->dentries = p;
		dpages->ndpage++;
	}

	dpage->dentries[dpage->ndentry++] = dget(dentry);
	return 0; /* success */

 out:
	//TraceErr(err);
	return err;
}

int au_dcsub_pages(struct au_dcsub_pages *dpages, struct dentry *root,
		   au_dpages_test test, void *arg)
{
	int err;
	struct dentry *this_parent = root;
	struct list_head *next;
	struct super_block *sb = root->d_sb;

	TraceEnter();

	err = 0;
	spin_lock(&dcache_lock);
 repeat:
	next = this_parent->d_subdirs.next;
 resume:
	if (this_parent->d_sb == sb
	    && !IS_ROOT(this_parent)
	    && atomic_read(&this_parent->d_count)
	    && this_parent->d_inode
	    && (!test || test(this_parent, arg))) {
		err = au_dpages_append(dpages, this_parent, GFP_ATOMIC);
		if (unlikely(err))
			goto out;
	}

	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, D_CHILD);
		next = tmp->next;
		if (unlikely(/*d_unhashed(dentry) || */!dentry->d_inode))
			continue;
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}
		if (dentry->d_sb == sb
		    && atomic_read(&dentry->d_count)
		    && (!test || test(dentry, arg))) {
			err = au_dpages_append(dpages, dentry, GFP_ATOMIC);
			if (unlikely(err))
				goto out;
		}
	}

	if (this_parent != root) {
		next = this_parent->D_CHILD.next;
		this_parent = this_parent->d_parent;
		goto resume;
	}
 out:
	spin_unlock(&dcache_lock);
#if 0
	if (!err) {
		int i, j;
		j = 0;
		for (i = 0; i < dpages->ndpage; i++) {
			if ((dpages->dpages + i)->ndentry)
				Dbg("%d: %d\n",
				    i, (dpages->dpages + i)->ndentry);
			j += (dpages->dpages + i)->ndentry;
		}
		if (j)
			Dbg("ndpage %d, %d\n", dpages->ndpage, j);
	}
#endif
	TraceErr(err);
	return err;
}

int au_dcsub_pages_rev(struct au_dcsub_pages *dpages, struct dentry *dentry,
		       int do_include, au_dpages_test test, void *arg)
{
	int err;

	TraceEnter();

	err = 0;
	spin_lock(&dcache_lock);
	if (do_include && (!test || test(dentry, arg))) {
		err = au_dpages_append(dpages, dentry, GFP_ATOMIC);
		if (unlikely(err))
			goto out;
	}
	while (!IS_ROOT(dentry)) {
		dentry = dentry->d_parent;
		if (!test || test(dentry, arg)) {
			err = au_dpages_append(dpages, dentry, GFP_ATOMIC);
			if (unlikely(err))
				break;
		}
	}

 out:
	spin_unlock(&dcache_lock);

	TraceErr(err);
	return err;
}

int au_is_subdir(struct dentry *d1, struct dentry *d2)
{
	int err;
#ifndef AuUse_ISSUBDIR
	int i, j;
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry **dentries;
#endif

	LKTRTrace("%.*s, %.*s\n", DLNPair(d1), DLNPair(d2));

#ifdef AuUse_ISSUBDIR
	spin_lock(&dcache_lock);
	err = is_subdir(d1, d2);
	spin_unlock(&dcache_lock);
#else
	err = au_dpages_init(&dpages, GFP_KERNEL);
	if (unlikely(err))
		goto out;
	err = au_dcsub_pages_rev(&dpages, d1, /*do_include*/1, NULL, NULL);
	if (unlikely(err))
		goto out_dpages;

	for (i = dpages.ndpage - 1; !err && i >= 0; i--) {
		dpage = dpages.dpages + i;
		dentries = dpage->dentries;
		for (j = dpage->ndentry - 1; !err && j >= 0; j--) {
			struct dentry *d;
			d = dentries[j];
			//Dbg("d %.*s\n", DLNPair(d));
			err = (d == d2);
		}
	}

 out_dpages:
	au_dpages_free(&dpages);
 out:
#endif
	TraceErr(err);
	return err;
}
