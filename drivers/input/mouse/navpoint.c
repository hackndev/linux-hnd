/*
 * Synaptics NavPoint (tm)
 * Uses Modular Embedded Protocol (MEP)
 *
 * Copyright (c) 2005 SDG Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/navpoint.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>

#include <asm/memory.h>

#include "mep.h"

#ifdef DEBUG
#define dbg(fmt, ...) printk(KERN_DEBUG fmt, __VA_ARGS__);
#else
#define dbg(fmt, ...)
#endif

#define NP_CMD_TOUCHPAD_INFO	(MEP_HOST_CMD_TYPE_QUERY | 0x20)

#define NP_PKT_ABS_FINGER_ON	0x01
#define NP_PKT_ABS_GESTURE	0x02
#define NP_PKT_ABS_RELXY	0x04
#define NP_PKT_ABS_W(byte)	((byte) >> 4)
#define NP_PKT_REL		(0x08 << 4)
#define NP_PKT_HLO		0x10

/* Reporting mode parameter and modes itself */
#define NP_PARM_REPMODE		0x20
#define NP_RM_ABS (1 << 0)
#define NP_RM_REL (1 << 1)
#define NP_RM_BUT (1 << 2)
#define NP_RM_SCR (1 << 3)
#define NP_RM_RES (1 << 4)
#define NP_RM_NFI (1 << 5)
#define NP_RM_R40 (1 << 6)
#define NP_RM_R80 (1 << 7)

#define ms2p(msecs) ((80*msecs)/1000)
#define back_ms2p(packets) ((packets*1000)/80)
#define none(x) (x)
#define back_none(x) (x)

struct navpoint_data {
	struct {
		char *mode_str;
		unsigned int mode;
		unsigned int tap_time;
		unsigned int dtap_time;
		unsigned int rep_wait;
		unsigned int rep_rate;
		u_int8_t     pressure;
		u_int8_t     threshold;
	} attrs;
	unsigned int want_to_init;

	struct mep_pkt_stream_desc ps;
	struct input_dev *input;
	void *mep_dev;  /* private data for MEP protocol */

	int wait_ack_tries;

	/* Used by mouse and keyboard modes */
	spinlock_t timer_lock;
	struct timer_list tap_timer;
	int8_t finger;
	int8_t old_finger;
	unsigned int finger_count;
	unsigned int tapped;
	u_int8_t z;
	
	/* Used by keyboard modes */
	unsigned int rep_count;
	unsigned int rep_rate;
	unsigned int key;
};

static void
send_parm( struct navpoint_dev *nav_dev, u_int8_t parm ) {
	u_int8_t host_pkt[MEP_HOST_PKT_SIZE];
	int size;
	u_int8_t mep_para[2] = { 0x00, parm };
	/* command sending must be syncronized with acknowledgement from the
	 * previous command. But sometimes navpoint forgets or delay ACK. As
	 * long this function used inside navpoint_rx it is safe, because we
	 * send params only when there is enough room in queue for incoming
	 * packets, so there is no risk of queue overrun. */
	size = mep_host_format_cmd( host_pkt, nav_dev->unit,
	  MEP_HOST_CMD_TYPE_PARM | NP_PARM_REPMODE, 2, mep_para );
	nav_dev->cb_func = NULL;
	nav_dev->priv = NULL;
	nav_dev->tx_func( host_pkt, size );
	dbg("txed: size: %d, data: %02x %02x %02x %02x \n",
	    size, host_pkt[0], host_pkt[1], host_pkt[2], host_pkt[3]);
	return;
}

