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
* Restrutured June 2002
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/mtd/mtd.h>
#include <linux/ctype.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/uaccess.h>   /* for copy to/from user space */
#include <asm/arch/hardware.h>
#include <asm/arch-sa1100/h3600_hal.h>
#include <asm/arch-sa1100/h3600_asic.h>
#include <asm/arch-sa1100/serial_h3800.h>

#include "h3600_asic_io.h"
#include "h3600_asic_core.h"

#define PDEBUG(format,arg...) printk(KERN_DEBUG __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )
#define PALERT(format,arg...) printk(KERN_ALERT __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )
#define PERROR(format,arg...) printk(KERN_ERR __FILE__ ":%s - " format "\n", __FUNCTION__ , ## arg )

/***********************************************************************************
 *      Sleeve ISR
 *
 *   Resources used:     KPIO interface on ASIC2
 *                       GPIO for Power button on SA1110
 ***********************************************************************************/

irqreturn_t ipaq_asic_sleeve_isr(int irq, void *dev_id, struct pt_regs *regs)
{
        int present = (GPLR & GPIO_H3800_NOPT_IND) ? 0 : 1;
	h3600_hal_option_detect( present );
        GEDR = GPIO_H3800_NOPT_IND;    /* Clear the interrupt */
	return IRQ_HANDLED;
}


/***********************************************************************************
 *   Backlight
 *
 *   Resources used:     PWM_0
 *                       PWM Clock enable on CLOCK (CX7)
 *                       GPIO pin on ASIC1 (for frontlight power)
 ***********************************************************************************/

static unsigned long backlight_shared;
static unsigned char backlight_level;
static unsigned char backlight_power;

int ipaq_asic_backlight_control( enum flite_pwr power )
{
	unsigned char level = backlight_level;
	backlight_power = power;
	if (0) PDEBUG("power=%d level=%d", power, level);
	switch (power) {
	case FLITE_PWR_OFF:
		H3800_ASIC1_GPIO_OUT       &= ~GPIO1_FL_PWR_ON;
		H3800_ASIC2_PWM_0_TimeBase &= ~PWM_TIMEBASE_ENABLE;
		H3800_ASIC2_CLOCK_Enable   &= ~ASIC2_CLOCK_PWM;
		ipaq_asic_shared_release( &backlight_shared, ASIC_SHARED_CLOCK_EX1 );
		break;
	case FLITE_PWR_ON:
		ipaq_asic_shared_add( &backlight_shared, ASIC_SHARED_CLOCK_EX1 );
		H3800_ASIC2_CLOCK_Enable    |= ASIC2_CLOCK_PWM;
		if ( level < 21 ) level = 21;
		if ( level > 64 ) level = 64;
		H3800_ASIC2_PWM_0_DutyTime = level;
		H3800_ASIC2_PWM_0_TimeBase |= PWM_TIMEBASE_ENABLE;
		H3800_ASIC1_GPIO_OUT       |= GPIO1_FL_PWR_ON;
		break;
	}
	return 0;
}

int ipaq_asic_backlight_brightness( unsigned char level )
{
	backlight_level = level;
	return ipaq_asic_backlight_power(backlight_power, level);
}

static int ipaq_asic_backlight_probe( struct device *dev )
{
	DEBUG_INIT();
	backlight_level = 0;
	backlight_power = FLITE_PWR_OFF;
	H3800_ASIC2_PWM_0_TimeBase   = PWM_TIMEBASE_VALUE(8);
	H3800_ASIC2_PWM_0_PeriodTime = 0x40;
	return 0;
}

static void ipaq_asic_backlight_shutdown( struct device *dev )
{
	DEBUG_INIT();
	H3800_ASIC1_GPIO_OUT       &= ~GPIO1_FL_PWR_ON;
	H3800_ASIC2_PWM_0_TimeBase &= ~PWM_TIMEBASE_ENABLE;
	H3800_ASIC2_CLOCK_Enable   &= ~ASIC2_CLOCK_PWM;
	ipaq_asic_shared_release( &backlight_shared, ASIC_SHARED_CLOCK_EX1 );
}

