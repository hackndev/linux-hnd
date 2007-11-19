/*
 * linux/drivers/input/keyboard/palmirkbd.c
 *
 *  A basic 5-row Palm IR keyboard driver
 *
 *  Author: Alex Osborne <bobofdoom@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/input.h>

#define STATE_OUTSIDE	0
#define STATE_BEGIN 	1
#define STATE_SCANCODE	2
#define STATE_CRC	3

#define XBOF 0xff
#define BOF 0xc0
#define EOF 0xc1

#define MASK_CODE	0x7f
#define MASK_KEYUP	0x80

static unsigned char keycode_table[] = {
//	Normal		Fn		NumLock		ScanCode
	0,		0,		0,		/* 0 */
	0,		0,		0,		/* 1 */
	KEY_RIGHTMETA,	0,	      	0,		/* 2 */
	KEY_LEFTMETA,	0,		0,		/* 3 */
	0,		0,		0,		/* 4 */
	0,		0,		0,		/* 5 */
	0,		0,		0,		/* 6 */
	0,		0,		0,		/* 7 */
	0,		0,		0,		/* 8 */
	0,		0,		0,		/* 9 */
	0,		0,		0,		/* 10 */
	0,		0,		0,		/* 11 */
	0,		0,		0,		/* 12 */
	KEY_TAB,	0,		0,		/* 13 */
	KEY_GRAVE,	KEY_ESC,	0,		/* 14 */
	0,		0,		0,		/* 15 */
	0,		0,		0,		/* 16 */
	KEY_LEFTALT,	0,		0,		/* 17 */
	KEY_LEFTSHIFT,	0,		0,		/* 18 */
	0,		0,		0,		/* 19 */
	KEY_LEFTCTRL,	0,		0,		/* 20 */
	KEY_Q,		KEY_MAIL,	0,		/* 21 */
	KEY_1,		KEY_F1,		0,		/* 22 */
	0,		0,		0,		/* 23 */
	0,		0,		0,		/* 24 */
	0,		0,		0,		/* 25 */
	KEY_Z,		0,		0,		/* 26 */
	KEY_S,		0,		0,		/* 27 */
	KEY_A,		0,		0,		/* 28 */
	KEY_W,		0,		0,		/* 29 */
	KEY_2,		KEY_F2,		0,		/* 30 */
	KEY_DELETE,	0,		0,		/* 31 */
	0,		0,		0,		/* 32 */
	KEY_C,		0,		0,		/* 33 */
	KEY_X,		0,		0,		/* 34 */
	KEY_D,		0,		0,		/* 35 */
	KEY_E,		0,		0,		/* 36 */
	KEY_4,		KEY_F4,		0,		/* 37 */
	KEY_3,		KEY_F3,		0,		/* 38 */
	0,		0,		0,		/* 39 */
	KEY_UP,		KEY_PAGEUP,	0,		/* 40 */
	KEY_SPACE,	0,		0,		/* 41 */
	KEY_V,		0,		0,		/* 42 */
	KEY_F,		0,		0,		/* 43 */
	KEY_T,		0,		0,		/* 44 */
	KEY_R,		0,		0,		/* 45 */
	KEY_5,		KEY_F5,		0,		/* 46 */
	KEY_RIGHT,	KEY_END,	0,		/* 47 */
	KEY_RIGHTALT,	KEY_RIGHTCTRL,	0,		/* 48 */
	KEY_N,		0,		0,		/* 49 */
	KEY_B,		0,		0,		/* 50 */
	KEY_H,		0,		0,		/* 51 */
	KEY_G,		0,		0,		/* 52 */
	KEY_Y,		0,		0,		/* 53 */
	KEY_6,		KEY_F6,		0,		/* 54 */
	0,		0,		0,		/* 55 */
	0,		0,		0,		/* 56 */
	0,		0,		0,		/* 57 */
	KEY_M,		0,		KEY_KP0,	/* 58 */
	KEY_J,		0,		KEY_KP1,	/* 59 */
	KEY_U,		0,		KEY_KP4,	/* 60 */
	KEY_7,		KEY_F7,		KEY_KP7,	/* 61 */
	KEY_8,		KEY_F8,		KEY_KP8,       	/* 62 */
	0,		0,		0,		/* 63 */
	0,		0,		0,		/* 64 */
	0,		0,		0,		/* 65 */
	KEY_K,		0,		KEY_KP2,	/* 66 */
	KEY_I,		0,		KEY_KP5,	/* 67 */
	KEY_O,		0,		KEY_KP6,	/* 68 */
	KEY_0,		KEY_F10,	KEY_KPASTERISK,	/* 69 */
	KEY_9,		KEY_F9,		KEY_KP9,	/* 70 */
	0,		0,		0,		/* 71 */
	KEY_COMMA,	KEY_INSERT,	0,		/* 72 */
	KEY_DOT,	0,		KEY_KPDOT,	/* 73 */
	KEY_SLASH,	0,		KEY_KPSLASH,	/* 74 */
	KEY_L,		0,		KEY_KP3,	/* 75 */
	KEY_SEMICOLON,	0,		KEY_KPPLUS,	/* 76 */
	KEY_P,		0,		KEY_KPMINUS,	/* 77 */
	KEY_MINUS,	KEY_F11,	0,		/* 78 */
	0,		0,		0,		/* 79 */
	0,		0,		0,		/* 80 */
	0,		0,		0,		/* 81 */
	KEY_APOSTROPHE,	0,		0,		/* 82 */
	0,		0,		0,		/* 83 */
	KEY_LEFTBRACE,	KEY_SCROLLLOCK,	0,		/* 84 */
	KEY_EQUAL,	KEY_F12,	0,		/* 85 */
	0,		0,		0,		/* 86 */
	0,		0,		0,		/* 87 */
	KEY_CAPSLOCK,	KEY_NUMLOCK,	0,		/* 88 */
	KEY_RIGHTSHIFT,	0,		0,		/* 89 */
	KEY_ENTER,	0,		KEY_KPENTER,	/* 90 */
	KEY_RIGHTBRACE,	KEY_SYSRQ,	0,		/* 91 */
	0,		0,		0,		/* 92 */
	KEY_BACKSLASH,	KEY_PAUSE,	0,		/* 93 */
	KEY_LEFT,	KEY_HOME,	0,		/* 94 */
	0,		0,		0,		/* 95 */
	KEY_DOWN,	KEY_PAGEDOWN,	0,		/* 96 */
	0,		0,		0,		/* 97 */
	0,		0,		0,		/* 98*/
	0,		0,		0,		/* 99 */
	0,		0,		0,		/* 100 */
	0,		0,		0,		/* 101 */
	KEY_BACKSPACE,	0,		0,		/* 102 */
	0,		0,		0,		/* 103 */
	0,		0,		0,		/* 104 */
	0,		0,		0,		/* 105 */
	0,		0,		0,		/* 106 */
	0,		0,		0,		/* 107 */
	0,		0,		0,		/* 108 */
	0,		0,		0,		/* 109 */
	0,		0,		0,		/* 110 */
	0,		0,		0,		/* 111 */
	0,		0,		0,		/* 112 */
	0,		0,		0,		/* 113 */
	0,		0,		0,		/* 114 */
	0,		0,		0,		/* 115 */
	0,		0,		0,		/* 116 */
	0,		0,		0,		/* 117 */
	0,		0,		0,		/* 118 */

};