static void
process_keyboard( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	u_int8_t *cp=navpt->ps.pkt.data;
	
	int x = (int)cp[1] << 8 | cp[2];
	int y = (int)cp[3] << 8 | cp[4];
	u_int8_t z = cp[5];
	navpt->finger = cp[0] & 1;

	dbg("f: %d x: %d	y: %d\n", navpt->finger, x, y);

	if (navpt->finger && z < navpt->attrs.threshold) return;

	if (x == 0 && y == 0);
	else if (x > 4000) navpt->key = KEY_RIGHT;
	else if (x < 2500) navpt->key = KEY_LEFT;
	else if (y > 3000) navpt->key = KEY_UP;
	else if (y < 2500) navpt->key = KEY_DOWN;
	else navpt->key = KEY_ENTER;
	
	if (navpt->finger && !navpt->old_finger && !navpt->tapped) {
		dbg("%d	press\n", navpt->key);
		input_report_key( navpt->input, navpt->key, 1 );
		navpt->tapped = navpt->key;
		navpt->rep_count = 0;
		navpt->rep_rate = navpt->attrs.rep_wait;
	}
	else if (!navpt->finger && navpt->old_finger && navpt->tapped) {
		dbg("%d	release\n", navpt->key);
		input_report_key( navpt->input, navpt->tapped, 0 );
		navpt->tapped = 0;
	}
	else if (navpt->finger && navpt->old_finger && navpt->tapped) {
		if (navpt->rep_count >= navpt->rep_rate) {
			dbg("%d	rep\n", navpt->key);
			navpt->rep_count = 0;
			navpt->rep_rate = navpt->attrs.rep_rate;
			if (navpt->tapped != KEY_ENTER && navpt->key != KEY_ENTER) {
				input_report_key( navpt->input, navpt->tapped, 0 );
				navpt->tapped = navpt->key;
				input_report_key( navpt->input, navpt->tapped, 1 );
			}
			else if (navpt->tapped == KEY_ENTER) {
				input_report_key( navpt->input, navpt->tapped, 0 );
				input_report_key( navpt->input, navpt->tapped, 1 );
			}
		}
		else navpt->rep_count++;
	}

	navpt->old_finger = navpt->finger;
	input_sync( navpt->input );
	return;
}

static void
process_keyboard2( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	u_int8_t *cp=navpt->ps.pkt.data;
	
	int x = (int)cp[1] << 8 | cp[2];
	int y = (int)cp[3] << 8 | cp[4];
	u_int8_t z = cp[5];
	navpt->finger = cp[0] & 1;

	if (navpt->finger && z < navpt->attrs.threshold) return;

	if (x == 0 && y == 0);
	else if (x > 4000) navpt->key = KEY_RIGHT;
	else if (x < 2500) navpt->key = KEY_LEFT;
	else if (y > 3000) navpt->key = KEY_UP;
	else if (y < 2500) navpt->key = KEY_DOWN;
	else navpt->key = 0;

	if ( navpt->finger ) {
		/* First two packets are ignored to get genuine Z value. */
		switch ( navpt->finger_count ) {
			case 2: /* First accounted packet,
				   send event immediately */
				navpt->z = z;
				navpt->rep_count = navpt->rep_rate;
				break;
			case 3: /* Second accounted packet,
				   wait for start repeating */
				navpt->rep_rate = navpt->attrs.rep_wait;
				break;
			default: break;
		}

		if ( navpt->finger_count >= 2 ) {
			if (navpt->z > navpt->attrs.pressure) {
				navpt->key = KEY_ENTER;
			}
			
			if (navpt->rep_count >= navpt->rep_rate && navpt->key) {
				dbg("%d	press&release or repeat\n", navpt->key);
				input_report_key(navpt->input, navpt->key, 1);
				input_report_key(navpt->input, navpt->key, 0);
				input_sync( navpt->input );
				navpt->rep_rate = navpt->attrs.rep_rate;
				navpt->rep_count = 0;
			}
			else navpt->rep_count++;
		}
		if ( navpt->finger_count <= navpt->attrs.tap_time ) {
			navpt->finger_count++;
		}
	}
	else {
		navpt->finger_count = 0;
		navpt->z = 0;
		navpt->rep_rate = navpt->attrs.rep_wait;
	}

	navpt->old_finger = navpt->finger;

	return;
}

