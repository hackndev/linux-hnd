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

/* $Id: opts.c,v 1.36 2007/06/04 02:17:35 sfjro Exp $ */

#include <asm/types.h> // a distribution requires
#include <linux/parser.h>
#include "aufs.h"

enum {
	Opt_br,
	Opt_add, Opt_del, Opt_mod, Opt_append, Opt_prepend,
	Opt_idel, Opt_imod,
	Opt_dirwh, Opt_rdcache, Opt_deblk, Opt_nhash,
	Opt_xino, Opt_zxino, Opt_noxino,
	Opt_plink, Opt_noplink, Opt_list_plink, Opt_clean_plink,
	Opt_udba,
	Opt_diropq_a, Opt_diropq_w,
	Opt_warn_perm, Opt_nowarn_perm,
	Opt_findrw_dir, Opt_findrw_br,
	Opt_coo,
	Opt_dlgt, Opt_nodlgt,
	Opt_tail, Opt_ignore, Opt_err
};

static match_table_t options = {
	{Opt_br, "br=%s"},
	{Opt_br, "br:%s"},

	{Opt_add, "add=%d:%s"},
	{Opt_add, "add:%d:%s"},
	{Opt_add, "ins=%d:%s"},
	{Opt_add, "ins:%d:%s"},
	{Opt_append, "append=%s"},
	{Opt_append, "append:%s"},
	{Opt_prepend, "prepend=%s"},
	{Opt_prepend, "prepend:%s"},

	{Opt_del, "del=%s"},
	{Opt_del, "del:%s"},
	//{Opt_idel, "idel:%d"},
	{Opt_mod, "mod=%s"},
	{Opt_mod, "mod:%s"},
	//{Opt_imod, "imod:%d:%s"},

	{Opt_dirwh, "dirwh=%d"},
	{Opt_dirwh, "dirwh:%d"},

	{Opt_xino, "xino=%s"},
	{Opt_xino, "xino:%s"},
	{Opt_noxino, "noxino"},
	//{Opt_zxino, "zxino=%s"},

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	{Opt_plink, "plink"},
	{Opt_noplink, "noplink"},
#ifdef CONFIG_AUFS_DEBUG
	{Opt_list_plink, "list_plink"},
#endif
	{Opt_clean_plink, "clean_plink"},
#endif

	{Opt_udba, "udba=%s"},

	{Opt_diropq_a, "diropq=always"},
	{Opt_diropq_a, "diropq=a"},
	{Opt_diropq_w, "diropq=whiteouted"},
	{Opt_diropq_w, "diropq=w"},

	{Opt_warn_perm, "warn_perm"},
	{Opt_nowarn_perm, "nowarn_perm"},

#ifdef CONFIG_AUFS_DLGT
	{Opt_dlgt, "dlgt"},
	{Opt_nodlgt, "nodlgt"},
#endif

	{Opt_rdcache, "rdcache=%d"},
	{Opt_rdcache, "rdcache:%d"},
#if 0
	{Opt_findrw_dir, "findrw=dir"},
	{Opt_findrw_br, "findrw=br"},

	{Opt_coo, "coo=%s"},

	{Opt_deblk, "deblk=%d"},
	{Opt_deblk, "deblk:%d"},
	{Opt_nhash, "nhash=%d"},
	{Opt_nhash, "nhash:%d"},
#endif

	{Opt_br, "dirs=%s"},
	{Opt_ignore, "debug=%d"},
	{Opt_ignore, "delete=whiteout"},
	{Opt_ignore, "delete=all"},
	{Opt_ignore, "imap=%s"},

	{Opt_err, NULL}
};

/* ---------------------------------------------------------------------- */

#define RW		"rw"
#define RO		"ro"
#define WH		"wh"
#define RR		"rr"
#define NoLinkWH	"nolwh"

