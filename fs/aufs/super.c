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

/* $Id: super.c,v 1.53 2007/06/04 02:17:35 sfjro Exp $ */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/smp_lock.h>
#include <linux/statfs.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#include <linux/mnt_namespace.h>
typedef struct mnt_namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->nsproxy
#define au_mnt_ns(tsk)	(tsk)->nsproxy->mnt_ns
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#include <linux/namespace.h>
typedef struct namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->nsproxy
#define au_mnt_ns(tsk)	(tsk)->nsproxy->namespace
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#include <linux/namespace.h>
typedef struct namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->namespace
#define au_mnt_ns(tsk)	(tsk)->namespace
#endif

#include "aufs.h"

/*
 * super_operations
 */
static struct inode *aufs_alloc_inode(struct super_block *sb)
{
	struct aufs_icntnr *c;

	TraceEnter();

	c = cache_alloc_icntnr();
	//if (LktrCond) {cache_free_icntnr(c); c = NULL;}
	if (c) {
		inode_init_once(&c->vfs_inode);
		c->vfs_inode.i_version = 1; //sigen(sb);
		c->iinfo.ii_hinode = NULL;
		return &c->vfs_inode;
	}
	return NULL;
}

static void aufs_destroy_inode(struct inode *inode)
{
	LKTRTrace("i%lu\n", inode->i_ino);
	au_iinfo_fin(inode);
	cache_free_icntnr(container_of(inode, struct aufs_icntnr, vfs_inode));
}

//todo: how about merge with alloc_inode()?
static void aufs_read_inode(struct inode *inode)
{
	int err;

	LKTRTrace("i%lu\n", inode->i_ino);

	err = au_iinfo_init(inode);
	//if (LktrCond) err = -1;
	if (!err) {
		inode->i_version++;
		inode->i_op = &aufs_iop;
		inode->i_fop = &aufs_file_fop;
		inode->i_mapping->a_ops = &aufs_aop;
		return; /* success */
	}

	LKTRTrace("intializing inode info failed(%d)\n", err);
	make_bad_inode(inode);
}

int au_show_brs(struct seq_file *seq, struct super_block *sb)
{
	int err;
	aufs_bindex_t bindex, bend;
	char a[16];
	struct dentry *root;

	TraceEnter();
	SiMustAnyLock(sb);
	root = sb->s_root;
	DiMustAnyLock(root);

	err = 0;
	bend = sbend(sb);
	for (bindex = 0; !err && bindex <= bend; bindex++) {
		err = br_perm_str(a, sizeof(a), sbr_perm(sb, bindex));
		if (!err)
			err = seq_path(seq, sbr_mnt(sb, bindex),
				       au_h_dptr_i(root, bindex), au_esc_chars);
		if (err > 0)
			err = seq_printf(seq, "=%s", a);
		if (!err && bindex != bend)
			err = seq_putc(seq, ':');
	}

	TraceErr(err);
	return err;
}

