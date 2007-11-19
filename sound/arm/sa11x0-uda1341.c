/*
 *  linux/sound/arm/sa11x0-uda1341.c
 *
 *  Copyright (C) 2003 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>

#include "sa11x0-ssp.h"

struct ssp_uda1341 {
	struct device		*dev;
	struct sa11x0_ssp_ops	*ops;
	struct uda1341		*uda;
	snd_pcm_t		*pcm;
};

static int sa11x0_uda1341_do_suspend(snd_card_t *card)
{
	struct ssp_uda1341 *ssp = card->private_data;

	if (card->power_state != SNDRV_CTL_POWER_D3cold) {
		snd_pcm_suspend_all(ssp->pcm);
		uda1341_suspend(ssp->uda);
		if (ssp->ops->suspend)
			ssp->ops->suspend(ssp->dev);
		snd_power_change_state(card, SNDRV_CTL_POWER_D3cold);
	}

	return 0;
}

static int sa11x0_uda1341_suspend(struct device *_dev, u32 state, u32 level)
{
	snd_card_t *card = dev_get_drvdata(_dev);
	int ret = 0;

	if (card && level == SUSPEND_DISABLE)
		ret = sa11x0_uda1341_do_suspend(card);

	return ret;
}

static int sa11x0_uda1341_do_resume(snd_card_t *card)
{
	struct ssp_uda1341 *ssp = card->private_data;

	if (card->power_state != SNDRV_CTL_POWER_D0) {
		if (ssp->ops->resume)
			ssp->ops->resume(ssp->dev);
		uda1341_resume(ssp->uda);
		snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	}

	return 0;
}

static int sa11x0_uda1341_resume(struct device *_dev, u32 level)
{
	snd_card_t *card = dev_get_drvdata(_dev);
	int ret = 0;

	if (card && level == SUSPEND_ENABLE) {
		ret = sa11x0_uda1341_do_resume(card);

	return ret;
}

static int sa11x0_uda1341_set_power_state(snd_card_t *card, int state)
{
	int ret = -EINVAL;
	switch (state) {
	case SNDRV_CTL_POWER_D0:
	case SNDRV_CTL_POWER_D1:
	case SNDRV_CTL_POWER_D2:
		ret = sa11x0_uda1341_do_resume(card);
		break;
	case SNDRV_CTL_POWER_D3hot:
	case SNDRV_CTL_POWER_D3cold:
		ret = sa11x0_uda1341_do_suspend(card);
		break;
	}
	return ret;
}

static int sa11x0_uda1341_probe(struct device *_dev)
{
	snd_card_t *card;
	struct ssp_uda1341 *ssp;
	int ret;

	card = snd_card_new(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			    THIS_MODULE, sizeof(struct ssp_uda1341));
	if (card == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	strncpy(card->driver, "sa11x0-uda1341", sizeof(card->driver));
	strncpy(card->shortname, "Philips UDA1341", sizeof(card->shortname));
	card->set_power_state = sa11x0_uda1341_set_power_state;

	ssp = card->private_data;
	ssp->dev = _dev;
	ssp->ops = _dev->platform_data;

	ssp->pcm = sa11x0_ssp_create(card);
	if (IS_ERR(ssp->pcm)) {
		ret = PTR_ERR(ssp->pcm);
		goto out;
	}

	ssp->uda = uda1341_create(card, "l3-bit-sa1100-gpio");
	if (IS_ERR(ssp->uda)) {
		ret = PTR_ERR(ssp->uda);
		goto out;
	}

	ret = snd_card_register(card);
	if (ret == 0) {
		dev_set_drvdata(_dev, card);
		return 0;
	}

 out:
	if (card)
		snd_card_free(card);
	return ret;
}

static int sa11x0_uda1341_remove(struct device *_dev)
{
	snd_card_t *card = dev_get_drvdata(_dev);

	if (card)
		snd_card_free(card);

	dev_set_drvdata(_dev, NULL);

	return 0;
}

static struct device_driver sa11x0_uda1341_driver = {
	.name		= "sa11x0-ssp",
	.bus		= &platform_bus_type,
	.probe		= sa11x0_uda1341_probe,
	.remove		= sa11x0_uda1341_remove,
	.suspend	= sa11x0_uda1341_suspend,
	.resume		= sa11x0_uda1341_resume,
};

static int __init sa11x0_uda1341_init(void)
{
	return driver_register(&sa11x0_uda1341_driver);
}

static void __exit sa11x0_uda1341_exit(void)
{
	driver_unregister(&sa11x0_uda1341_driver);
}

module_init(sa11x0_uda1341_init);
module_exit(sa11x0_uda1341_exit);

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("Generic SA11x0 Philips UDA1341TS audio driver.");
MODULE_LICENSE("GPL");
