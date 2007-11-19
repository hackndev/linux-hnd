/*
 * Philips UDA1341 mixer device driver
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * Portions are Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2000-08-19	Erik Bunce	More inline w/ OSS API and UDA1341 docs
 * 				including fixed AGC and audio source handling
 *
 * 2000-11-30	Nicolas Pitre	- More mixer functionalities.
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-08-13	Russell King	Re-written as part of the L3 interface
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/control.h>

/*
 * UDA1341 L3 address and command types
 */
#define UDA1341_L3ADDR		5
#define UDA1341_DATA0		(UDA1341_L3ADDR << 2 | 0)
#define UDA1341_DATA1		(UDA1341_L3ADDR << 2 | 1)
#define UDA1341_STATUS		(UDA1341_L3ADDR << 2 | 2)

struct uda1341_regs {
	unsigned char	stat0;
#define STAT0			0x00
#define STAT0_RST		(1 << 6)
#define STAT0_SC_MASK		(3 << 4)
#define STAT0_SC_512FS		(0 << 4)
#define STAT0_SC_384FS		(1 << 4)
#define STAT0_SC_256FS		(2 << 4)
#define STAT0_IF_MASK		(7 << 1)
#define STAT0_IF_I2S		(0 << 1)
#define STAT0_IF_LSB16		(1 << 1)
#define STAT0_IF_LSB18		(2 << 1)
#define STAT0_IF_LSB20		(3 << 1)
#define STAT0_IF_MSB		(4 << 1)
#define STAT0_IF_LSB16MSB	(5 << 1)
#define STAT0_IF_LSB18MSB	(6 << 1)
#define STAT0_IF_LSB20MSB	(7 << 1)
#define STAT0_DC_FILTER		(1 << 0)

	unsigned char	stat1;
#define STAT1			0x80
#define STAT1_DAC_GAIN		(1 << 6)	/* gain of DAC */
#define STAT1_ADC_GAIN		(1 << 5)	/* gain of ADC */
#define STAT1_ADC_POL		(1 << 4)	/* polarity of ADC */
#define STAT1_DAC_POL		(1 << 3)	/* polarity of DAC */
#define STAT1_DBL_SPD		(1 << 2)	/* double speed playback */
#define STAT1_ADC_ON		(1 << 1)	/* ADC powered */
#define STAT1_DAC_ON		(1 << 0)	/* DAC powered */

	unsigned char	data0[9];
#define REGDIR0	0
#define REGDIR1	1
#define REGDIR2	2
#define REGEXT0	3
#define REGEXT1	4
#define REGEXT2	5
#define REGEXT4	6
#define REGEXT5	7
#define REGEXT6	8

#define DIR0			0x00
#define DATA0_VOLUME_MASK	0x3f
#define DATA0_VOLUME(x)		(x)

#define DIR1			0x40
#define DATA1_BASS(x)		((x) << 2)
#define DATA1_BASS_MASK		(15 << 2)
#define DATA1_TREBLE(x)		((x))
#define DATA1_TREBLE_MASK	(3)

#define DIR2			0x80
#define DATA2_PEAKAFTER		(1 << 5)
#define DATA2_DEEMP_NONE	(0 << 3)
#define DATA2_DEEMP_32KHz	(1 << 3)
#define DATA2_DEEMP_44KHz	(2 << 3)
#define DATA2_DEEMP_48KHz	(3 << 3)
#define DATA2_MUTE		(1 << 2)
#define DATA2_FILTER_FLAT	(0 << 0)
#define DATA2_FILTER_MIN	(1 << 0)
#define DATA2_FILTER_MAX	(3 << 0)

#define EXTADDR(n)		(0xc0 | (n))
#define EXTDATA(d)		(0xe0 | (d))

#define EXT0			0
#define EXT0_CH1_GAIN(x)	(x)

#define EXT1			1
#define EXT1_CH2_GAIN(x)	(x)

#define EXT2			2
#define EXT2_MIC_GAIN_MASK	(7 << 2)
#define EXT2_MIC_GAIN(x)	((x) << 2)
#define EXT2_MIXMODE_DOUBLEDIFF	(0)
#define EXT2_MIXMODE_CH1	(1)
#define EXT2_MIXMODE_CH2	(2)
#define EXT2_MIXMODE_MIX	(3)

#define EXT4			4
#define EXT4_AGC_ENABLE		(1 << 4)
#define EXT4_INPUT_GAIN_MASK	(3)
#define EXT4_INPUT_GAIN(x)	((x) & 3)

#define EXT5			5
#define EXT5_INPUT_GAIN(x)	((x) >> 2)

#define EXT6			6
#define EXT6_AGC_CONSTANT_MASK	(7 << 2)
#define EXT6_AGC_CONSTANT(x)	((x) << 2)
#define EXT6_AGC_LEVEL_MASK	(3)
#define EXT6_AGC_LEVEL(x)	(x)
};