static int aufs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	int err, n;
	struct super_block *sb;
	struct aufs_sbinfo *sbinfo;
	struct dentry *root;
	struct file *xino;

	TraceEnter();

	sb = mnt->mnt_sb;
	root = sb->s_root;
	aufs_read_lock(root, !AUFS_I_RLOCK);
	if (au_flag_test(sb, AuFlag_XINO)) {
		err = seq_puts(m, ",xino=");
		if (unlikely(err))
			goto out;
		xino = stobr(sb, 0)->br_xino;
		err = seq_path(m, xino->f_vfsmnt, xino->f_dentry, au_esc_chars);
		if (unlikely(err <= 0))
			goto out;
		err = 0;

#define Deleted "\\040(deleted)"
		m->count -= sizeof(Deleted) - 1;
		AuDebugOn(memcmp(m->buf + m->count, Deleted,
				 sizeof(Deleted) - 1));
#undef Deleted
	} else
		err = seq_puts(m, ",noxino");

	n = au_flag_test(sb, AuFlag_PLINK);
	if (unlikely(!err && (AuDefFlags & AuFlag_PLINK) != n))
		err = seq_printf(m, ",%splink", n ? "" : "no");
	n = au_flag_test_udba(sb);
	if (unlikely(!err && (AuDefFlags & AuMask_UDBA) != n))
		err = seq_printf(m, ",udba=%s", udba_str(n));
	n = au_flag_test(sb, AuFlag_ALWAYS_DIROPQ);
	if (unlikely(!err && (AuDefFlags & AuFlag_ALWAYS_DIROPQ) != n))
		err = seq_printf(m, ",diropq=%c", n ? 'a' : 'w');
	n = au_flag_test(sb, AuFlag_DLGT);
	if (unlikely(!err && (AuDefFlags & AuFlag_DLGT) != n))
		err = seq_printf(m, ",%sdlgt", n ? "" : "no");
	n = au_flag_test(sb, AuFlag_WARN_PERM);
	if (unlikely(!err && (AuDefFlags & AuFlag_WARN_PERM) != n))
		err = seq_printf(m, ",%swarn_perm", n ? "" : "no");

	sbinfo = stosi(sb);
	n = sbinfo->si_dirwh;
	if (unlikely(!err && n != AUFS_DIRWH_DEF))
		err = seq_printf(m, ",dirwh=%d", n);
	n = sbinfo->si_rdcache / HZ;
	if (unlikely(!err && n != AUFS_RDCACHE_DEF))
		err = seq_printf(m, ",rdcache=%d", n);
#if 0
	n = au_flag_test_coo(sb);
	if (unlikely(!err && (AuDefFlags & AuMask_COO) != n))
		err = seq_printf(m, ",coo=%s", coo_str(n));
#endif

	if (!err && !sysaufs_brs) {
#ifdef CONFIG_AUFS_COMPAT
		err = seq_puts(m, ",dirs=");
#else
		err = seq_puts(m, ",br:");
#endif
		if (!err)
			err = au_show_brs(m, sb);
	}

 out:
	aufs_read_unlock(root, !AUFS_I_RLOCK);
	TraceErr(err);
	if (err)
		err = -E2BIG;
	TraceErr(err);
	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define StatfsLock(d)	aufs_read_lock((d)->d_sb->s_root, 0)
#define StatfsUnlock(d)	aufs_read_unlock((d)->d_sb->s_root, 0)
#define StatfsArg(d)	au_h_dptr((d)->d_sb->s_root)
#define StatfsHInode(d)	(StatfsArg(d)->d_inode)
#define StatfsSb(d)	((d)->d_sb)
static int aufs_statfs(struct dentry *arg, struct kstatfs *buf)
#else
#define StatfsLock(s)	si_read_lock(s)
#define StatfsUnlock(s)	si_read_unlock(s)
#define StatfsArg(s)	sbr_sb(s, 0)
#define StatfsHInode(s)	(StatfsArg(s)->s_root->d_inode)
#define StatfsSb(s)	(s)
static int aufs_statfs(struct super_block *arg, struct kstatfs *buf)
#endif
{
	int err;

	TraceEnter();

	StatfsLock(arg);
	err = vfsub_statfs(StatfsArg(arg), buf, need_dlgt(StatfsSb(arg)));
	//if (LktrCond) err = -1;
	StatfsUnlock(arg);
	if (!err) {
		//buf->f_type = AUFS_SUPER_MAGIC;
		buf->f_type = 0;
		buf->f_namelen -= AUFS_WH_PFX_LEN;
		memset(&buf->f_fsid, 0, sizeof(buf->f_fsid));
	}
	//buf->f_bsize = buf->f_blocks = buf->f_bfree = buf->f_bavail = -1;

	TraceErr(err);
	return err;
}

static void au_update_mnt(struct vfsmount *mnt)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
	struct vfsmount *pos;
	struct super_block *sb = mnt->mnt_sb;
	struct dentry *root = sb->s_root;
	struct aufs_sbinfo *sbinfo = stosi(sb);
	au_mnt_ns_t *ns;

	TraceEnter();
	AuDebugOn(!kernel_locked());

	if (sbinfo->si_mnt != mnt
	    || atomic_read(&sb->s_active) == 1
	    || !au_nsproxy(current))
		return;

	/* no get/put */
	ns = au_mnt_ns(current);
	AuDebugOn(!ns);
	sbinfo->si_mnt = NULL;
	list_for_each_entry(pos, &ns->list, mnt_list)
		if (pos != mnt && pos->mnt_sb->s_root == root) {
			sbinfo->si_mnt = pos;
			break;
		}
	AuDebugOn(!sbinfo->si_mnt);
