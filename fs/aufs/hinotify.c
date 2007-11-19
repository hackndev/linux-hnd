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

/* $Id: hinotify.c,v 1.23 2007/06/11 01:42:40 sfjro Exp $ */

#include "aufs.h"

static struct inotify_handle *in_handle;
static const __u32 in_mask = (IN_MOVE | IN_DELETE | IN_CREATE /* | IN_ACCESS */
			      | IN_MODIFY | IN_ATTRIB
			      | IN_DELETE_SELF | IN_MOVE_SELF);

int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *hidden_inode)
{
	int err;
	struct aufs_hinotify *hin;
	s32 wd;

	LKTRTrace("i%lu, hi%lu\n", inode->i_ino, hidden_inode->i_ino);

	err = -ENOMEM;
	hin = cache_alloc_hinotify();
	if (hin) {
		AuDebugOn(hinode->hi_notify);
		hinode->hi_notify = hin;
		hin->hin_aufs_inode = inode;
		inotify_init_watch(&hin->hin_watch);
		wd = inotify_add_watch(in_handle, &hin->hin_watch, hidden_inode,
				       in_mask);
		if (wd >= 0)
			return 0; /* success */

		err = wd;
		put_inotify_watch(&hin->hin_watch);
		cache_free_hinotify(hin);
		hinode->hi_notify = NULL;
	}

	TraceErr(err);
	return err;
}

void do_free_hinotify(struct aufs_hinode *hinode)
{
	int err;
	struct aufs_hinotify *hin;

	TraceEnter();

	hin = hinode->hi_notify;
	if (hin) {
		err = 0;
		if (atomic_read(&hin->hin_watch.count))
			err = inotify_rm_watch(in_handle, &hin->hin_watch);

		if (!err) {
			cache_free_hinotify(hin);
			hinode->hi_notify = NULL;
		} else
			IOErr1("failed inotify_rm_watch() %d\n", err);
	}
}

/* ---------------------------------------------------------------------- */

static void ctl_hinotify(struct aufs_hinode *hinode, const __u32 mask)
{
	struct inode *hi;
	struct inotify_watch *watch;

	hi = hinode->hi_inode;
	LKTRTrace("hi%lu, sb %p, 0x%x\n", hi->i_ino, hi->i_sb, mask);
	if (0 && !strcmp(current->comm, "link"))
		dump_stack();
	IMustLock(hi);
	if (!hinode->hi_notify)
		return;

	watch = &hinode->hi_notify->hin_watch;
#if 0
	{
		u32 wd;
		wd = inotify_find_update_watch(in_handle, hi, mask);
		TraceErr(wd);
		// ignore an err;
	}
#else
	watch->mask = mask;
	smp_mb();
#endif
	LKTRTrace("watch %p, mask %u\n", watch, watch->mask);
}

#define suspend_hinotify(hi)	ctl_hinotify(hi, 0)
#define resume_hinotify(hi)	ctl_hinotify(hi, in_mask)

void do_hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		  unsigned int lsc)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d, lsc %d\n", dir->i_ino, bindex, lsc);
	AuDebugOn(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	AuDebugOn(h_dir != hinode->hi_inode);

	hi_lock(h_dir, lsc);
	if (1 /* unlikely(au_flag_test(dir->i_sb, AuFlag_UDBA_HINOTIFY) */)
		suspend_hinotify(hinode);
}

void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d\n", dir->i_ino, bindex);
	AuDebugOn(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	AuDebugOn(h_dir != hinode->hi_inode);

	if (1 /* unlikely(au_flag_test(dir->i_sb, AuFlag_UDBA_HINOTIFY) */)
	    resume_hinotify(hinode);
	i_unlock(h_dir);
}

