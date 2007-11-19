/*
* Keyboard stub driver for the HP iPAQ H3xxx
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          October 30, 2001
*/

#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/errno.h>
#include <asm/keyboard.h>

int h3600_kbd_translate(unsigned char scancode, unsigned char *keycode, char raw_mode)
{
	*keycode = scancode;
	return 1;
}

char h3600_kbd_unexpected_up(unsigned char keycode)
{
	return 0200;
}

void __init h3600_kbd_init_hw(void)
{
	printk(KERN_INFO "iPAQ H3600 keyboard driver v1.0.1\n");

	k_translate	= h3600_kbd_translate;
	k_unexpected_up	= h3600_kbd_unexpected_up;
}