void
tap_timer( unsigned long arg )
{
	struct navpoint_dev *nav_dev = (struct navpoint_dev *)arg;
	struct navpoint_data *navpt = nav_dev->driver_data;
	
	spin_lock(&navpt->timer_lock);
	if (navpt->tapped && !navpt->finger) {
		input_report_key( navpt->input, navpt->tapped, 0 );
		navpt->tapped = 0;
		input_sync( navpt->input );
	}
	spin_unlock(&navpt->timer_lock);

	return;
}

static void
process_mouse_taps( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	u_int8_t *cp=navpt->ps.pkt.data;
	navpt->z = cp[5];
	navpt->finger = cp[0] & 1;
	
	dbg("f: %d z: %d\n", navpt->finger, navpt->z);

	if ( !navpt->finger && navpt->old_finger
			&& navpt->finger_count > 0
			&& navpt->finger_count < navpt->attrs.tap_time ) {
		spin_lock(&navpt->timer_lock);
		if ( timer_pending(&navpt->tap_timer) ) {
			navpt->tapped = 0;
			input_report_key(navpt->input, BTN_LEFT, 0);
			input_report_key(navpt->input, BTN_LEFT, 1);
			input_report_key(navpt->input, BTN_LEFT, 0);
		}
		else if ( navpt->tapped ) {
			navpt->tapped = 0;
			input_report_key(navpt->input, BTN_LEFT, 0);
		}
		else {
			navpt->tapped = BTN_LEFT;
			input_report_key(navpt->input, BTN_LEFT, 1);
			init_timer(&navpt->tap_timer);
			navpt->tap_timer.data = (unsigned long)nav_dev;
			navpt->tap_timer.expires = jiffies + msecs_to_jiffies(
					back_ms2p(navpt->attrs.dtap_time) );
			navpt->tap_timer.function = tap_timer;
			add_timer(&navpt->tap_timer);
		}
		spin_unlock(&navpt->timer_lock);
		input_sync( navpt->input );
	}

	if (navpt->finger && navpt->finger_count <= navpt->attrs.tap_time
			&& navpt->z > navpt->attrs.threshold) {
		navpt->finger_count++;
	}
	else if ( !navpt->finger ) navpt->finger_count = 0;

	navpt->old_finger = navpt->finger;

	return;
}

static void
process_mouse_rel( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	u_int8_t *cp=navpt->ps.pkt.data;

	if (navpt->finger && navpt->z < navpt->attrs.threshold) return;

	input_report_rel( navpt->input, REL_X,  (int8_t)cp[1] );
	input_report_rel( navpt->input, REL_Y, -(int8_t)cp[2] );
	input_sync( navpt->input );

	return;
}

