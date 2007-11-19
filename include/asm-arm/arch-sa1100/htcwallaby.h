/*
   linux/include/asm-arm/arch-sa1100/xda.h
   Copyright (C) 2005 Henk Vergonet

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/
/*
   This file contains the hardware specific definitions for the XDA
   There is a lot of guess work, so be alert.
*/

#ifndef __ASM_ARCH_XDA_H
#define __ASM_ARCH_XDA_H

#ifndef __ASSEMBLY__

/*  Source: http://wiki.xda-developers.com/index.php?pagename=WallabyGPIO

    GPIO pin 0		Power button
    GPIO pin 1	 	GSM on (active high)
    GPIO pin 2-9 	LDD(7:0) lcd control registers (Alternate GPIO function)
    GPIO pin 10 	If high skip boot loader
    GPIO pin 11 	if == 1 set A6000000/0x200 related to GSM
    GPIO pin 12 	Unkown, also used in Alternate mode for MCP (Sound)
    GPIO pin 13 	Unkown, also used in Alternate mode for MCP (Sound)
    GPIO pin 14 	Unkown, also used in Alternate mode for MCP (Sound)
    GPIO pin 15 	Unkown, also used in Alternate mode for MCP (Sound)
    GPIO pin 16 	Unkown, also used in Alternate mode for MCP (Sound)
    GPIO pin 17 	Touch screen clock
    GPIO pin 18 	Unknown (may be Touch screen busy? )
    GPIO pin 19 	Seems to be something to do with Sound too(?)
    GPIO pin 20 	Button clock
    GPIO pin 21 	Is SDcard inserted? (1 = inserted)
    GPIO pin 22 	Touch screen pressed (active low)
    GPIO pin 23 	Touch screen serial output
    GPIO pin 24 	Touch screen serial input
    GPIO pin 25 	Button serial input
    GPIO pin 26 	Button pressed
    GPIO pin 27 	SDcard write protect (0 = write protected)
*/

/* SA-1110 GPIO lines */
#define GPIO_XDA_BUTTON_PWR		GPIO_GPIO0
#define GPIO_XDA_BUTTON_PWR_IRQ		IRQ_GPIO0
#define GPIO_XDA_TOUCH_CLK		GPIO_GPIO17
#define GPIO_XDA_TOUCH_BUSY		GPIO_GPIO18
#define GPIO_XDA_BUTTON_CLK		GPIO_GPIO20
#define GPIO_XDA_TOUCH			GPIO_GPIO22
#define GPIO_XDA_TOUCH_IRQ		IRQ_GPIO22
#define GPIO_XDA_TOUCH_DATA_OUT		GPIO_GPIO23
#define GPIO_XDA_TOUCH_DATA_IN		GPIO_GPIO24
#define GPIO_XDA_BUTTON_DATA_IN		GPIO_GPIO25
#define GPIO_XDA_BUTTON			GPIO_GPIO26
#define GPIO_XDA_BUTTON_IRQ		IRQ_GPIO26

/* 30H8001250 HTC interface asic stuff?
   - touch screen ( may be AD7873 compatible )
   - SIM
   - Ext. Connector
   - MMC Card
   - Phillips UDA1341 Audio codec ?
*/

/* Is this the EGPIO ?? */
#define XDA_EGPIO_BASE		0xa4000000	/* physical 0x18000000 */

#define XDA_EGPIO_R64	__REG(XDA_EGPIO_BASE + 0x064)	/* direction reg? */
#define XDA_EGPIO_R68	__REG(XDA_EGPIO_BASE + 0x068)	/* IO reg?	  */

#define XDA_EGPIO_unknown3		(1<<  3)
#define XDA_EGPIO_BACKLIGHT_ENABLE	(1<<  4)
#define XDA_EGPIO_unknown6		(1<<  6)
#define XDA_EGPIO_TOUCH_ENABLE		(1<< 12)


/* This is a write only register that is assumed to control power on
   various devices. For now it's labled as the ASIC_A6 register.
*/
extern uint __xda_save_asic_a6;
#define XDA_ASIC_A6_BASE	0xa6000000 	/* physical 0x40000000 */
#define XDA_ASIC_A6_REG		 __REG(XDA_ASIC_A6_BASE)

uint __inline__ xda_asic_a6_set_bits(uint m) {
	__xda_save_asic_a6 |= m;
	__REG(XDA_ASIC_A6_BASE) = __xda_save_asic_a6;
	return __xda_save_asic_a6;
}
uint __inline__ xda_asic_a6_clear_bits(uint m) {
	__xda_save_asic_a6 &= ~m;
	__REG(XDA_ASIC_A6_BASE) = __xda_save_asic_a6;
	return __xda_save_asic_a6;
}
uint __inline__ xda_asic_a6_get_bits(void) {
	return __xda_save_asic_a6;
}

/* bootloader initialization setting, unfortunately we still have to work out
   the specific relations between the A6000000 and A4000006[48] registers
*/
#define XDA_ASIC_A6_INIT_VALUE	0xD47E

/* specific bit positions */

#define XDA_ASIC_A6_BUTTON_CLK		(1 <<  0)	
#define XDA_ASIC_A6_unknown9		(1 <<  9)
#define XDA_ASIC_A6_GSM_PWR		(1 << 13)

#define XDA_ASIC_A6_BACKLIGHT_CLK	(1 << 14)
#define XDA_ASIC_A6_ENABLE_SDCARD	(1 << 16)

#endif
#endif /* __ASM_ARCH_XDA_H */
