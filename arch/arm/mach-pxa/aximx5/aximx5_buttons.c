/*
 * Input driver for Dell Axim X5.
 * Creates input events for the buttons on the device
 *
 * Copyright 2004 Matthew Garrett
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/aximx5-gpio.h>

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

struct input_dev button_dev;
static volatile int joypad_event_count;
static wait_queue_head_t joypad_event;
struct completion thread_comp;
static volatile int must_die;
/* Current joystick state bits: ENTER (bit 0), UP (1), RIGHT, DOWN, LEFT */
static u32 joystate;

static struct {
	u16 keyno;
	u8 irq;
	u8 gpio;
        char *desc;
} buttontab [] = {
#define KEY_DEF(x) AXIMX5_IRQ (x), GPIO_NR_AXIMX5_##x
	{ KEY_POWER,      KEY_DEF (POWER_BUTTON_N),  "Sleep button"    },
	{ KEY_RECORD,     KEY_DEF (BUTTON_RECORD),   "Record button"   },
	{ KEY_CALENDAR,   KEY_DEF (BUTTON_CALENDAR), "Calendar button" },
	{ KEY_CONTACTS,   KEY_DEF (BUTTON_CONTACTS), "Contacts button" },
	{ KEY_MAIL,       KEY_DEF (BUTTON_MAILBOX),  "Mailbox button"  },
	{ KEY_HOMEPAGE,   KEY_DEF (BUTTON_HOME),     "Home button"     },
	{ KEY_SCROLLDOWN, KEY_DEF (SCROLL_DOWN),     "Scroll down"     },
	{ KEY_SELECT,     KEY_DEF (SCROLL_PUSH),     "Scroll push"     },
	{ KEY_SCROLLUP,   KEY_DEF (SCROLL_UP),       "Scroll up"       },
#undef KEY_DEF
};

static u8 joyirq [] = {
	AXIMX5_IRQ (JOY_PRESSED),
	AXIMX5_IRQ (JOY_NW),
	AXIMX5_IRQ (JOY_NE),
	AXIMX5_IRQ (JOY_SE),
	AXIMX5_IRQ (JOY_SW),
};

static char *joyirqdesc [] =
{
	"Joystick pressed",
	"Joystick NW",
	"Joystick NE",
	"Joystick SE",
	"Joystick SW"
};

/* This table is used to convert 4-bit joystick state into key indices.
 * If the bit combination have no logical meaning (say NW/SE are set at
 * the same time), we default to 0 (e.g. KEY_SELECT). At any given time
 * joystick cannot generate more than two pressed keys. So we're using
 * the lower nibble for key index 1 and upper nibble for key index 2
 * (if upper nibble is f, the particular bit combination generates just
 * one key press). Also note that a bit value of '0' means key is pressed,
 * while '1' means key is not pressed.
 */
static u8 joydecode [16] = {
	0xff,		/* 0000 */
	0x14,		/* 0001 */
	0x12,		/* 0010 */
	0xf3,		/* 0011 */
	0x34,		/* 0100 */
	0xff,		/* 0101 */
	0xf4,		/* 0110 */
	0x34,		/* 0111 */
	0x23,		/* 1000 */
	0xf2,		/* 1001 */
	0xff,		/* 1010 */
	0x23,		/* 1011 */
	0xf1,		/* 1100 */
	0x12,		/* 1101 */
	0x14,		/* 1110 */
	0xf0,		/* 1111 */
};

/* This table is used to convert key indices into actual key codes.
 */
static u16 joykeycode [5] = {
	KEY_ENTER,	/* 0 */
	KEY_UP,		/* 1 */
	KEY_RIGHT,	/* 2 */
	KEY_DOWN,	/* 3 */
	KEY_LEFT	/* 4 */
};

irqreturn_t aximx5_button_handle (int irq, void *dev_id, struct pt_regs *regs)
{
	int button, down;

	/* look up the keyno */
	for (button = 0; button < ARRAY_SIZE (buttontab); button++)
		if (buttontab [button].irq == irq)
			break;

	/* This happens with 100% probability */
	if (likely (button < ARRAY_SIZE (buttontab))) {
		down = GET_GPIO (buttontab [button].gpio) ? 1 : 0;
		if (buttontab [button].keyno == KEY_SLEEP)
			down ^= 1;
		input_report_key (&button_dev, buttontab [button].keyno, down);
		input_sync (&button_dev);
	}

	return IRQ_HANDLED;
}

