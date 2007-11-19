/*
 * usb/buffer.c
 *
 * Copyright (C) 2002 Russell King.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#include "buffer.h"

static LIST_HEAD(buffers);

struct usb_buf *usbb_alloc(int size, int gfp)
{
	unsigned long flags;
	struct usb_buf *buf;

	buf = kmalloc(sizeof(struct usb_buf) + size, gfp);
	if (buf) {
		atomic_set(&buf->users, 1);
		local_irq_save(flags);
		list_add(&buf->list, &buffers);
		local_irq_restore(flags);
		buf->len  = 0;
		buf->data = (unsigned char *) (buf + 1);
		buf->head = (unsigned char *) (buf + 1);
	}

	return buf;
}

void __usbb_free(struct usb_buf *buf)
{
	unsigned long flags;
	local_irq_save(flags);
	list_del(&buf->list);
	local_irq_restore(flags);
	kfree(buf);
}

EXPORT_SYMBOL(usbb_alloc);
EXPORT_SYMBOL(__usbb_free);

static void __exit usbb_exit(void)
{
	if (!list_empty(&buffers)) {
		struct list_head *l, *n;
		printk("usbb: buffers not freed:\n");

		list_for_each_safe(l, n, &buffers) {
			struct usb_buf *b = list_entry(l, struct usb_buf, list);

			printk(" %p: alloced from %p count %d\n",
				b, b->alloced_by, atomic_read(&b->users));

			__usbb_free(b);
		}
	}
}

module_exit(usbb_exit);

