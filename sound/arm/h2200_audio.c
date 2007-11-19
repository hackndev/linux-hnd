/*
 * Audio support for iPAQ h2200
 * It uses PXA2xx i2Sound and UDA1380 modules
 *
 * Copyright (c) 2005 Giorgio Padrin giorgio@mandarinlogiq.org
 * Copyright (c) 2004,2005 Matthew Reimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:
 *
 * 2005-03	Initial release.
 *		GPIOs code by Matthew Reimer from arch/arm/mach-pxa/h2200/h2200_audio.c
 *		-- Giorgio Padrin
 * 2005-05-02	Added stream events handling.
 *	  	Included hp detection and capture code by Matthew Reimer
 *		from arch/arm/mach-pxa/h2200/h2200_audio.c
 *		-- Giorgio Padrin
 * 2005-06	Power Management -- Giorgio Padrin
 * 2005-10	Some finishing in PXA2xx i2Sound and new UDA1380 code -- Giorgio Padrin
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
#include <asm/arch/pxa-regs.h>
#include <asm/arch/h2200-gpio.h>
#include <linux/mfd/hamcop_base.h>
#include <asm/hardware/ipaq-hamcop.h>
#include <asm/arch/h2200.h>


#include "pxa2xx-i2sound.h"
#include <sound/uda1380.h>

static struct snd_uda1380 uda;

static void snd_h2200_audio_set_codec_power(int mode) { SET_H2200_GPIO(CODEC_ON, mode); }

static void snd_h2200_audio_set_codec_reset(int mode) { SET_H2200_GPIO(CODEC_RESET, mode); }

static void snd_h2200_audio_set_mic_power(int mode) { SET_H2200_GPIO_N(MIC_ON, mode); }

static void snd_h2200_audio_set_speaker_power(int mode) {
	if (mode) {
		SET_H2200_GPIO(SPEAKER_ON, 1);
		SET_H2200_GPIO(TDA_MODE, 0);	/* Unmute the amplifier. */
	} else {
		SET_H2200_GPIO(TDA_MODE, 1);
		SET_H2200_GPIO(SPEAKER_ON, 0);	/* Mute the amplifier. */
	}
}

static inline int snd_h2200_audio_hp_detect(void) {
	return (hamcop_get_gpio_b(&h2200_hamcop.dev)
		& HAMCOP_GPIO_GPB_DAT_EAR_IN ) ?
	       1 : 0;
}

static irqreturn_t snd_h2200_audio_hp_isr (int isr, void *data) {
	snd_uda1380_hp_detected(&uda, snd_h2200_audio_hp_detect());
        return IRQ_HANDLED;
}

static void snd_h2200_audio_hp_detection_on(void) {
	struct device *hamcop = &h2200_hamcop.dev;
	unsigned long flags;

	/* Configure GPB[7] as an input and enable interrupts on both edges. */
	hamcop_set_gpio_b_con(hamcop, HAMCOP_GPIO_GPx_CON_MASK(7),
			      HAMCOP_GPIO_GPx_CON_MODE(7, INPUT));
	hamcop_set_gpio_b_int(hamcop, HAMCOP_GPIO_GPx_INT_MASK(7),
	      HAMCOP_GPIO_GPx_INT_MODE(7, HAMCOP_GPIO_GPx_INT_INTEN_ENABLE |
					  HAMCOP_GPIO_GPx_INT_INTLV_BOTHEDGES));

        request_irq(hamcop_irq_base(hamcop) + _HAMCOP_IC_INT_EAR_IN,
		    snd_h2200_audio_hp_isr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
		    "hp jack", NULL);

	local_irq_save(flags);
	snd_uda1380_hp_detected(&uda, snd_h2200_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_h2200_audio_hp_detection_off(void) {
	struct device *hamcop = &h2200_hamcop.dev;

	hamcop_set_gpio_b_int(hamcop, HAMCOP_GPIO_GPx_INT_MASK(7),
	      HAMCOP_GPIO_GPx_INT_MODE(7, HAMCOP_GPIO_GPx_INT_INTEN_DISABLE));

        free_irq(hamcop_irq_base(hamcop) + _HAMCOP_IC_INT_EAR_IN, NULL);
}

static struct snd_uda1380 uda = {
	.line_in_connected	= 0,
	.mic_connected		= 1,
	.hp_or_line_out		= 1,
	.capture_source		= SND_UDA1380_CAP_SOURCE_MIC,
	.power_on_chip		= snd_h2200_audio_set_codec_power,
	.reset_pin		= snd_h2200_audio_set_codec_reset,
	.line_out_on		= snd_h2200_audio_set_speaker_power,
	.mic_on			= snd_h2200_audio_set_mic_power
};

static int snd_h2200_audio_activate(void) {
	/* UDA1380 at address 0x18 on PXA2xx I2C bus */
	uda.i2c_client.adapter = i2c_get_adapter(0);
	uda.i2c_client.addr = 0x18;

	if (snd_uda1380_activate(&uda) == 0) {
		snd_h2200_audio_hp_detection_on();
		return 0;
	}
	else return -1;
}

static void snd_h2200_audio_deactivate(void) {
	snd_h2200_audio_hp_detection_off();
	snd_uda1380_deactivate(&uda);
}

static int snd_h2200_audio_open_stream(int stream)
	{ return snd_uda1380_open_stream(&uda, stream); }

static void snd_h2200_audio_close_stream(int stream)
	{ snd_uda1380_close_stream(&uda, stream); }

static int snd_h2200_audio_add_mixer_controls(struct snd_card *acard) {
	return snd_uda1380_add_mixer_controls(&uda, acard);
}

#ifdef CONFIG_PM
static int snd_h2200_audio_suspend(pm_message_t state) {
	snd_h2200_audio_hp_detection_off();
	snd_uda1380_suspend(&uda, state);
	return 0;
}

static int snd_h2200_audio_resume(void) {
	snd_uda1380_resume(&uda);
	snd_h2200_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board h2200_audio = {
	.name			= "h2200 Audio",
	.desc			= "iPAQ h2200 Audio [codec Philips UDA1380]",
	.acard_id		= "h2200 Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_h2200_audio_activate,
	.deactivate		= snd_h2200_audio_deactivate,
	.open_stream		= snd_h2200_audio_open_stream,
	.close_stream		= snd_h2200_audio_close_stream,
	.add_mixer_controls	= snd_h2200_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_h2200_audio_suspend,
	.resume			= snd_h2200_audio_resume
#endif
};

static int __init snd_h2200_audio_init(void) {
	/* check machine */
	if (!machine_is_h2200()) {
		snd_printk(KERN_INFO "Module snd-h2200_audio: not an iPAQ h2200!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&h2200_audio);
}

static void __exit snd_h2200_audio_exit(void) {
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_h2200_audio_init);
module_exit(snd_h2200_audio_exit);

MODULE_AUTHOR("Matthew Reimer, Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for iPAQ h2200");
MODULE_LICENSE("GPL");