static match_table_t brperms = {
	{AuBr_RR, RR},
	{AuBr_RO, RO},
	{AuBr_RW, RW},

	{AuBr_RRWH, RR "+" WH},
	{AuBr_ROWH, RO "+" WH},
	{AuBr_RWNoLinkWH, RW "+" NoLinkWH},

	{AuBr_ROWH, "nfsro"},
	{AuBr_RO, NULL}
};

static int br_perm_val(char *perm)
{
	int val;
	substring_t args[MAX_OPT_ARGS];

	AuDebugOn(!perm || !*perm);
	LKTRTrace("perm %s\n", perm);
	val = match_token(perm, brperms, args);
	TraceErr(val);
	return val;
}

int br_perm_str(char *p, unsigned int len, int brperm)
{
	struct match_token *bp = brperms;

	LKTRTrace("len %d, 0x%x\n", len, brperm);

	while (bp->pattern) {
		if (bp->token == brperm) {
			if (strlen(bp->pattern) < len) {
				strcpy(p, bp->pattern);
				return 0;
			} else
				return -E2BIG;
		}
		bp++;
	}

	return -EIO;
}

/* ---------------------------------------------------------------------- */

static match_table_t udbalevel = {
	{AuFlag_UDBA_REVAL, "reval"},
#ifdef CONFIG_AUFS_HINOTIFY
	{AuFlag_UDBA_INOTIFY, "inotify"},
#endif
	{AuFlag_UDBA_NONE, "none"},
	{-1, NULL}
};

static int udba_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, udbalevel, args);
}

au_parser_pattern_t udba_str(int udba)
{
	struct match_token *p = udbalevel;
	while (p->pattern) {
		if (p->token == udba)
			return p->pattern;
		p++;
	}
	BUG();
	return "??";
}

void udba_set(struct super_block *sb, unsigned int flg)
{
	au_flag_clr(sb, AuMask_UDBA);
	au_flag_set(sb, flg);
}

/* ---------------------------------------------------------------------- */

#if 0
static match_table_t coolevel = {
	{AuFlag_COO_LEAF, "leaf"},
	{AuFlag_COO_ALL, "all"},
	{AuFlag_COO_NONE, "none"},
	{-1, NULL}
};

#if 0
static int coo_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, coolevel, args);
}
#endif

au_parser_pattern_t coo_str(int coo)
{
	struct match_token *p = coolevel;
	while (p->pattern) {
		if (p->token == coo)
			return p->pattern;
		p++;
	}
	BUG();
	return "??";
}
static void coo_set(struct super_block *sb, unsigned int flg)
{
	au_flag_clr(sb, AuMask_COO);
	au_flag_set(sb, flg);
}
#endif

/* ---------------------------------------------------------------------- */

static const int lkup_dirflags = LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

