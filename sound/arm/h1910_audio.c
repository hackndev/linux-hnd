/*
 * Audio support for iPAQ h1910
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
#include <linux/irq.h>
#include <linux/pm.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h1900-gpio.h>
#include <asm/hardware/ipaq-hamcop.h>
#include <asm/arch/h1900-asic.h>
#include <linux/mfd/asic3_base.h>
#include <linux/platform_device.h>

#include "pxa2xx-i2sound.h"
#include <sound/uda1380.h>

static struct snd_uda1380 uda;

extern struct platform_device h1900_asic3;
#define asic3 &h1900_asic3.dev

static void snd_h1910_audio_set_codec_power(int mode) {
	if (mode == 1)
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_CODEC_AUDIO_POWER, GPIO3_H1900_CODEC_AUDIO_POWER);
	else
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_CODEC_AUDIO_POWER, 0);
}

static void snd_h1910_audio_set_codec_reset(int mode) {
	if (mode == 1)
		GPSR(GPIO_NR_H1900_CODEC_AUDIO_RESET) = GPIO_bit(GPIO_NR_H1900_CODEC_AUDIO_RESET);
	else
		GPCR(GPIO_NR_H1900_CODEC_AUDIO_RESET) = GPIO_bit(GPIO_NR_H1900_CODEC_AUDIO_RESET);
}

static void snd_h1910_audio_set_mic_power(int mode) {}

static void snd_h1910_audio_set_speaker_power(int mode) {
	if (mode == 1)
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_SPEAKER_POWER, GPIO3_H1900_SPEAKER_POWER);
	else
		asic3_set_gpio_out_c(asic3, GPIO3_H1900_SPEAKER_POWER, 0);
}

static inline int snd_h1910_audio_hp_detect(void) {
	return ((GPLR(GPIO_NR_H1900_HEADPHONE_IN_N) & GPIO_bit(GPIO_NR_H1900_HEADPHONE_IN_N)) == 0) ? 1 : 0;
}

static irqreturn_t snd_h1910_audio_hp_isr(int isr, void *data) {
	snd_uda1380_hp_detected(&uda, snd_h1910_audio_hp_detect());
	return IRQ_HANDLED;
}

static void snd_h1910_audio_hp_detection_on(void) {
	unsigned long flags;

	set_irq_type(IRQ_GPIO(GPIO_NR_H1900_HEADPHONE_IN_N), IRQT_BOTHEDGE);
	request_irq(IRQ_GPIO(GPIO_NR_H1900_HEADPHONE_IN_N), snd_h1910_audio_hp_isr,
		    SA_INTERRUPT | SA_SAMPLE_RANDOM, "earphone jack", NULL);

	local_irq_save(flags);
	snd_uda1380_hp_detected(&uda, snd_h1910_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_h1910_audio_hp_detection_off(void) {
	free_irq(IRQ_GPIO(GPIO_NR_H1900_HEADPHONE_IN_N), NULL);
}

static struct snd_uda1380 uda = {
	.line_in_connected	= 0,
	.mic_connected		= 1,
	.hp_or_line_out		= 1,
	.capture_source		= SND_UDA1380_CAP_SOURCE_MIC,
	.power_on_chip		= snd_h1910_audio_set_codec_power,
	.reset_pin		= snd_h1910_audio_set_codec_reset,
	.line_out_on		= snd_h1910_audio_set_speaker_power,
	.mic_on			= snd_h1910_audio_set_mic_power
};

static int snd_h1910_audio_activate(void) {
	/* UDA1380 at address 0x1a on PXA2xx I2C bus */
	uda.i2c_client.adapter = i2c_get_adapter(0);
	uda.i2c_client.addr = 0x1a;

	if (snd_uda1380_activate(&uda) == 0) {
		snd_h1910_audio_hp_detection_on();
		return 0;
	} else
		return -1;
}

static void snd_h1910_audio_deactivate(void) {
	snd_h1910_audio_hp_detection_off();
	snd_uda1380_deactivate(&uda);
}

static int snd_h1910_audio_open_stream(int stream)
	{ return snd_uda1380_open_stream(&uda, stream); }

static void snd_h1910_audio_close_stream(int stream)
	{ snd_uda1380_close_stream(&uda, stream); }

static int snd_h1910_audio_add_mixer_controls(struct snd_card *acard) {
	return snd_uda1380_add_mixer_controls(&uda, acard);
}

#ifdef CONFIG_PM
static int snd_h1910_audio_suspend(pm_message_t state) {
	snd_h1910_audio_hp_detection_off();
	snd_uda1380_suspend(&uda, state);
	return 0;
}

static int snd_h1910_audio_resume(void) {
	snd_uda1380_resume(&uda);
	snd_h1910_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board h1910_audio = {
	.name			= "h1910 Audio",
	.desc			= "iPAQ h1910/h1915 Audio [codec Philips UDA1380]",
	.acard_id		= "h1910 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_h1910_audio_activate,
	.deactivate		= snd_h1910_audio_deactivate,
	.open_stream		= snd_h1910_audio_open_stream,
	.close_stream		= snd_h1910_audio_close_stream,
	.add_mixer_controls	= snd_h1910_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_h1910_audio_suspend,
	.resume			= snd_h1910_audio_resume
#endif
};

static int __init snd_h1910_audio_init(void) {
	/* check machine */
	if (!machine_is_h1900()) {
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&h1910_audio);
}

static void __exit snd_h1910_audio_exit(void) {
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_h1910_audio_init);
module_exit(snd_h1910_audio_exit);

MODULE_AUTHOR("Pawel Kolodziejski, Giorgio Padrin, Matthew Reimer");
MODULE_DESCRIPTION("Audio support for iPAQ h1910/h1915");
MODULE_LICENSE("GPL");

