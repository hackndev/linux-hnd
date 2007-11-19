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

/* $Id: dir.h,v 1.18 2007/05/14 03:41:52 sfjro Exp $ */

#ifndef __AUFS_DIR_H__
#define __AUFS_DIR_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/aufs_type.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#define filldir_ino_t	u64
#else
#define filldir_ino_t	ino_t
#endif

/* ---------------------------------------------------------------------- */

/* need to be faster and smaller */

#define AUFS_DEBLK_SIZE 512 // todo: changable
#define AUFS_NHASH_SIZE	32 // todo: changable
#if AUFS_DEBLK_SIZE < NAME_MAX || PAGE_SIZE < AUFS_DEBLK_SIZE
#error invalid size AUFS_DEBLK_SIZE
#endif

typedef char aufs_deblk_t[AUFS_DEBLK_SIZE];

struct aufs_nhash {
	struct hlist_head heads[AUFS_NHASH_SIZE];
};

struct aufs_destr {
	unsigned char	len;
	char		name[0];
} __attribute__ ((packed));

struct aufs_dehstr {
	struct hlist_node hash;
	struct aufs_destr *str;
};

struct aufs_de {
	ino_t			de_ino;
	unsigned char		de_type;
	//caution: packed
	struct aufs_destr	de_str;
} __attribute__ ((packed));

struct aufs_wh {
	struct hlist_node	wh_hash;
	aufs_bindex_t		wh_bindex;
	struct aufs_destr	wh_str;
} __attribute__ ((packed));

union aufs_deblk_p {
	unsigned char	*p;
	aufs_deblk_t	*deblk;
	struct aufs_de	*de;
};

struct aufs_vdir {
	aufs_deblk_t	**vd_deblk;
	int		vd_nblk;
	struct {
		int			i;
		union aufs_deblk_p	p;
	} vd_last;

	unsigned long	vd_version;
	unsigned long	vd_jiffy;
};

/* ---------------------------------------------------------------------- */

/* dir.c */
extern struct file_operations aufs_dir_fop;
int au_test_empty_lower(struct dentry *dentry);
int test_empty(struct dentry *dentry, struct aufs_nhash *whlist);
void au_add_nlink(struct inode *dir, struct inode *h_dir);
void au_sub_nlink(struct inode *dir, struct inode *h_dir);

/* vdir.c */
struct aufs_nhash *nhash_new(gfp_t gfp);
void nhash_del(struct aufs_nhash *nhash);
void nhash_init(struct aufs_nhash *nhash);
void nhash_move(struct aufs_nhash *dst, struct aufs_nhash *src);
void nhash_fin(struct aufs_nhash *nhash);
int is_longer_wh(struct aufs_nhash *whlist, aufs_bindex_t btgt, int limit);
int test_known_wh(struct aufs_nhash *whlist, char *name, int namelen);
int append_wh(struct aufs_nhash *whlist, char *name, int namelen,
	      aufs_bindex_t bindex);
void free_vdir(struct aufs_vdir *vdir);
int au_init_vdir(struct file *file);
int au_fill_de(struct file *file, void *dirent, filldir_t filldir);

/* ---------------------------------------------------------------------- */

static inline
unsigned int au_name_hash(const unsigned char *name, unsigned int len)
{
	return (full_name_hash(name, len) % AUFS_NHASH_SIZE);
}

#endif /* __KERNEL__ */
#endif /* __AUFS_DIR_H__ */
