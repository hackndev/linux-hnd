/*
 * Input driver for Asus A620
 * Based on Input driver for Dell Axim X5.
 *
 * Creates input events for the buttons on the device
 *
 * Copyright 2004 Matthew Garrett
 *                Nicolas Pouillon: Adaptation for A620
 *
 * Copyright 2006 Vincent Benony
 *                Remap all keys in order to be compatible with A716 drivers
 *                Some corrections about declaration of used keys
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus620-gpio.h>

#define GET_GPIO(gpio) (GPLR(gpio) & GPIO_bit(gpio))

struct input_dev *button_dev;
static volatile int joypad_event_count;
/* Current joystick state bits: ENTER (bit 0), UP (1), RIGHT, DOWN, LEFT */
static u32 joystate;

static struct {
	u16	keyno;
	u8	irq;
	u8	gpio;
        char	*desc;
} buttontab [] = {
#define KEY_DEF(x) A620_IRQ (x), GPIO_NR_A620_##x
	{ KEY_POWER,      KEY_DEF (POWER_BUTTON_N),    "Sleep button"    },
	{ _KEY_RECORD,     KEY_DEF (RECORD_BUTTON_N),   "Record button"   },
	{ _KEY_CALENDAR,   KEY_DEF (CALENDAR_BUTTON_N), "Calendar button" },
	{ _KEY_CONTACTS,   KEY_DEF (CONTACTS_BUTTON_N), "Contacts button" },
	{ _KEY_MAIL,       KEY_DEF (TASKS_BUTTON_N),    "Tasks button"    },
	{ _KEY_HOMEPAGE,   KEY_DEF (HOME_BUTTON_N),     "Home button"     }
#undef KEY_DEF
};

static u8 joyirq [] = {
	A620_IRQ (JOY_PUSH_N),
	A620_IRQ (JOY_NW_N),
	A620_IRQ (JOY_NE_N),
	A620_IRQ (JOY_SE_N),
	A620_IRQ (JOY_SW_N),
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
 * the same time), we default to 0 (e.g. KEY_ENTER). At any given time
 * joystick cannot generate more than two pressed keys. So we're using
 * the lower nibble for key index 1 and upper nibble for key index 2
 * (if upper nibble is f, the particular bit combination generates just
 * one key press). Also note that a bit value of '0' means key is pressed,
 * while '1' means key is not pressed.
 */
static u8 joydecode [16] = {
/*                 SSNN */
/*                 WEEW */
	0xf0,		/* 0000 */
	0xf0,		/* 0001 */
	0xf0,		/* 0010 */
	0xf3,		/* 0011 */
	0xf0,		/* 0100 */
	0xf0,		/* 0101 */
	0xf2,		/* 0110 */
	0x34,		/* 0111 */
	0xf0,		/* 1000 */
	0xf4,		/* 1001 */
	0xf0,		/* 1010 */
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
	KEY_LEFT,	/* 2 */
	KEY_DOWN,	/* 3 */
	KEY_RIGHT	/* 4 */
};

irqreturn_t a620_button_handle(int irq, void *dev_id)
{
	int button, down;

	/* look up the keyno */
	for (button = 0; button < ARRAY_SIZE (buttontab); button++)
		if (buttontab [button].irq == irq)
			break;

	/* This happens with 100% probability */
	if (likely (button < ARRAY_SIZE (buttontab))) {
		down = GET_GPIO (buttontab [button].gpio) ? 0 : 1;
		input_report_key (button_dev, buttontab [button].keyno, down);
/* 		printk(KERN_INFO "%s button %d\n", (down ? "Pushed" : "Released"), button );  */
		input_sync (button_dev);
	}

	return IRQ_HANDLED;
}

static void joypad_decode (void)
{
	u32 i, code, newstate;

	/* Decode current joystick bit combination */
	code = joydecode [GPIO_A620_JOY_DIR];

	newstate = GET_A620_GPIO (JOY_PUSH_N) ? 0 :
		((1 << (code & 15)) | (1 << (code >> 4)));
	/* We're not interested in other bits (e.g. 1 << 0xf) */
	newstate &= 0x1f;

	/* Find out which bits have changed */
	joystate ^= newstate;
	for (i = 0; i <= 5; i++)
		if (joystate & (1 << i)) {
			int down = (newstate & (1 << i)) ? 1 : 0;
			input_report_key (button_dev, joykeycode [i], down);
			input_sync (button_dev);
		}
	joystate = newstate;

/* 	printk(KERN_INFO "Keys state: %02x\n", joystate); */
}

irqreturn_t a620_joy_handle(int irq, void* dev_id)
{
	joypad_event_count++;

	joypad_decode();

	return IRQ_HANDLED;
}

static int __init a620_button_init (void)
{
	int i;

	joystate = 0;

	button_dev = input_allocate_device();

	set_bit(EV_KEY, button_dev->evbit);

	/* Joystick emmits UP/DOWN/LEFT/RIGHT/SELECT key events */
	set_bit(KEY_UP,    button_dev->keybit);
	set_bit(KEY_DOWN,  button_dev->keybit);
	set_bit(KEY_LEFT,  button_dev->keybit);
	set_bit(KEY_RIGHT, button_dev->keybit);
	set_bit(KEY_ENTER, button_dev->keybit);

	for (i = 0; i < ARRAY_SIZE (buttontab); i++)
	{
		/* TODO : Configure GPIO to inputs */
		request_irq(buttontab[i].irq, a620_button_handle, SA_SAMPLE_RANDOM, buttontab[i].desc, NULL);
		set_irq_type(buttontab[i].irq, IRQT_BOTHEDGE);
		set_bit(buttontab[i].keyno, button_dev->keybit);
	}

	for (i = 0; i < ARRAY_SIZE (joyirq); i++)
	{
		request_irq(joyirq [i], a620_joy_handle, SA_SAMPLE_RANDOM, joyirqdesc[i], NULL);
		set_irq_type(joyirq [i], IRQT_BOTHEDGE);
	}

	button_dev->name = "Asus MyPal 620 buttons driver";
	input_register_device (button_dev);

	printk (KERN_INFO "%s installed\n", button_dev->name);

	return 0;
}

static void __exit a620_button_exit (void)
{
	int i;

	joypad_event_count++;

	input_unregister_device(button_dev);
	input_free_device(button_dev);

	for (i = 0; i < ARRAY_SIZE (joyirq); i++)
		free_irq(joyirq [i], NULL);

	for (i = 0; i < ARRAY_SIZE (buttontab); i++)
		free_irq(buttontab [i].irq, NULL);
}

module_init(a620_button_init);
module_exit(a620_button_exit);

MODULE_AUTHOR ("Nicolas Pouillon <nipo@ssji.net>, Vincent Benony");
MODULE_DESCRIPTION ("Button support for Asus MyPal 620");                         
MODULE_LICENSE ("GPL");
