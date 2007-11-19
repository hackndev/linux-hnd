/*
 *  linux/bootldr/copy.h
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 07, 2003
 *
 *  Copy functions for LAB
 */

#ifndef _LAB_COPY_H
#define _LAB_COPY_H

struct lab_src;
struct lab_dest;
struct lab_unlink;

struct lab_srclist {
	struct lab_src *src;
	struct lab_srclist *next;
};

struct lab_destlist {
	struct lab_dest *dest;
	struct lab_destlist *next;
};

struct lab_unlinklist {
	struct lab_unlink *unlink;
	struct lab_unlinklist *next;
};

struct lab_src {
	char* name;
	int (*check)(char* fn);
	unsigned char *(*get)(int* count,char* fn);
};

struct lab_dest {
	char* name;
	int (*check)(char* fn);
	int (*put)(int count,unsigned char* data,char* fn);
};

struct lab_unlink {
	char *name;
	int (*check)(char* fn);
	int (*unlink)(char* fn);
};

void lab_copy(char *source, char *sourcefile, char *dest, char *destfile);
void lab_copy_addsrc(char *name, int(*check)(char *fn), unsigned char*(*get)(int* count,char *fn));
void lab_copy_adddest(char *name, int(*check)(char *fn), int(*put)(int count,unsigned char *data,char *fn));
void lab_copy_addunlink(char *name, int(*check)(char *fn), int(*unlink)(char *fn));

#endif /* _LAB_COPY_H */