#endif
}

#define UmountBeginHasMnt \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18) || defined(UbuntuEdgy17Umount18))

#if UmountBeginHasMnt
#define UmountBeginSb(mnt)	(mnt)->mnt_sb
#define UmountBeginMnt(mnt)	(mnt)
static void aufs_umount_begin(struct vfsmount *arg, int flags)
#else
#define UmountBeginSb(sb)	sb
#define UmountBeginMnt(sb)	NULL
static void aufs_umount_begin(struct super_block *arg)
#endif
{
	struct super_block *sb = UmountBeginSb(arg);
	struct vfsmount *mnt = UmountBeginMnt(arg);
	struct aufs_sbinfo *sbinfo;

	TraceEnter();

	sbinfo = stosi(sb);
	if (unlikely(!sbinfo))
		return;

#if 1
	flush_scheduled_work();
#else
	wait_event(sbinfo->si_wkq_nowait_wq,
		   !atomic_read(&sbinfo->si_wkq_nowait));
#endif
	si_write_lock(sb);
	if (au_flag_test(sb, AuFlag_PLINK))
		au_put_plink(sb);
#if 0
	if (unlikely(au_flag_test(sb, AuFlag_UDBA_INOTIFY)))
		shrink_dcache_sb(sb);
#endif
	au_update_mnt(mnt);
	si_write_unlock(sb);
}

static void free_sbinfo(struct aufs_sbinfo *sbinfo)
{
	TraceEnter();
	AuDebugOn(!sbinfo
		  || !list_empty(&sbinfo->si_plink)
		  /* || atomic_read(&sbinfo->si_wkq_nowait) */
		);

	free_branches(sbinfo);
	kfree(sbinfo->si_branch);
	//destroy_workqueue(sbinfo->si_wkq);
	kfree(sbinfo);
}

/* final actions when unmounting a file system */
static void aufs_put_super(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	TraceEnter();

	sbinfo = stosi(sb);
	if (unlikely(!sbinfo))
		return;

	sysaufs_del(sbinfo);

#if !UmountBeginHasMnt
	// umount_begin() may not be called.
	aufs_umount_begin(sb);
#endif
	free_sbinfo(sbinfo);
}

/* ---------------------------------------------------------------------- */

/*
 * refresh directories at remount time.
 */
static int do_refresh_dir(struct dentry *dentry, unsigned int flags)
{
	int err;
	struct dentry *parent;
	struct inode *inode;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	inode = dentry->d_inode;
	AuDebugOn(!inode || !S_ISDIR(inode->i_mode));

	di_write_lock_child(dentry);
	parent = dget_parent(dentry);
	di_read_lock_parent(parent, AUFS_I_RLOCK);
	err = au_refresh_hdentry(dentry, S_IFDIR);
	if (err >= 0) {
		err = au_refresh_hinode(inode, dentry);
		if (!err)
			au_reset_hinotify(inode, flags);
	}
	if (unlikely(err))
		Err("unrecoverable error %d\n", err);
	di_read_unlock(parent, AUFS_I_RLOCK);
	dput(parent);
	di_write_unlock(dentry);

	TraceErr(err);
	return err;
}

static int test_dir(struct dentry *dentry, void *arg)
{
	return S_ISDIR(dentry->d_inode->i_mode);
}

