/* linux/include/asm/arch-s3c2410/lcd.h
 *
 * Copyright (c) 2005 Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *  Changelog:
 *     14-Mar-2005     RTP     Created file
 *     07-Apr-2005     RTP     Renamed to s3c2410_lcd.h
 *     03-Aug-2005     RTP     Renamed to lcd.h
 */

#ifndef __ASM_ARM_LCD_H
#define __ASM_ARM_LCD_H

struct s3c2410_bl_mach_info {
	int		lcd_power_value;
	int		backlight_power_value;
	int		brightness_value;
	int             backlight_max;
	int             backlight_default;
	void            (*backlight_power)(int);
	void            (*set_brightness)(int);
	void		(*lcd_power)(int);
};

void __init set_s3c2410bl_info(struct s3c2410_bl_mach_info *hard_s3c2410bl_info);

#endif /* __ASM_ARM_LCD_H */
