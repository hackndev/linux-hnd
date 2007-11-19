/*
 *
 *  Demultiplexer line discipline for TI Calypso GSM chipsets
 *  with HTC firmware (HTC Alpine, Blueangel, Himalaya, Magician)
 *
 *  Copyright (C) 2007  Philipp Zabel <philipp.zabel@gmail.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#define VERSION "0.1"
#define TIHTC_MAGIC 0x43544854

/* Ioctls */
#define TIHTCSETCHAN	_IOW('U', 200, int)
#define TIHTCGETCHAN	_IOR('U', 201, int)

/* UART channels */
#define TIHTC_MAX_CHAN	3

#define TIHTC_ATCMD	0x16
#define TIHTC_DEBUG	0x12
#define TIHTC_CONFIG	0x19

struct tihtc_buf {
	int			count;
	unsigned char		buf[256];
};

#define DEBUG 1
#ifdef DEBUG
#define DBG(format, arg...) printk(KERN_DEBUG format, ## arg)
#else
#define DBG(format, arg...) {}
#endif

/**
 * struct n_tihtc - per device instance data structure
 * @magic - magic value for structure
 * @flags - miscellaneous control flags
 * @tty - ptr to TTY structure
 * @tbusy - reentrancy flag for tx wakeup code
 * @tx_buf - tx buffer
 * @rx_buf - rx buffer
 */
struct n_tihtc {
	int			magic;
	__u32			flags;
	struct tty_struct	*tty;
	int			tbusy;
	struct tihtc_buf	rx;
	struct tihtc_buf	rx_at;	/* 0x16 */
	struct tihtc_buf	rx_dbg;	/* 0x12 */
	struct tihtc_buf	rx_cfg; /* 0x19 */
	struct tihtc_buf	tx;
	struct file		*file_dbg;
	struct file		*file_cfg;
	unsigned char		open_frame;
};

/**
 * dump_buffer
 * @buf - buffer to dump
 *
 * Dumps tihtc_buf buffer contents into the kernel ringbuffer.
 */
static void dump_buffer(struct tihtc_buf *buf)
{
	int i;
	printk(KERN_DEBUG "(%d)", buf->count);
	for (i = 0; i < buf->count; i++) {
		if (buf->buf[i] >= 32)
			printk("%c", buf->buf[i]);
		else switch (buf->buf[i]) {
		case 0xd:
			printk("\\r");break;
		case 0xa:
			printk("\\n");break;
		default:
			printk("\\x%x", buf->buf[i]);
		}
	}
	printk("\n");
}

/**
 * tihtc_set_channel
 * @n_tihtc
 * @file
 *
 * Sets the channel number for the given file.
 */