#ifdef CONFIG_AUFS_DEBUG
static void dump_opts(struct opts *opts)
{
	/* reduce stack space */
	union {
		struct opt_add *add;
		struct opt_del *del;
		struct opt_mod *mod;
		struct opt_xino *xino;
	} u;
	struct opt *opt;

	TraceEnter();

	opt = opts->opt;
	while (/* opt < opts_tail && */ opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
			u.add = &opt->add;
			LKTRTrace("add {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_del:
		case Opt_idel:
			u.del = &opt->del;
			LKTRTrace("del {%s, %p}\n", u.del->path, u.del->h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			u.mod = &opt->mod;
			LKTRTrace("mod {%s, 0x%x, %p}\n",
				  u.mod->path, u.mod->perm, u.mod->h_root);
			break;
		case Opt_append:
			u.add = &opt->add;
			LKTRTrace("append {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_prepend:
			u.add = &opt->add;
			LKTRTrace("prepend {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_dirwh:
			LKTRTrace("dirwh %d\n", opt->dirwh);
			break;
		case Opt_rdcache:
			LKTRTrace("rdcache %d\n", opt->rdcache);
			break;
		case Opt_xino:
			u.xino = &opt->xino;
			LKTRTrace("xino {%s %.*s}\n",
				  u.xino->path,
				  DLNPair(u.xino->file->f_dentry));
			break;
		case Opt_noxino:
			LKTRLabel(noxino);
			break;
		case Opt_plink:
			LKTRLabel(plink);
			break;
		case Opt_noplink:
			LKTRLabel(noplink);
			break;
		case Opt_list_plink:
			LKTRLabel(list_plink);
			break;
		case Opt_clean_plink:
			LKTRLabel(clean_plink);
			break;
		case Opt_udba:
			LKTRTrace("udba %d, %s\n",
				  opt->udba, udba_str(opt->udba));
			break;
		case Opt_diropq_a:
			LKTRLabel(diropq_a);
			break;
		case Opt_diropq_w:
			LKTRLabel(diropq_w);
			break;
		case Opt_warn_perm:
			LKTRLabel(warn_perm);
			break;
		case Opt_nowarn_perm:
			LKTRLabel(nowarn_perm);
			break;
		case Opt_dlgt:
			LKTRLabel(dlgt);
			break;
		case Opt_nodlgt:
			LKTRLabel(nodlgt);
			break;
#if 0
		case Opt_coo:
			LKTRTrace("coo %d, %s\n", opt->coo, coo_str(opt->coo));
			break;
#endif
		default:
			BUG();
		}
		opt++;
	}
}
#else
#define dump_opts(opts) do{}while(0)
#endif

void au_free_opts(struct opts *opts)
{
	struct opt *opt;

	TraceEnter();

	opt = opts->opt;
	while (opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
		case Opt_append:
		case Opt_prepend:
			path_release(&opt->add.nd);
			break;
		case Opt_del:
		case Opt_idel:
			dput(opt->del.h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			dput(opt->mod.h_root);
			break;
		case Opt_xino:
			fput(opt->xino.file);
			break;
		}
		opt++;
	}
}

static int opt_add(struct opt *opt, char *opt_str, struct super_block *sb,
		   aufs_bindex_t bindex)
{
	int err;
	struct opt_add *add = &opt->add;
	char *p;

	LKTRTrace("%s, b%d\n", opt_str, bindex);

	add->bindex = bindex;
	add->perm = AuBr_RO;
	if (!bindex && !(sb->s_flags & MS_RDONLY))
		add->perm = AuBr_RW;
#ifdef CONFIG_AUFS_COMPAT
	add->perm = AuBr_RW;
#endif
	add->path = opt_str;
	p = strchr(opt_str, '=');
	if (unlikely(p)) {
		*p++ = 0;
		if (*p)
			add->perm = br_perm_val(p);
	}

	// LSM may detect it
	// do not superio.
	err = path_lookup(add->path, lkup_dirflags, &add->nd);
	//err = -1;
	if (!err) {
		opt->type = Opt_add;
		goto out;
	}
	Err("lookup failed %s (%d)\n", add->path, err);
	err = -EINVAL;

 out:
	TraceErr(err);
	return err;
}

/* called without aufs lock */
int au_parse_opts(struct super_block *sb, char *str, struct opts *opts)
{
	int err, n;
	struct dentry *root;
	struct opt *opt, *opt_tail;
	char *opt_str;
	substring_t args[MAX_OPT_ARGS];
	aufs_bindex_t bindex;
	struct nameidata nd;
	/* reduce stack space */
	union {
		struct opt_del *del;
		struct opt_mod *mod;
		struct opt_xino *xino;
	} u;
	struct file *file;

	LKTRTrace("%s, nopts %d\n", str, opts->max_opt);

	root = sb->s_root;
	err = 0;
	bindex = 0;
	opt = opts->opt;
	opt_tail = opt + opts->max_opt - 1;
	opt->type = Opt_tail;
	while (!err && (opt_str = strsep(&str, ",")) && *opt_str) {
		int token, skipped;
		char *p;
		err = -EINVAL;
		token = match_token(opt_str, options, args);
		LKTRTrace("%s, token %d, args[0]{%p, %p}\n",
			  opt_str, token, args[0].from, args[0].to);

		skipped = 0;
		switch (token) {
		case Opt_br:
			err = 0;
			while (!err && (opt_str = strsep(&args[0].from, ":"))
			       && *opt_str) {
				err = opt_add(opt, opt_str, sb, bindex++);
				//if (LktrCond) err = -1;
				if (unlikely(!err && ++opt > opt_tail)) {
					err = -E2BIG;
					break;
				}
				opt->type = Opt_tail;
				skipped = 1;
			}
			break;
		case Opt_add:
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			err = opt_add(opt, args[1].from, sb, bindex);
			break;
		case Opt_append:
		case Opt_prepend:
			err = opt_add(opt, args[0].from, sb, /*dummy bindex*/1);
			if (!err)
				opt->type = token;
			break;
		case Opt_del:
			u.del = &opt->del;
			u.del->path = args[0].from;
			LKTRTrace("del path %s\n", u.del->path);
			// LSM may detect it
			// do not superio.
			err = path_lookup(u.del->path, lkup_dirflags, &nd);
			if (unlikely(err)) {
				Err("lookup failed %s (%d)\n",
				    u.del->path, err);
				break;
			}
			u.del->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#if 0
		case Opt_idel:
			u.del = &opt->del;
			u.del->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, !AUFS_I_RLOCK);
			if (bindex < 0 || sbend(sb) < bindex) {
				Err("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AUFS_I_RLOCK);
				break;
			}
			err = 0;
			u.del->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AUFS_I_RLOCK);
			break;
#endif

		case Opt_mod:
			u.mod = &opt->mod;
			u.mod->path = args[0].from;
			p = strchr(u.mod->path, '=');
			if (unlikely(!p)) {
				Err("no permssion %s\n", opt_str);
				break;
			}
			*p++ = 0;
			u.mod->perm = br_perm_val(p);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  u.mod->path, u.mod->perm, p);
			// LSM may detect it
			// do not superio.
			err = path_lookup(u.mod->path, lkup_dirflags, &nd);
			if (unlikely(err)) {
				Err("lookup failed %s (%d)\n",
				    u.mod->path, err);
				break;
			}
			u.mod->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#if 0
		case Opt_imod:
			u.mod = &opt->mod;
			u.mod->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, !AUFS_I_RLOCK);
			if (bindex < 0 || sbend(sb) < bindex) {
				Err("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AUFS_I_RLOCK);
				break;
			}
			u.mod->perm = br_perm_val(args[1].from);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  u.mod->path, u.mod->perm, args[1].from);
			err = 0;
			u.mod->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AUFS_I_RLOCK);
			break;
#endif
		case Opt_xino:
			u.xino = &opt->xino;
			file = xino_create(sb, args[0].from, /*silent*/0,
					   /*parent*/NULL);
			err = PTR_ERR(file);
			if (IS_ERR(file))
				break;
			err = -EINVAL;
			if (unlikely(file->f_dentry->d_sb == sb)) {
				fput(file);
				Err("%s must be outside\n", args[0].from);
				break;
			}
			err = 0;
			u.xino->file = file;
			u.xino->path = args[0].from;
			opt->type = token;
			break;

		case Opt_dirwh:
			if (unlikely(match_int(&args[0], &opt->dirwh)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_rdcache:
			if (unlikely(match_int(&args[0], &opt->rdcache)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_noxino:
		case Opt_plink:
		case Opt_noplink:
		case Opt_list_plink:
		case Opt_clean_plink:
		case Opt_diropq_a:
		case Opt_diropq_w:
		case Opt_warn_perm:
		case Opt_nowarn_perm:
		case Opt_dlgt:
		case Opt_nodlgt:
			err = 0;
			opt->type = token;
			break;

		case Opt_udba:
			opt->udba = udba_val(args[0].from);
			if (opt->udba >= 0) {
				err = 0;
				opt->type = token;
			}
			break;

#if 0
		case Opt_coo:
			opt->coo = coo_val(args[0].from);
			if (opt->coo >= 0) {
				err = 0;
				opt->type = token;
			}
			break;
#endif

		case Opt_ignore:
#ifndef CONFIG_AUFS_COMPAT
			Warn("ignored %s\n", opt_str);
#endif
			skipped = 1;
			err = 0;
			break;
		case Opt_err:
			Err("unknown option %s\n", opt_str);
			break;
		}

		if (!err && !skipped) {
			if (unlikely(++opt > opt_tail)) {
				err = -E2BIG;
				opt--;
				opt->type = Opt_tail;
				break;
			}
			opt->type = Opt_tail;
		}
	}

	dump_opts(opts);
	if (unlikely(err))
		au_free_opts(opts);
	TraceErr(err);
	return err;
}

/*
 * returns,
 * plus: processed without an error
 * zero: unprocessed
 */
static int au_do_opt_simple(struct super_block *sb, struct opt *opt,
			    int remount, unsigned int *given)
{
	int err;
	struct aufs_sbinfo *sbinfo = stosi(sb);

	TraceEnter();

	err = 1; /* handled */
	switch (opt->type) {
	case Opt_udba:
		udba_set(sb, opt->udba);
		*given |= opt->udba;
		break;

	case Opt_plink:
		au_flag_set(sb, AuFlag_PLINK);
		*given |= AuFlag_PLINK;
		break;
	case Opt_noplink:
		if (au_flag_test(sb, AuFlag_PLINK))
			au_put_plink(sb);
		au_flag_clr(sb, AuFlag_PLINK);
		*given |= AuFlag_PLINK;
		break;
	case Opt_list_plink:
		if (au_flag_test(sb, AuFlag_PLINK))
			au_list_plink(sb);
		break;
	case Opt_clean_plink:
		if (au_flag_test(sb, AuFlag_PLINK))
			au_put_plink(sb);
		break;

	case Opt_diropq_a:
		au_flag_set(sb, AuFlag_ALWAYS_DIROPQ);
		*given |= AuFlag_ALWAYS_DIROPQ;
		break;
	case Opt_diropq_w:
		au_flag_clr(sb, AuFlag_ALWAYS_DIROPQ);
		*given |= AuFlag_ALWAYS_DIROPQ;
		break;

	case Opt_dlgt:
		au_flag_set(sb, AuFlag_DLGT);
		*given |= AuFlag_DLGT;
		break;
	case Opt_nodlgt:
		au_flag_clr(sb, AuFlag_DLGT);
		*given |= AuFlag_DLGT;
		break;

	case Opt_warn_perm:
		au_flag_set(sb, AuFlag_WARN_PERM);
		*given |= AuFlag_WARN_PERM;
		break;
	case Opt_nowarn_perm:
		au_flag_clr(sb, AuFlag_WARN_PERM);
		*given |= AuFlag_WARN_PERM;
		break;

#if 0
	case Opt_coo:
		coo_set(sb, opt->coo);
		*given |= opt->coo;
		break;
#endif

	case Opt_dirwh:
		sbinfo->si_dirwh = opt->dirwh;
		break;

	case Opt_rdcache:
		sbinfo->si_rdcache = opt->rdcache * HZ;
		break;

	default:
		err = 0;
		break;
	}

	TraceErr(err);
	return err;
}

/*
 * returns tri-state.
 * plus: processed without an error
 * zero: unprocessed
 * minus: error
 */
static int au_do_opt_br(struct super_block *sb, struct opt *opt, int remount,
			int *do_refresh)
{
	int err;

	TraceEnter();

	err = 0;
	switch (opt->type) {
	case Opt_append:
		opt->add.bindex = sbend(sb) + 1;
		goto add;
	case Opt_prepend:
		opt->add.bindex = 0;
	add:
	case Opt_add:
		err = br_add(sb, &opt->add, remount);
		if (!err)
			*do_refresh = err = 1;
		break;

	case Opt_del:
	case Opt_idel:
		err = br_del(sb, &opt->del, remount);
		if (!err)
			*do_refresh = err = 1;
		break;

	case Opt_mod:
	case Opt_imod:
		err = br_mod(sb, &opt->mod, remount, do_refresh);
		if (!err)
			err = 1;
		break;
	}

	TraceErr(err);
	return err;
}

static int au_do_opt_xino(struct super_block *sb, struct opt *opt, int remount,
			  struct opt_xino **opt_xino)
{
	int err;

	TraceEnter();

	err = 0;
	switch (opt->type) {
	case Opt_xino:
		err = xino_set(sb, &opt->xino, remount);
		if (!err)
			*opt_xino = &opt->xino;
		break;
	case Opt_noxino:
		err = xino_clr(sb);
		break;
	}

	TraceErr(err);
	return err;
}

static int verify_opts(struct super_block *sb, int remount)
{
	int err;
	aufs_bindex_t bindex, bend;
	struct aufs_branch *br;
	struct dentry *root;
	struct inode *dir;
	unsigned int do_plink;

	TraceEnter();

	if (unlikely(!(sb->s_flags & MS_RDONLY)
		     && !br_writable(sbr_perm(sb, 0))))
		Warn("first branch should be rw\n");

	err = 0;
	root = sb->s_root;
	dir = sb->s_root->d_inode;
	do_plink = au_flag_test(sb, AuFlag_PLINK);
	bend = sbend(sb);
	for (bindex = 0; !err && bindex <= bend; bindex++) {
		struct inode *h_dir;
		int skip;

		skip = 0;
		h_dir = au_h_iptr_i(dir, bindex);
		br = stobr(sb, bindex);
		br_wh_read_lock(br);
		switch (br->br_perm) {
		case AuBr_RR:
		case AuBr_RO:
		case AuBr_RRWH:
		case AuBr_ROWH:
			skip = (!br->br_wh && !br->br_plink);
			break;

		case AuBr_RWNoLinkWH:
			skip = !br->br_wh;
			if (skip) {
				if (do_plink)
					skip = !!br->br_plink;
				else
					skip = !br->br_plink;
			}
			break;

		case AuBr_RW:
			skip = !!br->br_wh;
			if (skip) {
				if (do_plink)
					skip = !!br->br_plink;
				else
					skip = !br->br_plink;
			}
			break;

		default:
			BUG();
		}
		br_wh_read_unlock(br);

		if (skip)
			continue;

		hdir_lock(h_dir, dir, bindex);
		br_wh_write_lock(br);
		err = init_wh(au_h_dptr_i(root, bindex), br,
			      au_nfsmnt(sb, bindex), sb);
		br_wh_write_unlock(br);
		hdir_unlock(h_dir, dir, bindex);
	}

	TraceErr(err);
	return err;
}

int au_do_opts_mount(struct super_block *sb, struct opts *opts)
{
	int err, do_refresh;
	struct inode *dir;
	struct opt *opt;
	unsigned int flags, given;
	struct opt_xino *opt_xino;
	aufs_bindex_t bend, bindex;

	TraceEnter();
	SiMustWriteLock(sb);
	DiMustWriteLock(sb->s_root);
	dir = sb->s_root->d_inode;
	IiMustWriteLock(dir);

	err = 0;
	given = 0;
	opt_xino = NULL;
	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail)
		err = au_do_opt_simple(sb, opt++, /*remount*/0, &given);
	if (err > 0)
		err = 0;
	else if (unlikely(err < 0))
		goto out;

	/* disable them temporary */
	flags = au_flag_test(sb, AuFlag_XINO | AuMask_UDBA | AuFlag_DLGT);
	au_flag_clr(sb, AuFlag_XINO | AuFlag_DLGT);
	udba_set(sb, AuFlag_UDBA_REVAL);

	do_refresh = 0;
	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail)
		err = au_do_opt_br(sb, opt++, /*remount*/0, &do_refresh);
	if (err > 0)
		err = 0;
	else if (unlikely(err < 0))
		goto out;

	bend = sbend(sb);
	if (unlikely(bend < 0)) {
		err = -EINVAL;
		Err("no branches\n");
		goto out;
	}

	if (flags & AuFlag_XINO)
		au_flag_set(sb, AuFlag_XINO);
	opt = opts->opt;
	while (!err && opt->type != Opt_tail)
		err = au_do_opt_xino(sb, opt++, /*remount*/0, &opt_xino);
	if (unlikely(err))
		goto out;

	//todo: test this error case.
	err = verify_opts(sb, /*remount*/0);
	AuDebugOn(err);
	if (unlikely(err))
		goto out;

	/* enable xino */
	if (au_flag_test(sb, AuFlag_XINO) && !opt_xino) {
		struct file *xino_file = xino_def(sb);
		err = PTR_ERR(xino_file);
		if (IS_ERR(xino_file))
			goto out;

		err = 0;
		for (bindex = 0; !err && bindex <= bend; bindex++)
			err = xino_init(sb, bindex, xino_file,
					/*do_test*/bindex);
		fput(xino_file);
		if (unlikely(err))
			goto out;
	}

	/* restore hinotify */
	udba_set(sb, flags & AuMask_UDBA);
	if (flags & AuFlag_UDBA_INOTIFY)
		au_reset_hinotify(dir, au_hi_flags(dir, 1) & ~AUFS_HI_XINO);

	/* restore dlgt */
	if (flags & AuFlag_DLGT)
		au_flag_set(sb, AuFlag_DLGT);

 out:
	TraceErr(err);
	return err;
}

int au_do_opts_remount(struct super_block *sb, struct opts *opts,
		       int *do_refresh, unsigned int *given)
{
	int err, rerr;
	struct inode *dir;
	struct opt_xino *opt_xino;
	struct opt *opt;
	unsigned int dlgt;

	TraceEnter();
	SiMustWriteLock(sb);
	DiMustWriteLock(sb->s_root);
	dir = sb->s_root->d_inode;
	IiMustWriteLock(dir);
	//AuDebugOn(au_flag_test(sb, AuFlag_UDBA_INOTIFY));

	err = 0;
	*do_refresh = 0;
	*given = 0;
	dlgt = au_flag_test(sb, AuFlag_DLGT);
	opt_xino = NULL;
	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail) {
		err = au_do_opt_simple(sb, opt, /*remount*/1, given);

		/* disable it temporary */
		dlgt = au_flag_test(sb, AuFlag_DLGT);
		au_flag_clr(sb, AuFlag_DLGT);

		if (!err)
			err = au_do_opt_br(sb, opt, /*remount*/1, do_refresh);
		if (!err)
			err = au_do_opt_xino(sb, opt, /*remount*/1, &opt_xino);

		/* restore it */
		au_flag_set(sb, dlgt);
		opt++;
	}
	if (err > 0)
		err = 0;
	TraceErr(err);

	/* go on if err */

	//todo: test this error case.
	au_flag_clr(sb, AuFlag_DLGT);
	rerr = verify_opts(sb, /*remount*/1);
	au_flag_set(sb, dlgt);

	/* they are handled by the caller */
	if (!*do_refresh)
		*do_refresh = !!((*given & AuMask_UDBA)
				 || au_flag_test(sb, AuFlag_XINO));

	TraceErr(err);
	return err;
}
