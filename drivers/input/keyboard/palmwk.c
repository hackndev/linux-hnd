/*
 * linux/drivers/input/keyboard/palmwk.c
 *
 *  A 4-row Palm Wireless Keyboard driver
 *  Author: Jan Herman <2hp@seznam.cz>
 *  
 * This driver is light modified from original 
 * "Basic 5-row driver" from Alex Osborne <bobofdoom@gmail.com>
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
//	Normal		Fn_Blue		Fn_Green	ScanCode
	0,		0,		0,		/* 0 */
	0,		0,		0,		/* 1 */
	0,		0,	      	0,		/* 2 */
	KEY_Z,		KEY_OK,		0,		/* 3 */
	0,		0,		0,		/* 4 */
	0,		0,		0,		/* 5 */
	0,		0,		0,		/* 6 */
	0,		0,		0,		/* 7 */
	0,		0,		0,		/* 8 */
	KEY_Q,		KEY_1,		KEY_F1,		/* 9 */
	KEY_W,		KEY_2,		KEY_F2,		/* 10 */
	KEY_E,		KEY_3,		KEY_F3,		/* 11 */
	KEY_R,		KEY_4,		KEY_F4,		/* 12 */
	KEY_T,		KEY_5,		KEY_F5,		/* 13 */
	KEY_Y,		KEY_6,		KEY_F6,		/* 14 */
	0,		0,		0,		/* 15 */
	KEY_X,		0,		0,		/* 16 */
	KEY_A,		0,		0,		/* 17 */
	KEY_S,		0,		0,		/* 18 */
	KEY_D,		0,		0,		/* 19 */
	KEY_F,		0,		0,		/* 20 */
	KEY_G,		0,		0,		/* 21 */
	KEY_H,		0,		0,		/* 22 */
	KEY_SPACE,	0,		0,		/* 23 */
	KEY_CAPSLOCK,	0,		0,		/* 24 */
	0,		0,		0,		/* 25 */
	KEY_RIGHTCTRL,	0,		0,		/* 26 */
	KEY_TAB,	0,		0,		/* 27 */
	0,		0,		0,		/* 28 */
	0,		0,		0,		/* 29 */
	0,		0,		0,		/* 30 */
	0,		0,		0,		/* 31 */
	0,		0,		0,		/* 32 */
	0,		0,		0,		/* 33 */
	KEY_RIGHTMETA,	0,		0,		/* 34 */
	KEY_LEFTALT,	0,		0,		/* 35 */
	KEY_LEFTMETA,	0,		0,		/* 36 */
	0,		0,		0,		/* 37 */
	0,		0,		0,		/* 38 */
	0,		0,		0,		/* 39 */
	0,		0,		0,		/* 40 */
	0,		0,		0,		/* 41 */
	0,		0,		0,		/* 42 */
	0,		0,		0,		/* 43 */
	KEY_C,		KEY_CANCEL,	0,		/* 44 */
	KEY_V,		0,		0,		/* 45 */
	KEY_B,		0,		0,		/* 46 */
	KEY_N,		KEY_NEW,	0,		/* 47 */
	0,		0,		0,		/* 48 */
	0,		0,		0,		/* 49 */
	KEY_BACKSPACE,	0,		0,		/* 50 */
	0,		0,		0,		/* 51 */
	0,		0,		0,		/* 52 */
	0,		0,		0,		/* 53 */
	0,		0,		0,		/* 54 */
	KEY_SPACE,	0,		0,		/* 55 */
	KEY_MINUS,	KEY_LEFTBRACE,	KEY_F11,	/* 56 */
	KEY_EQUAL,	KEY_RIGHTBRACE,	KEY_F12,	/* 57 */
	KEY_SLASH,	KEY_BACKSLASH,	0,		/* 58 */
	0,		0,		0,		/* 59 */
	KEY_U,		KEY_7,		KEY_F7,		/* 60 */
	KEY_I,		KEY_8,		KEY_F8,		/* 61 */
	KEY_O,		KEY_9,		KEY_F9,       	/* 62 */
	KEY_P,		KEY_0,		KEY_F10,	/* 63 */
	0,		0,		0,		/* 64 */
	0,		0,		0,		/* 65 */
	0,		0,		0,		/* 66 */
	0,		0,		0,		/* 67 */
	KEY_J,		KEY_HOME,	0,		/* 68 */
	KEY_K,		KEY_MENU,	0,		/* 69 */
	KEY_L,		KEY_BOOKMARKS,	0,		/* 70 */
	KEY_SEMICOLON,	0,		0,		/* 71 */
	KEY_UP,		0,		KEY_PAGEUP,	/* 72 */
	0,		0,		0,		/* 73 */
	0,		0,		0,		/* 74 */
	0,		0,		0,		/* 75 */
	KEY_M,		KEY_DELETE,	KEY_KPPLUS,	/* 76 */
	KEY_COMMA,	0,		KEY_KPMINUS,	/* 77 */
	KEY_DOT,	KEY_SEND,	0,		/* 78 */
	0,		0,		0,		/* 79 */
	KEY_DELETE,	0,		0,		/* 80 */
	KEY_LEFT,	0,		KEY_HOME,	/* 81 */
	KEY_DOWN,	0,		KEY_PAGEDOWN,	/* 82 */
	KEY_RIGHT,	0,		KEY_END,	/* 83 */
	0,		0,		0,		/* 84 */
	0,		0,		0,		/* 85 */
	KEY_APOSTROPHE,	0,		0,		/* 86 */
	KEY_ENTER,	0,		0,		/* 87 */
	KEY_LEFTSHIFT,	0,		0,		/* 88 */
	KEY_RIGHTSHIFT,	0,		0,		/* 89 */
	0,		0,		0,		/* 90 */
	0,		0,		0,		/* 91 */
	0,		0,		0,		/* 92 */
	0,		0,		0,		/* 93 */
	0,		0,		0,		/* 94 */
	0,		0,		0,		/* 95 */
	0,		0,		0,		/* 96 */
	0,		0,		0,		/* 97 */
	0,		0,		0,		/* 98*/
	0,		0,		0,		/* 99 */
	0,		0,		0,		/* 100 */
	0,		0,		0,		/* 101 */
	0,		0,		0,		/* 102 */
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

