/* H5400 Fingerprint Sensor Interface driver 
 * Copyright (c) 2004 J°rgen Andreas Michaelsen <jorgenam@ifi.uio.no> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <asm/mach-types.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>

#include <asm/arch/fsi.h>

#include "samcop_fsi.h"

atomic_t fsi_in_use;
DECLARE_WAIT_QUEUE_HEAD(fsi_rqueue);
struct timer_list fsi_temp_timer;

unsigned int fsi_prescale = 19;
unsigned int fsi_dmi = 1124;
unsigned int fsi_treshold_on = 20;
unsigned int fsi_treshold_off = 4;
unsigned int fsi_buffer_size = FSI_FRAME_SIZE * (sizeof(unsigned long)) * 3;

volatile unsigned char fsi_lfrm = 0;

static struct samcop_fsi *fsi_ = NULL;

void fsi_set_mode(unsigned int cmd)
{
	static unsigned int current_mode = 0;
	static unsigned long control_register = 0;
	static unsigned int temp_on = 0;

	switch(cmd) {
	case FSI_CMD_MODE_STOP:
		if ((control_register & 0x3) == 0)
			break;
		control_register &= ~0x3;
		samcop_fsi_set_control(fsi_, control_register);
		current_mode = cmd;
		break;
	case FSI_CMD_MODE_START:
		if (current_mode == cmd)
			control_register |= 0x2;
		else
			control_register = 0xF9 | temp_on;

		samcop_fsi_set_control(fsi_, control_register);
		control_register |= 0x2;
		current_mode = cmd;
		fsi_lfrm = 0;
		break;
	case FSI_CMD_MODE_NAP:
		control_register = 0x2 | temp_on;
		samcop_fsi_set_control(fsi_, control_register);
		current_mode = cmd;
		break;
	case FSI_CMD_STOP_ACQ_INT:
		control_register &= 0x7F;
		samcop_fsi_set_control(fsi_, control_register);
		break;
	case FSI_CMD_START_ACQ_INT:
		control_register |= 0x80;
		samcop_fsi_set_control(fsi_, control_register);
		break;
	case FSI_CMD_STOP_INFO_INT:
		control_register &= 0x8F;
		samcop_fsi_set_control(fsi_, control_register);
		break;
	case FSI_CMD_START_INFO_INT:
		control_register |= 0x70;
		samcop_fsi_set_control(fsi_, control_register);
		break;
	case FSI_CMD_RESET:
		control_register = 0 | temp_on;
		current_mode = FSI_CMD_MODE_STOP;
		samcop_fsi_set_control(fsi_, control_register);
		break;
	case FSI_CMD_START_TEMP:
		control_register |= FSI_CONTROL_TEMP;
		samcop_fsi_set_control(fsi_, control_register);
		temp_on = FSI_CONTROL_TEMP;
		break;
	case FSI_CMD_STOP_TEMP:
		control_register &= ~FSI_CONTROL_TEMP;
		samcop_fsi_set_control(fsi_, control_register);
		temp_on = 0;
		break;
	}
}

void fsi_timer_temp_callback(unsigned long input)
{
	printk(KERN_DEBUG "%s: stopping temperature increase (status=%d)\n", __FUNCTION__,
	       samcop_fsi_get_control(fsi_) & FSI_CONTROL_TEMP);
	fsi_set_mode(FSI_CMD_STOP_TEMP);
}

inline int fsi_analyze_column(struct fsi_read_state *read_state, unsigned int data)
{
	if ((data & 0xEFEF) == 0xE0E0) { /* sync column */
		read_state->frame_number++;
		read_state->frame_offset = 0;
		read_state->detect_value = 0;

		return 1;

	} else if (read_state->frame_number < 1) { /* skip first frame */
		return 0;

	} else if (read_state->frame_offset > (((FSI_FRAME_SIZE-1)/2)-8) &&
		   read_state->frame_offset < ((FSI_FRAME_SIZE-1)/2)) {
		/* FIXME: how do we solve this? */

	} else if (read_state->frame_offset == ((FSI_FRAME_SIZE-1)/2)) {
		read_state->detect_value += abs(FSI_DB1_EVEN(data) - FSI_DB1_ODD(data));
		read_state->detect_value += abs(FSI_DB1_ODD(data) - FSI_DB2_EVEN(data));
		read_state->detect_value += abs(FSI_DB2_EVEN(data) - FSI_DB2_ODD(data));
		read_state->detect_value += abs(FSI_DB2_ODD(data) - FSI_DB3_EVEN(data));
		read_state->detect_value += abs(FSI_DB3_EVEN(data) - FSI_DB3_ODD(data));
		read_state->detect_value += abs(FSI_DB3_ODD(data) - FSI_DB4_EVEN(data));
		read_state->detect_value += abs(FSI_DB4_EVEN(data) - FSI_DB4_ODD(data));

		if (read_state->finger_present == 0 && read_state->detect_value > read_state->treshold_on)
			read_state->detect_count++;
		else if (read_state->finger_present && read_state->detect_value < read_state->treshold_off)
			read_state->detect_count++;
		else
			read_state->detect_count = 0;

		if (read_state->detect_count > 2) {
			read_state->finger_present = !read_state->finger_present;
#if 1
			printk(KERN_DEBUG "%s: finger_present=%d at frame=%d [value=%d]\n", __FUNCTION__, read_state->finger_present, read_state->frame_number, read_state->detect_value);
#endif
		}
	}

	read_state->frame_offset++;
	return 0;
}