struct uda1341 {
	spinlock_t		lock;
	int			active;
	struct l3_adapter	*l3_bus;
	struct uda1341_regs	regs;
};

static const unsigned char regv[] = {
	[REGDIR0] = DIR0,
	[REGDIR1] = DIR1,
	[REGDIR2] = DIR2,
	[REGEXT0] = EXT0,
	[REGEXT1] = EXT1,
	[REGEXT2] = EXT2,
	[REGEXT4] = EXT4,
	[REGEXT5] = EXT5,
	[REGEXT6] = EXT6
};

static inline char *uda1341_add_reg(struct uda1341 *uda, char *p, int ctl)
{
	if (ctl < REGEXT0) {
		*p++ = regv[ctl] | uda->regs.data0[ctl];
	} else {
		*p++ = regv[ctl];
		*p++ = uda->regs.data0[ctl];
	}
	return p;
}

static void uda1341_sync(struct uda1341 *uda)
{
	char buf[24], *p = buf;

	*p++ = STAT0 | uda->regs.stat0;
	*p++ = STAT1 | uda->regs.stat1;

	if (p != buf)
		l3_write(uda->l3_bus, UDA1341_STATUS, buf, p - buf);

	p = uda1341_add_reg(uda, buf, REGDIR0);
	p = uda1341_add_reg(uda, p, REGDIR1);
	p = uda1341_add_reg(uda, p, REGDIR2);
	p = uda1341_add_reg(uda, p, REGEXT0);
	p = uda1341_add_reg(uda, p, REGEXT1);
	p = uda1341_add_reg(uda, p, REGEXT2);
	p = uda1341_add_reg(uda, p, REGEXT4);
	p = uda1341_add_reg(uda, p, REGEXT5);
	p = uda1341_add_reg(uda, p, REGEXT6);

	if (p != buf)
		l3_write(uda->l3_bus, UDA1341_DATA0, buf, p - buf);
}

int uda1341_configure(struct uda1341 *uda, struct uda1341_cfg *conf)
{
	int ret = 0;

	uda->regs.stat0 &= ~(STAT0_SC_MASK | STAT0_IF_MASK);

	switch (conf->fs) {
	case 512: uda->regs.stat0 |= STAT0_SC_512FS;	break;
	case 384: uda->regs.stat0 |= STAT0_SC_384FS;	break;
	case 256: uda->regs.stat0 |= STAT0_SC_256FS;	break;
	default:  ret = -EINVAL;			break;
	}

	switch (conf->format) {
	case FMT_I2S:		uda->regs.stat0 |= STAT0_IF_I2S;	break;
	case FMT_LSB16:		uda->regs.stat0 |= STAT0_IF_LSB16;	break;
	case FMT_LSB18:		uda->regs.stat0 |= STAT0_IF_LSB18;	break;
	case FMT_LSB20:		uda->regs.stat0 |= STAT0_IF_LSB20;	break;
	case FMT_MSB:		uda->regs.stat0 |= STAT0_IF_MSB;	break;
	case FMT_LSB16MSB:	uda->regs.stat0 |= STAT0_IF_LSB16MSB;	break;
	case FMT_LSB18MSB:	uda->regs.stat0 |= STAT0_IF_LSB18MSB;	break;
	case FMT_LSB20MSB:	uda->regs.stat0 |= STAT0_IF_LSB20MSB;	break;
	default:		ret = -EINVAL;				break;
	}

	if (ret == 0 && uda->active) {
		char buf = uda->regs.stat0 | STAT0;
		l3_write(uda->l3_bus, UDA1341_STATUS, &buf, 1);
	}
	return ret;
}

