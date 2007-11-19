/*
 * usb/strings.h
 *
 * Copyright (C) 2002 Russell King.
 *
 * USB device string handling, built upon usb buffers.
 */
#ifndef USBDEV_STRINGS_H
#define USBDEV_STRINGS_H

#include <linux/spinlock.h>

struct usb_buf;

#define NR_STRINGS 8

struct usb_string_descriptor;

struct usbc_strs {
	spinlock_t lock;
	struct usb_buf *buf[NR_STRINGS];
};

#define usbc_string_desc(buf)	((struct usb_string_descriptor *)(buf)->data)

void usbc_string_from_cstr(struct usb_buf *buf, const char *str);
struct usb_buf *usbc_string_alloc(int len);
void usbc_string_free(struct usb_buf *buf);

int usbc_string_add(struct usbc_strs *table, struct usb_buf *buf);
void usbc_string_del(struct usbc_strs *table, int nr);

/*
 * Note: usbc_string_find() increments the buffer use count.
 * You must call usbb_put() after use.
 */
struct usb_buf *
usbc_string_find(struct usbc_strs *table, unsigned int lang, unsigned int idx);

void usbc_string_free_all(struct usbc_strs *table);
void usbc_string_init(struct usbc_strs *table);

#endif