void fsi_run_tasklet(unsigned long state_ptr)
{
	unsigned int i;
	unsigned long data;
	unsigned int words_in_fifo;
	int sync;
	struct fsi_read_state *read_state;


	read_state = (struct fsi_read_state *)state_ptr;	
	words_in_fifo = (samcop_fsi_get_status(fsi_)>>1) & 0x1FF;

	for (i = 0; i < words_in_fifo; i++) {
		data = samcop_fsi_read_data(fsi_);
		sync = fsi_analyze_column(read_state, data);

#if 0
		if (read_state->finger_present == 0 && sync == 0)
			data = 0;
#endif

		if (read_state->do_read) {
			read_state->buffer[read_state->write_pos] = data;
			read_state->write_pos++;
			if (read_state->write_pos >= read_state->buffer_size)
				read_state->write_pos = 0;

			if (sync && read_state->finger_present == 0)
				read_state->finished = 1;

			if (read_state->write_pos == read_state->read_pos)
				printk(KERN_WARNING "%s: DCOL frame=%d offset=%d\n", __FUNCTION__,
				       read_state->frame_number, read_state->frame_offset);
		}
		else if (sync && read_state->finger_present)
			read_state->do_read = 1;
	}

	fsi_set_mode(FSI_CMD_START_ACQ_INT);
	wake_up_interruptible (&fsi_rqueue);
}

DECLARE_TASKLET_DISABLED(fsi_tasklet,fsi_run_tasklet,0);

int fsi_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int fifo_status;


	fifo_status = samcop_fsi_get_status(fsi_);

	if (fifo_status & 1) {
		fsi_set_mode(FSI_CMD_STOP_ACQ_INT);
		tasklet_schedule(&fsi_tasklet);
	}
	if (fifo_status & (1<<10)) {
		printk( KERN_DEBUG "%s: DCOL\n", __FUNCTION__ );
		samcop_fsi_set_status(fsi_, 1<<10);
	}
	if (fifo_status & (1<<11)) {
		printk( KERN_DEBUG "%s: INVL\n", __FUNCTION__ );
		samcop_fsi_set_status(fsi_, 1<<11);
	}
	if (fifo_status & (1<<12)) { /* LFRM */
		samcop_fsi_set_status(fsi_, 1<<12);
		fsi_lfrm = 1;
		tasklet_schedule(&fsi_tasklet);
	}
	return 0;
}

