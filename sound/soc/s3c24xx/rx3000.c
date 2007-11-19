/*
 * rx3000.c  --  ALSA Soc Audio Layer
 *
 * Copyright (c) 2007 Roman Moravcik <roman.moravcik@gmail.com>
 *
 * Based on smdk2440.c and magician.c
 *
 * Authors: Graeme Gregory graeme.gregory@wolfsonmicro.com
 *          Philipp Zabel <philipp.zabel@gmail.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm/hardware/scoop.h>
#include <asm/arch/regs-iis.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/audio.h>
#include <asm/arch/rx3000-asic3.h>

#include <linux/mfd/asic3_base.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include "../codecs/uda1380.h"
#include "s3c24xx-pcm.h"
#include "s3c24xx-i2s.h"

extern struct platform_device s3c_device_asic3;

#define RX3000_DEBUG 0
#if RX3000_DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define RX3000_HP_OFF    0
#define RX3000_HP_ON     1
#define RX3000_MIC       2

#define RX3000_SPK_ON    0
#define RX3000_SPK_OFF   1

static int rx3000_jack_func = RX3000_HP_OFF;
static int rx3000_spk_func = RX3000_SPK_ON;

static void rx3000_ext_control(struct snd_soc_codec *codec)
{
	if (rx3000_spk_func == RX3000_SPK_ON)
		snd_soc_dapm_set_endpoint(codec, "Speaker", 1);
	else
		snd_soc_dapm_set_endpoint(codec, "Speaker", 0);
	    
	/* set up jack connection */
	switch (rx3000_jack_func) {
		case RX3000_HP_OFF:
	    		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
			snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
			break;
		case RX3000_HP_ON:
			snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 1);
			snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
			break;
		case RX3000_MIC:
			snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
			snd_soc_dapm_set_endpoint(codec, "Mic Jack", 1);
			break;
	}
	snd_soc_dapm_sync_endpoints(codec);
}

static int rx3000_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	DBG("Entered rx3000_startup\n");

	/* check the jack status at stream startup */
	rx3000_ext_control(codec);

	return 0;
}

static void rx3000_shutdown(struct snd_pcm_substream *substream)
{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_codec *codec = rtd->socdev->codec;

	DBG("Entered rx3000_shutdown\n");
}

static int rx3000_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	unsigned long iis_clkrate;
	int div, div256, div384, diff256, diff384, bclk;
	int ret;
	unsigned int rate=params_rate(params);

	DBG("Entered %s\n",__FUNCTION__);

	iis_clkrate = s3c24xx_i2s_get_clockrate();

	/* Using PCLK doesnt seem to suit audio particularly well on these cpu's
	 */

	div256 = iis_clkrate / (rate * 256);
	div384 = iis_clkrate / (rate * 384);

	if (((iis_clkrate / div256) - (rate * 256)) <
		((rate * 256) - (iis_clkrate / (div256 + 1)))) {
		diff256 = (iis_clkrate / div256) - (rate * 256);
	} else {
		div256++;
		diff256 = (iis_clkrate / div256) - (rate * 256);
	}

	if (((iis_clkrate / div384) - (rate * 384)) <
		((rate * 384) - (iis_clkrate / (div384 + 1)))) {
		diff384 = (iis_clkrate / div384) - (rate * 384);
	} else {
		div384++;
		diff384 = (iis_clkrate / div384) - (rate * 384);
	}

	DBG("diff256 %d, diff384 %d\n", diff256, diff384);

	if (diff256<=diff384) {
		DBG("Selected 256FS\n");
		div = div256;
		bclk = S3C2410_IISMOD_256FS;
	} else {
		DBG("Selected 384FS\n");
		div = div384;
		bclk = S3C2410_IISMOD_384FS;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the audio system clock for DAC and ADC */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, S3C24XX_CLKSRC_PCLK,
		rate, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* set MCLK division for sample rate */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, S3C24XX_DIV_MCLK, S3C2410_IISMOD_32FS );
	if (ret < 0)
		return ret;

	/* set BCLK division for sample rate */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, S3C24XX_DIV_BCLK, bclk);
	if (ret < 0)
		return ret;

	/* set prescaler division for sample rate */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, S3C24XX_DIV_PRESCALER,
		S3C24XX_PRESCALE(div,div));
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops rx3000_ops = {
	.startup 	= rx3000_startup,
	.shutdown 	= rx3000_shutdown,
	.hw_params 	= rx3000_hw_params,
};

static int rx3000_get_jack(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rx3000_jack_func;
	return 0;
}

static int rx3000_set_jack(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (rx3000_jack_func == ucontrol->value.integer.value[0])
		return 0;

	rx3000_jack_func = ucontrol->value.integer.value[0];
	rx3000_ext_control(codec);
	return 1;
}

static int rx3000_get_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rx3000_spk_func;
	return 0;
}

static int rx3000_set_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (rx3000_spk_func == ucontrol->value.integer.value[0])
		return 0;

	rx3000_spk_func = ucontrol->value.integer.value[0];
	rx3000_ext_control(codec);
	return 1;
}