struct backlight_driver ipaq_asic_backlight_controller = {
	.backlight_power = ipaq_asic_backlight_power,
	.backlight_brightness = ipaq_asic_backlight_brightness,
};

struct platform_device ipaq_asic_backlight_platform_driver {
	.device = {
		.name = "ipaq_asic_backlight",
		.probe = ipaq_asic_backlight_init,
		.shutdown = ipaq_asic_backlight_shutdown,
/* 
   The backlight should automatically be handled through suspend/resume
   by the framebuffer, so no special handling is necessary
*/
		.suspend = NULL,
		.remove = NULL,
	}
};

EXPORT_MODULE(ipaq_asic_backlight_device_platform_driver);

/***********************************************************************************
 *   LED control
 ***********************************************************************************/

void ipaq_asic_set_led( enum led_color color, int tbs, int pts, int dts )
{
	H3800_ASIC2_LED_TimeBase(color) = 0;
	if ( tbs ) {
		H3800_ASIC2_LED_PeriodTime(color)    = pts;
		H3800_ASIC2_LED_DutyTime(color)      = dts;
		H3800_ASIC2_LED_TimeBase(color)      = tbs;
	}
}

#define BLINK_TIMEBASE_FACTOR 5 
#define CYCLES_PER_SECOND     (1<<(BLINK_TIMEBASE_FACTOR+1))

int ipaq_asic_notify_led( unsigned char mode, unsigned char duration, 
			   unsigned char ontime, unsigned char offtime )
{
	int tbs, pts, dts;
	PDEBUG("mode=%d duration=%d ontime=%d offtime=%d", 
	       mode, duration, ontime, offtime);

	switch (mode) {
	case 0:
		ipaq_asic_led_off(GREEN_LED);
		break;
	case 1:
	case 2:
		if (ontime == 0)
			ipaq_asic_led_on(GREEN_LED);
		else {
			tbs = LEDTBS_BLINK | LEDTBS_AUTOSTOP | BLINK_TIMEBASE_FACTOR;
			if ( duration == 0 )
				tbs |= LEDTBS_ALWAYS;
			pts = (ontime + offtime) * CYCLES_PER_SECOND / 10;
			dts = ontime * CYCLES_PER_SECOND / 10;
			ipaq_asic_set_led(GREEN_LED, tbs, pts, dts );
		        if (0) PDEBUG("tbs=0x%02x pts=%d dts=%d", tbs, pts, dts);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


/***********************************************************************************
 *   Audio handlers
 ***********************************************************************************/

#define SET_ASIC2_CLOCK(x) \
	H3800_ASIC2_CLOCK_Enable = (H3800_ASIC2_CLOCK_Enable & ~ASIC2_CLOCK_AUDIO_MASK) | (x)

int ipaq_asic_audio_clock( long samplerate )
{
	static unsigned long shared;

	if ( !samplerate ) {
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX1 );
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX2 );
		return 0;
	}

	/* Set the external clock generator */
	switch (samplerate) {
	case 24000:
	case 32000:
	case 48000:
		/* 12.288 MHz - needs 24.576 MHz crystal */
		ipaq_asic_shared_add( &shared, ASIC_SHARED_CLOCK_EX1 );
		SET_ASIC2_CLOCK(ASIC2_CLOCK_AUDIO_2);
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX2 );
		break;
	case 22050:
	case 29400:
	case 44100:
		/* 11.2896 MHz - needs 33.869 MHz crystal */
		ipaq_asic_shared_add( &shared, ASIC_SHARED_CLOCK_EX2 );
		SET_ASIC2_CLOCK(ASIC2_CLOCK_AUDIO_4);
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX1 );
		break;
	case 8000:
	case 10666:
	case 16000:
		/* 4.096 MHz  - needs 24.576 MHz crystal */
		ipaq_asic_shared_add( &shared, ASIC_SHARED_CLOCK_EX1 );
		SET_ASIC2_CLOCK(ASIC2_CLOCK_AUDIO_1);
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX2 );
		break;
	case 10985:
	case 14647:
	case 21970:
		/* 5.6245 MHz  - needs 33.869 MHz crystal */
		ipaq_asic_shared_add( &shared, ASIC_SHARED_CLOCK_EX2 );
		SET_ASIC2_CLOCK(ASIC2_CLOCK_AUDIO_3);
		ipaq_asic_shared_release( &shared, ASIC_SHARED_CLOCK_EX1 );
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int ipaq_asic_audio_power( long samplerate )
{
	int retval;

	H3800_ASIC1_GPIO_OUT &= ~GPIO1_AUD_PWR_ON;
	retval = ipaq_asic_audio_clock(samplerate);

	if ( samplerate > 0 )
		H3800_ASIC1_GPIO_OUT |= GPIO1_AUD_PWR_ON;

	return retval;
}