int fsi_copy_to_user(struct fsi_read_state *read_state, char *buf)
{
	unsigned int avail_words;

	if (read_state->read_pos == read_state->write_pos)
		return 0;
	else if (read_state->write_pos < read_state->read_pos)
		avail_words = read_state->buffer_size - read_state->read_pos;
	else
		avail_words = read_state->write_pos - read_state->read_pos;
	
	if (copy_to_user(buf+read_state->word_count*4,read_state->buffer+read_state->read_pos,avail_words*4))
		return -EFAULT;

	read_state->word_count += avail_words;
	if ((read_state->word_count+FSI_FRAME_SIZE) > read_state->word_dest ||
	    read_state->finished)
		fsi_set_mode(FSI_CMD_MODE_STOP);		

	read_state->read_pos += avail_words;
	if (read_state->read_pos >= read_state->buffer_size)
		read_state->read_pos = 0;
		
	if (read_state->read_pos < read_state->write_pos)
		return fsi_copy_to_user(read_state, buf);

	return 0;
}

int fsi_open(struct inode *inode, struct file *filp)
{
	struct fsi_read_state *read_state;

	/* we allow only one user at a time */
	if (atomic_dec_and_test(&fsi_in_use)) {
		atomic_inc(&fsi_in_use);
		return -EBUSY;
	}

	/* init state */
	read_state = kmalloc(sizeof *read_state,GFP_KERNEL);
	if (read_state == NULL) {
		atomic_inc(&fsi_in_use);
		return -ENOMEM;
	}

	read_state->buffer = NULL;
	read_state->word_count = 0;
	read_state->word_dest = 0;
	read_state->detect_count = 0;
	read_state->frame_number = 0;
	read_state->frame_offset = 0;

	filp->private_data = read_state;

#if 0
	/* GPIO61 turns out to be serial power, not fingerchip power.
	   Any other guesses?  */
	SET_GPIO(POWER_FP_N, 0); /* power on */
#endif
	/* init hardware */
	samcop_fsi_up(fsi_);

	return 0;
}

int fsi_release(struct inode *inode, struct file *filp)
{
	/* put scanner to sleep before we exit */
	samcop_fsi_down(fsi_);

	kfree(filp->private_data);
	atomic_inc(&fsi_in_use);

	return 0;
}

ssize_t fsi_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
	int errval = 0;
	struct fsi_read_state *read_state;

	read_state = (struct fsi_read_state *)filp->private_data;

	/* round count request down to a whole frame */
	read_state->word_dest = ((count/(sizeof (unsigned long)))/FSI_FRAME_SIZE) * FSI_FRAME_SIZE;
	if (read_state->word_dest == 0)
		return 0;

	/* allocate buffer and initialize state */
	read_state->buffer_size = fsi_buffer_size;
	read_state->buffer = kmalloc(read_state->buffer_size,GFP_KERNEL);
	if (read_state->buffer == NULL)
		return -ENOMEM;

	read_state->word_count = 0;
	read_state->write_pos = 0;
	read_state->read_pos = 0;
	read_state->detect_count = 0;
	read_state->frame_number = 0;
	read_state->frame_offset = 0;
	read_state->do_read = 0;
	read_state->finished = 0;
	read_state->finger_present = 0;
	read_state->treshold_on = fsi_treshold_on;
	read_state->treshold_off = fsi_treshold_off;

	fsi_lfrm = 0;

	fsi_tasklet.data = (unsigned long)read_state;
	tasklet_enable(&fsi_tasklet);

	/* start clock, reset and frame receive start */
	samcop_fsi_set_fifo_control(fsi_, 0x3);
	fsi_set_mode(FSI_CMD_MODE_START);

	/* gather data */
	while (read_state->word_count < read_state->word_dest) {
		interruptible_sleep_on(&fsi_rqueue);
		if (signal_pending(current)) {
			errval = -ERESTARTSYS;
			goto fsi_read_done;
		}

		errval = fsi_copy_to_user(read_state, buf);
		if (errval)
			goto fsi_read_done;

		if (fsi_lfrm)
			break;
	}

 fsi_read_done:

	/* stop clock and set stop mode */
	fsi_set_mode(FSI_CMD_MODE_STOP);

	tasklet_disable(&fsi_tasklet);
	fsi_tasklet.data = 0;

	kfree(read_state->buffer);

	if (errval)
		return errval;

	count = read_state->word_count * (sizeof (unsigned long));

	*offp += count;
	return count;
}