void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n", DLNPair(h_parents[0]), DLNPair(h_parents[1]));

	vfsub_lock_rename(h_parents[0], h_parents[1]);
	hinode = itoii(dirs[0])->ii_hinode + bindex;
	AuDebugOn(h_parents[0]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
	if (issamedir)
		return;
	hinode = itoii(dirs[1])->ii_hinode + bindex;
	AuDebugOn(h_parents[1]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
}

void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n", DLNPair(h_parents[0]), DLNPair(h_parents[1]));

	hinode = itoii(dirs[0])->ii_hinode + bindex;
	AuDebugOn(h_parents[0]->d_inode != hinode->hi_inode);
	resume_hinotify(hinode);
	if (!issamedir) {
		hinode = itoii(dirs[1])->ii_hinode + bindex;
		AuDebugOn(h_parents[1]->d_inode != hinode->hi_inode);
		resume_hinotify(hinode);
	}
	vfsub_unlock_rename(h_parents[0], h_parents[1]);
}

void au_reset_hinotify(struct inode *inode, unsigned int flags)
{
	aufs_bindex_t bindex, bend;
	struct inode *hi;

	LKTRTrace("i%lu, 0x%x\n", inode->i_ino, flags);

	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		hi = au_h_iptr_i(inode, bindex);
		if (hi) {
			//hi_lock(hi, AUFS_LSC_H_CHILD);
			igrab(hi);
			set_h_iptr(inode, bindex, NULL, 0);
			set_h_iptr(inode, bindex, igrab(hi), flags);
			iput(hi);
			//i_unlock(hi);
		}
	}
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
static char *in_name(u32 mask)
{
#define test_ret(flag)	if (mask & flag) return #flag;
	test_ret(IN_ACCESS);
	test_ret(IN_MODIFY);
	test_ret(IN_ATTRIB);
	test_ret(IN_CLOSE_WRITE);
	test_ret(IN_CLOSE_NOWRITE);
	test_ret(IN_OPEN);
	test_ret(IN_MOVED_FROM);
	test_ret(IN_MOVED_TO);
	test_ret(IN_CREATE);
	test_ret(IN_DELETE);
	test_ret(IN_DELETE_SELF);
	test_ret(IN_MOVE_SELF);
	test_ret(IN_UNMOUNT);
	test_ret(IN_Q_OVERFLOW);
	test_ret(IN_IGNORED);
	return "";
#undef test_ret
}
#else
#define in_name(m) "??"
#endif

static int dec_gen_by_name(struct inode *dir, const char *_name, u32 mask)
{
	int err;
	struct dentry *parent, *child;
	struct inode *inode;
	struct qstr *dname;
	char *name = (void*)_name;
	unsigned int len;

	LKTRTrace("i%lu, %s, 0x%x %s\n",
		  dir->i_ino, name, mask, in_name(mask));

	err = -1;
	parent = d_find_alias(dir);
	if (unlikely(!parent))
		goto out;

#if 0
	if (unlikely(!memcmp(name, AUFS_WH_PFX, AUFS_WH_PFX_LEN)))
		name += AUFS_WH_PFX_LEN;
#endif
	len = strlen(name);
	spin_lock(&dcache_lock);
	list_for_each_entry(child, &parent->d_subdirs, d_u.d_child) {
		dname = &child->d_name;
		if (len == dname->len && !memcmp(dname->name, name, len)) {
			if (unlikely(child == dir->i_sb->s_root)) {
				err = 0;
				break;
			}

			au_digen_dec(child);
#if 1
			//todo: why both are needed
			if (mask & IN_MOVE) {
				spin_lock(&child->d_lock);
				__d_drop(child);
				spin_unlock(&child->d_lock);
			}
#endif

			inode = child->d_inode;
			if (inode)
				au_iigen_dec(inode);
			err = !!inode;

			// todo: the i_nlink of newly created name by link(2)
			// should be updated
			// todo: some nfs dentry doesn't notified at deleteing
			break;
		}
	}
	spin_unlock(&dcache_lock);
	dput(parent);

 out:
	TraceErr(err);
	return err;
}

struct postproc_args {
	struct inode *h_dir, *dir, *h_child_inode;
	char *h_child_name;
	u32 mask;
};