irqreturn_t aximx5_joy_handle (int irq, void* dev_id, struct pt_regs *regs)
{
	joypad_event_count++;
	wake_up_interruptible (&joypad_event);
	return IRQ_HANDLED;
}

static void joypad_decode (void)
{
	u32 i, code, newstate;

	/* Decode current joystick bit combination */
	code = joydecode [GPIO_JOY_DIR];

	newstate = GET_AXIMX5_GPIO (JOY_PRESSED) ? 0 :
		((1 << (code & 15)) | (1 << (code >> 4)));
	/* We're not interested in other bits (e.g. 1 << 0xf) */
	newstate &= 0x1f;

	/* Find out which bits have changed */
	joystate ^= newstate;
	for (i = 0; i <= 5; i++)
		if (joystate & (1 << i)) {
			int down = (newstate & (1 << i)) ? 1 : 0;
			input_report_key (&button_dev, joykeycode [i], down);
			input_sync (&button_dev);
		}
	joystate = newstate;
}

static int joypad_thread (void *arg)
{
	int old_count;

	daemonize ("kjoypad");
	set_user_nice (current, 5);

	for (;;) {
		if (must_die)
			break;

		joypad_decode ();

		old_count = joypad_event_count;
		wait_event_interruptible (joypad_event, old_count != joypad_event_count);

		if (must_die)
			break;

		do {
			old_count = joypad_event_count;
			set_current_state (TASK_INTERRUPTIBLE);
			schedule_timeout (HZ/50);
		} while (old_count != joypad_event_count);
	}

	complete_and_exit (&thread_comp, 0);
}

static int __init aximx5_button_init (void)
{
	int i;

        must_die = 0;
	joystate = 0;

	set_bit (EV_KEY, button_dev.evbit);

	/* Joystick emmits UP/DOWN/LEFT/RIGHT/SELECT key events */
	button_dev.keybit [LONG (KEY_UP)]    |= BIT (KEY_UP);
	button_dev.keybit [LONG (KEY_DOWN)]  |= BIT (KEY_DOWN);
	button_dev.keybit [LONG (KEY_LEFT)]  |= BIT (KEY_LEFT);
	button_dev.keybit [LONG (KEY_RIGHT)] |= BIT (KEY_RIGHT);
	button_dev.keybit [LONG (KEY_SELECT)] |= BIT (KEY_SELECT);

	for (i = 0; i < ARRAY_SIZE (buttontab); i++) {
		request_irq (buttontab [i].irq, aximx5_button_handle,
			     SA_SAMPLE_RANDOM, buttontab [i].desc, NULL);
		set_irq_type (buttontab [i].irq, IRQT_BOTHEDGE);
		button_dev.keybit [LONG (buttontab [i].keyno)] =
			BIT (buttontab[i].keyno);
	}

	for (i = 0; i < ARRAY_SIZE (joyirq); i++) {
		request_irq (joyirq [i], aximx5_joy_handle,
			     SA_SAMPLE_RANDOM, joyirqdesc [i], NULL);
		set_irq_type (joyirq [i], IRQT_BOTHEDGE);
	}

	button_dev.name = "Dell Axim X5 buttons driver";
	input_register_device (&button_dev);

	init_waitqueue_head (&joypad_event);
	init_completion (&thread_comp);
	kernel_thread (joypad_thread, NULL, 0);

	printk (KERN_INFO "%s installed\n", button_dev.name);

	return 0;
}

static void __exit aximx5_button_exit (void)
{
	int i;

	/* First of all, kill kthread */
	must_die = 1;
	joypad_event_count++;
	wake_up_interruptible (&joypad_event);
	wait_for_completion (&thread_comp);

        input_unregister_device (&button_dev);

	for (i = 0; i < ARRAY_SIZE (joyirq); i++)
		free_irq (joyirq [i], NULL);

	for (i = 0; i < ARRAY_SIZE (buttontab); i++)
		free_irq (buttontab [i].irq, NULL);
}

module_init (aximx5_button_init);
module_exit (aximx5_button_exit);

MODULE_AUTHOR ("Matthew Garrett <mjg59@srcf.ucam.org>");
MODULE_DESCRIPTION ("Button support for Dell Axim X5");                         
MODULE_LICENSE ("GPL");
