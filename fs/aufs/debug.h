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

/* $Id: debug.h,v 1.33 2007/06/04 02:17:34 sfjro Exp $ */

#ifndef __AUFS_DEBUG_H__
#define __AUFS_DEBUG_H__

#ifdef __KERNEL__

#include <linux/fs.h>

#ifdef CONFIG_AUFS_DEBUG
#define AuDebugOn(a)		BUG_ON(a)
extern atomic_t aufs_cond;
#define au_debug_on()		atomic_inc(&aufs_cond)
#define au_debug_off()		atomic_dec(&aufs_cond)
#define au_is_debug()		atomic_read(&aufs_cond)
#else
#define AuDebugOn(a)		do{}while(0)
#define au_debug_on()		do{}while(0)
#define au_debug_off()		do{}while(0)
#define au_is_debug()		0
#endif

#define MtxMustLock(mtx)	AuDebugOn(!mutex_is_locked(mtx))

/* ---------------------------------------------------------------------- */

/* debug print */
#if defined(CONFIG_LKTR) || defined(CONFIG_LKTR_MODULE)
#include <linux/lktr.h>
#ifdef CONFIG_AUFS_DEBUG
#undef LktrCond
#define LktrCond	unlikely((lktr_cond && lktr_cond()) || au_is_debug())
#endif
#else
#define LktrCond			au_is_debug()
#define LKTRDumpVma(pre, vma, suf)	do{}while(0)
#define LKTRDumpStack()			do{}while(0)
#define LKTRTrace(fmt, args...) do { \
	if (LktrCond) \
		Dbg(fmt, ##args); \
} while (0)
#define LKTRLabel(label)		LKTRTrace("%s\n", #label)
#endif /* CONFIG_LKTR */

#define TraceErr(e) do { \
	if (unlikely((e) < 0)) \
		LKTRTrace("err %d\n", (int)(e)); \
} while (0)
#define TraceErrPtr(p) do { \
	if (IS_ERR(p)) \
		LKTRTrace("err %ld\n", PTR_ERR(p)); \
} while (0)
#define TraceEnter()	LKTRLabel(enter)

/* dirty macros for debug print, use with "%.*s" and caution */
#define LNPair(qstr)		(qstr)->len,(qstr)->name
#define DLNPair(d)		LNPair(&(d)->d_name)

/* ---------------------------------------------------------------------- */

#define Dpri(lvl, fmt, arg...) \
	printk(lvl AUFS_NAME " %s:%d:%s[%d]: " fmt, \
	       __func__, __LINE__, current->comm, current->pid, ##arg)
#define Dbg(fmt, arg...)	Dpri(KERN_DEBUG, fmt, ##arg)
#define Warn(fmt, arg...)	Dpri(KERN_WARNING, fmt, ##arg)
#define Warn1(fmt, arg...) do { \
	static unsigned char c; \
	if (!c++) Warn(fmt, ##arg); \
	} while (0)
#define Err(fmt, arg...)	Dpri(KERN_ERR, fmt, ##arg)
#define Err1(fmt, arg...) do { \
	static unsigned char c; \
	if (!c++) Err(fmt, ##arg); \
	} while (0)
#define IOErr(fmt, arg...)	Err("I/O Error, " fmt, ##arg)
#define IOErr1(fmt, arg...) do { \
	static unsigned char c; \
	if (!c++) IOErr(fmt, ##arg); \
	} while (0)
#define IOErrWhck(fmt, arg...)	Err("I/O Error, try whck. " fmt, ##arg)

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
struct aufs_nhash;
void au_dpri_whlist(struct aufs_nhash *whlist);
struct aufs_vdir;
void au_dpri_vdir(struct aufs_vdir *vdir);
void au_dpri_inode(struct inode *inode);
void au_dpri_dentry(struct dentry *dentry);
void au_dpri_file(struct file *filp);
void au_dpri_sb(struct super_block *sb);
void au_dbg_sleep(int sec);
#define DbgWhlist(w)		do{LKTRTrace(#w "\n"); au_dpri_whlist(w);}while(0)
#define DbgVdir(v)		do{LKTRTrace(#v "\n"); au_dpri_vdir(v);}while(0)
#define DbgInode(i)		do{LKTRTrace(#i "\n"); au_dpri_inode(i);}while(0)
#define DbgDentry(d)		do{LKTRTrace(#d "\n"); au_dpri_dentry(d);}while(0)
#define DbgFile(f)		do{LKTRTrace(#f "\n"); au_dpri_file(f);}while(0)
#define DbgSb(sb)		do{LKTRTrace(#sb "\n"); au_dpri_sb(sb);}while(0)
#define DbgSleep(sec)		do{Dbg("sleep %d sec\n", sec); au_dbg_sleep(sec);}while(0)
#else
#define DbgWhlist(w)		do{}while(0)
#define DbgVdir(v)		do{}while(0)
#define DbgInode(i)		do{}while(0)
#define DbgDentry(d)		do{}while(0)
#define DbgFile(f)		do{}while(0)
#define DbgSb(sb)		do{}while(0)
#define DbgSleep(sec)		do{}while(0)
#endif

#endif /* __KERNEL__ */
#endif /* __AUFS_DEBUG_H__ */
