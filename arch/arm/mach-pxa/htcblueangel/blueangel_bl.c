/*
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Copyright (C) 2006 Paul Sokolosvky
 * Based on code from older versions of htcuniversal_lcd.c
 *
 * 2006-10-30: Adapted from the htcuniversal backlight code
 *             for the blueangel 
 *             Michael Horne <asylumed@gmail.com>
 *             
 */

#include <linux/types.h>
#include <linux/platform_device.h>
#include <asm/arch/hardware.h>  /* for pxa-regs.h (__REG) */
#include <asm/arch/pxa-regs.h>
#include <linux/corgi_bl.h>
#include <linux/err.h>

#include <asm/arch/htcblueangel-gpio.h>
#include <asm/arch/htcblueangel-asic.h>
#include <asm/hardware/ipaq-asic3.h>
#include <linux/mfd/asic3_base.h>

#define HTCBLUEANGEL_MAX_INTENSITY 0x40                                                                                      
                                                                                                                             
static void htcblueangel_set_bl_intensity(int intensity)                                                                     
{                                                                                                                            
    if (intensity > 0) {                                                                                                     
        // backlight on range will be from 1 to 64                                                                           
	asic3_write_register(&blueangel_asic3.dev, _IPAQ_ASIC3_PWM_1_Base+_IPAQ_ASIC3_PWM_DutyTime, intensity - 1);          
	asic3_set_gpio_out_b (&blueangel_asic3.dev, 1<<GPIOB_FL_PWR_ON, 1<<GPIOB_FL_PWR_ON);                                  
    } else {                                                                                                                 
        asic3_set_gpio_out_b (&blueangel_asic3.dev, 1<<GPIOB_FL_PWR_ON, 0);                                                
    }                                                                                                                        
}                                                                                                                            

static struct corgibl_machinfo htcblueangel_bl_machinfo = {                                                                  
    .default_intensity = HTCBLUEANGEL_MAX_INTENSITY / 4,                                                                     
    .limit_mask = 0xff,                                                                                                      
    .max_intensity = HTCBLUEANGEL_MAX_INTENSITY,                                                                             
    .set_bl_intensity = htcblueangel_set_bl_intensity,                                                                       
};                                                                                                                           
				                                                                                                                                
struct platform_device blueangel_bl = {                                                                                      
    .name = "corgi-bl",                                                                                                      
    .dev = {                                                                                                                 
	.platform_data = &htcblueangel_bl_machinfo,                                                                          
    },                                                                                                                       
}; 

MODULE_AUTHOR("Michael Horne <asylumed@gmail.com>");
MODULE_DESCRIPTION("Backlight driver for HTC Blueangel");
MODULE_LICENSE("GPL");