static struct input_dev *palmirkbd_dev;
static int state = STATE_OUTSIDE;
static int fn_pressed = 0;

static void palmirkbd_process_scancode(unsigned char data)
{
	int code = data & MASK_CODE;
	int keyup = data & MASK_KEYUP;
	int key = 0;

	/* modifiers */
	int numlock = test_bit(LED_NUML, palmirkbd_dev->key);
	
	/* translate scancode */
	if(fn_pressed) 		key = keycode_table[code*3 + 1];
	if(!key && numlock) 	key = keycode_table[code*3 + 2];
	if(!key) 		key = keycode_table[code*3];
	

	if(!key) {
		printk("palmirkbd_process_scancode: invalid scancode %d\n", code);
		return;
	}
	
	input_report_key(palmirkbd_dev, key, !keyup);
	if(key == KEY_RIGHTMETA) fn_pressed = !keyup;
}

static void palmirkbd_process_byte(unsigned char data)
{
	/* at any stage, c1 means end of frame */
	if(data == EOF) {
		state = STATE_OUTSIDE;
		return;
	}

	switch(state) {
		case STATE_OUTSIDE:
			if(data == XBOF)
				state = STATE_BEGIN;
			break;
		
		case STATE_BEGIN:
			if(data == BOF)
				state = STATE_SCANCODE;
			else
				state = STATE_OUTSIDE;
			break;
			
		case STATE_SCANCODE:
			palmirkbd_process_scancode(data);
			state = STATE_CRC;
			break;
			
		case STATE_CRC:
			/* todo: process this */
			break;
	}
}


int palmirkbd_receive(void *dev, const unsigned char *cp, size_t count) 
{
	int i;
	
	for(i=0; i<count; i++) {
		palmirkbd_process_byte(cp[i]);
	}
	
	return 0;
}
EXPORT_SYMBOL(palmirkbd_receive);

static int __init palmirkbd_init(void)
{
	int i, key;
	
	palmirkbd_dev = input_allocate_device();
		
	for(i=0; i<(127*3); i++) {
		if((key = keycode_table[i])) {
			palmirkbd_dev->keybit[LONG(key)] |= BIT(key);
		}
	}

	palmirkbd_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	palmirkbd_dev->name = "Palm IR keyboard";
	palmirkbd_dev->id.bustype = BUS_HOST;	

	return input_register_device(palmirkbd_dev);
}

static void __exit palmirkbd_exit(void)
{
	input_unregister_device(palmirkbd_dev);
}


module_init(palmirkbd_init);
module_exit(palmirkbd_exit);

MODULE_AUTHOR ("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION ("Palm IR Keyboard driver");
MODULE_LICENSE ("GPL");
