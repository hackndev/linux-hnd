/*
 * Audio support for iPAQ htcsable
 * It uses PXA2xx i2Sound and AK4641 modules
 *
 * Copyright (c) 2005 Todd Blumer, SDG Systems, LLC
 * Copyright (c) 2006 Giorgio Padrin <giorgio@mandarinlogiq.org>
 * Copyright (c) 2006 Elshin Roman <roxmail@list.ru>
 * Copyright (c) 2006 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
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
 * 2006-11  Attempt to create hw6915 driver from hx4700 code -- lkcl
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
#include <asm/arch/htcsable-gpio.h>
#include <asm/arch/htcsable-asic.h>
#include <linux/mfd/asic3_base.h>

#include "pxa2xx-i2sound.h"
#include <sound/ak4641.h>

static struct snd_ak4641 ak;
static int sable_headphone_irq; /* yuk */

static void snd_htcsable_audio_set_codec_power(int mode)
{
  snd_printk(KERN_INFO "snd_htcsable_audio_set_codec_power: %d\n", mode);
  asic3_set_gpio_out_a(&htcsable_asic3.dev, 1<<GPIOA_CODEC_ON , mode?(1<<GPIOA_CODEC_ON):0);
}

static void snd_htcsable_audio_set_codec_reset(int mode)
{
  snd_printk(KERN_INFO "snd_htcsable_audio_set_codec_reset: %d\n", mode);
  asic3_set_gpio_out_b(&htcsable_asic3.dev, 1<<GPIOB_CODEC_RESET , mode?(1<<GPIOB_CODEC_RESET):0);
}

static void snd_htcsable_audio_set_headphone_power(int mode)
{
  asic3_set_gpio_out_a(&htcsable_asic3.dev, 1<<GPIOA_HEADPHONE_PWR_ON , mode?(1<<GPIOA_HEADPHONE_PWR_ON ):0);
}

static void snd_htcsable_audio_set_speaker_power(int mode)
{
  snd_printk(KERN_INFO "snd_htcsable_audio_set_speaker_power: %d\n", mode);
  asic3_set_gpio_out_a(&htcsable_asic3.dev, 1<<GPIOA_SPKMIC_PWR2_ON , mode?(1<<GPIOA_SPKMIC_PWR2_ON):0);
}

static inline int snd_htcsable_audio_hp_detect(void)
{
  int hp_in = (asic3_get_gpio_status_b(&htcsable_asic3.dev) &
               (1<<GPIOB_HEADPHONE)) == 0;

  if (hp_in)
    set_irq_type(sable_headphone_irq, IRQ_TYPE_EDGE_RISING );
  else
    set_irq_type(sable_headphone_irq, IRQ_TYPE_EDGE_FALLING );

  return hp_in;
}

static irqreturn_t snd_htcsable_audio_hp_isr (int isr, void *data)
{
	snd_ak4641_hp_detected(&ak, snd_htcsable_audio_hp_detect());
	return IRQ_HANDLED;
}

static void snd_htcsable_audio_hp_detection_on(void)
{
	unsigned long flags;

  sable_headphone_irq = asic3_irq_base( &htcsable_asic3.dev ) +
                        ASIC3_GPIOB_IRQ_BASE + GPIOB_HEADPHONE;
	if (request_irq(sable_headphone_irq, snd_htcsable_audio_hp_isr,
       SA_INTERRUPT | SA_SAMPLE_RANDOM,
			"htcsable Headphone Jack", NULL) != 0)
		return;
	set_irq_type(sable_headphone_irq, IRQT_BOTHEDGE);

	local_irq_save(flags);
	snd_ak4641_hp_detected(&ak, snd_htcsable_audio_hp_detect());
	local_irq_restore(flags);
}

static void snd_htcsable_audio_hp_detection_off(void)
{
	free_irq(sable_headphone_irq, NULL);
}

static struct snd_ak4641 ak = {
	.power_on_chip		= snd_htcsable_audio_set_codec_power,
	.reset_pin		= snd_htcsable_audio_set_codec_reset,
	.headphone_out_on	= snd_htcsable_audio_set_headphone_power,
	.speaker_out_on		= snd_htcsable_audio_set_speaker_power,
};