static int refresh_dir(struct dentry *root, int sgen)
{
	int err, i, j, ndentry;
	const unsigned int flags = au_hi_flags(root->d_inode, /*isdir*/1);
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry **dentries;

	LKTRTrace("sgen %d\n", sgen);
	SiMustWriteLock(root->d_sb);
	AuDebugOn(au_digen(root) != sgen);
	DiMustWriteLock(root);

	err = au_dpages_init(&dpages, GFP_KERNEL);
	if (unlikely(err))
		goto out;
	err = au_dcsub_pages(&dpages, root, test_dir, NULL);
	if (unlikely(err))
		goto out_dpages;

	DiMustNoWaiters(root);
	IiMustNoWaiters(root->d_inode);
	di_write_unlock(root);
	for (i = 0; !err && i < dpages.ndpage; i++) {
		dpage = dpages.dpages + i;
		dentries = dpage->dentries;
		ndentry = dpage->ndentry;
		for (j = 0; !err && j < ndentry; j++) {
			struct dentry *d;
			d = dentries[j];
#ifdef CONFIG_AUFS_DEBUG
			{
				struct dentry *parent;
				parent = dget_parent(d);
				AuDebugOn(!S_ISDIR(d->d_inode->i_mode)
					  || IS_ROOT(d)
					  || au_digen(parent) != sgen);
				dput(parent);
			}
#endif
			if (au_digen(d) != sgen)
				err = do_refresh_dir(d, flags);
		}
	}
	di_write_lock_child(root); /* aufs_write_lock() calls ..._child() */

 out_dpages:
	au_dpages_free(&dpages);
 out:
	TraceErr(err);
	return err;
}

/* stop extra interpretation of errno in mount(8), and strange error messages */
static int cvt_err(int err)
{
	TraceErr(err);

	switch (err) {
	case -ENOENT:
	case -ENOTDIR:
	case -EEXIST:
	case -EIO:
		err = -EINVAL;
	}
	return err;
}

/* protected by s_umount */
static int aufs_remount_fs(struct super_block *sb, int *flags, char *data)
{
	int err, do_refresh;
	struct dentry *root;
	struct inode *inode;
	struct opts opts;
	unsigned int given, dlgt;
	struct aufs_sbinfo *sbinfo;

	//au_debug_on();
	LKTRTrace("flags 0x%x, data %s, len %lu\n",
		  *flags, data ? data : "NULL",
		  (unsigned long)(data ? strlen(data) : 0));

	err = 0;
	if (unlikely(!data || !*data))
		goto out; /* success */

	err = -ENOMEM;
	memset(&opts, 0, sizeof(opts));
	opts.opt = (void*)__get_free_page(GFP_KERNEL);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;
	opts.max_opt = PAGE_SIZE / sizeof(*opts.opt);

	/* parse it before aufs lock */
	err = au_parse_opts(sb, data, &opts);
	//if (LktrCond) {au_free_opts(&opts); err = -1;}
	if (unlikely(err))
		goto out_opts;

	sbinfo = stosi(sb);
	//Dbg("%d\n", atomic_read(&sbinfo->si_wkq_nowait));
#if 1
	flush_scheduled_work();
#else
	err = wait_event_interruptible(sbinfo->si_wkq_nowait_wq,
				       !atomic_read(&sbinfo->si_wkq_nowait));
	if (unlikely(err))
		goto out_opts;
#endif

	root = sb->s_root;
	inode = root->d_inode;
	i_lock(inode);
	aufs_write_lock(root);

	//DbgSleep(3);

	/* au_do_opts() may return an error */
	do_refresh = 0;
	given = 0;
	err = au_do_opts_remount(sb, &opts, &do_refresh, &given);
	//if (LktrCond) err = -1;
	au_free_opts(&opts);

	if (do_refresh) {
		int rerr;

		dlgt = au_flag_test(sb, AuFlag_DLGT);
		au_flag_clr(sb, AuFlag_DLGT);
		au_sigen_inc(sb);
		au_reset_hinotify(inode, au_hi_flags(inode, /*isdir*/1));
		sbinfo->si_failed_refresh_dirs = 0;
		rerr = refresh_dir(root, au_sigen(sb));
		if (unlikely(rerr)) {
			sbinfo->si_failed_refresh_dirs = 1;
			Warn("Refreshing directories failed, ignores (%d)\n",
			     rerr);
		}
		au_cpup_attr_all(inode);
		au_flag_set(sb, dlgt);
	}

	aufs_write_unlock(root);
	i_unlock(inode);
	if (do_refresh)
		sysaufs_notify_remount();

 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	err = cvt_err(err);
	TraceErr(err);
	//au_debug_off();
	return err;
}