static int tihtc_set_channel(struct n_tihtc *n_tihtc,
				struct file *file, unsigned long arg)
{
	switch (arg) {
	case TIHTC_ATCMD:
		if (n_tihtc->file_dbg == file)
			n_tihtc->file_dbg = NULL;
		if (n_tihtc->file_cfg == file)
			n_tihtc->file_cfg = NULL;
		break;
	case TIHTC_DEBUG:
		n_tihtc->file_dbg = file;
		if (n_tihtc->file_cfg == file)
			n_tihtc->file_cfg = NULL;
		break;
	case TIHTC_CONFIG:
		n_tihtc->file_cfg = file;
		if (n_tihtc->file_dbg == file)
			n_tihtc->file_dbg = NULL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * tihtc_get_channel
 * @n_tihtc
 * @file
 *
 * Returns the channel number for the given file (default: TIHTC_ATCMD).
 */
static int tihtc_get_channel(struct n_tihtc *n_tihtc,
						struct file *file)
{
	if (file == n_tihtc->file_dbg)
		return TIHTC_DEBUG;
	if (file == n_tihtc->file_cfg)
		return TIHTC_CONFIG;
	/* everything else goes to ATCMD */
	return TIHTC_ATCMD;
}

/**
 * tihtc_get_channel_buffer
 * @n_tihtc
 * @file
 *
 * Returns the rx buffer for the given file's channel.
 */
static struct tihtc_buf *tihtc_get_channel_buffer(struct n_tihtc *n_tihtc,
							struct file *file)
{
	switch (tihtc_get_channel(n_tihtc, file)) {
	case TIHTC_DEBUG:
		DBG("channel rx: DEBUG\n");
		return &n_tihtc->rx_dbg;
	case TIHTC_CONFIG:
		DBG("channel rx: CONFIG\n");
		return &n_tihtc->rx_cfg;
	default:
		DBG("channel rx: ATCMD\n");
		return &n_tihtc->rx_at;
	}
}

/**
 * tihtc_filter - filter frames from the TI HTC chipset
 * @n_tihtc
 *
 * Reads multiplexed input from rx and stores demultiplexed output
 * into rx_at/dbg/cfg. Frame format:
 * '0x2 (0x2 (0x2)) 0x16|0x12|0x19 [data] 0x2'
 */
void tihtc_filter(struct n_tihtc *n_tihtc)
{
	char *src = n_tihtc->rx.buf;
	char *end = src+n_tihtc->rx.count;
	struct tihtc_buf *rx_out = NULL;
	char *dst;
	int i, j = 0;

	DBG("tihtc_filter called\n");

	while (src < end) {
		if (n_tihtc->open_frame == 0) {
			/* strip initial 0x02's */
			while (*src == 0x2) {
				src++;
				j++;
				if (src >= end)
					goto src_end;
			}
			n_tihtc->open_frame = *src++;
			DBG("starting frame (channel 0x%x) at %d\n", n_tihtc->open_frame, j++);
			if (src >= end)
				goto src_end;
		} else
			DBG("resuming frame (channel 0x%x)\n", n_tihtc->open_frame);
		switch (n_tihtc->open_frame) {
		case TIHTC_ATCMD:
			rx_out = &n_tihtc->rx_at;
			break;
		case TIHTC_DEBUG:
			rx_out = &n_tihtc->rx_dbg;
			break;
		case TIHTC_CONFIG:
			rx_out = &n_tihtc->rx_cfg;
			break;
		default:
			printk(KERN_WARNING "tihtc_filter: unknown channel"
					" 0x%x\n", n_tihtc->open_frame);
			rx_out = NULL;
		}
		/* only consider known channels */
		if (rx_out) {
			i = rx_out->count;
			dst = rx_out->buf+i;
			/* read until frame end (0x02) */
			while (*src != 0x2) {
				if (src >= end) {
					rx_out->count = i;
					goto src_end;
				}
				if (i > 255) {
					/* FIXME how to handle this? */
					printk(KERN_ERR "rx_out FULL, dropping buffer:");
					dump_buffer(rx_out);
					rx_out->count = 0; i = 0;
					dst = rx_out->buf;
				}
				*dst = *src;
				dst++; i++;
				src++;
			}
			rx_out->count = i;
			n_tihtc->open_frame = 0;
		} else {
			/* ignore until frame end (0x02) */
			while (*src != 0x2) {
				if (src >= end)
					goto src_end;
				src++;
			}
			n_tihtc->open_frame = 0;
		}
		/* next frame */
	}
	return;
src_end:
	/* empty rx buffer */
	n_tihtc->rx.count = 0;
}

/**
 * n_tihtc_open - called when the line discipline is changed to n_tihtc
 * @tty - pointer to tty info structure
 *
 * Returns 0 if success, otherwise error code
 */
static int n_tihtc_open (struct tty_struct *tty)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)(tty->disc_data);

	DBG("n_tihtc_open called (device=%s)\n", tty->name);

	/* There should not be an existing table for this slot. */
	if (n_tihtc) {
		printk(KERN_ERR "n_tihtc_open: tty already associated!\n");
		return -EEXIST;
	}

