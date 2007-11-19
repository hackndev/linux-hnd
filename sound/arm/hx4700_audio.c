/*
 * Audio support for iPAQ hx4700
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
#include <asm/arch/hx4700-gpio.h>
#include <asm/arch/hx4700-asic.h>


#include "pxa2xx-i2sound.h"
#include <sound/ak4641.h>

static struct snd_ak4641 ak;

static void snd_hx4700_audio_set_codec_power(int mode)
{
	SET_HX4700_GPIO(CODEC_ON, mode);
}

static void snd_hx4700_audio_set_codec_reset(int mode)
{
	SET_HX4700_GPIO_N(CODEC_PDN, mode);
}

static void snd_hx4700_audio_set_headphone_power(int mode)
{
	SET_HX4700_GPIO(HP_DRIVER, mode);
}

static void snd_hx4700_audio_set_speaker_power(int mode)
{
	SET_HX4700_GPIO(SPK_SD_N, mode);
}

static inline int snd_hx4700_audio_hp_detect(void)
{
	return !GET_HX4700_GPIO(EARPHONE_DET_N);
}

static irqreturn_t snd_hx4700_audio_hp_isr (int isr, void *data)
{
	snd_ak4641_hp_detected(&ak, snd_hx4700_audio_hp_detect());
	return IRQ_HANDLED;
}

static void snd_hx4700_audio_hp_detection_on(void)
{
	unsigned long flags;
	int irq;

	irq = HX4700_IRQ(EARPHONE_DET_N);
	if (request_irq(irq, snd_hx4700_audio_hp_isr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
			"hx4700 Headphone Jack", NULL) != 0)
		return;
	set_irq_type(irq, IRQT_BOTHEDGE);

	local_irq_save(flags);
	snd_ak4641_hp_detected(&ak, snd_hx4700_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_hx4700_audio_hp_detection_off(void)
{
	free_irq(HX4700_IRQ(EARPHONE_DET_N), NULL);
}

static struct snd_ak4641 ak = {
	.power_on_chip		= snd_hx4700_audio_set_codec_power,
	.reset_pin		= snd_hx4700_audio_set_codec_reset,
	.headphone_out_on	= snd_hx4700_audio_set_headphone_power,
	.speaker_out_on		= snd_hx4700_audio_set_speaker_power,
};

static int snd_hx4700_audio_activate(void)
{
	/* AK4641 on PXA2xx I2C bus */
	ak.i2c_client.adapter = i2c_get_adapter(0);

	snd_pxa2xx_i2sound_i2slink_get();
	if (snd_ak4641_activate(&ak) == 0) {
		snd_hx4700_audio_hp_detection_on();
		return 0;
	}
	else {
		snd_pxa2xx_i2sound_i2slink_free();
		return -1;
	}
}

static void snd_hx4700_audio_deactivate(void)
{
	snd_hx4700_audio_hp_detection_off();
	snd_ak4641_deactivate(&ak);
	snd_pxa2xx_i2sound_i2slink_free();
}

static int snd_hx4700_audio_open_stream(int stream)
{
	return snd_ak4641_open_stream(&ak, stream);
}

static void snd_hx4700_audio_close_stream(int stream)
{
	snd_ak4641_close_stream(&ak, stream);
}

static int snd_hx4700_audio_add_mixer_controls(struct snd_card *acard)
{
	return snd_ak4641_add_mixer_controls(&ak, acard);
}

#ifdef CONFIG_PM
static int snd_hx4700_audio_suspend(pm_message_t state)
{
	snd_hx4700_audio_hp_detection_off();
	snd_ak4641_suspend(&ak, state);
	return 0;
}

static int snd_hx4700_audio_resume(void)
{
	snd_ak4641_resume(&ak);
	snd_hx4700_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board hx4700_audio = {
	.name			= "hx4700 Audio",
	.desc			= "iPAQ hx4700 Audio [codec Asahi Kasei AK4641]",
	.acard_id		= "hx4700 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_hx4700_audio_activate,
	.deactivate		= snd_hx4700_audio_deactivate,
	.open_stream		= snd_hx4700_audio_open_stream,
	.close_stream		= snd_hx4700_audio_close_stream,
	.add_mixer_controls	= snd_hx4700_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_hx4700_audio_suspend,
	.resume			= snd_hx4700_audio_resume
#endif
};

static int __init snd_hx4700_audio_init(void)
{
	/* check machine */
	if (!machine_is_h4700()) {
		snd_printk(KERN_INFO "Module snd-hx4700_audio: not an iPAQ hx4700!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&hx4700_audio);
}

static void __exit snd_hx4700_audio_exit(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_hx4700_audio_init);
module_exit(snd_hx4700_audio_exit);

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC; Elshin Roman; Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for iPAQ hx4700");
MODULE_LICENSE("GPL");
