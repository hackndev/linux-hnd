/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restructured June 2002
*/

#ifndef H3600_ASIC_IO_H
#define H3600_ASIC_IO_H

#include <asm/arch-sa1100/h3600_asic.h>
#include <linux/h3600_ts.h>

#include "../common/ipaq/asic2_io.h"

struct pt_regs;

irqreturn_t ipaq_asic_sleeve_isr(int irq, void *dev_id, struct pt_regs *regs);

int ipaq_asic_audio_clock( long samplerate );
int ipaq_asic_audio_power( long samplerate );
int ipaq_asic_audio_mute( int mute );
int ipaq_asic_backlight_control( enum flite_pwr power, unsigned char level );
int ipaq_mtd_asset_read( struct h3600_asset *asset );

/* LED control */
void ipaq_asic_set_led( enum led_color color, int tbs, int pts, int dts );
int ipaq_asic_notify_led( unsigned char mode, unsigned char duration, 
			   unsigned char ontime, unsigned char offtime );

#define ipaq_asic_led_on(color)  \
        ipaq_asic_set_led(color, LED_EN | LED_AUTOSTOP | LED_ALWAYS, 4, 4 )

#define ipaq_asic_led_off(color)  \
        ipaq_asic_set_led(color, 0, 0, 0)

#define ipaq_asic_led_blink(color,rate,pts,dts)  \
        ipaq_asic_set_led(color, LED_EN | LED_AUTOSTOP | LED_ALWAYS | rate, pts, dts )


int  ipaq_asic_backlight_init(void);   
void ipaq_asic_backlight_cleanup(void);

int  ipaq_asic_audio_init(void); 
void ipaq_asic_audio_cleanup(void);
int  ipaq_asic_audio_suspend(void); 
void ipaq_asic_audio_resume  (void);

int  ipaq_mtd_asset_init(void);   
void ipaq_mtd_asset_cleanup(void);

int  ipaq_asic_bluetooth_init(void);
void ipaq_asic_bluetooth_cleanup(void);
int  ipaq_asic_bluetooth_suspend(void);
void ipaq_asic_bluetooth_resume(void);



#endif /* IPAQ_ASIC_IO_H */