	/* Allocate per device structure */
	n_tihtc = kmalloc(sizeof(*n_tihtc), GFP_KERNEL);
	if (!n_tihtc) {
		printk (KERN_ERR "n_tihtc_alloc failed\n");
		return -ENFILE;
	}
	memset(n_tihtc, 0, sizeof(*n_tihtc));
	n_tihtc->magic = TIHTC_MAGIC;

	tty->disc_data = n_tihtc;
	n_tihtc->tty   = tty;
	tty->receive_room = 256;

	/* Flush any pending characters in the driver and discipline. */

	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer (tty);

	if (tty->driver->flush_buffer)
		tty->driver->flush_buffer (tty);

	DBG("n_tihtc_open success\n");

	return 0;
}

/**
 * n_tihtc_close - line discipline close
 * @tty - pointer to tty info structure
 *
 * Called when the line discipline is changed to something
 * else, the tty is closed, or the tty detects a hangup.
 */
static void n_tihtc_close(struct tty_struct *tty)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)(tty->disc_data);

	DBG("n_tihtc_close called\n");

	if (n_tihtc != NULL) {
		if (n_tihtc->magic != TIHTC_MAGIC) {
			printk (KERN_WARNING"n_tihtc: trying to close unopened tty!\n");
			return;
		}
		tty->disc_data = NULL;
		kfree(n_tihtc);
	}

	DBG("n_tihtc_close success\n");
}

/**
 * n_tihtc_read - Called to retrieve one frame of data (if available)
 * @tty - pointer to tty instance data
 * @file - pointer to open file object
 * @buf - pointer to returned data buffer
 * @nr - size of returned data buffer
 *
 * Returns the number of bytes returned or error code.
 */
static ssize_t n_tihtc_read(struct tty_struct *tty, struct file *file,
				unsigned char __user *buf, size_t nr)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)(tty->disc_data);
	struct tihtc_buf *rx;
	int count;
	int ret;

	DBG("n_tihtc_tty_read called\n");

	/* Validate the pointers */
	if (!n_tihtc)
		return -EIO;

	/* verify user access to buffer */
	if (!access_ok(VERIFY_WRITE, buf, nr)) {
		printk(KERN_WARNING "n_tihtc_tty_read can't verify user "
		"buffer\n");
		return -EFAULT;
	}

	for (;;) {
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
			return -EIO;

		n_tihtc = (struct n_tihtc *)(tty->disc_data);
		if (!n_tihtc || n_tihtc->magic != TIHTC_MAGIC ||
			 tty != n_tihtc->tty)
			return 0;

		rx = tihtc_get_channel_buffer(n_tihtc, file);

		if (rx->count > 0)
			break;

		/* no data */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		interruptible_sleep_on (&tty->read_wait);
		if (signal_pending(current))
			return -EINTR;
	}

	count = min(rx->count, (int)nr);
	/* Copy the data to the caller's buffer */
	if (copy_to_user(buf, rx->buf, rx->count))
		ret = -EFAULT;
	else
		ret = count;
	if (rx->count > nr) {
		/* frame was too large for caller's buffer */
		memcpy(rx->buf, rx->buf+nr, rx->count-nr);
		rx->count -= nr;
	} else
		rx->count = 0;

	return ret;
}

/**
 * n_tihtc_write - write a single frame of data to device
 * @tty	- pointer to associated tty device instance data
 * @file - pointer to file object data
 * @data - pointer to transmit data (one frame)
 * @count - size of transmit frame in bytes
 *
 * Returns the number of bytes written (or error code).
 * Serialized by tty layer.
 */
