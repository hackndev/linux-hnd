/*
 * SoC audio for HTC Blueangel
 *
 * Copyright (c) 2006 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * based on spitz.c,
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Richard Purdie <richard@openedhand.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/hardware/scoop.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/htcblueangel-asic.h>
#include <asm/arch/htcblueangel-gpio.h>
#include <asm/mach-types.h>
#include "../codecs/uda1380.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-i2s.h"
#include "pxa2xx-ssp.h"

#include <linux/mfd/asic3_base.h>

#define BLUEANGEL_HP_OFF    0
#define BLUEANGEL_HEADSET   1
#define BLUEANGEL_HP        2

#define BLUEANGEL_SPK_ON    0
#define BLUEANGEL_SPK_OFF   1

#define BLUEANGEL_MIC       0
#define BLUEANGEL_MIC_EXT   1
#define BLUEANGEL_BT_HS     2

/*
 * SSP GPIO's
 */
#define GPIO27_SSP_EXT_CLK_MD	(27 | GPIO_ALT_FN_1_IN)

static int blueangel_jack_func = BLUEANGEL_HP_OFF;
static int blueangel_spk_func = BLUEANGEL_SPK_ON;
static int blueangel_in_sel = BLUEANGEL_MIC;

extern struct platform_device blueangel_asic3;

static void blueangel_ext_control(struct snd_soc_codec *codec)
{
	asic3_set_clock_cdex (&blueangel_asic3.dev, CLOCK_CDEX_EX1, CLOCK_CDEX_EX1);
	udelay(1000);
	asic3_set_clock_cdex (&blueangel_asic3.dev, CLOCK_CDEX_SOURCE0, 0 );
	asic3_set_clock_cdex (&blueangel_asic3.dev, CLOCK_CDEX_SOURCE1, CLOCK_CDEX_SOURCE1);
	asic3_set_clock_cdex (&blueangel_asic3.dev, CLOCK_CDEX_CONTROL_CX, CLOCK_CDEX_CONTROL_CX);

	if (blueangel_spk_func == BLUEANGEL_SPK_ON)
		snd_soc_dapm_set_endpoint(codec, "Speaker", 1);
	else
		snd_soc_dapm_set_endpoint(codec, "Speaker", 0);

	/* set up jack connection */
	switch (blueangel_jack_func) {
	case BLUEANGEL_HP:
		/* enable and unmute hp jack, disable mic bias */
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 1);
		break;
	case BLUEANGEL_HEADSET:
		/* enable mic jack and bias, mute hp */
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 1);
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 1);
		break;
	case BLUEANGEL_HP_OFF:
		/* jack removed, everything off */
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
		break;
	}
#if 0
	/* fixme pH5, can we detect and config the correct Mic type ? */
	switch(blueangel_in_sel) {
	case BLUEANGEL_IN_MIC:
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 1);
		break;
	case BLUEANGEL_IN_MIC_EXT:
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 1);
		break;
	case BLUEANGEL_IN_BT_HS:
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
		break;
	}
#endif
	snd_soc_dapm_sync_endpoints(codec);
}

static int blueangel_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	/* check the jack status at stream startup */
	blueangel_ext_control(codec);

	return 0;
}

/*
 * Blueangel uses SSP port for playback.
 */
static int blueangel_playback_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int freq=705600, div=16; /*  */
	int ret = 0;

	/*
	 */
	switch (params_rate(params)) {
	case 8000:
	case 11025:
	case 22050:
	case 44100:
	case 48000:
		div = freq/params_rate(params);
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_MSB |
			                       SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_MSB |
			                       SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set external SSP clock as clock source */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_SSP_CLK_EXT, freq,
			SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the SSP external clock divider */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai,
			PXA2XX_SSP_DIV_SCR, div);
	if (ret < 0)
		return ret;

	/* set SSP audio pll clock */

	return 0;
}

/*
 * We have to enable the SSP port early so the UDA1380 can flush
 * it's register cache. The UDA1380 can only write it's interpolator and
 * decimator registers when the link is running.
 */
static int blueangel_playback_prepare(struct snd_pcm_substream *substream)
{
	/* enable SSP clock - is this needed ? */
	SSCR0_P(1) |= SSCR0_SSE;

	/* FIXME: ENABLE I2S */
	SACR0 |= SACR0_BCKD;
	SACR0 |= SACR0_ENB;
	pxa_set_cken(CKEN8_I2S, 1);

	return 0;
}

static int blueangel_playback_hw_free(struct snd_pcm_substream *substream)
{
	/* FIXME: DISABLE I2S */
	SACR0 &= ~SACR0_ENB;
	SACR0 &= ~SACR0_BCKD;
	pxa_set_cken(CKEN8_I2S, 0);
	return 0;
}

