/*
 * linux/include/asm-arm/arch-pxa/pxa27x-keypad.h
 *
 * Support for XScale PWM driven backlights.
 *
 * Author:  Alex Osborne <bobofdoom@gmail.com>
 * Created: May 2006
 *
 */

/*
#define PALMT3_MAX_INTENSITY      0x100
#define PALMT3_DEFAULT_INTENSITY  0x050
#define PALMT3_LIMIT_MASK	  0x7F
*/

#ifndef __ASM_ARCH_PXA27X_KEYPAD_H
#define __ASM_ARCH_PXA27X_KEYPAD_H

#include <linux/backlight.h>
#include <linux/spinlock.h>

struct pxapwmbl_platform_data {
	int		pwm;
	int		max_intensity;
	int		default_intensity;
	int		limit_mask;
	int		period;
	int		prescaler;

	int		intensity;
	int		limit;
	int		off_threshold; 
	
	spinlock_t	lock;
	struct backlight_device *dev;
	void (*turn_bl_on)(void);
	void (*turn_bl_off)(void);
		
};

#endif