static ssize_t n_tihtc_write(struct tty_struct *tty, struct file *file,
					const unsigned char *data, size_t count)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)tty->disc_data;
	struct tihtc_buf *tx = &n_tihtc->tx;
	int error = 0;
	int channel;
	int nr;

	DBG("n_tihtc_write called (count=%Zd)\n", count);

	channel = tihtc_get_channel(n_tihtc, file);
	switch (channel) {
	case TIHTC_ATCMD:
		DBG("channel tx: ATCMD\n");
		break;
	case TIHTC_CONFIG:
		DBG("channel tx: CONFIG\n");
		break;
	case TIHTC_DEBUG:
		DBG("channel tx: DEBUG is read-only!\n");
		return -EPERM;
	}

	/* verify frame size */
	if (count > 253) {
		printk (KERN_WARNING
			"n_tihtc_write: truncating user packet "
			"from %lu to %d\n", (unsigned long) count,
			253 );
		count = 253;
	}

	/* Retrieve the user's buffer */
	tx->buf[0] = 0x2;
	tx->buf[1] = channel;
	memcpy(tx->buf+2, data, count);
	tx->buf[2+count] = 0x2;
	tx->count = count+3;

	/* Debug print */
	dump_buffer(tx);

	/* Only send complete frames */
	if (tx->count > tty->driver->write_room(tty))
		return -EAGAIN;

	/* Send frame to device */
	nr = tty->driver->write(tty, tx->buf, tx->count);
	if (nr < tx->count) {
		DBG("written, ret = %d, count = %d\n", error, tx->count);
		return -EFAULT;
	}
	return count;
}

/**
 * n_tihtc_ioctl - process IOCTL system call for the tty device.
 * @tty - pointer to tty instance data
 * @file - pointer to open file object for device
 * @cmd - IOCTL command code
 * @arg - argument for IOCTL call (cmd dependent)
 *
 * Returns command dependent result.
 */
static int n_tihtc_ioctl(struct tty_struct *tty, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)tty->disc_data;
	int error = 0;
	int count;
	
	DBG("n_tihtc_tty_ioctl called %x\n", cmd);
	
	/* Verify the status of the device */
	if (!n_tihtc || n_tihtc->magic != TIHTC_MAGIC)
		return -EBADF;

	switch (cmd) {
	case FIONREAD:
		/* report count of read data available */
		/* in next available frame (if any) */
		count = n_tihtc->rx_at.count;
		error = put_user(count, (int __user *)arg);
		break;
	case TIOCOUTQ:
		/* get the pending tx byte count in the driver */
		count = tty->driver->chars_in_buffer ?
				tty->driver->chars_in_buffer(tty) : 0;
		/* add size of output buffer frame */
		count += n_tihtc->tx.count;
		error = put_user(count, (int __user *)arg);
		break;
	case TIHTCSETCHAN:
		error = tihtc_set_channel(n_tihtc, file, arg);
		break;
	case TIHTCGETCHAN:
		return tihtc_get_channel(n_tihtc, file);
	default:
		error = n_tty_ioctl (tty, file, cmd, arg);
		break;
	}
	return error;
	
}

/**
 * n_tihtc_poll - TTY callback for poll system call
 * @tty - pointer to tty instance data
 * @file - pointer to open file object for device
 * @poll_table - wait queue for operations
 *
 * Determine which operations (read/write) will not block and return info
 * to caller.
 * Returns a bit mask containing info on which ops will not block.
 */
static unsigned int n_tihtc_poll(struct tty_struct *tty, struct file *file,
				    poll_table *wait)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)tty->disc_data;
	unsigned int mask = 0;
	int channel;

	DBG("n_tihtc_poll called\n");

	if (n_tihtc && n_tihtc->magic == TIHTC_MAGIC && tty == n_tihtc->tty) {
		/* queue current process into any wait queue that */
		/* may awaken in the future (read and write) */

		poll_wait(file, &tty->read_wait, wait);
		poll_wait(file, &tty->write_wait, wait);

		channel = tihtc_get_channel(n_tihtc, file);

		/* set bits for operations that won't block */
		if((n_tihtc->rx_at.count && channel == TIHTC_ATCMD) ||
		   (n_tihtc->rx_dbg.count && channel == TIHTC_DEBUG) ||
		   (n_tihtc->rx_cfg.count && channel == TIHTC_CONFIG))
			mask |= POLLIN | POLLRDNORM;	/* readable */
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
			mask |= POLLHUP;
		if(tty_hung_up_p(file))
			mask |= POLLHUP;
//		if(n_tihtc->writeable)
			mask |= POLLOUT | POLLWRNORM;	/* writable */
	}
	return mask;
}

