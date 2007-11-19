/*
 * AK4535 mixer device driver
 *
 * Copyright (c) 2002 Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Copyright (c) 2002 Tomas Kasparek <tomas.kasparek@seznam.cz>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>

#include <asm/uaccess.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>
#include <sound/ak4535.h>

#include "ak4535.h"

#define chip_t snd_card_h5400_ak4535_t

#define AK4535_REG(xname, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
  .info = snd_ak4535_info_reg, \
  .get = snd_ak4535_get_reg, \
  .put = snd_ak4535_put_reg, \
  .private_value = 1 | (reg << 5) | (shift << 9) | (mask << 12) | (invert << 18) \
}

#define AK4535_REG_DUAL(xname, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
  .info = snd_ak4535_info_reg, \
  .get = snd_ak4535_get_reg, \
  .put = snd_ak4535_put_reg, \
  .private_value = 2 | (reg << 5) | (shift << 9) | (mask << 12) | (invert << 18) \
}

int snd_ak4535_info_reg(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t * uinfo)
{
	int count = (kcontrol->private_value >> 0) & 3;
	int mask = (kcontrol->private_value >> 12) & 63;

	if (0)
	printk("snd_ak4535_info_reg: kcontrol=%p:%s private_value=%p uinfo=%p\n", 
	       kcontrol, kcontrol->id.name, (void*)kcontrol->private_value, uinfo);

	uinfo->type = mask == 1 ? SNDRV_CTL_ELEM_TYPE_BOOLEAN : SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mask;

	return 0;
}

int snd_ak4535_get_reg(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	snd_card_h5400_ak4535_t *chip = snd_kcontrol_chip(kcontrol);
	struct i2c_client *clnt = NULL;
	struct ak4535 *akm = NULL;
	int count = (kcontrol->private_value >> 0) & 3;
	int reg = (kcontrol->private_value >> 5) & 15;
	int shift = (kcontrol->private_value >> 9) & 7;
	int mask = (kcontrol->private_value >> 12) & 63;
	int invert = (kcontrol->private_value >> 18) & 1;
        int rc = 0;
	unsigned char regval;
	int i;

	if (chip)
		clnt = chip->ak4535;
	if (clnt)
		akm = i2c_get_clientdata(clnt);

	if (0)
	printk("snd_ak4535_get_reg: kcontrol=%p:%s reg=%d\n",
	       kcontrol, kcontrol->id.name, reg);

	for (i = 0; i < count; i++) {
		regval= akm->regs[reg+i] = ak4535_read_reg(clnt, reg+i);
		
		ucontrol->value.integer.value[i] = (regval >> shift) & mask;
		if (invert)
			ucontrol->value.integer.value[i] = mask - ucontrol->value.integer.value[i];
	}

	return rc;
}

int snd_ak4535_put_reg(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	snd_card_h5400_ak4535_t *chip = snd_kcontrol_chip(kcontrol);
	struct i2c_client *clnt = NULL;
	struct ak4535 *akm = NULL;
	int count = (kcontrol->private_value >> 0) & 3;
	int reg = (kcontrol->private_value >> 5) & 15;
	int shift = (kcontrol->private_value >> 9) & 7;
	int mask = (kcontrol->private_value >> 12) & 63;
	int invert = (kcontrol->private_value >> 18) & 1;
	int i;
	unsigned char val;
	unsigned char regval;

	if (chip)
		clnt = chip->ak4535;
	if (clnt)
		akm = i2c_get_clientdata(clnt);

	if (0)
	printk("snd_ak4535_put_reg: control=%s reg=%d\n",
	       kcontrol->id.name, reg);

	for (i = 0; i < count; i++) {
		regval = akm->regs[reg+i];
		val = (ucontrol->value.integer.value[i] & mask);
		if (invert)
			val = mask - val;
		regval &= ~(mask << shift);
		regval |= (val << shift);

		akm->regs[reg+i] = regval;

		ak4535_write_reg(clnt, reg+i, regval);
	}
	return 0;
}

#if 0
#define AK4535_ENUM(xname, where, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .info = snd_ak4535_info_enum, \
  .get = snd_ak4535_get_enum, .put = snd_ak4535_put_enum, \
  .private_value = where | (reg << 5) | (shift << 9) | (mask << 12) | (invert << 18) \
}

static int snd_ak4535_info_enum(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t * uinfo)
{
	int where = kcontrol->private_value & 31;
	const char **texts;
	
	// this register we don't handle this way
	if (!ak4535_enum_items[where])
		return -EINVAL;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ak4535_enum_items[where];

	if (uinfo->value.enumerated.item >= ak4535_enum_items[where])
		uinfo->value.enumerated.item = ak4535_enum_items[where] - 1;

	texts = ak4535_enum_names[where];
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ak4535_get_enum(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct i2c_client *clnt = snd_kcontrol_chip(kcontrol);
	int where = kcontrol->private_value & 31;        
        
	ucontrol->value.enumerated.item[0] = akm->cfg[where];
	return 0;
}

static int snd_ak4535_put_enum(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct i2c_client *clnt = snd_kcontrol_chip(kcontrol);
	ak4535_t *akm = clnt->driver_data;
	int where = kcontrol->private_value & 31;        
	int reg = (kcontrol->private_value >> 5) & 15;
	int shift = (kcontrol->private_value >> 9) & 7;
	int mask = (kcontrol->private_value >> 12) & 63;

	akm->cfg[where] = (ucontrol->value.enumerated.item[0] & mask);
	
	return snd_ak4535_update_bits(clnt, reg, mask, shift, akm->cfg[where], FLUSH);
}
#endif /* 0 */