static int snd_htcsable_audio_activate(void)
{
	/* AK4641 on PXA2xx I2C bus */
	ak.i2c_client.adapter = i2c_get_adapter(0);


	snd_pxa2xx_i2sound_i2slink_get();
	if (snd_ak4641_activate(&ak) == 0) {
		snd_htcsable_audio_hp_detection_on();
  snd_printk(KERN_INFO "snd-htcsable_audio: audio activate ok\n");
		return 0;
	}
	else {
		snd_pxa2xx_i2sound_i2slink_free();
  snd_printk(KERN_INFO "snd-htcsable_audio: audio activate failed\n");
		return -1;
	}
}

static void snd_htcsable_audio_deactivate(void)
{
	snd_htcsable_audio_hp_detection_off();
	snd_ak4641_deactivate(&ak);
	snd_pxa2xx_i2sound_i2slink_free();
}

static int snd_htcsable_audio_open_stream(int stream)
{
	return snd_ak4641_open_stream(&ak, stream);
}

static void snd_htcsable_audio_close_stream(int stream)
{
	snd_ak4641_close_stream(&ak, stream);
}

static int snd_htcsable_audio_add_mixer_controls(struct snd_card *acard)
{
	return snd_ak4641_add_mixer_controls(&ak, acard);
}

#ifdef CONFIG_PM
static int snd_htcsable_audio_suspend(pm_message_t state)
{
	snd_htcsable_audio_hp_detection_off();
	snd_ak4641_suspend(&ak, state);
	return 0;
}

static int snd_htcsable_audio_resume(void)
{
	snd_ak4641_resume(&ak);
	snd_htcsable_audio_hp_detection_on();
	return 0;
}
#endif

static struct snd_pxa2xx_i2sound_board htcsable_audio = {
	.name			= "htcsable Audio",
	.desc			= "iPAQ htcsable Audio [codec Asahi Kasei AK4641]",
	.acard_id		= "htcsable Audio",
	.info			= SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA |
				  SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE,
	.activate		= snd_htcsable_audio_activate,
	.deactivate		= snd_htcsable_audio_deactivate,
	.open_stream		= snd_htcsable_audio_open_stream,
	.close_stream		= snd_htcsable_audio_close_stream,
	.add_mixer_controls	= snd_htcsable_audio_add_mixer_controls,
#ifdef CONFIG_PM
	.suspend		= snd_htcsable_audio_suspend,
	.resume			= snd_htcsable_audio_resume
#endif
};

static int __init snd_htcsable_audio_init(void)
{
	/* check machine */
	if (!machine_is_hw6900()) {
		snd_printk(KERN_INFO "Module snd-htcsable_audio: not an iPAQ htcsable!\n");
		return -1;
	}

  /* these might help... */
  pxa_gpio_mode(GPIO28_BITCLK_OUT_I2S_MD);
  pxa_gpio_mode(GPIO29_SDATA_IN_I2S_MD);
  pxa_gpio_mode(GPIO30_SDATA_OUT_I2S_MD);
  pxa_gpio_mode(GPIO31_SYNC_I2S_MD);
  pxa_gpio_mode(GPIO_NR_HTCSABLE_I2S_SYSCLK);

  /* gpio a0 is suspected to be the i2c power-up line, not audio power-up.
   * this is here as a temporary hack
   */
  asic3_set_gpio_out_a(&htcsable_asic3.dev, 1<<GPIOA_I2C_PWR_ON , 1<<GPIOA_I2C_PWR_ON);

	request_module("i2c_pxa");
	return snd_pxa2xx_i2sound_card_activate(&htcsable_audio);
}

static void __exit snd_htcsable_audio_exit(void)
{
	snd_pxa2xx_i2sound_card_deactivate();
}

module_init(snd_htcsable_audio_init);
module_exit(snd_htcsable_audio_exit);

MODULE_AUTHOR("Todd Blumer, SDG Systems, LLC; Elshin Roman; Giorgio Padrin; Luke Kenneth Casson Leighton");
MODULE_DESCRIPTION("Audio support for iPAQ htcsable");
MODULE_LICENSE("GPL");