void
navpoint_rx( struct navpoint_dev *nav_dev, u_int8_t octet,
             unsigned int pending )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	struct mep_pkt_stream_desc *ps = &navpt->ps;
	enum mep_rx_result ret;
	u_int8_t *cp;
	
	ret = mep_rx( ps, octet );
	if (ret == MEP_RX_PKT_DONE) {
		cp = ps->pkt.data;
		//printk( "navpoint: ctrl=%d len=%d data=%02x %02x %02x %02x "
		//	"%02x %02x %02x\n", ps->ctrl, ps->len, cp[0], cp[1],
		//	cp[2], cp[3], cp[4], cp[5], cp[6] );
		//if (ps->len == 0 && ps->ctrl == MEP_MODULE_PKT_CTRL_GEN) {
		//	printk("ack\n");
		//} else  printk("pkt\n");
		if (navpt->want_to_init) {
			if (navpt->want_to_init == 2 && cp[0] != 0xFF
					&& pending < 10) {
				/* cp[0] == 0xFF is startup-garbage packet,
				 * driver must eat all 0xFF and do not send
				 * anything. Next packet is always HELLO. */

				if (navpt->attrs.mode == 2) {
					send_parm( nav_dev,  NP_RM_R80 
						           | NP_RM_ABS
						           | NP_RM_REL );
				}
				else {
					send_parm( nav_dev,  NP_RM_R80
						           | NP_RM_ABS );
				}
				
				navpt->want_to_init = 1;
				navpt->wait_ack_tries = 0;
			}
			else if (navpt->want_to_init == 1) { // wait for ack
				if (ps->len == 0 && ps->ctrl == MEP_MODULE_PKT_CTRL_GEN) {
					navpt->old_finger = 0;
					navpt->finger_count = 0;
					navpt->tapped = 0;
					navpt->want_to_init = 0;
				}
				else if (navpt->wait_ack_tries >= 4) {
					navpt->wait_ack_tries = 0;
					navpt->want_to_init = 2;
				}
				else navpt->wait_ack_tries++;
			}
		}
		else if (ps->ctrl == MEP_MODULE_PKT_CTRL_NATIVE
				&& ps->len == 6) {
			switch ( navpt->attrs.mode ) {
				case 0:  process_keyboard( nav_dev );   break;
				case 1:  process_keyboard2( nav_dev );  break;
				case 2:  process_mouse_taps( nav_dev ); break;
				default: break;
			}
		}
		else if (ps->ctrl == MEP_MODULE_PKT_CTRL_GEN
		             && (cp[0] & 0xF0) == NP_PKT_REL && ps->len == 3) {
			if (navpt->attrs.mode == 2) process_mouse_rel( nav_dev );
			else printk("navpoint: unexpected relative packet\n");
		}
		else {
			printk("navpoint: completely unexpected packet "
			        "ctrl=%d len=%d data=%02x %02x %02x "
			        "%02x %02x %02x %02x\n", ps->ctrl, ps->len,
			        cp[0], cp[1], cp[2], cp[3], cp[4], cp[5],
			        cp[6]);
		}
	}
	return;
}

void
navpoint_reset( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;
	navpt->want_to_init = 2;
}

#define DEFINE_INT_ATTR(name, min, max, converter)                           \
	static ssize_t                                                       \
	__get_attr_##name(struct class_device *cdev, char *buf)              \
	{                                                                    \
		struct navpoint_data *navpt = (struct navpoint_data*)        \
			                  to_input_dev(cdev)->private;       \
		return sprintf(buf, "%d\n",                                  \
		                back_##converter(navpt->attrs.name));        \
	}                                                                    \
	                                                                     \
	                                                                     \
	static ssize_t                                                       \
	__set_attr_##name(struct class_device *cdev, const char *val,        \
			  size_t count)                                      \
	{                                                                    \
		struct navpoint_data *navpt = (struct navpoint_data*)        \
			                  to_input_dev(cdev)->private;       \
		unsigned long l;                                             \
		char *tmp;                                                   \
	                                                                     \
		if (!val) goto failed;                                       \
		l = simple_strtoul(val, &tmp, 0);                            \
		if (val == tmp || (unsigned int)l != l) goto failed;         \
		if (l < min && l > max) goto failed;                         \
		navpt->attrs.name = converter(l);                            \
		                                                             \
		return count;                                                \
	failed:                                                              \
		return -EINVAL;                                              \
	}                                                                    \
	static CLASS_DEVICE_ATTR(name, S_IWUSR | S_IRUGO, __get_attr_##name, \
			                                  __set_attr_##name );

static ssize_t
get_attr_mode(struct class_device *cdev, char *buf) {
	struct navpoint_data *navpt = (struct navpoint_data*)
		                  to_input_dev(cdev)->private;
	return sprintf(buf, "%s\n", navpt->attrs.mode_str);
}

static ssize_t
set_attr_mode(struct class_device *cdev, const char *val,
              size_t count)
{
	struct navpoint_data *navpt = (struct navpoint_data*)
		                  to_input_dev(cdev)->private;
	char tmp[11];

	if (count > 10 || strnlen(val, 11) > 10 || !sscanf(val, "%s", tmp)) {
		return -EINVAL;
	}

	if (strcmp(tmp,      "keyboard") == 0)  navpt->attrs.mode = 0;
	else if (strcmp(tmp, "keyboard2") == 0) navpt->attrs.mode = 1;
	else if (strcmp(tmp, "mouse") == 0)     navpt->attrs.mode = 2;
	else return -EINVAL;
	
	strcpy(navpt->attrs.mode_str, tmp);

	navpt->want_to_init = 2;
	return count;
}

static CLASS_DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, get_attr_mode,
                                                  set_attr_mode );