#if 0
static int uda1341_update_indirect(struct uda1341 *uda, int cmd, void *arg)
{
	struct l3_gain *gain = arg;
	struct l3_agc *agc = arg;
	char buf[8], *p = buf;
	int val, ret = 0;

	switch (cmd) {
	case L3_INPUT_AGC:
		if (agc->channel == 2) {
			if (agc->enable)
				uda->regs.data0[REGEXT4] |= EXT4_AGC_ENABLE;
			else
				uda->regs.data0[REGEXT4] &= ~EXT4_AGC_ENABLE;
			agc->level
			agc->attack
			agc->decay
			ADD_EXTFIELD(EXT4, data0[REGEXT4]);
		} else
			ret = -EINVAL;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (ret == 0 && uda->active)
		l3_write(uda->l3_bus, UDA1341_DATA0, buf, p - buf);

	return ret;
}
#endif

struct uda1341_ctl {
	const char *name;
	snd_kcontrol_new_t *ctl;
	unsigned char min;
	unsigned char max;
	unsigned char def;
	unsigned char reg;
	unsigned char mask;
	unsigned char off;
};

static int uda1341_ctl_simple_info(snd_kcontrol_t *kc, snd_ctl_elem_info_t *ei)
{
	struct uda1341_ctl *ctl = (struct uda1341_ctl *)kc->private_value;

	ei->type = ctl->mask > 1 ? SNDRV_CTL_ELEM_TYPE_INTEGER :
				  SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	ei->count = 1;
	if (ctl->min > ctl->max) {
		ei->value.integer.min = ctl->mask - ctl->min;
		ei->value.integer.max = ctl->mask - ctl->max;
	} else {
		ei->value.integer.min = ctl->min;
		ei->value.integer.max = ctl->max;
	}
	return 0;
}

static int uda1341_ctl_simple_get(snd_kcontrol_t *kc, snd_ctl_elem_value_t *ev)
{
	struct uda1341 *uda = kc->private_data;
	struct uda1341_ctl *ctl = (struct uda1341_ctl *)kc->private_value;
	unsigned int val;

	val = (uda->regs.data0[ctl->reg] >> ctl->off) & ctl->mask;
	if (ctl->min > ctl->max)
		val = ctl->mask - val;

	ev->value.integer.value[0] = val;

	return 0;
}

static int uda1341_ctl_simple_put(snd_kcontrol_t *kc, snd_ctl_elem_value_t *ev)
{
	struct uda1341 *uda = kc->private_data;
	struct uda1341_ctl *ctl = (struct uda1341_ctl *)kc->private_value;
	unsigned int val = ev->value.integer.value[0] & ctl->mask;

	if (ctl->min > ctl->max)
		val = ctl->mask - val;

	spin_lock(&uda->lock);
	uda->regs.data0[ctl->reg] &= ~(ctl->mask << ctl->off);
	uda->regs.data0[ctl->reg] |= val << ctl->off;
	spin_unlock(&uda->lock);

	if (uda->active) {
		char buf[2], *p = buf;

		p = uda1341_add_reg(uda, buf, ctl->reg);

		l3_write(uda->l3_bus, UDA1341_DATA0, buf, p - buf);
	}

	return 0;
}

static snd_kcontrol_new_t uda1341_ctl_simple = {
	.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
	.access	= SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info	= uda1341_ctl_simple_info,
	.get	= uda1341_ctl_simple_get,
	.put	= uda1341_ctl_simple_put,
};

static struct uda1341_ctl uda1341_controls[] = {
	{
		.name	= "Master Playback Volume",
		.ctl	= &uda1341_ctl_simple,
		.min	= 62,
		.max	= 1,
		.def	= 62,
		.reg	= REGDIR0,
		.mask	= 63,
		.off	= 0,
	},
	{
		.name	= "Master Playback Switch",
		.ctl	= &uda1341_ctl_simple,
		.min	= 0,
		.max	= 1,
		.def	= 0,
		.reg	= REGDIR2,
		.mask	= 1,
		.off	= 2,
	},
	{
		.name	= "Tone Control - Bass",
		.ctl	= &uda1341_ctl_simple,
		.min	= 0,
		.max	= 14,
		.def	= 0,
		.reg	= REGDIR1,
		.mask	= 15,
		.off	= 2,
	},
	{
		.name	= "Tone Control - Treble",
		.ctl	= &uda1341_ctl_simple,
		.min	= 0,
		.max	= 3,
		.def	= 0,
		.reg	= REGDIR1,
		.mask	= 3,
		.off	= 0,
	},
	{
		.name	= "Line Capture Volume",
		.ctl	= &uda1341_ctl_simple,
		.min	= 31,
		.max	= 0,
		.def	= 31,
		.reg	= REGEXT0,
		.mask	= 31,
		.off	= 0,
	},
	{
		.name	= "Mic Capture Volume",
		.ctl	= &uda1341_ctl_simple,
		.min	= 31,
		.max	= 0,
		.def	= 31,
		.reg	= REGEXT1,
		.mask	= 31,
		.off	= 0,
	},
};

static int uda1341_add_ctl(snd_card_t *card, struct uda1341 *uda, struct uda1341_ctl *udactl)
{
	snd_kcontrol_t *ctl;
	int ret = -ENOMEM;

	ctl = snd_ctl_new1(udactl->ctl, uda);
	if (ctl) {
		strlcpy(ctl->id.name, udactl->name, sizeof(ctl->id.name));
		ctl->private_value = (unsigned long)udactl;

		uda->regs.data0[udactl->reg] &= ~(udactl->mask << udactl->off);
		uda->regs.data0[udactl->reg] |= udactl->def << udactl->off;

		ret = snd_ctl_add(card, ctl);
		if (ret < 0)
			snd_ctl_free_one(ctl);
	}
	return ret;
}

static int uda1341_free(snd_device_t *dev)
{
	struct uda1341 *uda = dev->device_data;

	l3_put_adapter(uda->l3_bus);
	kfree(uda);

	return 0;
}

static snd_device_ops_t uda1341_ops = {
	.dev_free = uda1341_free,
};

struct uda1341 *uda1341_create(snd_card_t *card, const char *adapter)
{
	struct uda1341 *uda;
	int i, ret;

	uda = kmalloc(sizeof(*uda), GFP_KERNEL);
	if (!uda)
		return ERR_PTR(-ENOMEM);

	memset(uda, 0, sizeof(*uda));

	ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, uda, &uda1341_ops);
	if (ret)
		goto err;

	uda->regs.stat0 = STAT0_SC_256FS | STAT0_IF_LSB16;
	uda->regs.stat1 = STAT1_DAC_GAIN | STAT1_ADC_GAIN |
			  STAT1_ADC_ON | STAT1_DAC_ON;
	uda->regs.data0[REGDIR2] = DATA2_PEAKAFTER | DATA2_DEEMP_NONE |
			    DATA2_FILTER_MAX;
	uda->regs.data0[REGEXT2] = EXT2_MIXMODE_MIX | EXT2_MIC_GAIN(4);
	uda->regs.data0[REGEXT4] = EXT4_AGC_ENABLE | EXT4_INPUT_GAIN(0);
	uda->regs.data0[REGEXT5] = EXT5_INPUT_GAIN(0);
	uda->regs.data0[REGEXT6] = EXT6_AGC_CONSTANT(3) | EXT6_AGC_LEVEL(0);

	uda->l3_bus = l3_get_adapter(adapter);
	if (!uda->l3_bus) {
		kfree(uda);
		return ERR_PTR(-ENOENT);
	}

	strlcpy(card->mixername, "Philips UDA1341", sizeof(card->mixername));
	for (i = 0; i < ARRAY_SIZE(uda1341_controls); i++) {
		ret = uda1341_add_ctl(card, uda, &uda1341_controls[i]);
		if (ret)
			goto err;
	}

	return uda;

 err:
	kfree(uda);
	return ERR_PTR(ret);
}