static struct input_dev *palmwk_dev;
static int state = STATE_OUTSIDE;
static int fn_pressed = 0;
static int fn2_pressed = 0;

static void palmwk_process_scancode(unsigned char data)
{
	int code = data & MASK_CODE;
	int keyup = data & MASK_KEYUP;
	int key = 0;
	
	if(fn_pressed) 		key = keycode_table[code*3 + 1];
	if(fn2_pressed) 	key = keycode_table[code*3 + 2];
	if(!key) 		key = keycode_table[code*3];
	

	if(!key) {
		printk("palmwk_process_scancode: invalid scancode %d\n", code);
		return;
	}
	
	input_report_key(palmwk_dev, key, !keyup);
	if(key == KEY_RIGHTMETA) fn_pressed = !keyup; 		/* Blue Fn key  */
	if(key == KEY_LEFTMETA) fn2_pressed = !keyup;		/* Green Fn key */
}

static void palmwk_process_byte(unsigned char data)
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
			palmwk_process_scancode(data);
			state = STATE_CRC;
			break;
			
		case STATE_CRC:
			/* todo: process this */
			break;
	}
}


int palmwk_receive(void *dev, const unsigned char *cp, size_t count) 
{
	int i;
	
	for(i=0; i<count; i++) {
		palmwk_process_byte(cp[i]);
	}
	
	return 0;
}
EXPORT_SYMBOL(palmwk_receive);

static int __init palmwk_init(void)
{
	int i, key;
	
	palmwk_dev = input_allocate_device();
		
	for(i=0; i<(127*3); i++) {
		if((key = keycode_table[i])) {
			palmwk_dev->keybit[LONG(key)] |= BIT(key);
		}
	}

	palmwk_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	palmwk_dev->name = "Palm wireless keyboard";
	palmwk_dev->id.bustype = BUS_HOST;	

	return input_register_device(palmwk_dev);
}

static void __exit palmwk_exit(void)
{
	input_unregister_device(palmwk_dev);
}


module_init(palmwk_init);
module_exit(palmwk_exit);

MODULE_AUTHOR ("Jan Herman <2hp@seznam.cz>");
MODULE_DESCRIPTION ("Palm 4-row Wireless Keyboard driver");
MODULE_LICENSE ("GPL");