static int rx3000_spk_power(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA1, ASIC3_GPA1);
	else
		asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA1, 0);
	return 0;
}

/* rx3000 machine dapm widgets */
static const struct snd_soc_dapm_widget uda1380_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker", rx3000_spk_power),
};

/* rx3000 machine audio_map */
static const char *audio_map[][3] = {
	/* headphone connected to VOUTLHP, VOUTRHP */
	{"Headphone Jack", NULL, "VOUTLHP"},
	{"Headphone Jack", NULL, "VOUTRHP"},

	/* ext speaker connected to VOUTL, VOUTR  */
	{"Speaker", NULL, "VOUTL"},
	{"Speaker", NULL, "VOUTR"},

	/* mic is connected to VINM */
	{"VINM", NULL, "Mic Jack"},

	{NULL, NULL, NULL},
};

static const char *jack_function[] = {"Off", "Headphone", "Mic"};
static const char *spk_function[] = {"On", "Off"};

static const struct soc_enum rx3000_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static const struct snd_kcontrol_new uda1380_rx3000_controls[] = {
	SOC_ENUM_EXT("Jack Function", rx3000_enum[0], rx3000_get_jack,
			rx3000_set_jack),
	SOC_ENUM_EXT("Speaker Function", rx3000_enum[1], rx3000_get_spk,
			rx3000_set_spk),
};

/*
 * Logic for a UDA1380 as attached to RX3000
 */
static int rx3000_uda1380_init(struct snd_soc_codec *codec)
{
	int i, err;

	DBG("Staring rx3000 init\n");

	/* NC codec pins */
	snd_soc_dapm_set_endpoint(codec, "VOUTLHP", 0);
	snd_soc_dapm_set_endpoint(codec, "VOUTRHP", 0);

	/* Add rx3000 specific controls */
	for (i = 0; i < ARRAY_SIZE(uda1380_rx3000_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&uda1380_rx3000_controls[i],
				codec, NULL))) < 0)
			return err;
	}

	/* Add rx3000 specific widgets */
	for(i = 0; i < ARRAY_SIZE(uda1380_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &uda1380_dapm_widgets[i]);
	}

	/* Set up rx3000 specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
			audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);

	DBG("Ending rx3000 init\n");

	return 0;
}

/* s3c24xx digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link s3c24xx_dai = {
	.name 		= "uda1380",
	.stream_name 	= "UDA1380",
	.cpu_dai 	= &s3c24xx_i2s_dai,
	.codec_dai 	= &uda1380_dai[UDA1380_DAI_DUPLEX],
	.init 		= rx3000_uda1380_init,
	.ops 		= &rx3000_ops,
};

/* rx3000 audio machine driver */
static struct snd_soc_machine snd_soc_machine_rx3000 = {
	.name 		= "RX3000",
	.dai_link 	= &s3c24xx_dai,
	.num_links 	= 1,
};

static struct uda1380_setup_data rx3000_uda1380_setup = {
	.i2c_address 	= 0x1a,
	.dac_clk 	= UDA1380_DAC_CLK_SYSCLK,
};

/* s3c24xx audio subsystem */
static struct snd_soc_device s3c24xx_snd_devdata = {
	.machine 	= &snd_soc_machine_rx3000,
	.platform 	= &s3c24xx_soc_platform,
	.codec_dev 	= &soc_codec_dev_uda1380,
	.codec_data 	= &rx3000_uda1380_setup,
};

static struct platform_device *s3c24xx_snd_device;

static int __init rx3000_init(void)
{
	int ret;

	if (!machine_is_rx3715())
		return -ENODEV;

	/* enable uda1380 power supply */
	asic3_set_gpio_out_c(&s3c_device_asic3.dev, ASIC3_GPC7, ASIC3_GPC7);

	/* reset uda1380 */
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA2, 0);
	mdelay(1);
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA2, ASIC3_GPA2);
	udelay(1);
	asic3_set_gpio_out_a(&s3c_device_asic3.dev, ASIC3_GPA2, 0);

	/* correct place? we'll need it to talk to the uda1380 */
	request_module("i2c-s3c2410");

	s3c24xx_snd_device = platform_device_alloc("soc-audio", -1);
	if (!s3c24xx_snd_device) {
		DBG("platform_dev_alloc failed\n");
		free_irq(IRQ_EINT19, NULL);
		return -ENOMEM;
	}

	platform_set_drvdata(s3c24xx_snd_device, &s3c24xx_snd_devdata);
	s3c24xx_snd_devdata.dev = &s3c24xx_snd_device->dev;
	ret = platform_device_add(s3c24xx_snd_device);

	if (ret)
		platform_device_put(s3c24xx_snd_device);

	return ret;
}

static void __exit rx3000_exit(void)
{
	platform_device_unregister(s3c24xx_snd_device);
}

module_init(rx3000_init);
module_exit(rx3000_exit);

/* Module information */
MODULE_AUTHOR("Roman Moravcik, <roman.moravcik@gmail.com>");
MODULE_DESCRIPTION("ALSA SoC RX3000");
MODULE_LICENSE("GPL");