static void dec_gen_by_ino(struct postproc_args *a)
{
	struct super_block *sb;
	aufs_bindex_t bindex, bend, bfound;
	struct xino xino;
	struct inode *cinode;

	TraceEnter();

	sb = a->dir->i_sb;
	AuDebugOn(!au_flag_test(sb, AuFlag_XINO));

	bfound = -1;
	bend = ibend(a->dir);
	for (bindex = ibstart(a->dir); bfound == -1 && bindex <= bend; bindex++)
		if (au_h_iptr_i(a->dir, bindex) == a->h_dir)
			bfound = bindex;
	if (bfound < 0)
		return;

	bindex = find_brindex(sb, itoii(a->dir)->ii_hinode[bfound + 0].hi_id);
	if (bindex < 0)
		return;
	if (unlikely(xino_read(sb, bindex, a->h_child_inode->i_ino, &xino)))
		return;
	cinode = NULL;
	if (xino.ino != AUFS_ROOT_INO)
		cinode = ilookup(sb, xino.ino);
	if (cinode) {
#if 1
		if (1 || a->mask & IN_MOVE) {
			struct dentry *child;
			spin_lock(&dcache_lock);
			list_for_each_entry(child, &cinode->i_dentry, d_alias)
				au_digen_dec(child);
			spin_unlock(&dcache_lock);
		}
#endif
		au_iigen_dec(cinode);
		iput(cinode);
	}
}

static void reset_ino(struct postproc_args *a)
{
	aufs_bindex_t bindex, bend;
	struct super_block *sb;
	struct inode *h_dir;

	sb = a->dir->i_sb;
	bend = ibend(a->dir);
	for (bindex = ibstart(a->dir); bindex <= bend; bindex++) {
		h_dir = au_h_iptr_i(a->dir, bindex);
		if (h_dir && h_dir != a->h_dir)
			xino_write0(sb, bindex, h_dir->i_ino);
		/* ignore this error */
	}
}

static void postproc(void *args)
{
	struct postproc_args *a = args;
	struct super_block *sb;
	struct aufs_vdir *vdir;

	//au_debug_on();
	LKTRTrace("mask 0x%x %s, i%lu, hi%lu, hci%lu\n",
		  a->mask, in_name(a->mask), a->dir->i_ino, a->h_dir->i_ino,
		  a->h_child_inode ? a->h_child_inode->i_ino : 0);
	AuDebugOn(!a->dir);
#if 0//def ForceInotify
	Dbg("mask 0x%x %s, i%lu, hi%lu, hci%lu\n",
		  a->mask, in_name(a->mask), a->dir->i_ino, a->h_dir->i_ino,
		  a->h_child_inode ? a->h_child_inode->i_ino : 0);
#endif

	i_lock(a->dir);
	sb = a->dir->i_sb;
	si_read_lock(sb); // consider write_lock
	ii_write_lock_parent(a->dir);

	/* make dir entries obsolete */
	vdir = ivdir(a->dir);
	if (vdir)
		vdir->vd_jiffy = 0;
	a->dir->i_version++;

	/*
	 * special handling root directory,
	 * sine d_revalidate may not be called later.
	 * main purpose is maintaining i_nlink.
	 */
	if (unlikely(a->dir->i_ino == AUFS_ROOT_INO))
		au_cpup_attr_all(a->dir);

	if (a->h_child_inode && au_flag_test(sb, AuFlag_XINO)) {
		if (a->dir->i_ino != AUFS_ROOT_INO)
			dec_gen_by_ino(a);
	} else if (a->mask & (IN_MOVE_SELF | IN_DELETE_SELF))
		reset_ino(a);

	ii_write_unlock(a->dir);
	si_read_unlock(sb);
	i_unlock(a->dir);

	iput(a->h_child_inode);
	iput(a->h_dir);
	iput(a->dir);
	kfree(a);
	//au_debug_off();
}

static void aufs_inotify(struct inotify_watch *watch, u32 wd, u32 mask,
			 u32 cookie, const char *h_child_name,
			 struct inode *h_child_inode)
{
	struct aufs_hinotify *hinotify;
	struct postproc_args *args;
	int len, wkq_err;
	char *p;
	struct inode *dir;
	//static DECLARE_WAIT_QUEUE_HEAD(wq);