int uda1341_open(struct uda1341 *uda)
{
	unsigned char buf[2];

	uda->active = 1;

	buf[0] = uda->regs.stat0 | STAT0_RST;
	buf[1] = uda->regs.stat0;

	l3_write(uda->l3_bus, UDA1341_STATUS, buf, 2);

	/* resend all parameters */
	uda1341_sync(uda);

	return 0;
}

void uda1341_close(struct uda1341 *uda)
{
	unsigned char buf = uda->regs.stat1 & ~(STAT1_ADC_ON | STAT1_DAC_ON);

	l3_write(uda->l3_bus, UDA1341_STATUS, &buf, 1);

	uda->active = 0;
}

void uda1341_suspend(struct uda1341 *uda)
{
	unsigned char buf = uda->regs.stat1 & ~(STAT1_ADC_ON | STAT1_DAC_ON);

	if (uda->active)
		l3_write(uda->l3_bus, UDA1341_STATUS, &buf, 1);
}

void uda1341_resume(struct uda1341 *uda)
{
	unsigned char buf[2];

	if (uda->active) {
		buf[0] = uda->regs.stat0 | STAT0_RST;
		buf[1] = uda->regs.stat0;

		l3_write(uda->l3_bus, UDA1341_STATUS, buf, 2);

		/* resend all parameters */
		uda1341_sync(uda);
	}
}

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Philips UDA1341 CODEC driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(uda1341_create);
EXPORT_SYMBOL(uda1341_open);
EXPORT_SYMBOL(uda1341_close);
EXPORT_SYMBOL(uda1341_suspend);
EXPORT_SYMBOL(uda1341_resume);