static struct super_operations aufs_sop = {
	.alloc_inode	= aufs_alloc_inode,
	.destroy_inode	= aufs_destroy_inode,
	.read_inode	= aufs_read_inode,
	//.dirty_inode	= aufs_dirty_inode,
	//.write_inode	= aufs_write_inode,
	//void (*put_inode) (struct inode *);
	.drop_inode	= generic_delete_inode,
	//.delete_inode	= aufs_delete_inode,
	//.clear_inode	= aufs_clear_inode,

	.show_options	= aufs_show_options,
	.statfs		= aufs_statfs,

	.put_super	= aufs_put_super,
	//void (*write_super) (struct super_block *);
	//int (*sync_fs)(struct super_block *sb, int wait);
	//void (*write_super_lockfs) (struct super_block *);
	//void (*unlockfs) (struct super_block *);
	.remount_fs	= aufs_remount_fs,
	// depends upon umount flags. also use put_super() (< 2.6.18)
	.umount_begin	= aufs_umount_begin
};

/* ---------------------------------------------------------------------- */

/*
 * at first mount time.
 */

static int alloc_sbinfo(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	TraceEnter();

	sbinfo = kmalloc(sizeof(*sbinfo), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo); sbinfo = NULL;}
	if (unlikely(!sbinfo))
		goto out;
	sbinfo->si_branch = kzalloc(sizeof(*sbinfo->si_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo->si_branch); sbinfo->si_branch = NULL;}
	if (unlikely(!sbinfo->si_branch))
		goto out_sbinfo;

	rw_init_wlock(&sbinfo->si_rwsem);
	sbinfo->si_generation = 0;
	sbinfo->si_failed_refresh_dirs = 0;
	sbinfo->si_bend = -1;
	sbinfo->si_last_br_id = 0;
	sbinfo->si_flags = AuDefFlags;
	atomic_long_set(&sbinfo->si_xino, AUFS_FIRST_INO);
#if 0
	sbinfo->si_wkq = create_workqueue(AUFS_WKQ_NAME);
	if (IS_ERR(sbinfo->si_wkq))
		goto out_branch;
#endif
#if 0
	atomic_set(&sbinfo->si_wkq_nowait, 0);
	init_waitqueue_head(&sbinfo->si_wkq_nowait_wq);
#endif
	sbinfo->si_rdcache = AUFS_RDCACHE_DEF * HZ;
	sbinfo->si_dirwh = AUFS_DIRWH_DEF;
	spin_lock_init(&sbinfo->si_plink_lock);
	INIT_LIST_HEAD(&sbinfo->si_plink);
	init_lvma(sbinfo);

	sb->s_fs_info = sbinfo;
#ifdef ForceInotify
	udba_set(sb, AuFlag_UDBA_INOTIFY);
#endif
#ifdef ForceDlgt
	au_flag_set(sb, AuFlag_DLGT);
#endif
#ifdef ForceNoPlink
	au_flag_clr(sb, AuFlag_PLINK);
#endif
	return 0; /* success */

	//out_branch:
	kfree(sbinfo->si_branch);
 out_sbinfo:
	kfree(sbinfo);
 out:
	TraceErr(-ENOMEM);
	return -ENOMEM;
}

static int alloc_root(struct super_block *sb)
{
	int err;
	struct inode *inode;
	struct dentry *root;

	TraceEnter();

	err = -ENOMEM;
	inode = iget(sb, AUFS_ROOT_INO);
	//if (LktrCond) {iput(inode); inode = NULL;}
	if (unlikely(!inode))
		goto out;
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;
	err = -ENOMEM;
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;

	root = d_alloc_root(inode);
	//if (LktrCond) {igrab(inode); dput(root); root = NULL;}
	if (unlikely(!root))
		goto out_iput;
	err = PTR_ERR(root);
	if (IS_ERR(root))
		goto out_iput;

	err = au_alloc_dinfo(root);
	//if (LktrCond){rw_write_unlock(&dtodi(root)->di_rwsem);err=-1;}
	if (!err) {
		sb->s_root = root;
		return 0; /* success */
	}
	dput(root);
	goto out; /* do not iput */

 out_iput:
	iput(inode);
 out:
	TraceErr(err);
	return err;

}