int fsi_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fsi_read_state *read_state;

	read_state = (struct fsi_read_state *)filp->private_data;

	switch(cmd)
	{
	case FSI_IOCSTOPTEMP: /* unconditional stop temperature increase */
		fsi_set_mode(FSI_CMD_STOP_TEMP);
		printk(KERN_DEBUG "%s: FSI_IOCSTOPTEMP\n", __FUNCTION__);
		break;
	case FSI_IOCSTARTTEMP: /* start temperature increase */
		fsi_set_mode(FSI_CMD_START_TEMP);
		printk(KERN_DEBUG "%s: FSI_IOCSTARTTEMP\n", __FUNCTION__);
		break;
	case FSI_IOCINCTEMP: /* increase temperature [arg] steps */
		fsi_set_mode(FSI_CMD_START_TEMP);
		mod_timer(&fsi_temp_timer, jiffies + 150 * arg);
		printk(KERN_DEBUG "%s: FSI_IOCINCTEMP %ld steps\n", __FUNCTION__, arg);
		break;
	case FSI_IOCSETPRESCALE:
		if (arg > 255) {
			printk("prescaler=%ld must be in range [0,255]", arg);
			arg = 255;
		}
		samcop_fsi_set_prescaler(fsi_, arg);
		printk(KERN_DEBUG "%s: FSI_IOCSETPRESCALE %ld\n", __FUNCTION__, arg);
		break;
	case FSI_IOCSETDMI:
		if (arg > 4095) {
			printk("integration count=%ld must be in range [0,4095]", arg);
			arg = 4095;
		}
		samcop_fsi_set_dmc(fsi_, arg);
		printk(KERN_DEBUG "%s: FSI_IOCSETDMI %ld\n", __FUNCTION__, arg);
		break;
	case FSI_IOCSETTRESHOLDON:
		read_state->treshold_on = arg;
		printk(KERN_DEBUG "%s: FSI_IOCSETTRESHOLDON %ld\n", __FUNCTION__, arg);
		break;
	case FSI_IOCSETTRESHOLDOFF:
		read_state->treshold_off = arg;
		printk(KERN_DEBUG "%s: FSI_IOCSETTRESHOLDOFF %ld\n", __FUNCTION__, arg);
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations fsi_fops =
{
	owner:    THIS_MODULE,
	open:     fsi_open,
	release:  fsi_release,
	read:     fsi_read,
	ioctl:    fsi_ioctl,
};

static struct miscdevice fsi_miscdev = 
{
	MISC_DYNAMIC_MINOR,
	"fsi",
	&fsi_fops
};

int
fsi_attach(struct samcop_fsi* fsi)
{ 
	int irq_result;

	if (!machine_is_h5400())
		return -ENODEV;

	fsi_ = fsi;
	misc_register(&fsi_miscdev);


	irq_result = request_irq(fsi_->irq, fsi_irq_handler, 0, "fsi", NULL);
	if (irq_result) {
		misc_deregister(&fsi_miscdev);
		return irq_result;
	}

	init_timer(&fsi_temp_timer);
	fsi_temp_timer.function = fsi_timer_temp_callback;

	atomic_set(&fsi_in_use,0);

	return 0; 
}

void
fsi_detach(void)
{
	fsi_set_mode(FSI_CMD_STOP_TEMP);
	del_timer(&fsi_temp_timer);
	samcop_fsi_down(fsi_); /* Stop h/w */
	free_irq(fsi_->irq, NULL);
	fsi_ = NULL;
	misc_deregister(&fsi_miscdev);
}

MODULE_AUTHOR("J°rgen Andreas Michaelsen <jorgenam@ifi.uio.no>");
MODULE_DESCRIPTION("H5400 Fingerprint Scanner Interface driver");
MODULE_LICENSE("GPL");

module_param(fsi_prescale, uint, S_IRUGO);
module_param(fsi_dmi, uint, S_IRUGO);
module_param(fsi_treshold_on, uint, S_IRUGO);
module_param(fsi_treshold_off, uint, S_IRUGO);
module_param(fsi_buffer_size, uint, S_IRUGO);

