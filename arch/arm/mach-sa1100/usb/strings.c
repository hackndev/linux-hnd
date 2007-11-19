/*
 * usb/strings.c
 *
 * Copyright (C) 2002 Russell King.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/usb/ch9.h>

#include "buffer.h"
#include "strings.h"

struct usb_buf *usbc_string_alloc(int len)
{
	struct usb_buf *buf;
	int tot_len;

	tot_len = sizeof(struct usb_descriptor_header) + sizeof(u16) * len;

	buf = usbb_alloc(tot_len, GFP_KERNEL);

	if (buf) {
		struct usb_string_descriptor *desc = usbb_push(buf, tot_len);

		desc->bLength = tot_len;
		desc->bDescriptorType = USB_DT_STRING;
	}
	return buf;
}

void usbc_string_free(struct usb_buf *buf)
{
	if (buf)
		usbb_put(buf);
}

void usbc_string_from_cstr(struct usb_buf *buf, const char *str)
{
	struct usb_string_descriptor *desc = usbc_string_desc(buf);
	int i, len;

	len = strlen(str);
	BUG_ON((sizeof(__u16) * len) > desc->bLength - sizeof(struct usb_descriptor_header));

	for (i = 0; i < len; i++)
		desc->wData[i] = cpu_to_le16(str[i]);
}

int usbc_string_add(struct usbc_strs *table, struct usb_buf *buf)
{
	int nr, i;
	
	nr = -ENOSPC;
	spin_lock_irq(&table->lock);
	for (i = 0; i < NR_STRINGS; i++)
		if (table->buf[i] == NULL) {
			table->buf[i] = buf;
			nr = i;
			break;
		}
	spin_unlock_irq(&table->lock);

	return nr;
}

void usbc_string_del(struct usbc_strs *table, int nr)
{
	if (nr < NR_STRINGS) {
		spin_lock_irq(&table->lock);
		table->buf[nr] = NULL;
		spin_unlock_irq(&table->lock);
	}
}

struct usb_buf *
usbc_string_find(struct usbc_strs *table, unsigned int lang, unsigned int idx)
{
	struct usb_buf *buf = NULL;

	if (idx < NR_STRINGS) {
		spin_lock_irq(&table->lock);
		buf = usbb_get(table->buf[idx]);
		spin_unlock_irq(&table->lock);
	}

	return buf;
}

void usbc_string_free_all(struct usbc_strs *table)
{
	int i;

	spin_lock_irq(&table->lock);
	for (i = 0; i < NR_STRINGS; i++) {
		usbc_string_free(table->buf[i]);
		table->buf[i] = NULL;
	}
	spin_unlock_irq(&table->lock);
}

void usbc_string_init(struct usbc_strs *table)
{
	memset(table, 0, sizeof(struct usbc_strs));
	spin_lock_init(&table->lock);
}

EXPORT_SYMBOL(usbc_string_from_cstr);
EXPORT_SYMBOL(usbc_string_alloc);
EXPORT_SYMBOL(usbc_string_free);
EXPORT_SYMBOL(usbc_string_add);
EXPORT_SYMBOL(usbc_string_del);
EXPORT_SYMBOL(usbc_string_find);
EXPORT_SYMBOL(usbc_string_free_all);
EXPORT_SYMBOL(usbc_string_init);
