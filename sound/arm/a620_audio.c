/*
 * Audio support for Asus MyPal 620
 * It uses PXA2xx i2Sound and UDA1380 modules
 *
 * Copyright (c) 2007 Vincent Benony
 * Copyright (c) 2005 Pawel Kolodziejski
 *
 * Reference code: h2200_audio.c
 * Copyright (c) 2005 Giorgio Padrin giorgio@mandarinlogiq.org
 * Copyright (c) 2004,2005 Matthew Reimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <linux/pm.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/asus716-gpio.h>
#include <asm/mach-types.h>

#include "pxa2xx-i2sound.h"
#include <sound/uda1380.h>
#include <asm/arch/asus620-gpio.h>

static struct snd_uda1380 uda;

void pca9535_write_output(u16 param);

static void snd_a620_audio_set_codec_power(int mode) {}

static void snd_a620_audio_set_codec_reset(int mode) {}

static void snd_a620_audio_set_speaker_power(int mode)
{
	if (mode == 1)
		asus620_gpo_set(GPO_A620_SOUND);
	else
		asus620_gpo_clear(GPO_A620_SOUND);
}

static void snd_a620_audio_set_mic_power(int mode) {
	if (mode == 1)
		asus620_gpo_set(GPO_A620_MICROPHONE);
	else
		asus620_gpo_clear(GPO_A620_MICROPHONE);
}

void a620_audio_earphone_isr(u16 data)
{
	int earphone_in;

	earphone_in = (data & GPIO_I2C_EARPHONES_DETECTED) ? 0 : 1;
	if (earphone_in)
		snd_a620_audio_set_speaker_power(0);
	else
		snd_a620_audio_set_speaker_power(1);

	snd_uda1380_hp_detected(&uda, earphone_in);
}

static void snd_a620_audio_hp_detection_on(void) {}

static void snd_a620_audio_hp_detection_off(void) {}

static struct snd_uda1380 uda = {
	.line_in_connected	= 0,
	.mic_connected		= 1,
	.hp_or_line_out		= 1,
	.capture_source		= SND_UDA1380_CAP_SOURCE_MIC,
	.power_on_chip		= snd_a620_audio_set_codec_power,
	.reset_pin		= snd_a620_audio_set_codec_reset,
	.line_out_on		= snd_a620_audio_set_speaker_power,
	.mic_on			= snd_a620_audio_set_mic_power
};

static int snd_a620_audio_activate(void) {
	/* UDA1380 at address 0x1a on PXA2xx I2C bus */
	uda.i2c_client.adapter = i2c_get_adapter(0);
	uda.i2c_client.addr = 0x1a;

	if (snd_uda1380_activate(&uda) == 0) {
		snd_a620_audio_hp_detection_on();
		return 0;
	} else
		return -1;
}

static void snd_a620_audio_deactivate(void) {
	snd_a620_audio_hp_detection_off();
	snd_uda1380_deactivate(&uda);
}

static int snd_a620_audio_open_stream(int stream)
	{ return snd_uda1380_open_stream(&uda, stream); }

static void snd_a620_audio_close_stream(int stream)
	{ snd_uda1380_close_stream(&uda, stream); }

static int snd_a620_audio_add_mixer_controls(struct snd_card *acard) {
	return snd_uda1380_add_mixer_controls(&uda, acard);
}

#ifdef CONFIG_PM
static int snd_a620_audio_suspend(pm_message_t state) {
	snd_a620_audio_hp_detection_off();
	snd_uda1380_suspend(&uda, state);
	return 0;
}

static int snd_a620_audio_resume(void) {
	snd_uda1380_resume(&uda);
	snd_a620_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board a620_audio = {
	.name			= "Asus MyPal A620 Audio",
	.desc			= "Asus MyPal A620 Audio [codec Philips UDA1380]",
	.acard_id		= "a620 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_a620_audio_activate,
	.deactivate		= snd_a620_audio_deactivate,
	.open_stream		= snd_a620_audio_open_stream,
	.close_stream		= snd_a620_audio_close_stream,
	.add_mixer_controls	= snd_a620_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_a620_audio_suspend,
	.resume			= snd_a620_audio_resume
#endif
};

static int __init snd_a620_audio_init(void) {
	/* check machine */
	if (!machine_is_a620())
	{
		snd_printk(KERN_INFO "Module snd-a620_audio: not an Asus MyPal 620!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&a620_audio);
}

static void __exit snd_a620_audio_exit(void) {
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_a620_audio_init);
module_exit(snd_a620_audio_exit);

MODULE_AUTHOR("Pawel Kolodziejski, Giorgio Padrin, Matthew Reimer, Vincent Benony");
MODULE_DESCRIPTION("Audio support for Asus MyPal a620");
MODULE_LICENSE("GPL");