static int aufs_fill_super(struct super_block *sb, void *raw_data, int silent)
{
	int err;
	struct dentry *root;
	struct inode *inode;
	struct opts opts;
	char *arg = raw_data;

	//au_debug_on();
	if (unlikely(!arg || !*arg)) {
		err = -EINVAL;
		Err("no arg\n");
		goto out;
	}
	LKTRTrace("%s, silent %d\n", arg, silent);

	err = -ENOMEM;
	memset(&opts, 0, sizeof(opts));
	opts.opt = (void*)__get_free_page(GFP_KERNEL);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;
	opts.max_opt = PAGE_SIZE / sizeof(*opts.opt);

	err = alloc_sbinfo(sb);
	//if (LktrCond) {si_write_unlock(sb);free_sbinfo(stosi(sb));err=-1;}
	if (unlikely(err))
		goto out_opts;
	SiMustWriteLock(sb);
	/* all timestamps always follow the ones on the branch */
	sb->s_flags |= MS_NOATIME | MS_NODIRATIME;
	sb->s_op = &aufs_sop;
	au_init_export_op(sb);

	err = alloc_root(sb);
	//if (LktrCond) {rw_write_unlock(&dtodi(sb->s_root)->di_rwsem);
	//dput(sb->s_root);sb->s_root=NULL;err=-1;}
	if (unlikely(err)) {
		AuDebugOn(sb->s_root);
		si_write_unlock(sb);
		goto out_info;
	}
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	inode->i_nlink = 2;

	/*
	 * actually we can parse options regardless aufs lock here.
	 * but at remount time, parsing must be done before aufs lock.
	 * so we follow the same rule.
	 */
	ii_write_lock_parent(inode);
	aufs_write_unlock(root);
	err = au_parse_opts(sb, arg, &opts);
	//if (LktrCond) {au_free_opts(&opts); err = -1;}
	if (unlikely(err))
		goto out_root;

	/* lock vfs_inode first, then aufs. */
	i_lock(inode);
	inode->i_op = &aufs_dir_iop;
	inode->i_fop = &aufs_dir_fop;
	aufs_write_lock(root);

	sb->s_maxbytes = 0;
	err = au_do_opts_mount(sb, &opts);
	//if (LktrCond) err = -1;
	au_free_opts(&opts);
	if (unlikely(err))
		goto out_unlock;
	AuDebugOn(!sb->s_maxbytes);

	//DbgDentry(root);
	aufs_write_unlock(root);
	i_unlock(inode);
	//DbgSb(sb);
	goto out_opts; /* success */

 out_unlock:
	aufs_write_unlock(root);
	i_unlock(inode);
 out_root:
	dput(root);
	sb->s_root = NULL;
 out_info:
	free_sbinfo(stosi(sb));
	sb->s_fs_info = NULL;
 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	TraceErr(err);
	err = cvt_err(err);
	TraceErr(err);
	//au_debug_off();
	return err;
}

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static int aufs_get_sb(struct file_system_type *fs_type, int flags,
		       const char *dev_name, void *raw_data,
		       struct vfsmount *mnt)
{
	int err;

	/* all timestamps always follow the ones on the branch */
	//mnt->mnt_flags |= MNT_NOATIME | MNT_NODIRATIME;
	err = get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super, mnt);
	if (!err) {
		struct aufs_sbinfo *sbinfo = stosi(mnt->mnt_sb);
		sbinfo->si_mnt = mnt;
		sysaufs_add(sbinfo);
	}
	return err;
}
#else
static struct super_block *aufs_get_sb(struct file_system_type *fs_type,
				       int flags, const char *dev_name,
				       void *raw_data)
{
	return get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super);
}
#endif

struct file_system_type aufs_fs_type = {
	.name		= AUFS_FSTYPE,
	.fs_flags	= FS_REVAL_DOT, // for UDBA and NFS branch
	.get_sb		= aufs_get_sb,
	.kill_sb	= generic_shutdown_super,
	//no need to __module_get() and module_put().
	.owner		= THIS_MODULE,
};