static void ipaq_asic_audio_fix_jack( int mute )
{
	int earphone = H3800_ASIC2_GPIOPIOD & GPIO2_EAR_IN_N ? 0 : 1;

	if ( earphone )
		H3800_ASIC2_GPIINTESEL |= GPIO2_EAR_IN_N; /* Rising */
	else
		H3800_ASIC2_GPIINTESEL &= ~GPIO2_EAR_IN_N; /* Falling */

	if ( mute ) 
		H3800_ASIC1_GPIO_OUT = (H3800_ASIC1_GPIO_OUT | GPIO1_EAR_ON_N) & ~GPIO1_SPK_ON;
	else {
		if ( earphone )
			H3800_ASIC1_GPIO_OUT &= ~(GPIO1_SPK_ON | GPIO1_EAR_ON_N);
		else
			H3800_ASIC1_GPIO_OUT |= GPIO1_SPK_ON | GPIO1_EAR_ON_N;
	}
}

/* Called from HAL or the debounce timer (from an interrupt)*/
int ipaq_asic_audio_mute( int mute )
{
	unsigned long flags;
	local_irq_save(flags);
	ipaq_asic_audio_fix_jack(mute);
	local_irq_restore(flags);

	return 0;
}

static void audio_timer_callback( unsigned long nr )
{
	ipaq_asic_audio_mute( !(H3800_ASIC1_GPIO_OUT & GPIO1_AUD_PWR_ON) );
}

static struct timer_list g_audio_timer = { function: audio_timer_callback };

static void ipaq_asic_ear_in_isr( int irq, void *dev_id, struct pt_regs *regs )
{
	mod_timer( &g_audio_timer, jiffies + (2 * HZ) / 1000 );
}

int ipaq_asic_audio_suspend( void )
{
	DEBUG_INIT();
	del_timer_sync(&g_audio_timer);
	disable_irq( IRQ_H3800_EAR_IN );
	return 0;
}

void ipaq_asic_audio_resume( void )
{
	DEBUG_INIT();
	H3800_ASIC2_GPIINTTYPE |= GPIO2_EAR_IN_N;   /* Set edge-type interrupt */
	ipaq_asic_audio_fix_jack(1);
	enable_irq( IRQ_H3800_EAR_IN );
}

int ipaq_asic_audio_init( void )
{
	int result;

	DEBUG_INIT();
	init_timer(&g_audio_timer);
	H3800_ASIC2_GPIINTTYPE |= GPIO2_EAR_IN_N;   /* Set edge-type interrupt */
	ipaq_asic_audio_fix_jack(1);

	result = request_irq(IRQ_H3800_EAR_IN, ipaq_asic_ear_in_isr, 
			     SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "h3800_ear_in", NULL );

	if ( result )
		PERROR("unable to grab EAR_IN virtual IRQ");
	return result;
}

void ipaq_asic_audio_cleanup( void )
{
	DEBUG_INIT();
	del_timer_sync(&g_audio_timer);
	free_irq( IRQ_H3800_EAR_IN, NULL );
}

struct device_driver ipaq_asic_audio_device_driver = {
	.name = "asic-audioboard",
	.probe    = ipaq_asic_audio_init,
	.shutdown = ipaq_asic_audio_cleanup,
	.suspend  = ipaq_asic_audio_suspend,
	.resume   = ipaq_asic_audio_resume
};
EXPORT_SYMBOL(ipaq_asic_battery_device_driver);
