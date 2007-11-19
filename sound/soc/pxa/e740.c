/*
 * e740.c  --  SoC audio for e740
 *
 * Based on tosa.c
 *
 * Copyright 2005 (c) Ian Molton <spyro@f2s.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation; version 2 ONLY.
 *
 *  Revision history
 *    18th Nov 2006   Initial version.
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

static struct snd_soc_machine e740;

#define E740_HP        0
#define E740_MIC_INT   1
#define E740_HEADSET   2
#define E740_HP_OFF    3
#define E740_SPK_ON    0
#define E740_SPK_OFF   1

static int e740_jack_func;
static int e740_spk_func;

static struct snd_soc_machine e740 = {
	.name = "Toshiba e740",
//	.dai_link = e740_dai,
//	.num_links = ARRAY_SIZE(e740_dai),
//	.ops = &e740_ops,
};

static struct snd_soc_device e740_snd_devdata = {
	.machine = &e740,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = NULL, //&soc_codec_dev_wm9712,
};

static struct platform_device *e740_snd_device;

static int __init e740_init(void)
{
	int ret;

	if (!machine_is_e740())
		return -ENODEV;

	e740_snd_device = platform_device_alloc("soc-audio", -1);
	if (!e740_snd_device)
		return -ENOMEM;

	platform_set_drvdata(e740_snd_device, &e740_snd_devdata);
	e740_snd_devdata.dev = &e740_snd_device->dev;
	ret = platform_device_add(e740_snd_device);

	if (ret)
		platform_device_put(e740_snd_device);

	return ret;
}

static void __exit e740_exit(void)
{
	platform_device_unregister(e740_snd_device);
}

module_init(e740_init);
module_exit(e740_exit);

/* Module information */
MODULE_AUTHOR("Ian Molton <spyro@f2s.com>");
MODULE_DESCRIPTION("ALSA SoC driver for e740");
MODULE_LICENSE("GPL");
