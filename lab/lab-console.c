/*
 * Linux As Bootloader console input/output
 *
 * Authors: Andrew Zabolotny <zap@homelink.ru>
 *          Joshua Wise <joshua@joshuawise.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/termios.h>
#include <linux/poll.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

static struct
{
	unsigned char major, minor;
	int fd;
} lab_console [] = {
#if defined(CONFIG_USB_G_CHAR)
	{ 10, 240, -1},
#endif
#if defined(CONFIG_USB_G_SERIAL)
	{ 127, 0, -1},
#endif
#if defined(CONFIG_SERIAL_CORE)
	{ 4, 64, -1 },
#if 0
	{ 4, 65, -1 },
	{ 4, 66, -1 },
#endif
#endif
};

/*********************************************************** Input **********/

int lab_char_ready ()
{
	int i, fdc = 0;
	struct pollfd pfd [ARRAY_SIZE (lab_console)];

	for (i = 0; i < ARRAY_SIZE (lab_console); i++) {
		if (lab_console [i].fd >= 0) {
			pfd [fdc].fd = lab_console [i].fd;
			pfd [fdc].events = POLLIN | POLLPRI;
			fdc++;
		}
	}

	if (fdc == 0)
		return 0;
	
	i = sys_poll(pfd, fdc, 1);

	return i ? 1 : 0;
}
EXPORT_SYMBOL(lab_char_ready);

unsigned char lab_get_char ()
{
	int i, fdc = 0;
	struct pollfd pfd [ARRAY_SIZE (lab_console)];

	for (i = 0; i < ARRAY_SIZE (lab_console); i++) {
		if (lab_console [i].fd >= 0) {
			pfd [fdc].fd = lab_console [i].fd;
			pfd [fdc].events = POLLIN | POLLPRI;
			fdc++;
		}
	}

	if (fdc == 0)
		return 0;

	if (sys_poll (pfd, fdc, 0) <= 0)
		return 0;

	for (i = 0; i < fdc; i++) {
		if (pfd [i].revents & (POLLIN | POLLPRI)) {
			unsigned char c;
			
			sys_read (pfd [i].fd, &c, 1);
			return c;
		}
	}

	return 0;
}
EXPORT_SYMBOL(lab_get_char);

unsigned char lab_getc (void (*idler) (void), unsigned long timeout)
{
	int do_timeout = (timeout != 0);

	while (!lab_char_ready ()) {
		if (do_timeout) {
			if (!timeout)
				break;
			timeout--;
		}

		if (idler)
			idler ();
	}
	
	return lab_get_char();
}
EXPORT_SYMBOL(lab_getc);

unsigned char lab_getc_seconds (void (*idler) (void), unsigned long timeout_seconds)
{
	int i, fdc = 0;
	struct pollfd pfd [ARRAY_SIZE (lab_console)];

	for (i = 0; i < ARRAY_SIZE (lab_console); i++) {
		if (lab_console [i].fd >= 0) {
			pfd [fdc].fd = lab_console [i].fd;
			pfd [fdc].events = POLLIN | POLLPRI;
			fdc++;
		}
	}

	if (fdc == 0)
		return 0;
	
	i = sys_poll(pfd, fdc, timeout_seconds ? timeout_seconds*1000 : -1);
	if (i)
		return lab_get_char();
	else
		return 0;
}
EXPORT_SYMBOL(lab_getc_seconds);

/********************************************************** Output **********/

void lab_putc (unsigned char c)
{
	int i;

	for (i = 0; i < ARRAY_SIZE (lab_console); i++)
		if (lab_console [i].fd >= 0)
			sys_write (lab_console [i].fd, &c, 1);
}
EXPORT_SYMBOL(lab_putc);

void lab_putsn (const char *str, int sl)
{
	int i;

	for (i = 0; i < ARRAY_SIZE (lab_console); i++)
		if (lab_console [i].fd >= 0)
			sys_write (lab_console [i].fd, str, sl);
}
EXPORT_SYMBOL(lab_putsn);

static spinlock_t lab_print_lock = SPIN_LOCK_UNLOCKED;

void lab_printf (const char *fmt, ...)
{
	va_list args;
	static char buf [1024];

	spin_lock (&lab_print_lock);

	va_start (args, fmt);
	vsnprintf (buf, sizeof (buf), fmt, args);
	va_end (args);

	lab_puts (buf);

	spin_unlock (&lab_print_lock);
}
EXPORT_SYMBOL (lab_printf);

/*********************************************************** Setup **********/

void lab_setbaud (int fd, int baud)
{
	struct termios term;

	// get termios data
	if (sys_ioctl (fd, 0x5401, (unsigned int)&term) < 0)
		return;

	// set actual baud
	term.c_cflag &= ~CBAUD;
	term.c_cflag |= baud;

	// make raw, 8n1
	term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	term.c_oflag &= ~OPOST;
	term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	term.c_cflag &= ~(CSIZE|PARENB|CSTOPB);
	term.c_cflag |= CS8;

	// save termios data
	if (sys_ioctl (fd, 0x5403, (unsigned int)&term) < 0)
		return;
}

static void lab_initconsole_wrapper(int x, const char** y);

int lab_initconsole()
{
	int i, err, opened = 0;
	static int initialized = 0;

	if (!initialized)
	{
		lab_addcommand("console", lab_initconsole_wrapper, "Reinitializes the LAB consoles");
		initialized++;
	}

	/* Open device files for all available communications methods */
	for (i = 0; i < ARRAY_SIZE (lab_console); i++) {
		if (lab_console[i].fd >= 0)
			continue;
		err = lab_dev_open (S_IFCHR | 0600,
				    lab_console[i].major,
				    lab_console[i].minor,
				    O_RDWR | O_SYNC);
		lab_console[i].fd = err;
		if (err < 0)
			printk (KERN_INFO "lab: Failed to open device %d:%d (error %d)\n",
				lab_console[i].major, lab_console[i].minor, err);
		else
		{
			lab_setbaud (err, B115200);
			printk(KERN_INFO "lab: opened device %d:%d on fd %d\n", lab_console[i].major, lab_console[i].minor, lab_console[i].fd);
			opened++;
		}
	}
	return opened;
}

static void lab_initconsole_wrapper(int x, const char** y)
{
	lab_initconsole();
}
