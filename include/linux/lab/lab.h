/*
 *  include/linux/lab/lab.h
 *
 *  Copyright (C) 2004 Andrew Zabolotny
 *
 *  Linux As Bootloader public interface for LAB drivers.
 */

#ifndef _LAB_H
#define _LAB_H

#include <linux/types.h>
#include <linux/string.h>

/* The global failure counter */
extern int globfail;

/* Console I/O */
extern int lab_initconsole (void);
extern void lab_printf (const char *fmt, ...);
extern void lab_putsn (const char *str, int sl);
static inline void lab_puts (const char *str)
{ lab_putsn (str, strlen (str)); }
extern void lab_putc (unsigned char c);
extern unsigned char lab_getc_seconds (void (*idler) (void), unsigned long timeout_seconds);
extern unsigned char lab_getc (void (*idler) (void), unsigned long timeout);
extern unsigned char lab_get_char (void);
extern int lab_char_ready (void);

extern void lab_readline (char *buff, size_t buffsize);

/* Command line interpreter */
extern void lab_main (int cmdline);
extern void lab_exec_string (char *str);

/* Utility functions */
extern int lab_dev_open (int devmode, int major, int minor, int fmode);
extern void lab_delay (unsigned long seconds);

/* External functions written in assembler [bootldr*.S] */
extern void lab_boot (void *bootinfo, unsigned long startaddr);
extern void lab_boot_kernel (void *bootinfo, unsigned long machine_type,
                             unsigned long startaddr);

#endif /* _LAB_H */
