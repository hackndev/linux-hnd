/*
 * Audio support for Asus MyPal 716
 * It uses PXA2xx i2Sound and UDA1380 modules
 *
 * Copyright (c) 2005-2007 Pawel Kolodziejski
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

static struct snd_uda1380 uda;

void pca9535_write_output(u16 param);
u32 pca9535_read_input(void);

static void snd_a716_audio_set_codec_power(int mode) {}

static void snd_a716_audio_set_codec_reset(int mode) {}

static void snd_a716_audio_set_speaker_power(int mode) {}

static void snd_a716_audio_set_mic_power(int mode) {
	if (mode == 1)
		a716_gpo_clear(GPO_A716_MICROPHONE_N);
	else
		a716_gpo_set(GPO_A716_MICROPHONE_N);
}

void a716_audio_earphone_isr(u16 data)
{
	int earphone_in;

	earphone_in = (data & GPIO_I2C_EARPHONES_DETECTED) ? 0 : 1;
	if (earphone_in)
		pca9535_write_output(GPIO_I2C_SPEAKER_DISABLE);
	else
		pca9535_write_output(0);

	snd_uda1380_hp_detected(&uda, earphone_in);
}

static void snd_a716_audio_hp_detection_on(void) {}

static void snd_a716_audio_hp_detection_off(void) {}

static struct snd_uda1380 uda = {
	.line_in_connected	= 0,
	.mic_connected		= 1,
	.hp_or_line_out		= 1,
	.capture_source		= SND_UDA1380_CAP_SOURCE_MIC,
	.power_on_chip		= snd_a716_audio_set_codec_power,
	.reset_pin		= snd_a716_audio_set_codec_reset,
	.line_out_on		= snd_a716_audio_set_speaker_power,
	.mic_on			= snd_a716_audio_set_mic_power
};

static int snd_a716_audio_activate(void) {
	u32 i2c_data;

	/* UDA1380 at address 0x1a on PXA2xx I2C bus */
	uda.i2c_client.adapter = i2c_get_adapter(0);
	uda.i2c_client.addr = 0x1a;

	i2c_data = pca9535_read_input();
	if (i2c_data != (u32)-1) {
		a716_audio_earphone_isr(i2c_data);
	} else {
		pca9535_write_output(0);
	}

	if (snd_uda1380_activate(&uda) == 0) {
		snd_a716_audio_hp_detection_on();
		return 0;
	} else
		return -1;
}

static void snd_a716_audio_deactivate(void) {
	snd_a716_audio_hp_detection_off();
	snd_uda1380_deactivate(&uda);
}

static int snd_a716_audio_open_stream(int stream)
	{ return snd_uda1380_open_stream(&uda, stream); }

static void snd_a716_audio_close_stream(int stream)
	{ snd_uda1380_close_stream(&uda, stream); }

static int snd_a716_audio_add_mixer_controls(struct snd_card *acard) {
	return snd_uda1380_add_mixer_controls(&uda, acard);
}

#ifdef CONFIG_PM
static int snd_a716_audio_suspend(pm_message_t state) {
	snd_a716_audio_hp_detection_off();
	snd_uda1380_suspend(&uda, state);
	return 0;
}

static int snd_a716_audio_resume(void) {
	snd_uda1380_resume(&uda);
	snd_a716_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board a716_audio = {
	.name			= "Asus MyPal A716 Audio",
	.desc			= "Asus MyPal A716 Audio [codec Philips UDA1380]",
	.acard_id		= "a716 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_a716_audio_activate,
	.deactivate		= snd_a716_audio_deactivate,
	.open_stream		= snd_a716_audio_open_stream,
	.close_stream		= snd_a716_audio_close_stream,
	.add_mixer_controls	= snd_a716_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_a716_audio_suspend,
	.resume			= snd_a716_audio_resume
#endif
};

static int __init snd_a716_audio_init(void) {
	u32 i2c_data;

	/* check machine */
	if (!machine_is_a716()) {
		return -1;
	}

	i2c_data = pca9535_read_input();
	if (i2c_data != (u32)-1) {
		a716_audio_earphone_isr(i2c_data);
	} else {
		pca9535_write_output(0);
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&a716_audio);
}

static void __exit snd_a716_audio_exit(void) {
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_a716_audio_init);
module_exit(snd_a716_audio_exit);

MODULE_AUTHOR("Pawel Kolodziejski, Giorgio Padrin, Matthew Reimer");
MODULE_DESCRIPTION("Audio support for Asus MyPal a716");
MODULE_LICENSE("GPL");

