/*
 * Audio support for iPAQ h4000
 * It uses PXA2xx i2Sound and UDA1380 modules
 *
 * Copyright (c) 2006 Andy Potter
 *
 * Reference code: h1910_audio.c
 * Copyright (c) 2005 Pawel Kolodziejski
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
#include <linux/dpm.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h4000.h>
#include <asm/arch/h4000-gpio.h>
#include <asm/arch/h4000-asic.h>
#include <linux/mfd/asic3_base.h>
#include <linux/platform_device.h>

#include "pxa2xx-i2sound.h"
#include <sound/uda1380.h>

static struct snd_uda1380 uda;

extern struct platform_device h4000_asic3;
#define asic3 &h4000_asic3.dev

static void snd_h4000_audio_set_codec_power(int mode) {
	if (mode == 1) {
		DPM_DEBUG("h4000_audio: Turning codec on\n");
		asic3_set_gpio_out_c(asic3, GPIOC_AUD_POWER_ON, GPIOC_AUD_POWER_ON);
	} else {
		DPM_DEBUG("h4000_audio: Turning codec off\n");
		asic3_set_gpio_out_c(asic3, GPIOC_AUD_POWER_ON, 0);
	}
}
static void snd_h4000_audio_set_codec_reset(int mode) {
	if (mode == 1) {
		DPM_DEBUG("h4000_audio: Codec reset on\n");
		GPSR(GPIO_NR_H4000_CODEC_RST) = GPIO_bit(GPIO_NR_H4000_CODEC_RST);
	} else {
		DPM_DEBUG("h4000_audio: Codec reset off\n");
		GPCR(GPIO_NR_H4000_CODEC_RST) = GPIO_bit(GPIO_NR_H4000_CODEC_RST);
	}
}

static void snd_h4000_audio_set_mic_power(int mode) {}

static void snd_h4000_audio_set_speaker_power(int mode) {
	if (mode == 1) {
		DPM_DEBUG("h4000_audio: Turning speaker on\n");
		asic3_set_gpio_out_d(asic3, GPIOD_SPK_EN, GPIOD_SPK_EN);
	} else {
		DPM_DEBUG("h4000_audio: Turning speaker off\n");
		asic3_set_gpio_out_d(asic3, GPIOD_SPK_EN, 0);
	}
}

static inline int snd_h4000_audio_hp_detect(void) {
	int irq, hp_in;
	irq = asic3_irq_base(asic3) + H4000_HEADPHONE_IN_IRQ;
	hp_in = ((asic3_get_gpio_status_d(asic3) & GPIOD_HEADPHONE_IN_N) == 0);
	if (hp_in)
		set_irq_type(irq, IRQT_RISING);
	else
		set_irq_type(irq, IRQT_FALLING);
	return hp_in;
}

static irqreturn_t snd_h4000_audio_hp_isr(int isr, void *data) {
	snd_uda1380_hp_detected(&uda, snd_h4000_audio_hp_detect());
	return IRQ_HANDLED;
}

static void snd_h4000_audio_hp_detection_on(void) {
	unsigned long flags;
	int irq;
	irq = asic3_irq_base(asic3) + H4000_HEADPHONE_IN_IRQ;
	request_irq(irq, snd_h4000_audio_hp_isr,
			SA_SAMPLE_RANDOM, "Earphone jack", NULL);
	local_irq_save(flags);
	snd_uda1380_hp_detected(&uda, snd_h4000_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_h4000_audio_hp_detection_off(void) {
	free_irq(asic3_irq_base(asic3) + H4000_HEADPHONE_IN_IRQ, NULL);
}

static struct snd_uda1380 uda = {
	.line_in_connected	= 0,
	.mic_connected		= 1,
	.hp_or_line_out		= 1,
	.capture_source		= SND_UDA1380_CAP_SOURCE_MIC,
	.power_on_chip		= snd_h4000_audio_set_codec_power,
	.reset_pin		= snd_h4000_audio_set_codec_reset,
	.line_out_on		= snd_h4000_audio_set_speaker_power,
	.mic_on			= snd_h4000_audio_set_mic_power
};

static int snd_h4000_audio_activate(void) {
	/* UDA1380 at address 0x1a on PXA2xx I2C bus */
	uda.i2c_client.adapter = i2c_get_adapter(0);
	uda.i2c_client.addr = 0x1a;

	if (snd_uda1380_activate(&uda) == 0) {
		snd_h4000_audio_hp_detection_on();
		return 0;
	} else
		return -1;
}

static void snd_h4000_audio_deactivate(void) {
	snd_h4000_audio_hp_detection_off();
	snd_uda1380_deactivate(&uda);
}

static int snd_h4000_audio_open_stream(int stream)
	{ return snd_uda1380_open_stream(&uda, stream); }

static void snd_h4000_audio_close_stream(int stream)
	{ snd_uda1380_close_stream(&uda, stream); }

static int snd_h4000_audio_add_mixer_controls(struct snd_card *acard) {
	return snd_uda1380_add_mixer_controls(&uda, acard);
}

#ifdef CONFIG_PM
static int snd_h4000_audio_suspend(pm_message_t state) {
	snd_h4000_audio_hp_detection_off();
	snd_uda1380_suspend(&uda, state);
	return 0;
}

static int snd_h4000_audio_resume(void) {
	snd_uda1380_resume(&uda);
	snd_h4000_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board h4000_audio = {
	.name			= "h4000 Audio",
	.desc			= "iPAQ h4000 Audio [codec Philips UDA1380]",
	.acard_id		= "h4000 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_h4000_audio_activate,
	.deactivate		= snd_h4000_audio_deactivate,
	.open_stream		= snd_h4000_audio_open_stream,
	.close_stream		= snd_h4000_audio_close_stream,
	.add_mixer_controls	= snd_h4000_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_h4000_audio_suspend,
	.resume			= snd_h4000_audio_resume
#endif
};

static int __init snd_h4000_audio_init(void) {
	/* check machine */
	if (!h4000_machine_is_h4000()) {
		snd_printk(KERN_INFO "Module snd-h4000_audio: not an iPAQ h4000!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&h4000_audio);
}

static void __exit snd_h4000_audio_exit(void) {
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_h4000_audio_init);
module_exit(snd_h4000_audio_exit);

MODULE_AUTHOR("Andy Potter, Pawel Kolodziejski, Giorgio Padrin, Matthew Reimer");
MODULE_DESCRIPTION("Audio support for iPAQ h4000");
MODULE_LICENSE("GPL");