DEFINE_INT_ATTR(tap_time,  25, 1000, ms2p);
DEFINE_INT_ATTR(dtap_time, 25, navpt->attrs.tap_time, ms2p);
DEFINE_INT_ATTR(rep_wait,  25, 10000, ms2p);
DEFINE_INT_ATTR(rep_rate,  0,  10000, ms2p);
DEFINE_INT_ATTR(pressure,  0,  255, none);
DEFINE_INT_ATTR(threshold, 0,  255, none);

static struct attribute *navpoint_attrs[] = {
        &class_device_attr_mode.attr,
        &class_device_attr_tap_time.attr,
        &class_device_attr_dtap_time.attr,
        &class_device_attr_rep_wait.attr,
        &class_device_attr_rep_rate.attr,
        &class_device_attr_pressure.attr,
        &class_device_attr_threshold.attr,
        NULL
};

static struct attribute_group navpoint_attr_group = {
	        .attrs  = navpoint_attrs,
};

int
navpoint_init( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt;

	navpt = nav_dev->driver_data = kmalloc( sizeof *navpt, GFP_KERNEL );
	if (navpt == NULL) goto navpt_failed;
	memset( navpt, 0, sizeof *navpt );

	navpt->input = input_allocate_device();
	if (navpt->input == NULL) goto input_failed;

	navpt->attrs.mode_str = kmalloc( 11, GFP_KERNEL );
	if (navpt->attrs.mode_str == NULL) goto mode_str_failed;
	strcpy(navpt->attrs.mode_str, "keyboard");

	navpt->attrs.mode      = 0;
	navpt->attrs.tap_time  = ms2p(100);
	navpt->attrs.dtap_time = ms2p(175);
	navpt->attrs.rep_wait  = ms2p(625);
	navpt->attrs.rep_rate  = ms2p(75);
	navpt->attrs.pressure  = 140;
	navpt->attrs.threshold = 45;
	navpt->want_to_init    = 2;

	navpt->input->name = "NavPoint Device";
	navpt->input->private = navpt;
	navpt->input->evbit[0]  = BIT(EV_KEY) | BIT(EV_REL);
	navpt->input->relbit[0] = BIT(REL_X)  | BIT(REL_Y);
	navpt->input->keybit[LONG(BTN_MOUSE)] |= BIT(BTN_LEFT);
	navpt->input->keybit[LONG(KEY_UP)]    |= BIT(KEY_UP);
	navpt->input->keybit[LONG(KEY_DOWN)]  |= BIT(KEY_DOWN);
	navpt->input->keybit[LONG(KEY_RIGHT)] |= BIT(KEY_RIGHT);
	navpt->input->keybit[LONG(KEY_LEFT)]  |= BIT(KEY_LEFT);
	navpt->input->keybit[LONG(KEY_ENTER)] |= BIT(KEY_ENTER);

	input_register_device( navpt->input );
	sysfs_create_group(&navpt->input->cdev.kobj, &navpoint_attr_group);
	
	spin_lock_init(&navpt->timer_lock);

	return 0;

mode_str_failed:
	input_free_device( navpt->input );
input_failed:
	kfree( navpt );
navpt_failed:
	printk(KERN_ERR "navpoint: Failed to initialize\n");
	return -ENOMEM;
}

void
navpoint_exit( struct navpoint_dev *nav_dev )
{
	struct navpoint_data *navpt = nav_dev->driver_data;

	sysfs_remove_group( &navpt->input->cdev.kobj, &navpoint_attr_group );
	input_unregister_device( navpt->input );
	kfree( nav_dev->driver_data );

	return;
}

EXPORT_SYMBOL(navpoint_rx);
EXPORT_SYMBOL(navpoint_reset);
EXPORT_SYMBOL(navpoint_init);
EXPORT_SYMBOL(navpoint_exit);

MODULE_LICENSE("GPL");

