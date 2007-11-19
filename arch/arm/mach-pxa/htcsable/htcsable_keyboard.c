/*
 *  htcsable_keyboard.c
 *  Keyboard support for the hw6915 ipaq
 *
 *  Copyright (C) 2006 Luke Kenneth Casson Leighton
 *
 *  This code is released under the GNU General Public License
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input_pda.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa27x_keyboard.h>
#include <asm/hardware.h>
#include <asm/arch/bitfield.h>

/****************************************************************
 * Pull out keyboard
 ****************************************************************/

static struct pxa27x_keyboard_platform_data htcsable_kbd = {
	.nr_rows = 7,
	.nr_cols = 7,
	.keycodes = {
		{
			/* row 0 */
			-1,		// Unused
			KEY_LEFTSHIFT,	// Left Shift .
			-1,		// Unused
			KEY_Q,		// Q .
			KEY_W,		// W .
			KEY_E,		// E .
			KEY_R,	 	// R .
		}, {	/* row 1 */
			-1,		// Unused
			-1,		// Unused
			KEY_LEFTALT,	// Red Dot .
			KEY_T,		// T .
			KEY_Y,		// Y .
			KEY_U,		// U .
			KEY_I,		// I .
		}, {	/* row 2 */
			KEY_PHONE,		// Phone Up .
			KEY_TAB,	// Tab .
			-1,		// Unused
			KEY_ENTER,	// Return .
			KEY_SPACE,	// Space .
			KEY_BACKSPACE,	// Backspace .
			KEY_A,		// A .
		}, {	/* row 3 */
			KEY_STOP,		// Phone Down .
			KEY_S,		// S .
			KEY_D,		// D .
			KEY_F,		// F .
			KEY_G,		// G .
			KEY_H,		// H .
			KEY_J,		// J .
		}, {	/* row 4 */
			KEY_LEFTMETA,	// Left Menu .
			KEY_K,		// K .
			KEY_Z,		// Z .
			KEY_X,		// X .
			KEY_C,		// C .
			KEY_V,		// V .
			KEY_B,		// B .
		}, {	/* row 5 */
			KEY_RIGHTMETA,	// Right Menu .
			KEY_N,		// N .
			KEY_M,		// M .
			KEY_O,		// O .
			KEY_L,		// L .
			KEY_P,		// P .
			KEY_APOSTROPHE, 	// ' .
		}, {	/* row 6 */
			KEY_CAMERA,		// Camera .
			KEY_DOT,	// . .
			KEY_COMMA,	// , .
			-1,		// unused
			-1,	// unused
			KEY_MENU,	// Menu button .
			KEY_OK,	// OK button .
		},
	},
	.gpio_modes = {
		 GPIO_NR_HTCSABLE_KP_MKIN0_MD, 
		 GPIO_NR_HTCSABLE_KP_MKIN1_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN2_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN3_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN4_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN5_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN6_MD,
		 GPIO_NR_HTCSABLE_KP_MKIN7_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT0_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT1_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT2_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT3_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT4_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT5_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT6_MD,
		 GPIO_NR_HTCSABLE_KP_MKOUT7_MD,
	 },
};

struct platform_device htcsable_keyboard = {
        .name   = "pxa27x-keyboard",
        .id     = -1,
	.dev	=  {
		.platform_data	= &htcsable_kbd,
	},
};