/*
 * Blueangel uses I2S for capture.
 */
static int blueangel_capture_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai,
			SND_SOC_DAIFMT_MSB | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai,
			SND_SOC_DAIFMT_MSB | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as output */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_I2S_SYSCLK, 0,
			SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * We have to enable the I2S port early so the UDA1380 can flush
 * it's register cache. The UDA1380 can only write it's interpolator and
 * decimator registers when the link is running.
 */
static int blueangel_capture_prepare(struct snd_pcm_substream *substream)
{
	SACR0 |= SACR0_ENB;
	return 0;
}

static struct snd_soc_ops blueangel_capture_ops = {
	.startup = blueangel_startup,
	.hw_params = blueangel_capture_hw_params,
	.prepare = blueangel_capture_prepare,
};

static struct snd_soc_ops blueangel_playback_ops = {
	.startup = blueangel_startup,
	.hw_params = blueangel_playback_hw_params,
	.prepare = blueangel_playback_prepare,
	.hw_free = blueangel_playback_hw_free,
};

static int blueangel_get_jack(struct snd_kcontrol * kcontrol,
			     struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = blueangel_jack_func;
	return 0;
}

static int blueangel_set_jack(struct snd_kcontrol * kcontrol,
			     struct snd_ctl_elem_value * ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (blueangel_jack_func == ucontrol->value.integer.value[0])
		return 0;

	blueangel_jack_func = ucontrol->value.integer.value[0];
	blueangel_ext_control(codec);
	return 1;
}

static int blueangel_get_spk(struct snd_kcontrol * kcontrol,
			    struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = blueangel_spk_func;
	return 0;
}

static int blueangel_set_spk(struct snd_kcontrol * kcontrol,
			    struct snd_ctl_elem_value * ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (blueangel_spk_func == ucontrol->value.integer.value[0])
		return 0;

	blueangel_spk_func = ucontrol->value.integer.value[0];
	blueangel_ext_control(codec);
	return 1;
}

static int blueangel_get_input(struct snd_kcontrol * kcontrol,
			      struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = blueangel_in_sel;
	return 0;
}

static int blueangel_set_input(struct snd_kcontrol * kcontrol,
			      struct snd_ctl_elem_value * ucontrol)
{
//	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (blueangel_in_sel == ucontrol->value.integer.value[0])
		return 0;

	blueangel_in_sel = ucontrol->value.integer.value[0];

	switch (blueangel_in_sel) {
	case BLUEANGEL_MIC:
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1<<GPIOA_MIC_PWR_ON, 1<<GPIOA_MIC_PWR_ON);
		break;
	case BLUEANGEL_MIC_EXT:
		break;
	}

	return 1;
}

static int blueangel_spk_power(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_SPK_PWR_ON, 1<<GPIOB_SPK_PWR_ON);
	else
		asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_SPK_PWR_ON, 0);
	return 0;
}

static int blueangel_hp_power(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_EARPHONE_PWR_ON, 1 << GPIOA_EARPHONE_PWR_ON);
	else
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << GPIOA_EARPHONE_PWR_ON, 0);
	return 0;
}

static int blueangel_mic_bias(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << 8 , 1 << 8 );
	else
		asic3_set_gpio_out_a(&blueangel_asic3.dev, 1 << 8 , 0);
	return 0;
}

/* blueangel machine dapm widgets */
static const struct snd_soc_dapm_widget uda1380_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", blueangel_hp_power),
	SND_SOC_DAPM_MIC("Mic Jack", blueangel_mic_bias),
	SND_SOC_DAPM_SPK("Speaker", blueangel_spk_power),
};

/* blueangel machine audio_map */
static const char *audio_map[][3] = {

	/* headphone connected to VOUTLHP, VOUTRHP */
	{"Headphone Jack", NULL, "VOUTLHP"},
	{"Headphone Jack", NULL, "VOUTRHP"},

	/* ext speaker connected to VOUTL, VOUTR  */
	{"Speaker", NULL, "VOUTL"},
	{"Speaker", NULL, "VOUTR"},

	/* mic is connected to VINM */
	{"VINM", NULL, "Mic Jack"},

	/* line is connected to VINL, VINR */
	{"VINL", NULL, "Line Jack"},
	{"VINR", NULL, "Line Jack"},

	{NULL, NULL, NULL},
};

static const char *jack_function[] = { "Off", "Headset", "Headphone" };
static const char *spk_function[] = { "On", "Off" };
static const char *input_select[] = { "Internal Mic", "External Mic" };
static const struct soc_enum blueangel_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, input_select),
};