/**
 * n_tihtc_receive_buf - Called by tty driver when receive data is available
 * @tty	- pointer to tty instance data
 * @data - pointer to received data
 * @flags - pointer to flags for data
 * @count - count of received data in bytes
 *
 * Called by tty low level driver when receive data is available. Data is
 * interpreted as one frame.
 */
static void n_tihtc_receive_buf(struct tty_struct *tty,
			const unsigned char *data, char *flags, int count)
{
	struct n_tihtc *n_tihtc = (struct n_tihtc *)tty->disc_data;
	struct tihtc_buf *rx = &n_tihtc->rx;

	DBG("n_tihtc_tty_receive called (count=%d)\n", count);

	/* verify line is using TIHTC discipline */
	if (n_tihtc->magic != TIHTC_MAGIC) {
		printk(KERN_WARNING "- line not using TIHTC discipline\n");
		return;
	}

	if (count>(256-rx->count)) {
		printk(KERN_ERR "- rx count>%d, data discarded\n",
				(256-rx->count));
		return;
	}

	/* copy received data to rx buffer */
	memcpy(rx->buf+rx->count, data, count);
	rx->count += count;

	/* Debug print */
	dump_buffer(rx);

	/* filter data from rx into rx_at/dbg/cfg */
	tihtc_filter(n_tihtc);

	/* Debug print */
	rx = &n_tihtc->rx_at;
	if (rx->count) {
		DBG("AT  ");
		dump_buffer(rx);
	}
	rx = &n_tihtc->rx_dbg;
	if (rx->count) {
		DBG("DBG ");
		dump_buffer(rx);
	}
	rx = &n_tihtc->rx_cfg;
	if (rx->count) {
		DBG("CFG ");
		dump_buffer(rx);
	}

	/* wake up blocked reads and perform async signalling */
	wake_up_interruptible (&tty->read_wait);
	if (n_tihtc->tty->fasync != NULL)
		kill_fasync (&n_tihtc->tty->fasync, SIGIO, POLL_IN);
}

static struct tty_ldisc tihtc_ldisc = {
	.magic		= TTY_LDISC_MAGIC,
	.name		= "n_tihtc",
	.open		= n_tihtc_open,
	.close		= n_tihtc_close,
	.read		= n_tihtc_read,
	.write		= n_tihtc_write,
	.ioctl		= n_tihtc_ioctl,
	.poll		= n_tihtc_poll,
	.receive_buf	= n_tihtc_receive_buf,
	.owner		= THIS_MODULE,
};

static char __initdata driver_info[] = KERN_INFO
	"TI Calypso / HTC firmware GSM UART line discipline ver " VERSION;
static char __initdata reg_err[] = KERN_ERR
	"TIHTC line discipline registration failed. (%d)";

static int __init tihtc_uart_init(void)
{
	int err;

	printk(KERN_INFO "TI Calypso / HTC firmware GSM UART line discipline ver " VERSION);

	/* Register the tty discipline */

	if ((err = tty_register_ldisc(N_TIHTC, &tihtc_ldisc))) {
		printk(reg_err, err);
		return err;
	}

	return 0;
}

static char __exitdata unreg_err[] = KERN_ERR
	"Can't unregister TIHTC line discipline (%d)";

static void __exit tihtc_uart_exit(void)
{
	int err;

	/* Release tty registration of line discipline */
	if ((err = tty_unregister_ldisc(N_TIHTC)))
		printk(unreg_err, err);
}

module_init(tihtc_uart_init);
module_exit(tihtc_uart_exit);

MODULE_AUTHOR("Philipp Zabel <philipp.zabel@gmail.com>");
MODULE_DESCRIPTION("TI Calypso / HTC firmware GSM UART line discipline ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_TIHTC);
