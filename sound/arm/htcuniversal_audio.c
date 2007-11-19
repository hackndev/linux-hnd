/*
 * Audio support for HTC Universal
 * It uses PXA2xx i2Sound and AK4641 modules
 *
 * Copyright (c) 2006 Oleg Gusev
 * 
 * Reference code: hx4700_audio.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or (at your option) any later version.
 *
 * History:
 * 
 * 2006-07	Initial release	-- Oleg Gusev
 * 2006-09	Use new AK4641 code -- Giorgio Padrin
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
#include <linux/mfd/asic3_base.h>
#include <asm/arch/htcuniversal-gpio.h>
#include <asm/arch/htcuniversal-asic.h>

#include "pxa2xx-i2sound.h"
#include <sound/ak4641.h>

static struct snd_ak4641 ak;

static void snd_htcuniversal_audio_set_codec_power(int mode)
{
	printk( KERN_NOTICE "snd_htcuniversal_audio_set_codec_power: %d\n", mode );

	asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_AUDIO_PWR_ON , mode?(1<<GPIOA_AUDIO_PWR_ON):0);
}

static void snd_htcuniversal_audio_set_codec_reset(int mode)
{
	printk( KERN_NOTICE "snd_htcuniversal_audio_set_codec_reset: %d\n", mode );
	asic3_set_gpio_out_b(&htcuniversal_asic3.dev, 1<<GPIOB_CODEC_PDN , mode?0:(1<<GPIOB_CODEC_PDN));	
}

static void snd_htcuniversal_audio_set_headphone_power(int mode)
{
	printk( KERN_NOTICE "snd_htcuniversal_audio_set_headphone_power: %d\n", mode );

	asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_EARPHONE_PWR_ON , mode?(1<<GPIOA_EARPHONE_PWR_ON):0);	
}

static void snd_htcuniversal_audio_set_speaker_power(int mode)
{
	/* speaker shutdown (0=shutdown)*/
	printk( KERN_NOTICE "snd_htcuniversal_audio_set_speaker_power: %d\n", mode );

	asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_SPK_PWR1_ON , mode?(1<<GPIOA_SPK_PWR1_ON):0);
	asic3_set_gpio_out_a(&htcuniversal_asic3.dev, 1<<GPIOA_SPK_PWR2_ON , mode?(1<<GPIOA_SPK_PWR2_ON):0);
}

static inline int snd_htcuniversal_audio_hp_detect(void)
{
 int irq, hp_in;

	hp_in= (((asic3_get_gpio_status_b( &htcuniversal_asic3.dev )) & (1<<GPIOB_EARPHONE_N)) == 0);

	printk( KERN_NOTICE "snd_htcuniversal_audio_set_headphone_detect: %d\n", hp_in);

	irq = asic3_irq_base( &htcuniversal_asic3.dev ) + ASIC3_GPIOB_IRQ_BASE + GPIOB_EARPHONE_N;

	if (hp_in)
	 set_irq_type(irq, IRQ_TYPE_EDGE_RISING );
	else
         set_irq_type(irq, IRQ_TYPE_EDGE_FALLING );

	return hp_in;
}

static irqreturn_t snd_htcuniversal_audio_hp_isr (int isr, void *data)
{
	snd_ak4641_hp_detected(&ak, snd_htcuniversal_audio_hp_detect());
        return IRQ_HANDLED;
}

static void snd_htcuniversal_audio_hp_detection_on(void)
{
	unsigned long flags;
	int irq;

	irq = asic3_irq_base( &htcuniversal_asic3.dev ) + ASIC3_GPIOB_IRQ_BASE + GPIOB_EARPHONE_N;
	if (request_irq(irq, snd_htcuniversal_audio_hp_isr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
			"HTC Universal Headphone Jack", NULL) != 0)
		return;
	set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);

	local_irq_save(flags);
	snd_ak4641_hp_detected(&ak, snd_htcuniversal_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_htcuniversal_audio_hp_detection_off(void)
{
	int irq = asic3_irq_base( &htcuniversal_asic3.dev ) + ASIC3_GPIOB_IRQ_BASE + GPIOB_EARPHONE_N;
	free_irq(irq, NULL);
}

static struct snd_ak4641 ak = {
	.power_on_chip		= snd_htcuniversal_audio_set_codec_power,
	.reset_pin		= snd_htcuniversal_audio_set_codec_reset,
	.headphone_out_on	= snd_htcuniversal_audio_set_headphone_power,
	.speaker_out_on		= snd_htcuniversal_audio_set_speaker_power
};

static int snd_htcuniversal_audio_activate(void)
{
	/* AK4641 on PXA2xx I2C bus */
	ak.i2c_client.adapter = i2c_get_adapter(0);

	snd_pxa2xx_i2sound_i2slink_get();
	if (snd_ak4641_activate(&ak) == 0) {
		snd_htcuniversal_audio_hp_detection_on();
		return 0;
	}
	else {
		snd_pxa2xx_i2sound_i2slink_free();
		return -1;
	}
}

static void snd_htcuniversal_audio_deactivate(void)
{
	snd_htcuniversal_audio_hp_detection_off();
	snd_ak4641_deactivate(&ak);
	snd_pxa2xx_i2sound_i2slink_free();
}

static int snd_htcuniversal_audio_open_stream(int stream)
{
	return snd_ak4641_open_stream(&ak, stream);
}

static void snd_htcuniversal_audio_close_stream(int stream)
{
	snd_ak4641_close_stream(&ak, stream);
}

static int snd_htcuniversal_audio_add_mixer_controls(struct snd_card *acard)
{
	return snd_ak4641_add_mixer_controls(&ak, acard);
}

#ifdef CONFIG_PM
static int snd_htcuniversal_audio_suspend(pm_message_t state)
{
	snd_htcuniversal_audio_hp_detection_off();
	snd_ak4641_suspend(&ak, state);
	return 0;
}

static int snd_htcuniversal_audio_resume(void)
{
	snd_ak4641_resume(&ak);
	snd_htcuniversal_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board htcuniversal_audio = {
	.name			= "HTC Universal Audio",
	.desc			= "HTC Universal Audio [codec Asahi Kasei AK4641]",
	.acard_id		= "HTC Universal Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_htcuniversal_audio_activate,
	.deactivate		= snd_htcuniversal_audio_deactivate,
	.open_stream		= snd_htcuniversal_audio_open_stream,
	.close_stream		= snd_htcuniversal_audio_close_stream,
	.add_mixer_controls	= snd_htcuniversal_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_htcuniversal_audio_suspend,
	.resume			= snd_htcuniversal_audio_resume
#endif
};

static int __init snd_htcuniversal_audio_init(void)
{
	/* check machine */
	if (!machine_is_htcuniversal()) {
		snd_printk(KERN_INFO "Module snd-htcuniversal_audio: not a HTC Universal!\n");
		return -1;
	}

	request_module("i2c-pxa");
	return snd_pxa2xx_i2sound_card_activate(&htcuniversal_audio);
}

static void __exit snd_htcuniversal_audio_exit(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_htcuniversal_audio_init);
module_exit(snd_htcuniversal_audio_exit);

MODULE_AUTHOR("Oleg, Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for HTC Universal");
MODULE_LICENSE("GPL");