	//au_debug_on();
	LKTRTrace("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
		  watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		  h_child_name ? h_child_name : "",
		  h_child_inode ? h_child_inode->i_ino : 0);
	//au_debug_off();
	//IMustLock(h_dir);
#if 0 //defined(ForceInotify) || defined(DbgInotify)
	Dbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
		  watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		  h_child_name ? h_child_name : "",
		  h_child_inode ? h_child_inode->i_ino : 0);
#endif
	/* if IN_UNMOUNT happens, there must be another bug */
	if (mask & (IN_IGNORED | IN_UNMOUNT)) {
		put_inotify_watch(watch);
		return;
	}
#ifdef DbgInotify
	Dbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
		  watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		  h_child_name ? h_child_name : "",
		  h_child_inode ? h_child_inode->i_ino : 0);
	WARN_ON(1);
#endif

	switch (mask & IN_ALL_EVENTS) {
	case IN_MODIFY:
	case IN_ATTRIB:
		if (h_child_name)
			return;
		break;

	case IN_MOVED_FROM:
	case IN_MOVED_TO:
	case IN_CREATE:
		AuDebugOn(!h_child_name || !h_child_inode);
		break;
	case IN_DELETE:
		/*
		 * aufs never be able to get this child inode.
		 * revalidation should be in d_revalide()
		 * by checking i_nlink, i_generation or d_unhashed().
		 */
		AuDebugOn(!h_child_name);
		break;

	case IN_DELETE_SELF:
	case IN_MOVE_SELF:
		AuDebugOn(h_child_name || h_child_inode);
		break;

	case IN_ACCESS:
	default:
		AuDebugOn(1);
	}

#if 0 //def DbgInotify
	WARN_ON(1);
#endif

	/* iput() will be called in postproc() */
	hinotify = container_of(watch, struct aufs_hinotify, hin_watch);
	AuDebugOn(!hinotify || !hinotify->hin_aufs_inode);
	dir = hinotify->hin_aufs_inode;

	/* force re-lookup in next d_revalidate() */
	if (dir->i_ino != AUFS_ROOT_INO)
		au_iigen_dec(dir);
	len = 0;
	if (h_child_name && dec_gen_by_name(dir, h_child_name, mask))
		len = strlen(h_child_name);

	//wait_event(wq, (args = kmalloc(sizeof(*args), GFP_KERNEL)));
	args = kmalloc(sizeof(*args) + len + 1, GFP_KERNEL);
	if (unlikely(!args)) {
		Err("no memory\n");
		return;
	}
	args->mask = mask;
	args->dir = igrab(dir);
	args->h_dir = igrab(watch->inode);
	args->h_child_inode = NULL;
	if (len) {
		if (h_child_inode)
			args->h_child_inode = igrab(h_child_inode);
		p = (void*)args;
		args->h_child_name = p + sizeof(*args);
		memcpy(args->h_child_name, h_child_name, len + 1);
	}
	//atomic_inc(&stosi(args->dir->i_sb)->si_hinotify);
	wkq_err = au_wkq_nowait(postproc, args, dir->i_sb, /*dlgt*/0);
	if (unlikely(wkq_err))
		Err("wkq %d\n", wkq_err);
}

static void aufs_inotify_destroy(struct inotify_watch *watch)
{
	return;
}

static struct inotify_operations aufs_inotify_ops = {
	.handle_event	= aufs_inotify,
	.destroy_watch	= aufs_inotify_destroy
};

/* ---------------------------------------------------------------------- */

int __init au_inotify_init(void)
{
	in_handle = inotify_init(&aufs_inotify_ops);
	if (!IS_ERR(in_handle))
		return 0;
	TraceErrPtr(in_handle);
	return PTR_ERR(in_handle);
}

void au_inotify_fin(void)
{
	inotify_destroy(in_handle);
}