static const struct snd_kcontrol_new uda1380_blueangel_controls[] = {
	SOC_ENUM_EXT("Jack Function", blueangel_enum[0], blueangel_get_jack,
			blueangel_set_jack),
	SOC_ENUM_EXT("Speaker Function", blueangel_enum[1], blueangel_get_spk,
			blueangel_set_spk),
	SOC_ENUM_EXT("Input Select", blueangel_enum[2], blueangel_get_input,
			blueangel_set_input),
};

/*
 * Logic for a uda1380 as connected on a HTC Blueangel
 */
static int blueangel_uda1380_init(struct snd_soc_codec *codec)
{
	int i, err;

 	//pxa2xx_ssp_set_clock(&pxa_ssp_dai[0], PXA2XX_SSP_CLK_AUDIO);

	/* NC codec pins */
	snd_soc_dapm_set_endpoint(codec, "VOUTLHP", 0);
	snd_soc_dapm_set_endpoint(codec, "VOUTRHP", 0);



	/* Add blueangel specific controls */
	for (i = 0; i < ARRAY_SIZE(uda1380_blueangel_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&uda1380_blueangel_controls[i],
				codec, NULL))) < 0)
			return err;
	}

	/* Add blueangel specific widgets */
	for (i = 0; i < ARRAY_SIZE(uda1380_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &uda1380_dapm_widgets[i]);
	}

	/* Set up blueangel specific audio path interconnects */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
				audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

/* blueangel digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link blueangel_dai[] = {
{
	.name = "uda1380",
	.stream_name = "UDA1380 Playback",
	.cpu_dai = &pxa_ssp_dai[0],
	.codec_dai = &uda1380_dai[UDA1380_DAI_PLAYBACK],
	.init = blueangel_uda1380_init,
	.ops = &blueangel_playback_ops,
},
{
	.name = "uda1380",
	.stream_name = "UDA1380 Capture",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &uda1380_dai[UDA1380_DAI_CAPTURE],
	.ops = &blueangel_capture_ops,
}
};

/* blueangel audio machine driver */
static struct snd_soc_machine snd_soc_machine_blueangel = {
	.name = "Blueangel",
	.dai_link = blueangel_dai,
	.num_links = ARRAY_SIZE(blueangel_dai),
};

/* blueangel audio private data */
static struct uda1380_setup_data blueangel_uda1380_setup = {
	.i2c_address = 0x18,
	.dac_clk = UDA1380_DAC_CLK_WSPLL,
};

/* blueangel audio subsystem */
static struct snd_soc_device blueangel_snd_devdata = {
	.machine = &snd_soc_machine_blueangel,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_uda1380,
	.codec_data = &blueangel_uda1380_setup,
};

static struct platform_device *blueangel_snd_device;

static int __init blueangel_init(void)
{
	int ret;

	if (!machine_is_blueangel())
		return -ENODEV;
	
#if 0
	asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_CODEC_RESET, 1<<GPIOB_CODEC_RESET); /* reset on? */
	udelay(15);
	asic3_set_gpio_out_a(&blueangel_asic3.dev, 1<<GPIOA_CODEC_PWR_ON, 1<<GPIOA_CODEC_PWR_ON); /* power? */
	/* we may need to have the clock running here - pH5 */
	udelay(5);
	asic3_set_gpio_out_b(&blueangel_asic3.dev, 1<<GPIOB_CODEC_RESET, 0); /* reset on? */
#endif

	/* correct place? we'll need it to talk to the uda1380 */
	ret=request_module("i2c-pxa");

	blueangel_snd_device = platform_device_alloc("soc-audio", -1);
	if (!blueangel_snd_device)
		return -ENOMEM;

	platform_set_drvdata(blueangel_snd_device, &blueangel_snd_devdata);
	blueangel_snd_devdata.dev = &blueangel_snd_device->dev;
	ret = platform_device_add(blueangel_snd_device);

	if (ret)
		platform_device_put(blueangel_snd_device);

	GPDR(GPIO23_SCLK) |= GPIO_bit(GPIO23_SCLK);
	GPDR(GPIO24_SFRM) |= GPIO_bit(GPIO24_SFRM);
	GPDR(GPIO25_STXD) |= GPIO_bit(GPIO25_STXD);
	pxa_gpio_mode(GPIO23_SCLK_MD);
	pxa_gpio_mode(GPIO24_SFRM_MD);
	pxa_gpio_mode(GPIO25_STXD_MD);
	pxa_gpio_mode(GPIO27_SSP_EXT_CLK_MD);

	return ret;
}

static void __exit blueangel_exit(void)
{
	platform_device_unregister(blueangel_snd_device);

}

module_init(blueangel_init);
module_exit(blueangel_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("ALSA SoC HTC Blueangel");
MODULE_LICENSE("GPL");
