/*
 * palmt650.c  --  SoC audio for Palm Treo 650
 *
 * Based on tosa.c
 *
 * Copyright 2007 (C) Alex Osborne <bobofdoom@gmail.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation; version 2 ONLY.
 *
 *  Revision history
 *    31 Jul 2007   Initial version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/audio.h>

#include "../codecs/wm9712.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

static struct snd_soc_machine palmt650;

static int palmt650_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	/* TODO */
	return 0;
}

static struct snd_soc_ops palmt650_ops = {
	.startup = palmt650_startup,
};

static int palmt650_ac97_init(struct snd_soc_codec *codec)
{
	/* TODO */
	return 0;
}

static struct snd_soc_dai_link palmt650_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
	.codec_dai = &wm9712_dai[WM9712_DAI_AC97_HIFI],
	.init = palmt650_ac97_init,
	.ops = &palmt650_ops,
},
{
	.name = "AC97 Aux",
	.stream_name = "AC97 Aux",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
	.codec_dai = &wm9712_dai[WM9712_DAI_AC97_AUX],
	.ops = &palmt650_ops,
},
};

static struct snd_soc_machine palmt650 = {
	.name = "Palm Treo 650",
	.dai_link = palmt650_dai,
	.num_links = ARRAY_SIZE(palmt650_dai),
};

static struct snd_soc_device palmt650_snd_devdata = {
	.machine = &palmt650,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm9712,
};

static struct platform_device *palmt650_snd_device;

static int __init palmt650_soc_init(void)
{
	int ret;

	if (!machine_is_xscale_treo650())
		return -ENODEV;

	palmt650_snd_device = platform_device_alloc("soc-audio", -1);
	if (!palmt650_snd_device)
		return -ENOMEM;

	platform_set_drvdata(palmt650_snd_device, &palmt650_snd_devdata);
	palmt650_snd_devdata.dev = &palmt650_snd_device->dev;
	ret = platform_device_add(palmt650_snd_device);

	if (ret)
		platform_device_put(palmt650_snd_device);

	return ret;
}

static void __exit palmt650_soc_exit(void)
{
	platform_device_unregister(palmt650_snd_device);
}

module_init(palmt650_soc_init);
module_exit(palmt650_soc_exit);

MODULE_AUTHOR("Alex Osborne <bobofdoom@gmail.com>");
MODULE_DESCRIPTION("ALSA SoC driver for Palm Treo 650");
MODULE_LICENSE("GPL");
