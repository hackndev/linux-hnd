/*
 * usb/buffer.h: USB client buffers
 *
 * Copyright (C) 2002 Russell King.
 *
 * Loosely based on linux/skbuff.h
 */
#ifndef USBDEV_BUFFER_H
#define USBDEV_BUFFER_H

#include <linux/list.h>

struct usb_buf {
	atomic_t	users;
	struct list_head list;
	void		*alloced_by;
	unsigned char	*data;
	unsigned char	*head;
	unsigned int	len;
};

extern struct usb_buf *usbb_alloc(int size, int gfp);
extern void __usbb_free(struct usb_buf *);

static inline struct usb_buf *usbb_get(struct usb_buf *buf)
{
	atomic_inc(&buf->users);
	return buf;
}

static inline void usbb_put(struct usb_buf *buf)
{
	if (atomic_dec_and_test(&buf->users))
		__usbb_free(buf);
}

static inline void *usbb_push(struct usb_buf *buf, int len)
{
	unsigned char *b = buf->head;
	buf->head += len;
	buf->len += len;
	return b;
}

#endif
