/*
 * Linux As Bootloader utility routines
 *
 * Authors: Andrew Zabolotny <zap@homelink.ru>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>

int lab_dev_open (int devmode, int major, int minor, int fmode)
{
	int err = sys_mknod ("/dev/tmp", devmode,
			     new_encode_dev (MKDEV (major, minor)));
	if (err) {
		printk (KERN_ERR "lab: failed to open device %d:%d\n",
			major, minor);
		return -1;
	}

	err = sys_open ("/dev/tmp", fmode, 0);
	sys_unlink ("/dev/tmp");

        return err;
}
EXPORT_SYMBOL (lab_dev_open);

void lab_delay (unsigned long seconds)
{
	set_task_state (current, TASK_INTERRUPTIBLE);
	schedule_timeout(seconds * HZ);
}
EXPORT_SYMBOL (lab_delay);
