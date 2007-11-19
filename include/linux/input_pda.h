#ifndef _INPUT_PDA_H
#define _INPUT_PDA_H

/*
 * This is temporary virtual button key codes map
 * for keyboardless handheld computers.
 * Its purpose is to provide map common to all devices
 * and known to work with current software and its bugs
 * and misfeatures. Once issues with the software are
 * solved, codes from input.h will be used directly
 * (missing key definitions will be added).
 */

/* Some directly usable keycodes:
KEY_POWER - Power/suspend button
KEY_ENTER - Enter/Action/Central button on joypad
KEY_UP
KEY_DOWN
KEY_LEFT
KEY_RIGHT
*/

/* XXX Instead of using any values in include/linux/input.h, we have to use
       use values < 128 due to some munging that kdrive does to get keystrokes.
       When kdrive gets its key events from evdev instead of the console,
       we should be able to switch to using input.h values and get rid of
       xmodmap. */

#define _KEY_APP1	KEY_F9		// xmodmap sees 67 + 8 = 75  
#define _KEY_APP2	KEY_F10		// xmodmap 76                
#define _KEY_APP3	KEY_F11		// xmodmap 95                
#define _KEY_APP4	KEY_F12		// xmodmap 96                

#define _KEY_RECORD	KEY_RO

/* It is highly recommended to use exactly 4 codes above for
   4 buttons the device has. This will ensure that console and 
   framebuffer applications (e.g. games) will work ok on all 
   devices. If you'd like more distinguishable names, following
   convenience defines are provided, suiting many devices. */

#define _KEY_CALENDAR	_KEY_APP1
#define _KEY_CONTACTS	_KEY_APP2
#define _KEY_MAIL	_KEY_APP3
#define _KEY_HOMEPAGE	_KEY_APP4

#endif