#define AK4535_CONTROLS (sizeof(snd_ak4535_controls)/sizeof(snd_kcontrol_new_t))

static snd_kcontrol_new_t snd_ak4535_controls[] = {

	AK4535_REG("ADC Power Switch", 		REG_PWR1, 0, 1, 0),
	AK4535_REG("Mic Power Switch", 		REG_PWR1, 1, 1, 0),
	AK4535_REG("Aux In Power Switch",       REG_PWR1, 2, 1, 0),
	AK4535_REG("Mono Out Power Switch", 	REG_PWR1, 3, 1, 0),
	AK4535_REG("Line Out Power Switch", 	REG_PWR1, 4, 1, 0),
	AK4535_REG("VCOM Power Switch", 	REG_PWR1, 7, 1, 0),

	AK4535_REG("DAC Power Switch", 		   REG_PWR2, 0, 1, 0),
	AK4535_REG("Headphone Right Power Switch", REG_PWR2, 1, 1, 0),
	AK4535_REG("Headphone Left Power Switch",  REG_PWR2, 2, 1, 0),
	AK4535_REG("Speaker Power Switch",        REG_PWR2, 3, 1, 0),

	AK4535_REG("Tone Control - Bass",	REG_DAC, 2, 3, 0),

	AK4535_REG("Mic 20dB Gain Switch",	REG_MIC, 0, 1, 0),
	AK4535_REG("Internal/External Mic Select Switch",		REG_MIC, 1, 1, 1),
	AK4535_REG("Mic Playback Switch",	REG_MIC, 2, 1, 1),
	AK4535_REG("Internal Mic Power Switch",	REG_MIC, 3, 1, 0),
	AK4535_REG("External Mic Power Switch",	REG_MIC, 3, 1, 0),

	AK4535_REG("Mic Playback Volume",	REG_PGA, 0, 127, 1),

	AK4535_REG_DUAL("PCM Playback Volume",	REG_LATT, 0, 63, 1),

	AK4535_REG("External Mic Detect",	REG_STATUS, 0, 1, 0),

#if 0
	AK4535_ENUM("Peak detection", CMD_PEAK, data0_2, 5, 1, 0),
	AK4535_ENUM("De-emphasis", CMD_DEEMP, data0_2, 3, 3, 0),
	AK4535_ENUM("Mixer mode", CMD_MIXER, ext2, 0, 3, 0),
	AK4535_ENUM("Filter mode", CMD_FILTER, data0_2, 0, 3, 0),
#endif

#if 0
	AK4535_2REGS("Gain Input Amplifier Gain (channel 2)", CMD_IG, ext4, ext5, 0, 0, 3, 31, 0),
#endif
};


int snd_card_ak4535_mixer_new(snd_card_t *card)
{
	snd_card_h5400_ak4535_t *chip = card->private_data;
	int idx, err;

	printk("%s:\n", __FUNCTION__);
        
	snd_assert(card != NULL, return -EINVAL);

	for (idx = 0; idx < AK4535_CONTROLS; idx++) {
		printk("ak4535 adding control %s to card %p\n", snd_ak4535_controls[idx].name, card);
		if ((err = snd_ctl_add(card, snd_ctl_new1(&snd_ak4535_controls[idx], chip))) < 0) {
			return err;
		}
	}


#if 0
	/* set default values */
	{
		snd_kcontrol_t * tmp_k = kmalloc(sizeof(snd_kcontrol_t), GFP_KERNEL);
		snd_ctl_elem_value_t * tmp_v = kmalloc(sizeof(snd_ctl_elem_value_t), GFP_KERNEL);

		tmp_k->private_value = CMD_VOLUME;
		tmp_v->value.integer.value[0] = 0;
		snd_ak4535_put_reg(tmp_k, tmp_v);

		kfree(tmp_k);
		kfree(tmp_v);
	}
#endif
	strcpy(card->mixername, "AK4535 Mixer");

	return 0;
}

void snd_card_ak4535_mixer_del(snd_card_t *card)
{
	printk("ak4535 mixer_del\n");
}

EXPORT_SYMBOL(snd_card_ak4535_mixer_new);
EXPORT_SYMBOL(snd_card_ak4535_mixer_del);

