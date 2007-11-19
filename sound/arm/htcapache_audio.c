/*
 * Audio support for HTC Apache
 * It uses PXA2xx i2Sound and AK4641 modules
 *
 * Copyright (c) 2005 Todd Blumer, SDG Systems, LLC
 * Copyright (c) 2006 Giorgio Padrin <giorgio@mandarinlogiq.org>
 * Copyright (c) 2006 Elshin Roman <roxmail@list.ru>
 * 
 * Reference code: h2200_audio.c
 * Copyright (c) 2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 * Copyright (c) 2004,2005 Matthew Reimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:
 * 
 * 2005-06	Initial release	-- Todd Blumer
 * 2006-03	Use new AK4641 code -- Giorgio Padrin
 * 2006-09	Test and debug on machine with new AK4641 code -- Elshin Roman
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/hardware.h>
#include <linux/pm.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/htcapache-gpio.h>

#include "pxa2xx-i2sound.h"
#include <sound/ak4641.h>

static struct snd_ak4641 ak;

static void audio_set_codec_power(int mode)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_SND_POWER, mode);
}

static void audio_set_codec_reset(int mode)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_SND_RESET, !mode);
}

static void audio_set_headphone_power(int mode)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_SND_PWRJACK, mode);
}

static void audio_set_speaker_power(int mode)
{
	htcapache_egpio_set(EGPIO_NR_HTCAPACHE_SND_PWRSPKR, mode);
}

static inline int audio_hp_detect(void)
{
	return !htcapache_egpio_get(EGPIO_NR_HTCAPACHE_SND_IN_JACK);
}

static irqreturn_t audio_hp_isr(int isr, void *data)
{
	snd_ak4641_hp_detected(&ak, audio_hp_detect());
	return IRQ_HANDLED;
}

static void audio_hp_detection_on(void)
{
	unsigned long flags;
	int irq;

	irq = htcapache_egpio_to_irq(EGPIO_NR_HTCAPACHE_SND_IN_JACK);
	if (request_irq(irq, audio_hp_isr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
			"Headphone Jack", NULL) != 0)
		return;
	set_irq_type(irq, IRQT_BOTHEDGE);

	local_irq_save(flags);
	snd_ak4641_hp_detected(&ak, audio_hp_detect());
	local_irq_restore(flags);
}

static void audio_hp_detection_off(void)
{
	free_irq(htcapache_egpio_to_irq(EGPIO_NR_HTCAPACHE_SND_IN_JACK), NULL);
}

static struct snd_ak4641 ak = {
	.power_on_chip		= audio_set_codec_power,
	.reset_pin		= audio_set_codec_reset,
	.headphone_out_on	= audio_set_headphone_power,
	.speaker_out_on		= audio_set_speaker_power,
};

static int audio_activate(void)
{
	/* AK4641 on PXA2xx I2C bus */
	ak.i2c_client.adapter = i2c_get_adapter(0);

	snd_pxa2xx_i2sound_i2slink_get();
	if (snd_ak4641_activate(&ak) == 0) {
		audio_hp_detection_on();
		return 0;
	}
	else {
		snd_pxa2xx_i2sound_i2slink_free();
		return -1;
	}
}

static void audio_deactivate(void)
{
	audio_hp_detection_off();
	snd_ak4641_deactivate(&ak);
	snd_pxa2xx_i2sound_i2slink_free();
}

static int audio_open_stream(int stream)
{
	return snd_ak4641_open_stream(&ak, stream);
}

static void audio_close_stream(int stream)
{
	snd_ak4641_close_stream(&ak, stream);
}

static int audio_add_mixer_controls(struct snd_card *acard)
{
	return snd_ak4641_add_mixer_controls(&ak, acard);
}

#ifdef CONFIG_PM
static int audio_suspend(pm_message_t state)
{
	audio_hp_detection_off();
	snd_ak4641_suspend(&ak, state);
	return 0;
}

static int audio_resume(void)
{
	snd_ak4641_resume(&ak);
	audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board apache_audio = {
	.name			= "HTCApache Audio",
	.desc			= "HTCApache Audio [codec Asahi Kasei AK4641]",
	.acard_id		= "HTCApache Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= audio_activate,
	.deactivate		= audio_deactivate,
	.open_stream		= audio_open_stream,
	.close_stream		= audio_close_stream,
	.add_mixer_controls	= audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= audio_suspend,
	.resume			= audio_resume
#endif
};

static int __init audio_init(void)
{
	/* check machine */
	if (!machine_is_htcapache()) {
		snd_printk(KERN_INFO "Module snd-htcapache_audio: not an apache!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&apache_audio);
}

static void __exit audio_exit(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC; Elshin Roman; Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for iPAQ hx4700");
MODULE_LICENSE("GPL");
