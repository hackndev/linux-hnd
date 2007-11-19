/*
 * PXA i2Sound code for TDS Recon
 *
 * Copyright (c) 2005 Todd Blumer, SDG Systems, LLC
 *
 * Based on code:
 *   Copyright (c) 2005 Giorgio Padrin giorgio@mandarinlogiq.org
 *   Copyright (c) 2004,2005 Matthew Reimer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * 
 * 2005-09	Todd Blumer		Initial release
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <linux/pm.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/recon.h>

#include "pxa2xx-i2sound.h"

#ifdef CONFIG_RECON_X
#   define is_recon_x 1
#else
#   define is_recon_x 0
#endif

typedef enum { RECON_PLAYBACK_ON, RECON_RECORD_ON, RECON_PLAYBACK_OFF, RECON_RECORD_OFF  } stream_mode_t;

static int output_enabled = 0;
static int input_enabled  = 0;

static int
recon_create_mixer( snd_card_t *card )
{
	return 0;
}

static void
recon_audio_set_codec_power( stream_mode_t mode )
{
	static int ref_count     = 0;

	switch(mode) {
		case RECON_RECORD_ON:
			/* Microphone control */
			SET_RECON_GPIO(RECON_GPIO_MIC_ON, 1);

			SACR1 &= ~SACR1_DREC;	/* record enabled */

			ref_count++;
			break;
		case RECON_RECORD_OFF:
			SACR1 |= SACR1_DREC;	/* rec disabled */
			/* Microphone control */
			SET_RECON_GPIO(RECON_GPIO_MIC_ON, 0);

			SACR1 |= SACR1_DREC;	/* record disabled */

			ref_count--;
			break;
		case RECON_PLAYBACK_ON:
			SACR1 &= ~SACR1_DRPL;	/* playback enabled */
			ref_count++;
			break;
		case RECON_PLAYBACK_OFF:
			SACR1 |= SACR1_DRPL;	/* playback disabled */
			ref_count--;
			break;
		default:
			break;
	}

	
	if(ref_count == 0) {
		/* flush data in i2s registers */
		while (((SASR0 >> 8) & 0xf) >= 4)
			;
		/* Just to make sure the MIC is off */
		SET_RECON_GPIO(RECON_GPIO_MIC_ON, 0);
	}
}

/* ==== BEGIN PLUGIN IFACE ==== */

static int
snd_pxa_i2sound_recon_activate(void)
{
	return 0;
}

static void
snd_pxa_i2sound_recon_deactivate(void)
{
}

static int
snd_pxa_i2sound_recon_stream_on(int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		output_enabled = 1;
		recon_audio_set_codec_power(RECON_PLAYBACK_ON);
	}
	if(is_recon_x) {
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			input_enabled = 1;
			recon_audio_set_codec_power(RECON_RECORD_ON);
		}
	}
	return 0;
}		

static void
snd_pxa_i2sound_recon_stream_off(int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		output_enabled = 0;
		recon_audio_set_codec_power(RECON_PLAYBACK_OFF);
	}
	if(is_recon_x) {
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			input_enabled = 0;
			recon_audio_set_codec_power(RECON_RECORD_OFF);
		}
	}
}		


#ifdef CONFIG_PM

static int
snd_pxa_i2sound_recon_suspend(pm_message_t state)
{
	if(output_enabled) recon_audio_set_codec_power(RECON_PLAYBACK_OFF);
	if(input_enabled)  recon_audio_set_codec_power(RECON_RECORD_OFF);
	return 0;
}

static int
snd_pxa_i2sound_recon_resume(void)
{
	SET_RECON_GPIO(RECON_GPIO_MIC_GAIN, 1);
	/* set to pre-suspend state */
	if(output_enabled) recon_audio_set_codec_power(RECON_PLAYBACK_ON);
	if(input_enabled)  recon_audio_set_codec_power(RECON_RECORD_ON);
	return 0;
}

#else
#define snd_pxa_i2sound_recon_resume	NULL
#define snd_pxa_i2sound_recon_suspend	NULL
#endif

static struct snd_pxa2xx_i2sound_board recon_audio = {
	.acard_id	= "recon-audio",
	.name		= "recon-audio",
	.desc		= "Recon/Recon X Audio",
	.info		= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA
			  | SND_PXA2xx_I2SOUND_INFO_HALF_DUPLEX
			  /* no capture */
			  | SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE * is_recon_x
			  ,
	.activate	= snd_pxa_i2sound_recon_activate,
	.deactivate	= snd_pxa_i2sound_recon_deactivate,
	.open_stream	= snd_pxa_i2sound_recon_stream_on,
	.close_stream	= snd_pxa_i2sound_recon_stream_off,
	.add_mixer_controls	= recon_create_mixer,
	.suspend	= snd_pxa_i2sound_recon_suspend,
	.resume		= snd_pxa_i2sound_recon_resume
};

/* ===== END PLUGIN IFACE ===== */

static int __init
snd_pxa_i2sound_recon_init(void)
{
	printk( KERN_NOTICE "TDS Recon i2Sound Module\n" );
	SET_RECON_GPIO(RECON_GPIO_MIC_GAIN, 1);
	pxa_gpio_mode(RECON_GPIO_MIC_GAIN | GPIO_OUT);
	pxa_gpio_mode(GPIO29_SDATA_IN_I2S_MD);
	pxa_gpio_mode(GPIO30_SDATA_OUT_I2S_MD);
	pxa_gpio_mode(GPIO31_SYNC_I2S_MD);
	pxa_gpio_mode(GPIO32_SYSCLK_I2S_MD);
	SET_RECON_GPIO(RECON_GPIO_MIC_ON, 0);
	pxa_gpio_mode(RECON_GPIO_MIC_ON | GPIO_OUT | GPIO_DFLT_HIGH);
				
	return snd_pxa2xx_i2sound_card_activate( &recon_audio );
}

static void __exit
snd_pxa_i2sound_recon_free(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_pxa_i2sound_recon_init);
module_exit(snd_pxa_i2sound_recon_free);

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC");
MODULE_DESCRIPTION("PXA i2Sound module for Recon");
MODULE_LICENSE("GPL");

/* vim600: set noexpandtab sw=8 ts=8 :*/

